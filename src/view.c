/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * view.c - Window/view lifecycle, focus, and XDG shell handling
 */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>

#include "animation.h"
#include "config.h"
#include "decoration.h"
#include "foreign_toplevel.h"
#include "menu.h"
#include "ipc.h"
#include "output.h"
#include "placement.h"
#include "rules.h"
#include "server.h"
#include "slit.h"
#include "tabgroup.h"
#include "text_input.h"
#include "toolbar.h"
#include "util.h"
#include "view.h"
#include "view_focus.h"
#include "view_geometry.h"
#include "workspace.h"

/* --- Opacity helpers --- */

static void
set_buffer_opacity_iter(struct wlr_scene_buffer *buffer, int sx, int sy,
	void *user_data)
{
	float *opacity = user_data;
	wlr_scene_buffer_set_opacity(buffer, *opacity);
}

void
wm_view_set_opacity(struct wm_view *view, int alpha)
{
	if (!view || !view->scene_tree) {
		return;
	}

	/* Clamp to 0-255 */
	if (alpha < 0) alpha = 0;
	if (alpha > 255) alpha = 255;

	float opacity = (float)alpha / 255.0f;
	wlr_scene_node_for_each_buffer(&view->scene_tree->node,
		set_buffer_opacity_iter, &opacity);
}

/* --- View lookup --- */

struct wm_view *
wm_view_at(struct wm_server *server, double lx, double ly,
	struct wlr_surface **surface, double *sx, double *sy)
{
	struct wlr_scene_node *node =
		wlr_scene_node_at(&server->view_tree->node, lx, ly, sx, sy);
	if (!node || node->type != WLR_SCENE_NODE_BUFFER) {
		return NULL;
	}

	struct wlr_scene_buffer *scene_buffer =
		wlr_scene_buffer_from_node(node);
	struct wlr_scene_surface *scene_surface =
		wlr_scene_surface_try_from_buffer(scene_buffer);
	if (!scene_surface) {
		return NULL;
	}

	*surface = scene_surface->surface;

	/*
	 * Walk up the scene tree to the first node with data set. Only
	 * xdg views live in server->views; XWayland views and other
	 * scene users also set node.data, so validate before returning.
	 */
	struct wlr_scene_tree *tree = node->parent;
	while (tree && &tree->node != &server->view_tree->node) {
		if (tree->node.data) {
			struct wm_view *v;
			wl_list_for_each(v, &server->views, link) {
				if (v == tree->node.data)
					return v;
			}
			return NULL;
		}
		tree = tree->node.parent;
	}

	return NULL;
}

/* --- Backend-neutral accessors --- */

static const struct wm_view_impl *
impl_of(struct wm_view *view)
{
	return view->impl ? view->impl : &wm_xdg_view_impl;
}

struct wlr_surface *
wm_view_surface(struct wm_view *view)
{
	return view ? impl_of(view)->surface(view) : NULL;
}

void
wm_view_content_geometry(struct wm_view *view, struct wlr_box *geo)
{
	impl_of(view)->content_geometry(view, geo);
}

void
wm_view_set_size(struct wm_view *view, int width, int height)
{
	impl_of(view)->set_size(view, width, height);
}

void
wm_view_set_activated(struct wm_view *view, bool activated)
{
	impl_of(view)->set_activated(view, activated);
}

void
wm_view_set_maximized_state(struct wm_view *view, bool maximized)
{
	impl_of(view)->set_maximized(view, maximized);
}

void
wm_view_set_fullscreen_state(struct wm_view *view, bool fullscreen)
{
	impl_of(view)->set_fullscreen(view, fullscreen);
}

void
wm_view_set_tiled_state(struct wm_view *view, uint32_t edges)
{
	impl_of(view)->set_tiled(view, edges);
}

void
wm_view_set_minimized_state(struct wm_view *view, bool minimized)
{
	if (impl_of(view)->set_minimized)
		impl_of(view)->set_minimized(view, minimized);
}

void
wm_view_close(struct wm_view *view)
{
	impl_of(view)->close(view);
}

struct wl_client *
wm_view_client(struct wm_view *view)
{
	return impl_of(view)->client(view);
}

pid_t
wm_view_pid(struct wm_view *view)
{
	return impl_of(view)->pid(view);
}

void
wm_view_kill(struct wm_view *view)
{
	/* Wayland: drop the connection. X11 windows share Xwayland's
	 * connection, so kill the owning process instead (like xkill). */
	struct wl_client *client = wm_view_client(view);
	if (client) {
		wl_client_destroy(client);
		return;
	}
	pid_t pid = wm_view_pid(view);
	if (pid > 0)
		kill(pid, SIGKILL);
	else
		wm_view_close(view);
}

struct wm_view *
wm_view_parent(struct wm_view *view)
{
	return impl_of(view)->parent(view);
}

struct wm_view *
wm_view_from_surface(struct wm_server *server, struct wlr_surface *surface)
{
	if (!surface)
		return NULL;
	struct wm_view *v;
	wl_list_for_each(v, &server->views, link) {
		if (wm_view_surface(v) == surface)
			return v;
	}
	return NULL;
}

void
wm_view_move(struct wm_view *view, int x, int y)
{
	view->x = x;
	view->y = y;
	wlr_scene_node_set_position(&view->scene_tree->node, x, y);
	if (impl_of(view)->moved)
		impl_of(view)->moved(view);
}

/* --- Geometry helpers --- */

void
wm_view_get_geometry(struct wm_view *view, struct wlr_box *box)
{
	struct wlr_box geo;
	wm_view_content_geometry(view, &geo);
	box->x = view->x;
	box->y = view->y;
	box->width = geo.width;
	box->height = geo.height;
}

/* --- Interactive move/resize --- */

void
wm_view_begin_interactive(struct wm_view *view,
	enum wm_cursor_mode mode, uint32_t edges)
{
	struct wm_server *server = view->server;
	struct wlr_surface *focused =
		server->seat->pointer_state.focused_surface;

	/*
	 * Only the focused view may be interactively moved/resized.
	 * When the pointer is on a decoration (titlebar, handle, grip),
	 * pointer focus is NULL because decorations are scene buffers,
	 * not wl_surfaces. Allow interactive mode in that case.
	 */
	if (focused && view->xdg_toplevel->base->surface !=
			wlr_surface_get_root_surface(focused)) {
		return;
	}

	server->grabbed_view = view;
	server->cursor_mode = mode;

	if (mode == WM_CURSOR_MOVE) {
		server->grab_x = server->cursor->x - view->x;
		server->grab_y = server->cursor->y - view->y;
		/* Reset grab_geobox so stale resize values don't affect move */
		memset(&server->grab_geobox, 0, sizeof(server->grab_geobox));
	} else if (mode == WM_CURSOR_TABBING) {
		server->grab_x = server->cursor->x - view->x;
		server->grab_y = server->cursor->y - view->y;
		/* Save original position for cancel/restore */
		server->grab_geobox.x = view->x;
		server->grab_geobox.y = view->y;
		server->grab_geobox.width = 0;
		server->grab_geobox.height = 0;
	} else {
		struct wlr_box geo;
		wlr_xdg_surface_get_geometry(view->xdg_toplevel->base,
			&geo);

		double border_x = (view->x + geo.x) +
			((edges & WLR_EDGE_RIGHT) ? geo.width : 0);
		double border_y = (view->y + geo.y) +
			((edges & WLR_EDGE_BOTTOM) ? geo.height : 0);

		server->grab_x = server->cursor->x - border_x;
		server->grab_y = server->cursor->y - border_y;

		server->grab_geobox = geo;
		server->grab_geobox.x += view->x;
		server->grab_geobox.y += view->y;

		server->resize_edges = edges;
	}
}

/* --- Shared lifecycle (xdg-shell and XWayland) --- */

bool
wm_view_init_common(struct wm_view *view, struct wm_server *server)
{
	view->server = server;

	/* Assign unique view ID for IPC */
	static uint32_t next_view_id = 1;
	view->id = next_view_id++;
	if (next_view_id == 0)
		next_view_id = 1;

	/* Default opacity from config or fully opaque */
	view->focus_alpha = 255;
	view->unfocus_alpha = 255;
	if (server->config) {
		view->focus_alpha = server->config->window_focus_alpha;
		view->unfocus_alpha = server->config->window_unfocus_alpha;
	}

	/* Decorations shown by default */
	view->show_decoration = true;

	/* Default layer: normal */
	view->layer = WM_LAYER_NORMAL;

	/* Initialize tab group link (not in any group yet) */
	wl_list_init(&view->tab_link);

	/* Assign to active workspace (per-output aware) */
	view->workspace = wm_workspace_get_active(server);

	/*
	 * Create scene tree for this view under its workspace's tree.
	 * If workspaces aren't initialized yet, fall back to view_tree.
	 */
	struct wlr_scene_tree *parent = view->workspace ?
		view->workspace->tree : server->view_tree;
	view->scene_tree = wlr_scene_tree_create(parent);
	if (!view->scene_tree)
		return false;

	/* Initially hidden until map */
	wlr_scene_node_set_enabled(&view->scene_tree->node, false);

	/* Back-pointer for wm_view_at lookups */
	view->scene_tree->node.data = view;

	wl_list_insert(&server->views, &view->link);
	return true;
}

/* Position the client surface tree inside the decoration frame */
static void
offset_surface_tree(struct wm_view *view)
{
	int top, bottom, left, right;
	wm_decoration_get_extents(view->decoration,
		&top, &bottom, &left, &right);

	/*
	 * The surface tree is the child of view->scene_tree that isn't
	 * the decoration tree.
	 */
	struct wlr_scene_node *child;
	wl_list_for_each(child, &view->scene_tree->children, link) {
		if (child != &view->decoration->tree->node) {
			wlr_scene_node_set_position(child, left, top);
			break;
		}
	}
}

void
wm_view_map_common(struct wm_view *view, bool want_maximized,
	bool want_fullscreen)
{
	view->mapped = true;
	view->app_id_at_map = view->app_id && *view->app_id;

	/* Create server-side decorations */
	if (!view->decoration && view->server->style &&
	    !view->no_decoration) {
		view->decoration = wm_decoration_create(view,
			view->server->style);
		if (view->decoration)
			offset_surface_tree(view);
	}

	/* Apply per-window rules (workspace, geometry, state flags) */
	wm_rules_apply(&view->server->rules, view);

	/* Auto-group by rules (tab grouping) */
	struct wm_view *group_peer = wm_rules_find_group(
		&view->server->rules, view, view->server);
	if (group_peer && group_peer->tab_group) {
		wm_tab_group_add(group_peer->tab_group, view);
	}

	/* Auto-tab placement: group new windows with matching app_id */
	if (!view->tab_group && view->app_id && *view->app_id &&
	    view->server->config &&
	    view->server->config->auto_tab_placement) {
		struct wm_view *peer;
		wl_list_for_each(peer, &view->server->views, link) {
			if (peer == view || !peer->app_id || !peer->mapped)
				continue;
			if (peer->workspace != view->workspace &&
			    !peer->sticky)
				continue;
			if (strcmp(peer->app_id, view->app_id) != 0)
				continue;
			/* Found a matching peer */
			if (peer->tab_group) {
				wm_tab_group_add(peer->tab_group, view);
			} else {
				struct wm_tab_group *tg =
					wm_tab_group_create(peer);
				if (tg)
					wm_tab_group_add(tg, view);
			}
			break;
		}
	}

	/* Honor pre-map maximize/fullscreen requests from client */
	if (want_maximized && !view->maximized)
		wm_view_toggle_maximize(view);
	if (want_fullscreen && !view->fullscreen)
		wm_view_toggle_fullscreen(view);

	/* Apply automatic window placement */
	wm_placement_apply(view->server, view);
	wm_view_move(view, view->x, view->y);

	wlr_scene_node_set_enabled(&view->scene_tree->node, true);
	if (!view->no_focus)
		wm_focus_view(view, wm_view_surface(view));

	/* Broadcast window open event via IPC */
	{
		char esc_app[256], esc_title[256], buf[1024];
		wm_json_escape(esc_app, sizeof(esc_app),
			view->app_id);
		wm_json_escape(esc_title, sizeof(esc_title),
			view->title);
		snprintf(buf, sizeof(buf),
			"{\"event\":\"window_open\","
			"\"id\":%u,"
			"\"app_id\":\"%s\","
			"\"title\":\"%s\"}",
			view->id,
			esc_app, esc_title);
		wm_ipc_broadcast_event(&view->server->ipc,
			WM_IPC_EVENT_WINDOW_OPEN, buf);
	}

	/* Create foreign toplevel handle for taskbar clients */
	wm_foreign_toplevel_handle_create(view);

	/* Taskbar and iconbar entries */
	wm_toolbar_update_iconbar(view->server->toolbar);

	/* Animate fade-in if configured */
	if (view->server->config &&
	    view->server->config->animate_window_map) {
		wm_animation_start(view, WM_ANIM_FADE_IN,
			0, view->focus_alpha,
			view->server->config->animation_duration_ms,
			NULL, NULL);
	}
}

static void
unmap_fade_done(struct wm_view *view, bool completed, void *data)
{
	(void)completed;
	(void)data;
	if (!view)
		return;
	wlr_scene_node_set_enabled(&view->scene_tree->node, false);
	wm_toolbar_update_iconbar(view->server->toolbar);
}

void
wm_view_unmap_common(struct wm_view *view)
{
	view->mapped = false;
	if (view->server->focus_cycle.restore_minimized == view)
		view->server->focus_cycle.restore_minimized = NULL;

	/* Cancel any running fade-in animation */
	if (view->animation)
		wm_animation_cancel(view->animation);

	/* Destroy foreign toplevel handle (sends closed to taskbars) */
	wm_foreign_toplevel_handle_destroy(view);

	/* Broadcast window close event via IPC */
	{
		char buf[256];
		snprintf(buf, sizeof(buf),
			"{\"event\":\"window_close\","
			"\"id\":%u}",
			view->id);
		wm_ipc_broadcast_event(&view->server->ipc,
			WM_IPC_EVENT_WINDOW_CLOSE, buf);
	}

	/* If this was the grabbed view, cancel the grab */
	if (view == view->server->grabbed_view) {
		view->server->cursor_mode = WM_CURSOR_PASSTHROUGH;
		view->server->grabbed_view = NULL;
	}

	/* Cancel pending auto-raise if this view was the target */
	if (view == view->server->auto_raise_view) {
		if (view->server->auto_raise_timer)
			wl_event_source_timer_update(
				view->server->auto_raise_timer, 0);
		view->server->auto_raise_view = NULL;
	}

	/* If this was the focused view, focus the next one */
	if (view == view->server->focused_view)
		wm_focus_fallback(view->server, view);

	/* Animate fade-out if configured, otherwise hide immediately */
	if (view->server->config &&
	    view->server->config->animate_window_unmap) {
		int current_alpha = view->focus_alpha;
		wm_animation_start(view, WM_ANIM_FADE_OUT,
			current_alpha, 0,
			view->server->config->animation_duration_ms,
			unmap_fade_done, NULL);
	} else {
		wlr_scene_node_set_enabled(&view->scene_tree->node, false);
		wm_toolbar_update_iconbar(view->server->toolbar);
	}
}

void
wm_view_destroy_common(struct wm_view *view)
{
	if (!view->docked) {
		/* Cancel any running animation before destroying */
		if (view->animation) {
			/* Prevent done callback from using the view
			 * after free */
			struct wm_animation *anim = view->animation;
			anim->done_fn = NULL;
			wm_animation_cancel(anim);
		}

		/* Destroy foreign toplevel handle if still alive */
		wm_foreign_toplevel_handle_destroy(view);

		/* Destroy decorations */
		if (view->decoration) {
			wm_decoration_destroy(view->decoration);
			view->decoration = NULL;
		}

		/* Remove from tab group if grouped */
		if (view->tab_group) {
			wm_tab_group_remove(view);
		}
	}

	if (view == view->server->focused_view)
		view->server->focused_view = NULL;
	if (view == view->server->grabbed_view) {
		view->server->cursor_mode = WM_CURSOR_PASSTHROUGH;
		view->server->grabbed_view = NULL;
	}
	if (view->server->focus_cycle.restore_minimized == view)
		view->server->focus_cycle.restore_minimized = NULL;

	wl_list_remove(&view->link);

	/* Clear ShowDesktop saved view if this view is being destroyed */
	if (view->server->show_desktop_saved_view == view)
		view->server->show_desktop_saved_view = NULL;

	/* Update toolbar icon bar (view removed from list) */
	if (!view->docked) {
		wm_toolbar_update_iconbar(view->server->toolbar);
	}

	free(view->title);
	free(view->app_id);
	free(view->instance);
	free(view->role);
	view->title = view->app_id = view->instance = view->role = NULL;

	/* The frame tree (the client's surface tree goes with it) */
	wlr_scene_node_destroy(&view->scene_tree->node);
	view->scene_tree = NULL;
}

static void
minimize_fade_done(struct wm_view *view, bool completed, void *data)
{
	(void)completed;
	(void)data;
	if (!view)
		return;
	wlr_scene_node_set_enabled(&view->scene_tree->node, false);
	wm_toolbar_update_iconbar(view->server->toolbar);
}

void
wm_view_minimize(struct wm_view *view)
{
	if (!view->scene_tree->node.enabled)
		return; /* already minimized */

	/* Cancel any running animation */
	if (view->animation)
		wm_animation_cancel(view->animation);

	wm_foreign_toplevel_set_minimized(view, true);
	if (impl_of(view)->set_minimized)
		impl_of(view)->set_minimized(view, true);

	if (view == view->server->focused_view)
		wm_focus_fallback(view->server, view);

	/* Animate fade-out on minimize if configured */
	if (view->server->config &&
	    view->server->config->animate_minimize) {
		int current_alpha = view->unfocus_alpha;
		wm_animation_start(view, WM_ANIM_FADE_OUT,
			current_alpha, 0,
			view->server->config->animation_duration_ms,
			minimize_fade_done, NULL);
	} else {
		wlr_scene_node_set_enabled(&view->scene_tree->node, false);
		wm_toolbar_update_iconbar(view->server->toolbar);
	}
}

void
wm_view_unminimize(struct wm_view *view)
{
	if (view->animation)
		wm_animation_cancel(view->animation);
	if (impl_of(view)->set_minimized)
		impl_of(view)->set_minimized(view, false);
	deiconify_view(view);
}

void
wm_view_set_title(struct wm_view *view, const char *title)
{
	if (view->title && title && strcmp(view->title, title) == 0)
		return;
	free(view->title);
	view->title = title ? strdup(title) : NULL;

	/* Re-render decoration to update title text */
	if (view->decoration && view->server->style) {
		wm_decoration_update(view->decoration, view->server->style);
	}

	/* Update foreign toplevel handle */
	wm_foreign_toplevel_update_title(view);

	/* Broadcast title change event via IPC */
	{
		char esc_title[256], buf[512];
		wm_json_escape(esc_title, sizeof(esc_title), view->title);
		snprintf(buf, sizeof(buf),
			"{\"event\":\"window_title\","
			"\"id\":%u,"
			"\"title\":\"%s\"}",
			view->id,
			esc_title);
		wm_ipc_broadcast_event(&view->server->ipc,
			WM_IPC_EVENT_WINDOW_TITLE, buf);
	}

	/* Update toolbar icon bar to reflect title change */
	wm_toolbar_update_iconbar(view->server->toolbar);
}

void
wm_view_set_app_id(struct wm_view *view, const char *app_id)
{
	if (view->app_id && app_id && strcmp(view->app_id, app_id) == 0)
		return;
	free(view->app_id);
	view->app_id = app_id ? strdup(app_id) : NULL;

	/* Update foreign toplevel handle */
	wm_foreign_toplevel_update_app_id(view);

	/*
	 * Some clients only set their app_id after mapping. Rules are
	 * matched at map time, so match again the first time a mapped
	 * window gets an app_id.
	 */
	if (view->mapped && !view->app_id_at_map &&
	    view->app_id && *view->app_id) {
		view->app_id_at_map = true;
		wm_rules_apply(&view->server->rules, view);
		wm_view_move(view, view->x, view->y);
	}
	wm_toolbar_update_iconbar(view->server->toolbar);
}

/* --- Toggle decoration --- */

void
wm_view_toggle_decoration(struct wm_view *view)
{
	if (!view || !view->decoration) {
		return;
	}

	view->show_decoration = !view->show_decoration;

	if (view->show_decoration) {
		/* Show decoration and adjust view position for border */
		wlr_scene_node_set_enabled(
			&view->decoration->tree->node, true);

		int top, bottom, left, right;
		wm_decoration_get_extents(view->decoration,
			&top, &bottom, &left, &right);

		/* Offset the client surface tree within the decoration frame */
		struct wlr_scene_node *child;
		wl_list_for_each(child,
			&view->scene_tree->children, link) {
			if (child != &view->decoration->tree->node) {
				wlr_scene_node_set_position(child,
					left, top);
				break;
			}
		}
	} else {
		/* Hide decoration and reset client surface offset */
		wlr_scene_node_set_enabled(
			&view->decoration->tree->node, false);

		/* Move client surface back to origin of scene tree */
		struct wlr_scene_node *child;
		wl_list_for_each(child,
			&view->scene_tree->children, link) {
			if (child != &view->decoration->tree->node) {
				wlr_scene_node_set_position(child, 0, 0);
				break;
			}
		}
	}
}

/* --- Activate tab by index --- */

void
wm_view_activate_tab(struct wm_view *view, int index)
{
	if (!view || !view->tab_group || index < 0) {
		return;
	}

	struct wm_tab_group *group = view->tab_group;
	if (index >= group->count) {
		return;
	}

	int i = 0;
	struct wm_view *tab_view;
	wl_list_for_each(tab_view, &group->views, tab_link) {
		if (i == index) {
			wm_tab_group_activate(group, tab_view);
			return;
		}
		i++;
	}
}

/* --- Close all windows on current workspace --- */

void
wm_view_close_all(struct wm_server *server)
{
	struct wm_view *v;
	struct wm_workspace *active = wm_workspace_get_active(server);
	wl_list_for_each(v, &server->views, link) {
		if (v->workspace == active && v->mapped)
			wm_view_close(v);
	}
}
