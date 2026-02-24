/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * workspace.c - Virtual desktop / workspace management
 */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>

#include "config.h"
#include "ipc.h"
#include "server.h"
#include "toolbar.h"
#include "view.h"
#include "workspace.h"

static struct wm_workspace *
workspace_create(struct wm_server *server, const char *name, int index)
{
	struct wm_workspace *ws = calloc(1, sizeof(*ws));
	if (!ws) {
		return NULL;
	}

	ws->server = server;
	ws->name = strdup(name);
	if (!ws->name) {
		free(ws);
		return NULL;
	}
	ws->index = index;

	/*
	 * Each workspace gets its own scene tree as a child of the
	 * server's normal layer tree. Switching workspaces means
	 * enabling/disabling these subtrees.
	 */
	ws->tree = wlr_scene_tree_create(server->view_layer_normal);
	if (!ws->tree) {
		free(ws->name);
		free(ws);
		return NULL;
	}

	/* Only the first workspace starts enabled */
	wlr_scene_node_set_enabled(&ws->tree->node, index == 0);

	wl_list_insert(server->workspaces.prev, &ws->link);
	return ws;
}

static void
workspace_destroy(struct wm_workspace *ws)
{
	wl_list_remove(&ws->link);
	wlr_scene_node_destroy(&ws->tree->node);
	free(ws->name);
	free(ws);
}

void
wm_workspaces_init(struct wm_server *server, int count)
{
	wl_list_init(&server->workspaces);
	server->workspace_count = count;

	/* Create the sticky tree (always visible, above all workspaces) */
	server->sticky_tree = wlr_scene_tree_create(server->view_layer_normal);
	wlr_scene_node_raise_to_top(&server->sticky_tree->node);

	/* Create workspaces, using config names when available */
	struct wm_config *config = server->config;
	for (int i = 0; i < count; i++) {
		const char *name;
		char default_name[32];
		if (config && i < config->workspace_name_count &&
		    config->workspace_names[i]) {
			name = config->workspace_names[i];
		} else {
			snprintf(default_name, sizeof(default_name),
				"Workspace %d", i + 1);
			name = default_name;
		}
		struct wm_workspace *ws = workspace_create(server, name, i);
		if (!ws) {
			wlr_log(WLR_ERROR, "failed to create workspace %d", i);
			continue;
		}
	}

	/* Set the first workspace as current */
	server->current_workspace = wm_workspace_get(server, 0);
}

void
wm_workspaces_finish(struct wm_server *server)
{
	struct wm_workspace *ws, *tmp;
	wl_list_for_each_safe(ws, tmp, &server->workspaces, link) {
		workspace_destroy(ws);
	}
	server->current_workspace = NULL;
}

struct wm_workspace *
wm_workspace_get_current(struct wm_server *server)
{
	return server->current_workspace;
}

struct wm_workspace *
wm_workspace_get(struct wm_server *server, int index)
{
	struct wm_workspace *ws;
	wl_list_for_each(ws, &server->workspaces, link) {
		if (ws->index == index) {
			return ws;
		}
	}
	return NULL;
}

void
wm_workspace_switch(struct wm_server *server, int index)
{
	struct wm_workspace *target = wm_workspace_get(server, index);
	if (!target || target == server->current_workspace) {
		return;
	}

	struct wm_workspace *old = server->current_workspace;

	/* Disable old workspace scene tree */
	wlr_scene_node_set_enabled(&old->tree->node, false);

	/* Enable new workspace scene tree */
	wlr_scene_node_set_enabled(&target->tree->node, true);

	server->current_workspace = target;

	wlr_log(WLR_DEBUG, "workspace switch: %s -> %s",
		old->name, target->name);

	/* Update toolbar workspace buttons and icon bar */
	wm_toolbar_update_workspace(server->toolbar);

	/* Broadcast workspace switch event via IPC */
	{
		char buf[256];
		snprintf(buf, sizeof(buf),
			"{\"event\":\"workspace\","
			"\"index\":%d,"
			"\"name\":\"%s\"}",
			target->index, target->name);
		wm_ipc_broadcast_event(&server->ipc,
			WM_IPC_EVENT_WORKSPACE, buf);
	}

	/*
	 * Focus the topmost view on the new workspace.
	 * Walk the views list (stacking order) and find the first
	 * view that belongs to this workspace.
	 */
	struct wm_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (view->workspace == target) {
			wm_focus_view(view,
				view->xdg_toplevel->base->surface);
			return;
		}
		/* Also consider sticky views */
		if (view->sticky) {
			wm_focus_view(view,
				view->xdg_toplevel->base->surface);
			return;
		}
	}

	/* No views on new workspace, clear focus */
	wm_unfocus_current(server);
}

void
wm_workspace_switch_next(struct wm_server *server)
{
	int next = (server->current_workspace->index + 1) %
		server->workspace_count;
	wm_workspace_switch(server, next);
}

void
wm_workspace_switch_prev(struct wm_server *server)
{
	int prev = (server->current_workspace->index - 1 +
		server->workspace_count) % server->workspace_count;
	wm_workspace_switch(server, prev);
}

void
wm_view_move_to_workspace(struct wm_view *view,
	struct wm_workspace *workspace)
{
	assert(view);
	assert(workspace);

	if (view->workspace == workspace) {
		return;
	}

	/* Reparent the view's scene tree to the new workspace tree */
	view->workspace = workspace;
	wlr_scene_node_reparent(&view->scene_tree->node, workspace->tree);

	/* Update toolbar icon bar (view changed workspace) */
	wm_toolbar_update_iconbar(view->server->toolbar);
}

void
wm_view_send_to_workspace(struct wm_server *server, int index)
{
	struct wm_view *view = server->focused_view;
	if (!view) {
		return;
	}

	struct wm_workspace *target = wm_workspace_get(server, index);
	if (!target || target == view->workspace) {
		return;
	}

	/*
	 * If the view is sticky, un-stick it first since we're
	 * explicitly moving it to a workspace.
	 */
	if (view->sticky) {
		wm_view_set_sticky(view, false);
	}

	wm_view_move_to_workspace(view, target);

	/*
	 * The view is now on a different workspace that may not be
	 * visible. If the target workspace is not the current one,
	 * hide the view and focus the next view on the current workspace.
	 */
	if (target != server->current_workspace) {
		/* View will be hidden because its workspace tree is disabled */
		struct wm_view *next;
		wl_list_for_each(next, &server->views, link) {
			if (next != view &&
					(next->workspace == server->current_workspace ||
					 next->sticky)) {
				wm_focus_view(next,
					next->xdg_toplevel->base->surface);
				return;
			}
		}
		wm_unfocus_current(server);
	}
}

void
wm_view_set_sticky(struct wm_view *view, bool sticky)
{
	if (view->sticky == sticky) {
		return;
	}

	view->sticky = sticky;

	if (sticky) {
		/* Move to sticky tree (always visible) */
		wlr_scene_node_reparent(&view->scene_tree->node,
			view->server->sticky_tree);
	} else {
		/* Move back to its workspace tree */
		if (view->workspace) {
			wlr_scene_node_reparent(&view->scene_tree->node,
				view->workspace->tree);
		}
	}
}
