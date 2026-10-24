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
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>

#include "animation.h"
#include "client_pattern.h"
#include "tabgroup.h"
#include "atspi_bridge.h"
#include "config.h"
#include "decoration.h"
#include "foreign_toplevel.h"
#include "ipc.h"
#include "output.h"
#include "rules.h"
#include "server.h"
#include "text_input.h"
#include "toolbar.h"
#include "util.h"
#include "view.h"
#include "view_focus.h"
#include "workspace.h"

/* --- Focus Management --- */

/* A mapped modal dialog (xdg-dialog) of this window, innermost first */
static struct wm_view *
modal_child(struct wm_view *view, int depth)
{
	if (depth > 8)
		return NULL;
	struct wm_view *v;
	wl_list_for_each(v, &view->server->views, link) {
		if (v != view && v->modal && v->mapped &&
		    wm_view_parent(v) == view) {
			struct wm_view *inner = modal_child(v, depth + 1);
			return inner ? inner : v;
		}
	}
	return NULL;
}

static void
focus_view(struct wm_view *view, struct wlr_surface *surface, bool raise)
{
	if (!view) {
		return;
	}
	/* The modal dialog keeps the focus while it is open */
	struct wm_view *modal = modal_child(view, 0);
	if (modal) {
		if (raise)
			wm_view_raise(view);
		view = modal;
		surface = NULL;
	}
	struct wm_server *server = view->server;
	struct wlr_seat *seat = server->seat;
	struct wlr_surface *prev_surface =
		seat->keyboard_state.focused_surface;

	/* Callers may pass NULL for "the view's own surface" */
	if (!surface)
		surface = wm_view_surface(view);
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

	/* It has the user's attention now */
	if (view->urgent) {
		view->urgent = false;
		wm_toolbar_update_iconbar(server->toolbar);
	}

	/* Deactivate the previously focused toplevel */
	if (prev_surface) {
		struct wm_view *prev_view =
			wm_view_from_surface(server, prev_surface);
		if (prev_view && prev_view != view)
			wm_view_set_activated(prev_view, false);
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

	if (raise) {
		/* Move view to front of the views list (stacking order) */
		wl_list_remove(&view->link);
		wl_list_insert(&server->views, &view->link);

		/* Raise in the scene graph */
		wlr_scene_node_raise_to_top(&view->scene_tree->node);
	}

	/* Focus order stays frozen while NextWindow cycling is active */
	if (!server->focus_cycle.active)
		view->focus_seq = ++server->focus_seq_counter;

	/* Activate the new toplevel */
	wm_view_set_activated(view, true);

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

	/* Announce to AT-SPI for screen readers */
	wm_atspi_announce_focus(server->atspi_bridge,
		"window", view->title ? view->title : view->app_id);

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
			wm_view_surface(view),
			keyboard->keycodes, keyboard->num_keycodes,
			&keyboard->modifiers);
	}

	/* Notify text input relay of focus change for IME */
	wm_text_input_focus_change(server,
		wm_view_surface(view));

	/* Update pointer constraint for the newly focused surface */
	wm_protocols_update_pointer_constraint(server,
		wm_view_surface(view));

	/* Update keyboard shortcuts inhibitor for the newly focused surface */
	wm_protocols_update_kb_shortcuts_inhibitor(server,
		wm_view_surface(view));

	/* Update toolbar icon bar to reflect focus change */
	wm_toolbar_update_iconbar(server->toolbar);

	/* Clear the user-initiated flag after focus change completes */
	server->focus_user_initiated = false;
}

void
wm_focus_view(struct wm_view *view, struct wlr_surface *surface)
{
	focus_view(view, surface, true);
}

void
wm_focus_view_noraise(struct wm_view *view)
{
	focus_view(view, NULL, false);
}

void
wm_unfocus_current(struct wm_server *server)
{
	struct wlr_surface *prev_surface =
		server->seat->keyboard_state.focused_surface;
	if (prev_surface) {
		struct wm_view *prev_view =
			wm_view_from_surface(server, prev_surface);
		if (prev_view)
			wm_view_set_activated(prev_view, false);
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
wm_focus_fallback(struct wm_server *server, struct wm_view *except)
{
	struct wm_workspace *ws = wm_workspace_get_active(server);
	struct wm_view *v, *best = NULL;

	/* Most recently focused; ties keep stacking (list) order */
	wl_list_for_each(v, &server->views, link) {
		/* Disabled scene node: unmapped or minimized */
		if (v == except || !v->scene_tree->node.enabled)
			continue;
		if (v->workspace != ws && !v->sticky)
			continue;
		if (v->focus_hidden)
			continue;
		if (!best || v->focus_seq > best->focus_seq)
			best = v;
	}
	if (best)
		wm_focus_view(best, wm_view_surface(best));
	else
		wm_unfocus_current(server);
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
	wm_focus_view(next, wm_view_surface(next));
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
	wm_focus_view(second, wm_view_surface(second));
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
		server->focus_user_initiated = true;
		wm_focus_view_noraise(view);
	}
}

/* --- Deiconify helper --- */

void
deiconify_view(struct wm_view *view)
{
	wlr_scene_node_set_enabled(&view->scene_tree->node, true);
	wm_foreign_toplevel_set_minimized(view, false);
	wm_view_set_minimized_state(view, false);
	wm_focus_view(view, wm_view_surface(view));
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

/* --- Window cycling (Fluxbox FocusControl::cycleFocus) --- */

static bool
cycle_eligible(struct wm_server *server, struct wm_view *v,
	const struct wm_window_list_spec *spec)
{
	if (!v->mapped || v->docked || v->focus_hidden)
		return false;
	if (spec && spec->groups && v->tab_group &&
	    v->tab_group->active_view != v)
		return false;
	if (spec && spec->pattern &&
	    !wm_client_pattern_match(server, spec->pattern, v))
		return false;
	return true;
}

/* True if a sorts before b: most recently focused first */
static bool
before_focus_order(const struct wm_view *a, const struct wm_view *b)
{
	return a->focus_seq > b->focus_seq;
}

static bool
before_creation_order(const struct wm_view *a, const struct wm_view *b)
{
	return a->id < b->id;
}

/* Stable insertion sort: ties keep stacking (list) order */
static void
sort_views(struct wm_view **list, int count,
	bool (*before)(const struct wm_view *, const struct wm_view *))
{
	for (int i = 1; i < count; i++) {
		struct wm_view *v = list[i];
		int j = i - 1;
		while (j >= 0 && before(v, list[j])) {
			list[j + 1] = list[j];
			j--;
		}
		list[j + 1] = v;
	}
}

static bool
modifiers_held(struct wm_server *server)
{
	struct wlr_keyboard *kb = wlr_seat_get_keyboard(server->seat);
	if (!kb)
		return false;
	/* Lock modifiers don't count as "held" */
	return (wlr_keyboard_get_modifiers(kb) &
		~(WLR_MODIFIER_CAPS | WLR_MODIFIER_MOD2)) != 0;
}

void
wm_focus_cycle_end(struct wm_server *server)
{
	server->focus_cycle.active = false;
	server->focus_cycle.restore_minimized = NULL;
	if (server->focused_view)
		server->focused_view->focus_seq =
			++server->focus_seq_counter;
}

void
wm_focus_cycle(struct wm_server *server,
	const struct wm_window_list_spec *spec, bool reverse)
{
	int n = wl_list_length(&server->views);
	if (n <= 0)
		return;
	struct wm_view **list = calloc((size_t)n, sizeof(*list));
	if (!list)
		return;

	int count = 0;
	struct wm_view *v;
	wl_list_for_each(v, &server->views, link) {
		if (count < n && cycle_eligible(server, v, spec))
			list[count++] = v;
	}
	sort_views(list, count, spec && spec->static_order ?
		before_creation_order : before_focus_order);

	struct wm_view *focused = server->focused_view;
	int cur = -1;
	for (int i = 0; i < count; i++) {
		if (list[i] == focused ||
		    (focused && focused->tab_group &&
		     list[i]->tab_group == focused->tab_group))
			cur = i;
	}

	struct wm_view *cand = NULL;
	if (count > 0) {
		if (cur < 0)
			cand = list[reverse ? count - 1 : 0];
		else if (count > 1)
			cand = list[(cur + (reverse ? count - 1 : 1)) % count];
	}
	free(list);
	if (!cand)
		return;

	bool held = modifiers_held(server);
	struct wm_view *prev_restored = server->focus_cycle.restore_minimized;
	server->focus_cycle.restore_minimized = NULL;
	server->focus_cycle.active = held;

	/* Focusing a window on another workspace switches to it */
	if (!cand->sticky && cand->workspace &&
	    cand->workspace != wm_workspace_get_active(server))
		wm_workspace_switch(server, cand->workspace->index);

	server->focus_user_initiated = true;
	if (!cand->scene_tree->node.enabled) {
		deiconify_view(cand);
		/* Re-iconified if the cycle moves past it */
		if (held)
			server->focus_cycle.restore_minimized = cand;
	} else {
		wm_focus_view(cand, NULL);
		wm_view_raise(cand);
	}

	if (prev_restored && prev_restored != cand && prev_restored->mapped)
		wm_view_minimize(prev_restored);

	if (!held)
		wm_focus_cycle_end(server);
}

void
wm_view_set_urgent(struct wm_view *view, bool urgent)
{
	if (view->urgent == urgent)
		return;
	view->urgent = urgent;
	wm_toolbar_update_iconbar(view->server->toolbar);
}

void
wm_view_present(struct wm_view *view)
{
	struct wm_server *server = view->server;

	/* On another workspace: go there */
	if (!view->sticky && view->workspace &&
	    view->workspace != wm_workspace_get_active(server))
		wm_workspace_switch(server, view->workspace->index);

	/* A background tab becomes the visible one */
	if (view->tab_group && view->tab_group->active_view != view)
		wm_tab_group_activate(view->tab_group, view);

	server->focus_user_initiated = true;
	if (!view->scene_tree->node.enabled) {
		wm_view_unminimize(view);
	} else {
		wm_focus_view(view, NULL);
		wm_view_raise(view);
	}
}

bool
wm_view_request_activate(struct wm_view *view, bool allowed)
{
	if (!view || !view->mapped || view->docked || view->no_focus)
		return false;
	struct wm_view *focused = view->server->focused_view;

	/* [FocusProtection]: Deny never claims focus; nothing beats Lock */
	if (view->focus_protection & WM_FOCUS_PROT_DENY)
		allowed = false;
	if (focused && focused != view &&
	    (focused->focus_protection & WM_FOCUS_PROT_LOCK) ==
	    WM_FOCUS_PROT_LOCK)
		allowed = false;

	if (!allowed) {
		/* Not now: the iconbar shows it wants attention */
		if (focused != view)
			wm_view_set_urgent(view, true);
		return false;
	}
	wm_view_present(view);
	return true;
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
		if (view->mapped && !view->docked &&
		    !view->scene_tree->node.enabled) {
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
		if (view->mapped && !view->docked &&
		    !view->scene_tree->node.enabled) {
			wlr_scene_node_set_enabled(
				&view->scene_tree->node, true);
			wm_foreign_toplevel_set_minimized(view, false);
			wm_view_set_minimized_state(view, false);
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
		if (view->mapped && !view->docked &&
		    !view->scene_tree->node.enabled) {
			wlr_scene_node_set_enabled(
				&view->scene_tree->node, true);
			wm_foreign_toplevel_set_minimized(view, false);
			wm_view_set_minimized_state(view, false);
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
		wm_focus_view(best, wm_view_surface(best));
}
