/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * view_focus.c - Focus management, layer stacking, view cycling
 */

#define _POSIX_C_SOURCE 200809L
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
#include "server.h"
#include "text_input.h"
#include "toolbar.h"
#include "util.h"
#include "view.h"
#include "view_focus.h"
#include "workspace.h"

/* --- Focus Management --- */

void
wm_focus_view(struct wm_view *view, struct wlr_surface *surface)
{
	if (!view) {
		return;
	}
	struct wm_server *server = view->server;
	struct wlr_seat *seat = server->seat;
	struct wlr_surface *prev_surface =
		seat->keyboard_state.focused_surface;

	if (prev_surface == surface) {
		server->focus_user_initiated = false;
		return;
	}

	/* Focus protection: block non-user-initiated focus changes */
	if (!server->focus_user_initiated) {
		/* Current view has REFUSE: block unless target has GAIN */
		if (server->focused_view &&
		    (server->focused_view->focus_protection &
		     WM_FOCUS_PROT_REFUSE) &&
		    !(view->focus_protection & WM_FOCUS_PROT_GAIN)) {
			server->focus_user_initiated = false;
			return;
		}
		/* Target view has DENY: block programmatic focus */
		if (view->focus_protection & WM_FOCUS_PROT_DENY) {
			server->focus_user_initiated = false;
			return;
		}
	}

	/* Deactivate the previously focused toplevel */
	if (prev_surface) {
		struct wlr_xdg_toplevel *prev_toplevel =
			wlr_xdg_toplevel_try_from_wlr_surface(prev_surface);
		if (prev_toplevel) {
			wlr_xdg_toplevel_set_activated(prev_toplevel, false);
		}
		/* Update decoration on previously focused view */
		if (server->focused_view && server->focused_view->decoration) {
			wm_decoration_set_focused(
				server->focused_view->decoration, false,
				server->style);
		}
		/* Apply unfocus opacity to previous view */
		if (server->focused_view) {
			wm_view_set_opacity(server->focused_view,
				server->focused_view->unfocus_alpha);
			wm_foreign_toplevel_set_activated(
				server->focused_view, false);
		}
	}

	/* Move view to front of the views list (stacking order) */
	wl_list_remove(&view->link);
	wl_list_insert(&server->views, &view->link);

	/* Raise in the scene graph */
	wlr_scene_node_raise_to_top(&view->scene_tree->node);

	/* Activate the new toplevel */
	wlr_xdg_toplevel_set_activated(view->xdg_toplevel, true);

	/* Track focused view */
	server->focused_view = view;

	/* Notify foreign toplevel (taskbar) of activation */
	wm_foreign_toplevel_set_activated(view, true);

	/* Broadcast focus event via IPC */
	{
		char esc_app[256], esc_title[256], buf[1024];
		wm_json_escape(esc_app, sizeof(esc_app),
			view->app_id);
		wm_json_escape(esc_title, sizeof(esc_title),
			view->title);
		struct wlr_box geo;
		wm_view_get_geometry(view, &geo);
		snprintf(buf, sizeof(buf),
			"{\"event\":\"window_focus\","
			"\"id\":%u,"
			"\"app_id\":\"%s\","
			"\"title\":\"%s\","
			"\"x\":%d,\"y\":%d,"
			"\"width\":%d,\"height\":%d}",
			view->id,
			esc_app, esc_title,
			geo.x, geo.y, geo.width, geo.height);
		wm_ipc_broadcast_event(&server->ipc,
			WM_IPC_EVENT_WINDOW_FOCUS, buf);
		/* Also broadcast on focus_changed for screen readers */
		snprintf(buf, sizeof(buf),
			"{\"event\":\"focus_changed\","
			"\"target\":\"window\","
			"\"id\":%u,"
			"\"app_id\":\"%s\","
			"\"title\":\"%s\","
			"\"x\":%d,\"y\":%d,"
			"\"width\":%d,\"height\":%d}",
			view->id,
			esc_app, esc_title,
			geo.x, geo.y, geo.width, geo.height);
		wm_ipc_broadcast_event(&server->ipc,
			WM_IPC_EVENT_FOCUS_CHANGED, buf);
	}

	/* Update decoration on newly focused view */
	if (view->decoration) {
		wm_decoration_set_focused(view->decoration, true,
			server->style);
	}

	/* Apply focus opacity to newly focused view */
	wm_view_set_opacity(view, view->focus_alpha);

	/* Send keyboard focus */
	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
	if (keyboard) {
		wlr_seat_keyboard_notify_enter(seat,
			view->xdg_toplevel->base->surface,
			keyboard->keycodes, keyboard->num_keycodes,
			&keyboard->modifiers);
	}

	/* Notify text input relay of focus change for IME */
	wm_text_input_focus_change(server,
		view->xdg_toplevel->base->surface);

	/* Update pointer constraint for the newly focused surface */
	wm_protocols_update_pointer_constraint(server,
		view->xdg_toplevel->base->surface);

	/* Update keyboard shortcuts inhibitor for the newly focused surface */
	wm_protocols_update_kb_shortcuts_inhibitor(server,
		view->xdg_toplevel->base->surface);

	/* Update toolbar icon bar to reflect focus change */
	wm_toolbar_update_iconbar(server->toolbar);

	/* Clear the user-initiated flag after focus change completes */
	server->focus_user_initiated = false;
}

void
wm_unfocus_current(struct wm_server *server)
{
	struct wlr_surface *prev_surface =
		server->seat->keyboard_state.focused_surface;
	if (prev_surface) {
		struct wlr_xdg_toplevel *prev_toplevel =
			wlr_xdg_toplevel_try_from_wlr_surface(prev_surface);
		if (prev_toplevel) {
			wlr_xdg_toplevel_set_activated(prev_toplevel, false);
		}
	}
	/* Notify text input relay of focus loss for IME */
	wm_text_input_focus_change(server, NULL);
	/* Deactivate pointer constraint when unfocusing */
	wm_protocols_update_pointer_constraint(server, NULL);
	/* Deactivate keyboard shortcuts inhibitor when unfocusing */
	wm_protocols_update_kb_shortcuts_inhibitor(server, NULL);
	server->focused_view = NULL;
	wlr_seat_keyboard_notify_clear_focus(server->seat);
}

void
wm_focus_next_view(struct wm_server *server)
{
	if (wl_list_length(&server->views) < 2) {
		return;
	}

	/* The last entry in the list is the bottom of the stack */
	struct wm_view *next = wl_container_of(
		server->views.prev, next, link);
	wm_focus_view(next, next->xdg_toplevel->base->surface);
}

void
wm_focus_prev_view(struct wm_server *server)
{
	if (wl_list_length(&server->views) < 2) {
		return;
	}

	/* The current top view is at the front. Focus the second view
	 * (one position down) by moving the bottom view to the top. */
	struct wm_view *current = wl_container_of(
		server->views.next, current, link);
	struct wm_view *second = wl_container_of(
		current->link.next, second, link);
	if (&second->link == &server->views) {
		return; /* only one view */
	}
	wm_focus_view(second, second->xdg_toplevel->base->surface);
}

void
wm_view_raise(struct wm_view *view)
{
	wl_list_remove(&view->link);
	wl_list_insert(&view->server->views, &view->link);
	wlr_scene_node_raise_to_top(&view->scene_tree->node);
}

void
wm_view_lower(struct wm_view *view)
{
	/* Move to bottom of the views list */
	wl_list_remove(&view->link);
	wl_list_insert(view->server->views.prev, &view->link);
	wlr_scene_node_lower_to_bottom(&view->scene_tree->node);
}

struct wlr_scene_tree *
get_layer_tree(struct wm_server *server, enum wm_view_layer layer)
{
	switch (layer) {
	case WM_LAYER_DESKTOP:
		return server->view_layer_desktop;
	case WM_LAYER_BELOW:
		return server->view_layer_below;
	case WM_LAYER_ABOVE:
		return server->view_layer_above;
	case WM_LAYER_NORMAL:
	default:
		return server->view_layer_normal;
	}
}

void
wm_view_set_layer(struct wm_view *view, enum wm_view_layer layer)
{
	if (layer < 0 || layer >= WM_VIEW_LAYER_COUNT) {
		return;
	}
	if (view->layer == layer) {
		return;
	}

	view->layer = layer;

	struct wlr_scene_tree *target;
	if (layer == WM_LAYER_NORMAL) {
		/* Normal layer: reparent to workspace or sticky tree */
		if (view->sticky) {
			target = view->server->sticky_tree;
		} else if (view->workspace) {
			target = view->workspace->tree;
		} else {
			target = view->server->view_layer_normal;
		}
	} else {
		target = get_layer_tree(view->server, layer);
	}

	wlr_scene_node_reparent(&view->scene_tree->node, target);
	wlr_scene_node_raise_to_top(&view->scene_tree->node);
}

void
wm_view_raise_layer(struct wm_view *view)
{
	if (view->layer < WM_LAYER_ABOVE) {
		wm_view_set_layer(view, view->layer + 1);
	}
}

void
wm_view_lower_layer(struct wm_view *view)
{
	if (view->layer > WM_LAYER_DESKTOP) {
		wm_view_set_layer(view, view->layer - 1);
	}
}

enum wm_view_layer
wm_view_layer_from_name(const char *name)
{
	if (!name) {
		return WM_LAYER_NORMAL;
	}
	if (strcasecmp(name, "Desktop") == 0) {
		return WM_LAYER_DESKTOP;
	} else if (strcasecmp(name, "Bottom") == 0 ||
		   strcasecmp(name, "Below") == 0) {
		return WM_LAYER_BELOW;
	} else if (strcasecmp(name, "Top") == 0 ||
		   strcasecmp(name, "Above") == 0 ||
		   strcasecmp(name, "AboveDock") == 0 ||
		   strcasecmp(name, "Dock") == 0) {
		return WM_LAYER_ABOVE;
	}
	return WM_LAYER_NORMAL;
}

void
wm_focus_update_for_cursor(struct wm_server *server,
	double cursor_x, double cursor_y)
{
	/* Only apply focus-follows-mouse policies (sloppy or strict) */
	if (server->focus_policy != WM_FOCUS_SLOPPY &&
	    server->focus_policy != WM_FOCUS_STRICT_MOUSE) {
		return;
	}

	/* Don't change focus during interactive move/resize */
	if (server->cursor_mode != WM_CURSOR_PASSTHROUGH) {
		return;
	}

	double sx, sy;
	struct wlr_surface *surface = NULL;
	struct wm_view *view = wm_view_at(server, cursor_x, cursor_y,
		&surface, &sx, &sy);

	if (view && view != server->focused_view) {
		/*
		 * Sloppy focus: focus the view under the pointer but
		 * do NOT raise it. Only raising happens on click.
		 */
		struct wlr_seat *seat = server->seat;
		struct wlr_surface *prev_surface =
			seat->keyboard_state.focused_surface;

		if (prev_surface) {
			struct wlr_xdg_toplevel *prev_toplevel =
				wlr_xdg_toplevel_try_from_wlr_surface(
					prev_surface);
			if (prev_toplevel) {
				wlr_xdg_toplevel_set_activated(
					prev_toplevel, false);
			}
		}

		/* Apply unfocus opacity to previous view */
		if (server->focused_view) {
			wm_view_set_opacity(server->focused_view,
				server->focused_view->unfocus_alpha);
			wm_foreign_toplevel_set_activated(
				server->focused_view, false);
		}

		wlr_xdg_toplevel_set_activated(view->xdg_toplevel, true);
		server->focused_view = view;

		/* Apply focus opacity to newly focused view */
		wm_view_set_opacity(view, view->focus_alpha);
		wm_foreign_toplevel_set_activated(view, true);

		struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
		if (keyboard) {
			wlr_seat_keyboard_notify_enter(seat,
				view->xdg_toplevel->base->surface,
				keyboard->keycodes, keyboard->num_keycodes,
				&keyboard->modifiers);
		}
	}
}

/* --- Deiconify helper --- */

void
deiconify_view(struct wm_view *view)
{
	wlr_scene_node_set_enabled(&view->scene_tree->node, true);
	wm_foreign_toplevel_set_minimized(view, false);
	wm_focus_view(view, view->xdg_toplevel->base->surface);
	wm_view_raise(view);
	wm_toolbar_update_iconbar(view->server->toolbar);

	/* Animate fade-in on restore if configured */
	if (view->server->config &&
	    view->server->config->animate_minimize) {
		wm_animation_start(view, WM_ANIM_FADE_IN,
			0, view->focus_alpha,
			view->server->config->animation_duration_ms,
			NULL, NULL);
	}
}

/* --- Window cycling (workspace-aware, with de-iconify) --- */

void
wm_view_cycle_next(struct wm_server *server)
{
	struct wm_workspace *ws = wm_workspace_get_active(server);
	struct wm_view *focused = server->focused_view;

	/*
	 * Find the next view after the focused one on the current workspace.
	 * If the focused view is NULL, pick the first view on the workspace.
	 */
	struct wm_view *candidate = NULL;
	bool past_focused = (focused == NULL);

	struct wm_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (view->workspace != ws && !view->sticky) {
			continue;
		}
		if (past_focused) {
			candidate = view;
			break;
		}
		if (view == focused) {
			past_focused = true;
		}
	}

	/* Wrap around: if we didn't find one after focused, pick first */
	if (!candidate) {
		wl_list_for_each(view, &server->views, link) {
			if (view->workspace != ws && !view->sticky) {
				continue;
			}
			candidate = view;
			break;
		}
	}

	if (!candidate || candidate == focused) {
		return;
	}

	/* De-iconify if minimized */
	if (!candidate->scene_tree->node.enabled) {
		deiconify_view(candidate);
	} else {
		wm_focus_view(candidate,
			candidate->xdg_toplevel->base->surface);
	}
}

void
wm_view_cycle_prev(struct wm_server *server)
{
	struct wm_workspace *ws = wm_workspace_get_active(server);
	struct wm_view *focused = server->focused_view;

	/*
	 * Find the previous view before the focused one on the current
	 * workspace. Walk the list and track the last eligible view seen
	 * before the focused one.
	 */
	struct wm_view *candidate = NULL;
	struct wm_view *last_on_ws = NULL;

	struct wm_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (view->workspace != ws && !view->sticky) {
			continue;
		}
		if (view == focused) {
			if (candidate) {
				break; /* candidate is the view before focused */
			}
			/* focused is first — need to wrap to last */
			continue;
		}
		candidate = view;
		last_on_ws = view;
	}

	/* If no candidate before focused, wrap to the last view on ws */
	if (!candidate) {
		/* Walk to find the absolute last view on this workspace */
		wl_list_for_each(view, &server->views, link) {
			if (view->workspace == ws || view->sticky) {
				last_on_ws = view;
			}
		}
		candidate = last_on_ws;
	}

	if (!candidate || candidate == focused) {
		return;
	}

	/* De-iconify if minimized */
	if (!candidate->scene_tree->node.enabled) {
		deiconify_view(candidate);
	} else {
		wm_focus_view(candidate,
			candidate->xdg_toplevel->base->surface);
	}
}

void
wm_view_deiconify_last(struct wm_server *server)
{
	struct wm_workspace *ws = wm_workspace_get_active(server);

	/*
	 * Walk the view list to find the most recently minimized view on
	 * the current workspace. Views at the front of the list were most
	 * recently focused/created, so the first iconified one we find
	 * is the most recently minimized.
	 */
	struct wm_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (view->workspace != ws && !view->sticky) {
			continue;
		}
		if (!view->scene_tree->node.enabled) {
			deiconify_view(view);
			return;
		}
	}
}

void
wm_view_deiconify_all(struct wm_server *server)
{
	struct wm_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (!view->scene_tree->node.enabled) {
			wlr_scene_node_set_enabled(
				&view->scene_tree->node, true);
			wm_foreign_toplevel_set_minimized(view, false);
		}
	}
	wm_toolbar_update_iconbar(server->toolbar);
}

void
wm_view_deiconify_all_workspace(struct wm_server *server)
{
	struct wm_workspace *ws = wm_workspace_get_active(server);
	struct wm_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (view->workspace != ws && !view->sticky) {
			continue;
		}
		if (!view->scene_tree->node.enabled) {
			wlr_scene_node_set_enabled(
				&view->scene_tree->node, true);
			wm_foreign_toplevel_set_minimized(view, false);
		}
	}
	wm_toolbar_update_iconbar(server->toolbar);
}

/* --- Directional focus --- */

void
wm_view_focus_direction(struct wm_server *server, int dx, int dy)
{
	struct wm_view *focused = server->focused_view;
	if (!focused)
		return;

	struct wlr_box fgeo;
	wm_view_get_geometry(focused, &fgeo);
	int fx = fgeo.x + fgeo.width / 2;
	int fy = fgeo.y + fgeo.height / 2;

	struct wm_view *best = NULL;
	double best_score = 1e18;

	struct wm_view *v;
	wl_list_for_each(v, &server->views, link) {
		if (v == focused)
			continue;
		if (v->workspace != wm_workspace_get_active(server) &&
		    !v->sticky)
			continue;
		if (!v->scene_tree->node.enabled)
			continue;

		struct wlr_box vgeo;
		wm_view_get_geometry(v, &vgeo);
		int vx = vgeo.x + vgeo.width / 2;
		int vy = vgeo.y + vgeo.height / 2;

		int ddx = vx - fx;
		int ddy = vy - fy;

		/* Filter: view must be in the requested direction */
		if (dx > 0 && ddx <= 0)
			continue;
		if (dx < 0 && ddx >= 0)
			continue;
		if (dy > 0 && ddy <= 0)
			continue;
		if (dy < 0 && ddy >= 0)
			continue;

		/* Weighted distance: penalize off-axis deviation */
		double primary = (dx != 0) ? abs(ddx) : abs(ddy);
		double secondary = (dx != 0) ? abs(ddy) : abs(ddx);
		double score = primary + secondary * 2.0;

		if (score < best_score) {
			best_score = score;
			best = v;
		}
	}

	if (best)
		wm_focus_view(best, best->xdg_toplevel->base->surface);
}
