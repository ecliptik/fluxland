/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * view.c - Window/view lifecycle, focus, and XDG shell handling
 */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>

#include "animation.h"
#include "config.h"
#include "decoration.h"
#include "foreign_toplevel.h"
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
	 * Walk up the scene tree to find the wm_view.
	 * Each view's scene_tree->node.data points to the wm_view.
	 */
	struct wlr_scene_tree *tree = node->parent;
	while (tree && &tree->node != &server->view_tree->node) {
		if (tree->node.data) {
			return tree->node.data;
		}
		tree = tree->node.parent;
	}

	return NULL;
}

/* --- Geometry helpers --- */

void
wm_view_get_geometry(struct wm_view *view, struct wlr_box *box)
{
	struct wlr_box geo;
	wlr_xdg_surface_get_geometry(view->xdg_toplevel->base, &geo);
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

/* --- XDG toplevel event handlers --- */

static void
handle_xdg_toplevel_map(struct wl_listener *listener, void *data)
{
	struct wm_view *view = wl_container_of(listener, view, map);

	/* Check if this view should be docked into the slit */
	if (view->server->slit &&
	    wm_rules_should_dock(&view->server->rules, view)) {
		wm_slit_add_native_client(view->server->slit,
			view->xdg_toplevel, view->scene_tree);
		view->docked = true;
		wlr_scene_node_set_enabled(
			&view->scene_tree->node, true);
		wlr_log(WLR_INFO, "view docked to slit: %s",
			view->app_id ? view->app_id : "(null)");
		return;
	}

	/* Create server-side decorations */
	if (!view->decoration && view->server->style) {
		view->decoration = wm_decoration_create(view,
			view->server->style);
		if (view->decoration) {
			/*
			 * Position the client surface within the decoration
			 * frame. The XDG surface tree needs to be offset by
			 * the decoration extents.
			 */
			int top, bottom, left, right;
			wm_decoration_get_extents(view->decoration,
				&top, &bottom, &left, &right);

			/*
			 * Find the XDG surface tree (second child of
			 * view->scene_tree, first is decoration->tree).
			 * Iterate children to find the non-decoration tree.
			 */
			struct wlr_scene_node *child;
			wl_list_for_each(child,
				&view->scene_tree->children, link) {
				if (child != &view->decoration->tree->node) {
					wlr_scene_node_set_position(child,
						left, top);
					break;
				}
			}
		}
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
			if (peer == view || !peer->app_id)
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

	/* Apply automatic window placement */
	wm_placement_apply(view->server, view);
	wlr_scene_node_set_position(&view->scene_tree->node,
		view->x, view->y);

	wlr_scene_node_set_enabled(&view->scene_tree->node, true);
	wm_focus_view(view, view->xdg_toplevel->base->surface);

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

static void
handle_xdg_toplevel_unmap(struct wl_listener *listener, void *data)
{
	struct wm_view *view = wl_container_of(listener, view, unmap);

	/* Docked views: slit handles unmap via its own listener */
	if (view->docked) {
		return;
	}

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
	if (view == view->server->focused_view) {
		view->server->focused_view = NULL;
		struct wm_view *next;
		wl_list_for_each(next, &view->server->views, link) {
			if (next != view && next->xdg_toplevel->base->surface) {
				wm_focus_view(next,
					next->xdg_toplevel->base->surface);
				goto after_focus;
			}
		}
		/* No other views, clear focus */
		wlr_seat_keyboard_notify_clear_focus(view->server->seat);
	}

after_focus:
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

static void
handle_xdg_toplevel_commit(struct wl_listener *listener, void *data)
{
	struct wm_view *view = wl_container_of(listener, view, commit);
	struct wlr_xdg_surface *xdg_surface = view->xdg_toplevel->base;

	if (xdg_surface->initial_commit) {
		/*
		 * On initial commit, send a configure so the client
		 * knows it can map. This is required by wlroots 0.18+.
		 */
		wlr_xdg_toplevel_set_size(view->xdg_toplevel, 0, 0);
		return;
	}

	/* Refresh decoration geometry when client commits a new size */
	if (view->decoration && view->server->style) {
		wm_decoration_refresh_geometry(view->decoration,
			view->server->style);
	}
}

static void
handle_xdg_toplevel_destroy(struct wl_listener *listener, void *data)
{
	struct wm_view *view = wl_container_of(listener, view, destroy);

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

	wl_list_remove(&view->map.link);
	wl_list_remove(&view->unmap.link);
	wl_list_remove(&view->commit.link);
	wl_list_remove(&view->destroy.link);
	wl_list_remove(&view->request_move.link);
	wl_list_remove(&view->request_resize.link);
	wl_list_remove(&view->request_maximize.link);
	wl_list_remove(&view->request_fullscreen.link);
	wl_list_remove(&view->request_minimize.link);
	wl_list_remove(&view->set_title.link);
	wl_list_remove(&view->set_app_id.link);

	wl_list_remove(&view->link);

	/* Update toolbar icon bar (view removed from list) */
	if (!view->docked) {
		wm_toolbar_update_iconbar(view->server->toolbar);
	}

	free(view->title);
	free(view->app_id);
	free(view);
}

static void
handle_xdg_toplevel_request_move(struct wl_listener *listener, void *data)
{
	struct wm_view *view = wl_container_of(listener, view, request_move);
	wm_view_begin_interactive(view, WM_CURSOR_MOVE, 0);
}

static void
handle_xdg_toplevel_request_resize(struct wl_listener *listener, void *data)
{
	struct wlr_xdg_toplevel_resize_event *event = data;
	struct wm_view *view = wl_container_of(listener, view,
		request_resize);
	wm_view_begin_interactive(view, WM_CURSOR_RESIZE, event->edges);
}

static void
handle_xdg_toplevel_request_maximize(struct wl_listener *listener,
	void *data)
{
	struct wm_view *view = wl_container_of(listener, view,
		request_maximize);
	wm_view_toggle_maximize(view);
}

static void
handle_xdg_toplevel_request_fullscreen(struct wl_listener *listener,
	void *data)
{
	struct wm_view *view = wl_container_of(listener, view,
		request_fullscreen);
	wm_view_toggle_fullscreen(view);
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

static void
handle_xdg_toplevel_request_minimize(struct wl_listener *listener,
	void *data)
{
	struct wm_view *view = wl_container_of(listener, view,
		request_minimize);

	/* Cancel any running animation */
	if (view->animation)
		wm_animation_cancel(view->animation);

	wm_foreign_toplevel_set_minimized(view, true);

	/* Focus next view */
	struct wm_view *next;
	wl_list_for_each(next, &view->server->views, link) {
		if (next != view && next->xdg_toplevel->base->surface) {
			wm_focus_view(next,
				next->xdg_toplevel->base->surface);
			goto after_focus;
		}
	}
	wlr_seat_keyboard_notify_clear_focus(view->server->seat);

after_focus:
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

static void
handle_xdg_toplevel_set_title(struct wl_listener *listener, void *data)
{
	struct wm_view *view = wl_container_of(listener, view, set_title);
	free(view->title);
	view->title = view->xdg_toplevel->title ?
		strdup(view->xdg_toplevel->title) : NULL;

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

static void
handle_xdg_toplevel_set_app_id(struct wl_listener *listener, void *data)
{
	struct wm_view *view = wl_container_of(listener, view, set_app_id);
	free(view->app_id);
	view->app_id = view->xdg_toplevel->app_id ?
		strdup(view->xdg_toplevel->app_id) : NULL;

	/* Update foreign toplevel handle */
	wm_foreign_toplevel_update_app_id(view);
}

/* --- New XDG toplevel handler --- */

static void
handle_new_xdg_toplevel(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, new_xdg_toplevel);
	struct wlr_xdg_toplevel *xdg_toplevel = data;

	struct wm_view *view = calloc(1, sizeof(*view));
	if (!view) {
		wlr_log(WLR_ERROR, "%s", "allocation failed");
		return;
	}

	view->server = server;
	view->xdg_toplevel = xdg_toplevel;

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
	if (!view->scene_tree) {
		free(view);
		return;
	}

	/* Initially hidden until map */
	wlr_scene_node_set_enabled(&view->scene_tree->node, false);

	/* Attach the XDG surface to the scene tree */
	struct wlr_scene_tree *surface_tree =
		wlr_scene_xdg_surface_create(view->scene_tree,
			xdg_toplevel->base);
	if (!surface_tree) {
		wlr_scene_node_destroy(&view->scene_tree->node);
		free(view);
		return;
	}

	/* Store back-pointer for wm_view_at lookups */
	view->scene_tree->node.data = view;

	/* Connect XDG toplevel signals */
	view->map.notify = handle_xdg_toplevel_map;
	wl_signal_add(&xdg_toplevel->base->surface->events.map,
		&view->map);

	view->unmap.notify = handle_xdg_toplevel_unmap;
	wl_signal_add(&xdg_toplevel->base->surface->events.unmap,
		&view->unmap);

	view->commit.notify = handle_xdg_toplevel_commit;
	wl_signal_add(&xdg_toplevel->base->surface->events.commit,
		&view->commit);

	view->destroy.notify = handle_xdg_toplevel_destroy;
	wl_signal_add(&xdg_toplevel->events.destroy, &view->destroy);

	view->request_move.notify = handle_xdg_toplevel_request_move;
	wl_signal_add(&xdg_toplevel->events.request_move,
		&view->request_move);

	view->request_resize.notify = handle_xdg_toplevel_request_resize;
	wl_signal_add(&xdg_toplevel->events.request_resize,
		&view->request_resize);

	view->request_maximize.notify =
		handle_xdg_toplevel_request_maximize;
	wl_signal_add(&xdg_toplevel->events.request_maximize,
		&view->request_maximize);

	view->request_fullscreen.notify =
		handle_xdg_toplevel_request_fullscreen;
	wl_signal_add(&xdg_toplevel->events.request_fullscreen,
		&view->request_fullscreen);

	view->request_minimize.notify =
		handle_xdg_toplevel_request_minimize;
	wl_signal_add(&xdg_toplevel->events.request_minimize,
		&view->request_minimize);

	view->set_title.notify = handle_xdg_toplevel_set_title;
	wl_signal_add(&xdg_toplevel->events.set_title,
		&view->set_title);

	view->set_app_id.notify = handle_xdg_toplevel_set_app_id;
	wl_signal_add(&xdg_toplevel->events.set_app_id,
		&view->set_app_id);

	/* Add to the server's views list */
	wl_list_insert(&server->views, &view->link);

	wlr_log(WLR_DEBUG, "new xdg toplevel: %s (%s)",
		xdg_toplevel->title ? xdg_toplevel->title : "(untitled)",
		xdg_toplevel->app_id ? xdg_toplevel->app_id : "(no app_id)");
}

/* --- XDG popup handler --- */

static void
handle_new_xdg_popup(struct wl_listener *listener, void *data)
{
	struct wlr_xdg_popup *popup = data;

	/*
	 * Popups are parented to a surface. The scene graph manages
	 * their positioning relative to the parent automatically when
	 * we create them under the correct parent tree.
	 */
	struct wlr_xdg_surface *parent_surface =
		wlr_xdg_surface_try_from_wlr_surface(popup->parent);
	assert(parent_surface);

	struct wlr_scene_tree *parent_tree = parent_surface->data;
	if (!parent_tree) {
		/* Fallback: use the server popup tree */
		struct wm_server *server = wl_container_of(listener, server,
			new_xdg_popup);
		parent_tree = server->xdg_popup_tree;
	}

	struct wlr_scene_tree *popup_tree =
		wlr_scene_xdg_surface_create(parent_tree, popup->base);
	if (popup_tree) {
		popup->base->data = popup_tree;
	}
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
		if (v->workspace == active)
			wlr_xdg_toplevel_send_close(v->xdg_toplevel);
	}
}

/* --- Module initialization --- */

void
wm_view_init(struct wm_server *server)
{
	server->new_xdg_toplevel.notify = handle_new_xdg_toplevel;
	wl_signal_add(&server->xdg_shell->events.new_toplevel,
		&server->new_xdg_toplevel);

	server->new_xdg_popup.notify = handle_new_xdg_popup;
	wl_signal_add(&server->xdg_shell->events.new_popup,
		&server->new_xdg_popup);
}
