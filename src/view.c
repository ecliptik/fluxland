/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
 * view.c - Window/view lifecycle, focus, and XDG shell handling
 */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>

#include "config.h"
#include "decoration.h"
#include "rules.h"
#include "server.h"
#include "tabgroup.h"
#include "view.h"
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

	/* Update decoration on newly focused view */
	if (view->decoration) {
		wm_decoration_set_focused(view->decoration, true,
			server->style);
	}

	/* Send keyboard focus */
	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
	if (keyboard) {
		wlr_seat_keyboard_notify_enter(seat,
			view->xdg_toplevel->base->surface,
			keyboard->keycodes, keyboard->num_keycodes,
			&keyboard->modifiers);
	}
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
wm_view_raise(struct wm_view *view)
{
	wl_list_remove(&view->link);
	wl_list_insert(&view->server->views, &view->link);
	wlr_scene_node_raise_to_top(&view->scene_tree->node);
}

void
wm_focus_update_for_cursor(struct wm_server *server,
	double cursor_x, double cursor_y)
{
	/* Only apply sloppy focus policy */
	if (server->focus_policy != WM_FOCUS_SLOPPY) {
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

		wlr_xdg_toplevel_set_activated(view->xdg_toplevel, true);
		server->focused_view = view;

		struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
		if (keyboard) {
			wlr_seat_keyboard_notify_enter(seat,
				view->xdg_toplevel->base->surface,
				keyboard->keycodes, keyboard->num_keycodes,
				&keyboard->modifiers);
		}
	}
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

	/* Only the focused view may be interactively moved/resized */
	if (view->xdg_toplevel->base->surface !=
			wlr_surface_get_root_surface(focused)) {
		return;
	}

	server->grabbed_view = view;
	server->cursor_mode = mode;

	if (mode == WM_CURSOR_MOVE) {
		server->grab_x = server->cursor->x - view->x;
		server->grab_y = server->cursor->y - view->y;
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

	wlr_scene_node_set_enabled(&view->scene_tree->node, true);
	wm_focus_view(view, view->xdg_toplevel->base->surface);
}

static void
handle_xdg_toplevel_unmap(struct wl_listener *listener, void *data)
{
	struct wm_view *view = wl_container_of(listener, view, unmap);

	wlr_scene_node_set_enabled(&view->scene_tree->node, false);

	/* If this was the grabbed view, cancel the grab */
	if (view == view->server->grabbed_view) {
		view->server->cursor_mode = WM_CURSOR_PASSTHROUGH;
		view->server->grabbed_view = NULL;
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
		view->maximized = true;
	}

	wlr_xdg_toplevel_set_maximized(view->xdg_toplevel,
		view->maximized);
}

static void
handle_xdg_toplevel_request_fullscreen(struct wl_listener *listener,
	void *data)
{
	struct wm_view *view = wl_container_of(listener, view,
		request_fullscreen);

	if (view->fullscreen) {
		/* Restore */
		wlr_xdg_toplevel_set_size(view->xdg_toplevel,
			view->saved_geometry.width,
			view->saved_geometry.height);
		wlr_scene_node_set_position(&view->scene_tree->node,
			view->saved_geometry.x, view->saved_geometry.y);
		view->x = view->saved_geometry.x;
		view->y = view->saved_geometry.y;
		view->fullscreen = false;
	} else {
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
	}

	wlr_xdg_toplevel_set_fullscreen(view->xdg_toplevel,
		view->fullscreen);
}

static void
handle_xdg_toplevel_request_minimize(struct wl_listener *listener,
	void *data)
{
	struct wm_view *view = wl_container_of(listener, view,
		request_minimize);

	/* Basic minimize: just hide the view */
	wlr_scene_node_set_enabled(&view->scene_tree->node, false);

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
}

static void
handle_xdg_toplevel_set_app_id(struct wl_listener *listener, void *data)
{
	struct wm_view *view = wl_container_of(listener, view, set_app_id);
	free(view->app_id);
	view->app_id = view->xdg_toplevel->app_id ?
		strdup(view->xdg_toplevel->app_id) : NULL;
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
		wlr_log(WLR_ERROR, "allocation failed");
		return;
	}

	view->server = server;
	view->xdg_toplevel = xdg_toplevel;

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
