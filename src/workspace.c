/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * workspace.c - Virtual desktop / workspace management
 */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>

#include "config.h"
#include "ipc.h"
#include "output.h"
#include "server.h"
#include "toolbar.h"
#include "util.h"
#include "view.h"
#include "wallpaper.h"
#include "workspace.h"

#define WS_ANIM_FRAME_MS 16 /* ~60fps */

/* Forward declarations for workspace transition */
static void ws_transition_cancel(struct wm_server *server);
static void ws_transition_finish(struct wm_ws_transition *trans);

/* Check if per-output workspace mode is active */
static bool
is_per_output_mode(struct wm_server *server)
{
	return server->config &&
		server->config->workspace_mode == WM_WORKSPACE_PER_OUTPUT;
}

/* Find which output a view is positioned on */
static struct wm_output *
get_view_output(struct wm_view *view)
{
	struct wlr_output *wlr_output = wlr_output_layout_output_at(
		view->server->output_layout,
		view->x + 1, view->y + 1);
	if (!wlr_output)
		return NULL;
	return wm_output_from_wlr(view->server, wlr_output);
}

/*
 * Update visibility of all views based on per-output workspace state.
 * In per-output mode, a view is visible if its workspace matches the
 * active workspace of the output it's positioned on.
 */
static void
update_views_per_output(struct wm_server *server)
{
	struct wm_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (view->sticky) {
			wlr_scene_node_set_enabled(
				&view->scene_tree->node, true);
			continue;
		}
		struct wm_output *output = get_view_output(view);
		if (!output) {
			/* View not on any output, keep visible */
			wlr_scene_node_set_enabled(
				&view->scene_tree->node, true);
			continue;
		}
		bool visible = (view->workspace == output->active_workspace);
		wlr_scene_node_set_enabled(
			&view->scene_tree->node, visible);
	}
}

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

	/*
	 * In per-output mode, all workspace trees must be enabled because
	 * visibility is controlled per-view, not per-tree.
	 */
	if (is_per_output_mode(server)) {
		struct wm_workspace *ws;
		wl_list_for_each(ws, &server->workspaces, link) {
			wlr_scene_node_set_enabled(&ws->tree->node, true);
		}
	}
}

void
wm_workspaces_finish(struct wm_server *server)
{
	ws_transition_cancel(server);

	struct wm_workspace *ws, *tmp;
	wl_list_for_each_safe(ws, tmp, &server->workspaces, link) {
		workspace_destroy(ws);
	}
	server->current_workspace = NULL;
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

/* Ease-in-out cubic for smooth animation */
static float
ease_in_out_cubic(float t)
{
	if (t < 0.5f)
		return 4.0f * t * t * t;
	float f = 2.0f * t - 2.0f;
	return 0.5f * f * f * f + 1.0f;
}

static void
ws_transition_finish(struct wm_ws_transition *trans)
{
	/* Snap to final positions */
	wlr_scene_node_set_position(&trans->new_ws->tree->node, 0, 0);
	wlr_scene_node_set_enabled(&trans->old_ws->tree->node, false);
	wlr_scene_node_set_position(&trans->old_ws->tree->node, 0, 0);

	wl_event_source_remove(trans->timer);
	trans->server->ws_transition = NULL;
	free(trans);
}

static int
ws_transition_timer_callback(void *data)
{
	struct wm_ws_transition *trans = data;

	trans->elapsed_ms += WS_ANIM_FRAME_MS;

	if (trans->elapsed_ms >= trans->duration_ms) {
		ws_transition_finish(trans);
		return 0;
	}

	float t = (float)trans->elapsed_ms / (float)trans->duration_ms;
	float eased = ease_in_out_cubic(t);

	/* Old workspace slides out, new workspace slides in */
	int offset = (int)((float)trans->output_width * eased);
	int old_x = -trans->direction * offset;
	int new_x = trans->direction * (trans->output_width - offset);

	wlr_scene_node_set_position(&trans->old_ws->tree->node,
		old_x, 0);
	wlr_scene_node_set_position(&trans->new_ws->tree->node,
		new_x, 0);

	wl_event_source_timer_update(trans->timer, WS_ANIM_FRAME_MS);
	return 0;
}

/* Cancel any in-progress workspace transition immediately */
static void
ws_transition_cancel(struct wm_server *server)
{
	struct wm_ws_transition *trans = server->ws_transition;
	if (!trans)
		return;
	ws_transition_finish(trans);
}

static void
ws_transition_start_slide(struct wm_server *server,
	struct wm_workspace *old_ws, struct wm_workspace *new_ws)
{
	/* Cancel any existing transition */
	ws_transition_cancel(server);

	int duration = server->config->workspace_transition_duration_ms;
	if (duration <= 0) {
		/* No animation, instant switch */
		wlr_scene_node_set_enabled(&old_ws->tree->node, false);
		wlr_scene_node_set_enabled(&new_ws->tree->node, true);
		return;
	}

	/* Determine slide direction based on workspace index */
	int direction = (new_ws->index > old_ws->index) ? 1 : -1;

	/* Get output width for slide distance */
	struct wm_output *output = wm_server_get_focused_output(server);
	int output_width = 1920; /* fallback */
	if (output) {
		struct wlr_box box;
		wlr_output_layout_get_box(server->output_layout,
			output->wlr_output, &box);
		if (box.width > 0)
			output_width = box.width;
	}

	struct wm_ws_transition *trans = calloc(1, sizeof(*trans));
	if (!trans) {
		/* Fallback to instant switch */
		wlr_scene_node_set_enabled(&old_ws->tree->node, false);
		wlr_scene_node_set_enabled(&new_ws->tree->node, true);
		return;
	}

	trans->server = server;
	trans->old_ws = old_ws;
	trans->new_ws = new_ws;
	trans->direction = direction;
	trans->output_width = output_width;
	trans->duration_ms = duration;
	trans->elapsed_ms = 0;

	trans->timer = wl_event_loop_add_timer(server->wl_event_loop,
		ws_transition_timer_callback, trans);
	if (!trans->timer) {
		free(trans);
		wlr_scene_node_set_enabled(&old_ws->tree->node, false);
		wlr_scene_node_set_enabled(&new_ws->tree->node, true);
		return;
	}

	server->ws_transition = trans;

	/* Enable both trees for the duration of the transition */
	wlr_scene_node_set_enabled(&old_ws->tree->node, true);
	wlr_scene_node_set_enabled(&new_ws->tree->node, true);

	/* Position new workspace off-screen */
	wlr_scene_node_set_position(&new_ws->tree->node,
		direction * output_width, 0);

	/* Start animation */
	wl_event_source_timer_update(trans->timer, WS_ANIM_FRAME_MS);
}

void
wm_workspace_switch(struct wm_server *server, int index)
{
	struct wm_workspace *target = wm_workspace_get(server, index);
	if (!target) {
		return;
	}

	if (is_per_output_mode(server)) {
		/*
		 * Per-output mode: switch only the focused output's
		 * active workspace. All workspace trees stay enabled;
		 * individual view visibility is managed per-view.
		 */
		struct wm_output *focused = wm_server_get_focused_output(
			server);
		if (!focused || target == focused->active_workspace)
			return;

		struct wm_workspace *old = focused->active_workspace;
		focused->active_workspace = target;

		/* Track as the server-wide current workspace too */
		server->current_workspace = target;

		/* Update per-view visibility */
		update_views_per_output(server);

		wlr_log(WLR_DEBUG,
			"workspace switch (output %s): %s -> %s",
			focused->wlr_output->name,
			old->name, target->name);
	} else {
		/* Global mode */
		if (target == server->current_workspace)
			return;

		struct wm_workspace *old = server->current_workspace;
		server->current_workspace = target;

		/* Check for animated transition */
		if (server->config &&
		    server->config->workspace_transition ==
		    WM_TRANSITION_SLIDE) {
			ws_transition_start_slide(server, old, target);
		} else {
			/* Cancel any in-progress transition first */
			ws_transition_cancel(server);
			/* Instant switch */
			wlr_scene_node_set_enabled(&old->tree->node, false);
			wlr_scene_node_set_enabled(&target->tree->node, true);
		}

		wlr_log(WLR_DEBUG, "workspace switch: %s -> %s",
			old->name, target->name);
	}

	/* Update toolbar workspace buttons and icon bar */
	wm_toolbar_update_workspace(server->toolbar);

	/* Switch wallpaper for the new workspace */
	wm_wallpaper_switch(server->wallpaper, target->index);

	/* Broadcast workspace switch event via IPC */
	{
		char esc_name[128], buf[256];
		wm_json_escape(esc_name, sizeof(esc_name), target->name);
		snprintf(buf, sizeof(buf),
			"{\"event\":\"workspace\","
			"\"index\":%d,"
			"\"name\":\"%s\"}",
			target->index, esc_name);
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
		if (view->workspace == target &&
		    view->scene_tree->node.enabled) {
			wm_focus_view(view,
				view->xdg_toplevel->base->surface);
			return;
		}
		/* Also consider sticky views */
		if (view->sticky && view->scene_tree->node.enabled) {
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
	struct wm_workspace *current = wm_workspace_get_active(server);
	int next = (current->index + 1) % server->workspace_count;
	wm_workspace_switch(server, next);
}

void
wm_workspace_switch_prev(struct wm_server *server)
{
	struct wm_workspace *current = wm_workspace_get_active(server);
	int prev = (current->index - 1 + server->workspace_count) %
		server->workspace_count;
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
	struct wm_workspace *active = wm_workspace_get_active(server);

	if (is_per_output_mode(server)) {
		/* Update per-view visibility */
		update_views_per_output(server);
	}

	if (target != active) {
		/* View will be hidden because its workspace tree is disabled
		 * (global mode) or per-view visibility toggled it off
		 * (per-output mode) */
		struct wm_view *next;
		wl_list_for_each(next, &server->views, link) {
			if (next != view &&
			    next->scene_tree->node.enabled &&
			    (next->workspace == active || next->sticky)) {
				wm_focus_view(next,
					next->xdg_toplevel->base->surface);
				return;
			}
		}
		wm_unfocus_current(server);
	}
}

void
wm_view_take_to_workspace(struct wm_server *server, int index)
{
	struct wm_view *view = server->focused_view;
	if (!view) {
		return;
	}

	struct wm_workspace *target = wm_workspace_get(server, index);
	if (!target) {
		return;
	}

	if (view->sticky) {
		wm_view_set_sticky(view, false);
	}

	if (view->workspace != target) {
		wm_view_move_to_workspace(view, target);
	}

	wm_workspace_switch(server, index);
	wm_focus_view(view, view->xdg_toplevel->base->surface);
}

void
wm_view_send_to_next_workspace(struct wm_server *server)
{
	struct wm_workspace *current = wm_workspace_get_active(server);
	int next = (current->index + 1) % server->workspace_count;
	wm_view_send_to_workspace(server, next);
}

void
wm_view_send_to_prev_workspace(struct wm_server *server)
{
	struct wm_workspace *current = wm_workspace_get_active(server);
	int prev = (current->index - 1 + server->workspace_count) %
		server->workspace_count;
	wm_view_send_to_workspace(server, prev);
}

void
wm_view_take_to_next_workspace(struct wm_server *server)
{
	struct wm_workspace *current = wm_workspace_get_active(server);
	int next = (current->index + 1) % server->workspace_count;
	wm_view_take_to_workspace(server, next);
}

void
wm_view_take_to_prev_workspace(struct wm_server *server)
{
	struct wm_workspace *current = wm_workspace_get_active(server);
	int prev = (current->index - 1 + server->workspace_count) %
		server->workspace_count;
	wm_view_take_to_workspace(server, prev);
}

void
wm_workspace_add(struct wm_server *server)
{
	int new_index = server->workspace_count;
	char name[32];
	snprintf(name, sizeof(name), "Workspace %d", new_index + 1);

	struct wm_workspace *ws = workspace_create(server, name, new_index);
	if (!ws) {
		wlr_log(WLR_ERROR, "failed to add workspace");
		return;
	}

	server->workspace_count++;
	wm_toolbar_update_workspace(server->toolbar);
}

void
wm_workspace_remove_last(struct wm_server *server)
{
	if (server->workspace_count <= 1) {
		return;
	}

	int last_index = server->workspace_count - 1;
	struct wm_workspace *last = wm_workspace_get(server, last_index);
	if (!last) {
		return;
	}

	/* Move all views from the last workspace to the previous one */
	struct wm_workspace *prev_ws = wm_workspace_get(server,
		last_index - 1);
	if (!prev_ws) {
		return;
	}

	struct wm_view *view, *tmp;
	wl_list_for_each_safe(view, tmp, &server->views, link) {
		if (view->workspace == last) {
			wm_view_move_to_workspace(view, prev_ws);
		}
	}

	/* If current workspace is the last one, switch to previous */
	if (server->current_workspace == last) {
		wm_workspace_switch(server, last_index - 1);
	}

	/*
	 * In per-output mode, reset any output whose active workspace
	 * points to the removed workspace.
	 */
	if (is_per_output_mode(server)) {
		struct wm_output *output;
		wl_list_for_each(output, &server->outputs, link) {
			if (output->active_workspace == last)
				output->active_workspace = prev_ws;
		}
		update_views_per_output(server);
	}

	workspace_destroy(last);
	server->workspace_count--;
	wm_toolbar_update_workspace(server->toolbar);
}

void
wm_workspace_switch_right(struct wm_server *server)
{
	struct wm_workspace *current = wm_workspace_get_active(server);
	int next = current->index + 1;
	if (next >= server->workspace_count)
		return;
	wm_workspace_switch(server, next);
}

void
wm_workspace_switch_left(struct wm_server *server)
{
	struct wm_workspace *current = wm_workspace_get_active(server);
	int prev = current->index - 1;
	if (prev < 0)
		return;
	wm_workspace_switch(server, prev);
}

void
wm_workspace_set_name(struct wm_server *server, const char *name)
{
	struct wm_workspace *ws = wm_workspace_get_active(server);
	if (!ws || !name)
		return;

	free(ws->name);
	ws->name = strdup(name);

	wm_toolbar_update_workspace(server->toolbar);
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

	/* In per-output mode, refresh visibility */
	if (is_per_output_mode(view->server))
		update_views_per_output(view->server);
}

struct wm_workspace *
wm_workspace_get_active(struct wm_server *server)
{
	if (is_per_output_mode(server)) {
		struct wm_output *output =
			wm_server_get_focused_output(server);
		if (output && output->active_workspace)
			return output->active_workspace;
	}
	return server->current_workspace;
}

