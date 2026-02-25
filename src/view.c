/*
 * fluxland - A Fluxbox-inspired Wayland compositor
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

#include "config.h"
#include "decoration.h"
#include "foreign_toplevel.h"
#include "ipc.h"
#include "output.h"
#include "placement.h"
#include "rules.h"
#include "server.h"
#include "tabgroup.h"
#include "text_input.h"
#include "toolbar.h"
#include "view.h"
#include "workspace.h"

/* Escape a string for safe inclusion in JSON.
 * Writes into dst (up to dst_size bytes including NUL).
 * Escapes backslash, double-quote, and control characters. */
static void
json_escape(char *dst, size_t dst_size, const char *src)
{
	if (!src) {
		if (dst_size > 0)
			dst[0] = '\0';
		return;
	}
	size_t j = 0;
	for (size_t i = 0; src[i] && j + 1 < dst_size; i++) {
		unsigned char c = (unsigned char)src[i];
		if (c == '"' || c == '\\') {
			if (j + 2 >= dst_size) break;
			dst[j++] = '\\';
			dst[j++] = c;
		} else if (c < 0x20) {
			/* Skip control characters */
			continue;
		} else {
			dst[j++] = c;
		}
	}
	dst[j] = '\0';
}

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
		return;
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
		json_escape(esc_app, sizeof(esc_app),
			view->app_id);
		json_escape(esc_title, sizeof(esc_title),
			view->title);
		snprintf(buf, sizeof(buf),
			"{\"event\":\"window_focus\","
			"\"id\":%u,"
			"\"app_id\":\"%s\","
			"\"title\":\"%s\"}",
			view->id,
			esc_app, esc_title);
		wm_ipc_broadcast_event(&server->ipc,
			WM_IPC_EVENT_WINDOW_FOCUS, buf);
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

static struct wlr_scene_tree *
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

static void
deiconify_view(struct wm_view *view)
{
	wlr_scene_node_set_enabled(&view->scene_tree->node, true);
	wm_foreign_toplevel_set_minimized(view, false);
	wm_focus_view(view, view->xdg_toplevel->base->surface);
	wm_view_raise(view);
	wm_toolbar_update_iconbar(view->server->toolbar);
}

/* --- Window cycling (workspace-aware, with de-iconify) --- */

void
wm_view_cycle_next(struct wm_server *server)
{
	struct wm_workspace *ws = server->current_workspace;
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
	struct wm_workspace *ws = server->current_workspace;
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
	struct wm_workspace *ws = server->current_workspace;

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
	struct wm_workspace *ws = server->current_workspace;
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
		json_escape(esc_app, sizeof(esc_app),
			view->app_id);
		json_escape(esc_title, sizeof(esc_title),
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
}

static void
handle_xdg_toplevel_unmap(struct wl_listener *listener, void *data)
{
	struct wm_view *view = wl_container_of(listener, view, unmap);

	/* Destroy foreign toplevel handle (sends closed to taskbars) */
	wm_foreign_toplevel_handle_destroy(view);

	wlr_scene_node_set_enabled(&view->scene_tree->node, false);

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
				return;
			}
		}
		/* No other views, clear focus */
		wlr_seat_keyboard_notify_clear_focus(view->server->seat);
	}

	/* Update toolbar icon bar (window removed from list) */
	wm_toolbar_update_iconbar(view->server->toolbar);
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
}

static void
handle_xdg_toplevel_destroy(struct wl_listener *listener, void *data)
{
	struct wm_view *view = wl_container_of(listener, view, destroy);

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
	wm_toolbar_update_iconbar(view->server->toolbar);

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

	if (view->maximized) {
		/* Restore saved geometry */
		wlr_xdg_toplevel_set_size(view->xdg_toplevel,
			view->saved_geometry.width,
			view->saved_geometry.height);
		wlr_scene_node_set_position(&view->scene_tree->node,
			view->saved_geometry.x, view->saved_geometry.y);
		view->x = view->saved_geometry.x;
		view->y = view->saved_geometry.y;
		view->maximized = false;
	} else {
		/* Save current geometry and maximize */
		wm_view_get_geometry(view, &view->saved_geometry);

		struct wlr_output *wlr_output =
			wlr_output_layout_output_at(
				view->server->output_layout,
				view->server->cursor->x,
				view->server->cursor->y);
		if (wlr_output) {
			struct wlr_box max_box;
			bool use_full = view->server->config &&
				view->server->config->full_maximization;

			if (use_full) {
				wlr_output_layout_get_box(
					view->server->output_layout,
					wlr_output, &max_box);
			} else {
				/* Use usable area (respects toolbar/slit) */
				bool found = false;
				struct wm_output *wm_out;
				wl_list_for_each(wm_out,
					&view->server->outputs, link) {
					if (wm_out->wlr_output == wlr_output &&
					    wm_out->usable_area.width > 0 &&
					    wm_out->usable_area.height > 0) {
						max_box = wm_out->usable_area;
						found = true;
						break;
					}
				}
				if (!found) {
					wlr_output_layout_get_box(
						view->server->output_layout,
						wlr_output, &max_box);
				}
			}

			wlr_xdg_toplevel_set_size(view->xdg_toplevel,
				max_box.width, max_box.height);
			wlr_scene_node_set_position(
				&view->scene_tree->node,
				max_box.x, max_box.y);
			view->x = max_box.x;
			view->y = max_box.y;
		}
		view->maximized = true;
	}

	wlr_xdg_toplevel_set_maximized(view->xdg_toplevel,
		view->maximized);
	wm_foreign_toplevel_set_maximized(view, view->maximized);
}

static void
handle_xdg_toplevel_request_fullscreen(struct wl_listener *listener,
	void *data)
{
	struct wm_view *view = wl_container_of(listener, view,
		request_fullscreen);

	if (view->fullscreen) {
		/* Restore from fullscreen */
		wlr_xdg_toplevel_set_size(view->xdg_toplevel,
			view->saved_geometry.width,
			view->saved_geometry.height);
		wlr_scene_node_set_position(&view->scene_tree->node,
			view->saved_geometry.x, view->saved_geometry.y);
		view->x = view->saved_geometry.x;
		view->y = view->saved_geometry.y;
		view->fullscreen = false;

		/* Reparent back from overlay to workspace/sticky tree */
		struct wlr_scene_tree *parent;
		if (view->sticky) {
			parent = view->server->sticky_tree;
		} else if (view->workspace) {
			parent = view->workspace->tree;
		} else {
			parent = view->server->view_tree;
		}
		wlr_scene_node_reparent(&view->scene_tree->node, parent);

		/* Show decorations and reposition XDG surface */
		if (view->decoration) {
			wlr_scene_node_set_enabled(
				&view->decoration->tree->node, true);
			int top, bottom, left, right;
			wm_decoration_get_extents(view->decoration,
				&top, &bottom, &left, &right);
			struct wlr_scene_node *child;
			wl_list_for_each(child,
				&view->scene_tree->children, link) {
				if (child !=
					&view->decoration->tree->node) {
					wlr_scene_node_set_position(
						child, left, top);
					break;
				}
			}
		}
	} else {
		/* Enter fullscreen */
		if (!view->maximized) {
			wm_view_get_geometry(view, &view->saved_geometry);
		}

		struct wlr_output *output =
			wlr_output_layout_output_at(
				view->server->output_layout,
				view->server->cursor->x,
				view->server->cursor->y);
		if (output) {
			struct wlr_box output_box;
			wlr_output_layout_get_box(
				view->server->output_layout,
				output, &output_box);
			wlr_xdg_toplevel_set_size(view->xdg_toplevel,
				output_box.width, output_box.height);
			wlr_scene_node_set_position(
				&view->scene_tree->node,
				output_box.x, output_box.y);
			view->x = output_box.x;
			view->y = output_box.y;
		}
		view->fullscreen = true;

		/* Hide decorations and move XDG surface to (0,0) */
		if (view->decoration) {
			wlr_scene_node_set_enabled(
				&view->decoration->tree->node, false);
			struct wlr_scene_node *child;
			wl_list_for_each(child,
				&view->scene_tree->children, link) {
				if (child !=
					&view->decoration->tree->node) {
					wlr_scene_node_set_position(
						child, 0, 0);
					break;
				}
			}
		}

		/* Reparent to overlay layer (above toolbar) */
		wlr_scene_node_reparent(&view->scene_tree->node,
			view->server->layer_overlay);
	}

	wlr_xdg_toplevel_set_fullscreen(view->xdg_toplevel,
		view->fullscreen);
	wm_foreign_toplevel_set_fullscreen(view, view->fullscreen);
}

static void
handle_xdg_toplevel_request_minimize(struct wl_listener *listener,
	void *data)
{
	struct wm_view *view = wl_container_of(listener, view,
		request_minimize);

	/* Basic minimize: just hide the view */
	wlr_scene_node_set_enabled(&view->scene_tree->node, false);
	wm_foreign_toplevel_set_minimized(view, true);

	/* Focus next view */
	struct wm_view *next;
	wl_list_for_each(next, &view->server->views, link) {
		if (next != view && next->xdg_toplevel->base->surface) {
			wm_focus_view(next,
				next->xdg_toplevel->base->surface);
			return;
		}
	}
	wlr_seat_keyboard_notify_clear_focus(view->server->seat);

	/* Update toolbar icon bar (window iconified) */
	wm_toolbar_update_iconbar(view->server->toolbar);
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
		json_escape(esc_title, sizeof(esc_title), view->title);
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

	/* Assign to current workspace */
	view->workspace = server->current_workspace;

	/*
	 * Create scene tree for this view under its workspace's tree.
	 * If workspaces aren't initialized yet, fall back to view_tree.
	 */
	struct wlr_scene_tree *parent = server->current_workspace ?
		server->current_workspace->tree : server->view_tree;
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

/* --- Maximize vertical/horizontal --- */

void
wm_view_maximize_vert(struct wm_view *view)
{
	if (!view) {
		return;
	}

	if (view->maximized_vert) {
		/* Restore saved y and height */
		view->y = view->saved_geometry.y;
		wlr_xdg_toplevel_set_size(view->xdg_toplevel,
			0, view->saved_geometry.height);
		wlr_scene_node_set_position(&view->scene_tree->node,
			view->x, view->y);
		view->maximized_vert = false;
	} else {
		/* Save current geometry */
		struct wlr_box geo;
		wm_view_get_geometry(view, &geo);
		view->saved_geometry.y = view->y;
		view->saved_geometry.height = geo.height;

		struct wlr_output *output =
			wlr_output_layout_output_at(
				view->server->output_layout,
				view->server->cursor->x,
				view->server->cursor->y);
		if (output) {
			struct wlr_box output_box;
			wlr_output_layout_get_box(
				view->server->output_layout,
				output, &output_box);
			view->y = output_box.y;
			wlr_xdg_toplevel_set_size(view->xdg_toplevel,
				0, output_box.height);
			wlr_scene_node_set_position(
				&view->scene_tree->node,
				view->x, view->y);
		}
		view->maximized_vert = true;
	}
}

void
wm_view_maximize_horiz(struct wm_view *view)
{
	if (!view) {
		return;
	}

	if (view->maximized_horiz) {
		/* Restore saved x and width */
		view->x = view->saved_geometry.x;
		wlr_xdg_toplevel_set_size(view->xdg_toplevel,
			view->saved_geometry.width, 0);
		wlr_scene_node_set_position(&view->scene_tree->node,
			view->x, view->y);
		view->maximized_horiz = false;
	} else {
		/* Save current geometry */
		struct wlr_box geo;
		wm_view_get_geometry(view, &geo);
		view->saved_geometry.x = view->x;
		view->saved_geometry.width = geo.width;

		struct wlr_output *output =
			wlr_output_layout_output_at(
				view->server->output_layout,
				view->server->cursor->x,
				view->server->cursor->y);
		if (output) {
			struct wlr_box output_box;
			wlr_output_layout_get_box(
				view->server->output_layout,
				output, &output_box);
			view->x = output_box.x;
			wlr_xdg_toplevel_set_size(view->xdg_toplevel,
				output_box.width, 0);
			wlr_scene_node_set_position(
				&view->scene_tree->node,
				view->x, view->y);
		}
		view->maximized_horiz = true;
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

/* --- Helper: get usable area for a view's output --- */

static bool
get_view_output_area(struct wm_view *view, struct wlr_box *area)
{
	struct wlr_output *output = wlr_output_layout_output_at(
		view->server->output_layout,
		view->x + 1, view->y + 1);
	if (!output) {
		output = wlr_output_layout_output_at(
			view->server->output_layout,
			view->server->cursor->x,
			view->server->cursor->y);
	}
	if (!output)
		return false;

	struct wm_output *wm_out;
	wl_list_for_each(wm_out, &view->server->outputs, link) {
		if (wm_out->wlr_output == output) {
			if (wm_out->usable_area.width > 0 &&
			    wm_out->usable_area.height > 0) {
				*area = wm_out->usable_area;
			} else {
				wlr_output_layout_get_box(
					view->server->output_layout,
					output, area);
			}
			return true;
		}
	}

	wlr_output_layout_get_box(view->server->output_layout,
		output, area);
	return true;
}

/* --- LHalf / RHalf (half-screen tiling) --- */

void
wm_view_lhalf(struct wm_view *view)
{
	if (!view)
		return;

	if (view->lhalf) {
		/* Toggle off — restore saved geometry */
		view->x = view->saved_geometry.x;
		view->y = view->saved_geometry.y;
		wlr_xdg_toplevel_set_size(view->xdg_toplevel,
			view->saved_geometry.width,
			view->saved_geometry.height);
		wlr_scene_node_set_position(&view->scene_tree->node,
			view->x, view->y);
		view->lhalf = false;
		return;
	}

	/* Save geometry if not already in a tiled/maximized state */
	if (!view->maximized && !view->rhalf)
		wm_view_get_geometry(view, &view->saved_geometry);

	struct wlr_box area;
	if (!get_view_output_area(view, &area))
		return;

	int half_w = area.width / 2;
	view->x = area.x;
	view->y = area.y;
	wlr_xdg_toplevel_set_size(view->xdg_toplevel,
		half_w, area.height);
	wlr_scene_node_set_position(&view->scene_tree->node,
		view->x, view->y);

	view->lhalf = true;
	view->rhalf = false;
	view->maximized = false;
}

void
wm_view_rhalf(struct wm_view *view)
{
	if (!view)
		return;

	if (view->rhalf) {
		/* Toggle off — restore saved geometry */
		view->x = view->saved_geometry.x;
		view->y = view->saved_geometry.y;
		wlr_xdg_toplevel_set_size(view->xdg_toplevel,
			view->saved_geometry.width,
			view->saved_geometry.height);
		wlr_scene_node_set_position(&view->scene_tree->node,
			view->x, view->y);
		view->rhalf = false;
		return;
	}

	/* Save geometry if not already in a tiled/maximized state */
	if (!view->maximized && !view->lhalf)
		wm_view_get_geometry(view, &view->saved_geometry);

	struct wlr_box area;
	if (!get_view_output_area(view, &area))
		return;

	int half_w = area.width / 2;
	view->x = area.x + half_w;
	view->y = area.y;
	wlr_xdg_toplevel_set_size(view->xdg_toplevel,
		half_w, area.height);
	wlr_scene_node_set_position(&view->scene_tree->node,
		view->x, view->y);

	view->rhalf = true;
	view->lhalf = false;
	view->maximized = false;
}

/* --- Relative resize --- */

void
wm_view_resize_by(struct wm_view *view, int dw, int dh)
{
	if (!view)
		return;

	struct wlr_box geo;
	wm_view_get_geometry(view, &geo);
	int new_w = geo.width + dw;
	int new_h = geo.height + dh;
	if (new_w > 10 && new_h > 10)
		wlr_xdg_toplevel_set_size(view->xdg_toplevel,
			new_w, new_h);
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
		if (v->workspace != server->current_workspace && !v->sticky)
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

/* --- Multi-monitor helpers --- */

static void
move_view_to_output(struct wm_view *view, struct wm_output *out)
{
	struct wlr_box area;
	if (out->usable_area.width > 0 &&
	    out->usable_area.height > 0) {
		area = out->usable_area;
	} else {
		wlr_output_layout_get_box(
			view->server->output_layout,
			out->wlr_output, &area);
	}

	struct wlr_box vgeo;
	wm_view_get_geometry(view, &vgeo);
	view->x = area.x + (area.width - vgeo.width) / 2;
	view->y = area.y + (area.height - vgeo.height) / 2;
	wlr_scene_node_set_position(&view->scene_tree->node,
		view->x, view->y);
}

void
wm_view_set_head(struct wm_view *view, int head_index)
{
	if (!view)
		return;

	int idx = 0;
	struct wm_output *out;
	wl_list_for_each(out, &view->server->outputs, link) {
		if (idx == head_index) {
			move_view_to_output(view, out);
			return;
		}
		idx++;
	}
}

void
wm_view_send_to_next_head(struct wm_view *view)
{
	if (!view)
		return;

	struct wlr_output *current = wlr_output_layout_output_at(
		view->server->output_layout,
		view->x + 1, view->y + 1);
	if (!current)
		current = wlr_output_layout_output_at(
			view->server->output_layout,
			view->server->cursor->x,
			view->server->cursor->y);

	struct wm_output *out;
	struct wm_output *first_out = NULL;
	bool found_current = false;

	wl_list_for_each(out, &view->server->outputs, link) {
		if (!first_out)
			first_out = out;
		if (out->wlr_output == current) {
			found_current = true;
			continue;
		}
		if (found_current) {
			move_view_to_output(view, out);
			return;
		}
	}

	/* Wrap around to first output */
	if (first_out)
		move_view_to_output(view, first_out);
}

void
wm_view_send_to_prev_head(struct wm_view *view)
{
	if (!view)
		return;

	struct wlr_output *current = wlr_output_layout_output_at(
		view->server->output_layout,
		view->x + 1, view->y + 1);
	if (!current)
		current = wlr_output_layout_output_at(
			view->server->output_layout,
			view->server->cursor->x,
			view->server->cursor->y);

	struct wm_output *out;
	struct wm_output *prev_out = NULL;
	struct wm_output *last_out = NULL;

	wl_list_for_each(out, &view->server->outputs, link) {
		last_out = out;
		if (out->wlr_output == current) {
			if (prev_out) {
				move_view_to_output(view, prev_out);
				return;
			}
			/* Current is first output, wrap to last */
			break;
		}
		prev_out = out;
	}

	/* Wrap to last output */
	if (last_out)
		move_view_to_output(view, last_out);
}

/* --- Close all windows on current workspace --- */

void
wm_view_close_all(struct wm_server *server)
{
	struct wm_view *v;
	wl_list_for_each(v, &server->views, link) {
		if (v->workspace == server->current_workspace)
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
