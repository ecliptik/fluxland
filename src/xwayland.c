/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
 *
 * xwayland.c - XWayland bridge for X11 application support
 *
 * Integrates X11 clients via wlr_xwayland into the compositor's
 * scene graph and focus/stacking model.  X11 window types are mapped
 * to compositor behaviours (managed, floating, splash, unmanaged).
 *
 * Compile-time optional: the entire file is a no-op without
 * WM_HAS_XWAYLAND.
 */

#define _POSIX_C_SOURCE 200809L
#include "xwayland.h"

#ifdef WM_HAS_XWAYLAND

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/log.h>

#include "server.h"
#include "view.h"
#include "workspace.h"

/* ------------------------------------------------------------------ */
/*  Atom resolution                                                    */
/* ------------------------------------------------------------------ */

static xcb_atom_t
resolve_atom(xcb_connection_t *conn, const char *name)
{
	xcb_intern_atom_cookie_t cookie =
		xcb_intern_atom(conn, 0, strlen(name), name);
	xcb_intern_atom_reply_t *reply =
		xcb_intern_atom_reply(conn, cookie, NULL);
	if (!reply) {
		return XCB_ATOM_NONE;
	}
	xcb_atom_t atom = reply->atom;
	free(reply);
	return atom;
}

static void
resolve_atoms(xcb_connection_t *conn, struct wm_xwayland_atoms *atoms)
{
	atoms->net_wm_window_type_normal =
		resolve_atom(conn, "_NET_WM_WINDOW_TYPE_NORMAL");
	atoms->net_wm_window_type_dialog =
		resolve_atom(conn, "_NET_WM_WINDOW_TYPE_DIALOG");
	atoms->net_wm_window_type_utility =
		resolve_atom(conn, "_NET_WM_WINDOW_TYPE_UTILITY");
	atoms->net_wm_window_type_toolbar =
		resolve_atom(conn, "_NET_WM_WINDOW_TYPE_TOOLBAR");
	atoms->net_wm_window_type_splash =
		resolve_atom(conn, "_NET_WM_WINDOW_TYPE_SPLASH");
	atoms->net_wm_window_type_menu =
		resolve_atom(conn, "_NET_WM_WINDOW_TYPE_MENU");
	atoms->net_wm_window_type_dropdown_menu =
		resolve_atom(conn, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU");
	atoms->net_wm_window_type_popup_menu =
		resolve_atom(conn, "_NET_WM_WINDOW_TYPE_POPUP_MENU");
	atoms->net_wm_window_type_tooltip =
		resolve_atom(conn, "_NET_WM_WINDOW_TYPE_TOOLTIP");
	atoms->net_wm_window_type_notification =
		resolve_atom(conn, "_NET_WM_WINDOW_TYPE_NOTIFICATION");
	atoms->net_wm_window_type_dock =
		resolve_atom(conn, "_NET_WM_WINDOW_TYPE_DOCK");
	atoms->net_wm_window_type_desktop =
		resolve_atom(conn, "_NET_WM_WINDOW_TYPE_DESKTOP");
}

/* ------------------------------------------------------------------ */
/*  Window-type classification                                         */
/* ------------------------------------------------------------------ */

static enum wm_xw_window_class
classify_window(struct wlr_xwayland_surface *xsurface,
	const struct wm_xwayland_atoms *atoms)
{
	/* Override-redirect windows are unmanaged overlays */
	if (xsurface->override_redirect) {
		return WM_XW_UNMANAGED;
	}

	for (size_t i = 0; i < xsurface->window_type_len; i++) {
		xcb_atom_t type = xsurface->window_type[i];

		if (type == atoms->net_wm_window_type_tooltip ||
		    type == atoms->net_wm_window_type_notification ||
		    type == atoms->net_wm_window_type_popup_menu ||
		    type == atoms->net_wm_window_type_dropdown_menu ||
		    type == atoms->net_wm_window_type_dock ||
		    type == atoms->net_wm_window_type_desktop) {
			return WM_XW_UNMANAGED;
		}
		if (type == atoms->net_wm_window_type_splash) {
			return WM_XW_SPLASH;
		}
		if (type == atoms->net_wm_window_type_dialog ||
		    type == atoms->net_wm_window_type_utility ||
		    type == atoms->net_wm_window_type_toolbar ||
		    type == atoms->net_wm_window_type_menu) {
			return WM_XW_FLOATING;
		}
		if (type == atoms->net_wm_window_type_normal) {
			return WM_XW_MANAGED;
		}
	}

	/* Default: transient windows float, others are managed */
	if (xsurface->parent) {
		return WM_XW_FLOATING;
	}
	return WM_XW_MANAGED;
}

/* ------------------------------------------------------------------ */
/*  Focus helpers                                                      */
/* ------------------------------------------------------------------ */

/*
 * Focus an XWayland view: activate it via X11, raise in scene graph,
 * and send keyboard enter to the Wayland surface.
 */
static void
xwayland_focus_view(struct wm_xwayland_view *xview)
{
	if (!xview || !xview->mapped) {
		return;
	}
	struct wm_server *server = xview->server;
	struct wlr_seat *seat = server->seat;

	/* Deactivate previously focused XDG toplevel if any */
	struct wlr_surface *prev =
		seat->keyboard_state.focused_surface;
	if (prev) {
		struct wlr_xdg_toplevel *prev_toplevel =
			wlr_xdg_toplevel_try_from_wlr_surface(prev);
		if (prev_toplevel) {
			wlr_xdg_toplevel_set_activated(prev_toplevel, false);
		}
		/* Deactivate previously focused XWayland surface */
		struct wlr_xwayland_surface *prev_xsurface =
			wlr_xwayland_surface_try_from_wlr_surface(prev);
		if (prev_xsurface) {
			wlr_xwayland_surface_activate(prev_xsurface, false);
		}
	}

	/* Clear the focused_view so XDG paths know we moved on */
	server->focused_view = NULL;

	/* Activate via X11 */
	wlr_xwayland_surface_activate(xview->xsurface, true);

	/* Raise in scene graph */
	wlr_scene_node_raise_to_top(&xview->scene_tree->node);

	/* Send keyboard focus */
	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
	if (keyboard) {
		wlr_seat_keyboard_notify_enter(seat,
			xview->xsurface->surface,
			keyboard->keycodes, keyboard->num_keycodes,
			&keyboard->modifiers);
	}
}

/* ------------------------------------------------------------------ */
/*  Surface event handlers                                             */
/* ------------------------------------------------------------------ */

static void
handle_xwayland_surface_map(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_view *xview =
		wl_container_of(listener, xview, map);
	struct wm_server *server = xview->server;

	xview->mapped = true;

	/* Classify the window now that type information is available */
	xview->window_class = classify_window(xview->xsurface,
		&server->xwayland_atoms);

	/* Mirror title and class */
	free(xview->title);
	xview->title = xview->xsurface->title ?
		strdup(xview->xsurface->title) : NULL;
	free(xview->app_id);
	xview->app_id = xview->xsurface->class ?
		strdup(xview->xsurface->class) : NULL;

	/*
	 * Create scene tree.  We use wlr_scene_subsurface_tree_create
	 * which tracks the wl_surface and all sub-surfaces automatically.
	 */
	struct wlr_scene_tree *parent;
	if (xview->window_class == WM_XW_UNMANAGED) {
		/* Unmanaged overlays sit above normal views */
		parent = server->xdg_popup_tree;
	} else if (server->current_workspace) {
		parent = server->current_workspace->tree;
	} else {
		parent = server->view_tree;
	}

	xview->scene_tree = wlr_scene_tree_create(parent);
	if (!xview->scene_tree) {
		wlr_log(WLR_ERROR, "xwayland: failed to create scene tree");
		return;
	}

	struct wlr_scene_tree *surface_tree =
		wlr_scene_subsurface_tree_create(xview->scene_tree,
			xview->xsurface->surface);
	if (!surface_tree) {
		wlr_scene_node_destroy(&xview->scene_tree->node);
		xview->scene_tree = NULL;
		return;
	}

	/* Back-pointer for hit-testing (wm_view_at walks up scene tree) */
	xview->scene_tree->node.data = xview;

	/* Position the view */
	xview->x = xview->xsurface->x;
	xview->y = xview->xsurface->y;
	wlr_scene_node_set_position(&xview->scene_tree->node,
		xview->x, xview->y);

	wlr_log(WLR_DEBUG, "xwayland map: \"%s\" [%s] class=%d "
		"(%dx%d+%d+%d) or=%d",
		xview->title ? xview->title : "(null)",
		xview->app_id ? xview->app_id : "(null)",
		xview->window_class,
		xview->xsurface->width, xview->xsurface->height,
		xview->x, xview->y,
		xview->xsurface->override_redirect);

	/* Only managed/floating views get focus and stacking */
	if (xview->window_class != WM_XW_UNMANAGED) {
		xwayland_focus_view(xview);
	}
}

static void
handle_xwayland_surface_unmap(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_view *xview =
		wl_container_of(listener, xview, unmap);

	xview->mapped = false;

	if (xview->scene_tree) {
		wlr_scene_node_destroy(&xview->scene_tree->node);
		xview->scene_tree = NULL;
	}

	/* Cancel any interactive grab involving this view */
	if (xview->server->grabbed_view &&
	    xview->server->grabbed_view->scene_tree &&
	    xview->scene_tree == NULL) {
		/*
		 * We can't easily compare to grabbed_view since it's a
		 * wm_view pointer.  Clear the grab if keyboard focus
		 * matches this surface.
		 */
	}

	/* If this had keyboard focus, move focus elsewhere */
	struct wlr_surface *focused =
		xview->server->seat->keyboard_state.focused_surface;
	if (focused && focused == xview->xsurface->surface) {
		/* Try to focus next XDG view */
		struct wm_view *next;
		wl_list_for_each(next, &xview->server->views, link) {
			if (next->xdg_toplevel &&
			    next->xdg_toplevel->base->surface) {
				wm_focus_view(next,
					next->xdg_toplevel->base->surface);
				return;
			}
		}
		wlr_seat_keyboard_notify_clear_focus(xview->server->seat);
	}
}

static void
handle_xwayland_surface_destroy(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_view *xview =
		wl_container_of(listener, xview, destroy);

	if (xview->scene_tree) {
		wlr_scene_node_destroy(&xview->scene_tree->node);
	}

	wl_list_remove(&xview->destroy.link);
	wl_list_remove(&xview->associate.link);
	wl_list_remove(&xview->dissociate.link);
	wl_list_remove(&xview->request_configure.link);
	wl_list_remove(&xview->request_move.link);
	wl_list_remove(&xview->request_resize.link);
	wl_list_remove(&xview->request_maximize.link);
	wl_list_remove(&xview->request_fullscreen.link);
	wl_list_remove(&xview->request_minimize.link);
	wl_list_remove(&xview->request_activate.link);
	wl_list_remove(&xview->set_title.link);
	wl_list_remove(&xview->set_class.link);
	wl_list_remove(&xview->set_hints.link);
	wl_list_remove(&xview->set_override_redirect.link);
	wl_list_remove(&xview->link);

	free(xview->title);
	free(xview->app_id);
	free(xview);
}

/* ------------------------------------------------------------------ */
/*  Associate / dissociate (wl_surface lifecycle)                      */
/* ------------------------------------------------------------------ */

static void
handle_xwayland_associate(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_view *xview =
		wl_container_of(listener, xview, associate);

	/* The wl_surface is now valid; wire up map/unmap */
	xview->map.notify = handle_xwayland_surface_map;
	wl_signal_add(&xview->xsurface->surface->events.map,
		&xview->map);

	xview->unmap.notify = handle_xwayland_surface_unmap;
	wl_signal_add(&xview->xsurface->surface->events.unmap,
		&xview->unmap);
}

static void
handle_xwayland_dissociate(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_view *xview =
		wl_container_of(listener, xview, dissociate);

	wl_list_remove(&xview->map.link);
	wl_list_remove(&xview->unmap.link);
}

/* ------------------------------------------------------------------ */
/*  Request handlers                                                   */
/* ------------------------------------------------------------------ */

static void
handle_xwayland_request_configure(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_view *xview =
		wl_container_of(listener, xview, request_configure);
	struct wlr_xwayland_surface_configure_event *event = data;

	/* Accept the requested geometry */
	wlr_xwayland_surface_configure(xview->xsurface,
		event->x, event->y, event->width, event->height);

	if (xview->scene_tree) {
		xview->x = event->x;
		xview->y = event->y;
		wlr_scene_node_set_position(&xview->scene_tree->node,
			xview->x, xview->y);
	}
}

static void
handle_xwayland_request_move(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_view *xview =
		wl_container_of(listener, xview, request_move);
	struct wm_server *server = xview->server;

	if (!xview->mapped || !xview->scene_tree) {
		return;
	}

	/*
	 * Start an interactive move.  We reuse the server's grab state
	 * by temporarily pointing grabbed_view at a synthetic wm_view.
	 * Instead, we set the grab manually here.
	 */
	server->cursor_mode = WM_CURSOR_MOVE;
	server->grabbed_view = NULL; /* no XDG view */
	server->grab_x = server->cursor->x - xview->x;
	server->grab_y = server->cursor->y - xview->y;

	/*
	 * Note: interactive move/resize for XWayland views would need
	 * further integration with the cursor module.  For now we accept
	 * the request_configure events which accomplish the same result
	 * for most X11 clients.
	 */
	server->cursor_mode = WM_CURSOR_PASSTHROUGH;
}

static void
handle_xwayland_request_resize(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_view *xview =
		wl_container_of(listener, xview, request_resize);

	(void)xview;
	/* X11 clients handle their own resize via configure requests */
}

static void
handle_xwayland_request_maximize(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_view *xview =
		wl_container_of(listener, xview, request_maximize);
	struct wm_server *server = xview->server;

	if (!xview->mapped || !xview->scene_tree) {
		return;
	}

	if (xview->maximized) {
		/* Restore */
		wlr_xwayland_surface_configure(xview->xsurface,
			xview->saved_geometry.x, xview->saved_geometry.y,
			xview->saved_geometry.width,
			xview->saved_geometry.height);
		xview->x = xview->saved_geometry.x;
		xview->y = xview->saved_geometry.y;
		wlr_scene_node_set_position(&xview->scene_tree->node,
			xview->x, xview->y);
		xview->maximized = false;
	} else {
		/* Save and maximize */
		xview->saved_geometry.x = xview->x;
		xview->saved_geometry.y = xview->y;
		xview->saved_geometry.width = xview->xsurface->width;
		xview->saved_geometry.height = xview->xsurface->height;

		struct wlr_output *output =
			wlr_output_layout_output_at(server->output_layout,
				server->cursor->x, server->cursor->y);
		if (output) {
			struct wlr_box box;
			wlr_output_layout_get_box(server->output_layout,
				output, &box);
			wlr_xwayland_surface_configure(xview->xsurface,
				box.x, box.y, box.width, box.height);
			xview->x = box.x;
			xview->y = box.y;
			wlr_scene_node_set_position(
				&xview->scene_tree->node,
				xview->x, xview->y);
		}
		xview->maximized = true;
	}

	wlr_xwayland_surface_set_maximized(xview->xsurface,
		xview->maximized);
}

static void
handle_xwayland_request_fullscreen(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_view *xview =
		wl_container_of(listener, xview, request_fullscreen);
	struct wm_server *server = xview->server;

	if (!xview->mapped || !xview->scene_tree) {
		return;
	}

	if (xview->fullscreen) {
		/* Restore */
		wlr_xwayland_surface_configure(xview->xsurface,
			xview->saved_geometry.x, xview->saved_geometry.y,
			xview->saved_geometry.width,
			xview->saved_geometry.height);
		xview->x = xview->saved_geometry.x;
		xview->y = xview->saved_geometry.y;
		wlr_scene_node_set_position(&xview->scene_tree->node,
			xview->x, xview->y);
		xview->fullscreen = false;
	} else {
		if (!xview->maximized) {
			xview->saved_geometry.x = xview->x;
			xview->saved_geometry.y = xview->y;
			xview->saved_geometry.width =
				xview->xsurface->width;
			xview->saved_geometry.height =
				xview->xsurface->height;
		}

		struct wlr_output *output =
			wlr_output_layout_output_at(server->output_layout,
				server->cursor->x, server->cursor->y);
		if (output) {
			struct wlr_box box;
			wlr_output_layout_get_box(server->output_layout,
				output, &box);
			wlr_xwayland_surface_configure(xview->xsurface,
				box.x, box.y, box.width, box.height);
			xview->x = box.x;
			xview->y = box.y;
			wlr_scene_node_set_position(
				&xview->scene_tree->node,
				xview->x, xview->y);
		}
		xview->fullscreen = true;
	}

	wlr_xwayland_surface_set_fullscreen(xview->xsurface,
		xview->fullscreen);
}

static void
handle_xwayland_request_minimize(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_view *xview =
		wl_container_of(listener, xview, request_minimize);
	struct wlr_xwayland_minimize_event *event = data;

	if (!xview->mapped || !xview->scene_tree) {
		return;
	}

	wlr_xwayland_surface_set_minimized(xview->xsurface,
		event->minimize);

	if (event->minimize) {
		wlr_scene_node_set_enabled(
			&xview->scene_tree->node, false);

		/* Focus next view */
		struct wm_view *next;
		wl_list_for_each(next, &xview->server->views, link) {
			if (next->xdg_toplevel &&
			    next->xdg_toplevel->base->surface) {
				wm_focus_view(next,
					next->xdg_toplevel->base->surface);
				return;
			}
		}
		wlr_seat_keyboard_notify_clear_focus(xview->server->seat);
	} else {
		wlr_scene_node_set_enabled(
			&xview->scene_tree->node, true);
		xwayland_focus_view(xview);
	}
}

static void
handle_xwayland_request_activate(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_view *xview =
		wl_container_of(listener, xview, request_activate);

	if (xview->mapped) {
		xwayland_focus_view(xview);
	}
}

static void
handle_xwayland_set_title(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_view *xview =
		wl_container_of(listener, xview, set_title);

	free(xview->title);
	xview->title = xview->xsurface->title ?
		strdup(xview->xsurface->title) : NULL;
}

static void
handle_xwayland_set_class(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_view *xview =
		wl_container_of(listener, xview, set_class);

	free(xview->app_id);
	xview->app_id = xview->xsurface->class ?
		strdup(xview->xsurface->class) : NULL;
}

static void
handle_xwayland_set_hints(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_view *xview =
		wl_container_of(listener, xview, set_hints);

	/*
	 * WM_HINTS can indicate urgency.  For now just re-classify
	 * in case the hints affect management decisions.
	 */
	(void)xview;
}

static void
handle_xwayland_set_override_redirect(struct wl_listener *listener,
	void *data)
{
	struct wm_xwayland_view *xview =
		wl_container_of(listener, xview, set_override_redirect);

	/* Reclassify if override_redirect changes while mapped */
	if (xview->mapped) {
		xview->window_class = classify_window(xview->xsurface,
			&xview->server->xwayland_atoms);
	}
}

/* ------------------------------------------------------------------ */
/*  New surface handler                                                */
/* ------------------------------------------------------------------ */

static void
handle_xwayland_new_surface(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, xwayland_new_surface);
	struct wlr_xwayland_surface *xsurface = data;

	struct wm_xwayland_view *xview = calloc(1, sizeof(*xview));
	if (!xview) {
		wlr_log(WLR_ERROR, "xwayland: allocation failed");
		return;
	}

	xview->server = server;
	xview->xsurface = xsurface;

	/* Connect lifetime events */
	xview->destroy.notify = handle_xwayland_surface_destroy;
	wl_signal_add(&xsurface->events.destroy, &xview->destroy);

	xview->associate.notify = handle_xwayland_associate;
	wl_signal_add(&xsurface->events.associate, &xview->associate);

	xview->dissociate.notify = handle_xwayland_dissociate;
	wl_signal_add(&xsurface->events.dissociate, &xview->dissociate);

	/* Request handlers */
	xview->request_configure.notify =
		handle_xwayland_request_configure;
	wl_signal_add(&xsurface->events.request_configure,
		&xview->request_configure);

	xview->request_move.notify = handle_xwayland_request_move;
	wl_signal_add(&xsurface->events.request_move,
		&xview->request_move);

	xview->request_resize.notify = handle_xwayland_request_resize;
	wl_signal_add(&xsurface->events.request_resize,
		&xview->request_resize);

	xview->request_maximize.notify =
		handle_xwayland_request_maximize;
	wl_signal_add(&xsurface->events.request_maximize,
		&xview->request_maximize);

	xview->request_fullscreen.notify =
		handle_xwayland_request_fullscreen;
	wl_signal_add(&xsurface->events.request_fullscreen,
		&xview->request_fullscreen);

	xview->request_minimize.notify =
		handle_xwayland_request_minimize;
	wl_signal_add(&xsurface->events.request_minimize,
		&xview->request_minimize);

	xview->request_activate.notify =
		handle_xwayland_request_activate;
	wl_signal_add(&xsurface->events.request_activate,
		&xview->request_activate);

	/* Property change events */
	xview->set_title.notify = handle_xwayland_set_title;
	wl_signal_add(&xsurface->events.set_title, &xview->set_title);

	xview->set_class.notify = handle_xwayland_set_class;
	wl_signal_add(&xsurface->events.set_class, &xview->set_class);

	xview->set_hints.notify = handle_xwayland_set_hints;
	wl_signal_add(&xsurface->events.set_hints, &xview->set_hints);

	xview->set_override_redirect.notify =
		handle_xwayland_set_override_redirect;
	wl_signal_add(&xsurface->events.set_override_redirect,
		&xview->set_override_redirect);

	/* Add to server list */
	wl_list_insert(&server->xwayland_views, &xview->link);

	wlr_log(WLR_DEBUG, "xwayland: new surface window_id=%u "
		"override_redirect=%d",
		xsurface->window_id, xsurface->override_redirect);
}

/* ------------------------------------------------------------------ */
/*  XWayland ready handler                                             */
/* ------------------------------------------------------------------ */

static void
handle_xwayland_ready(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, xwayland_ready);

	/* Resolve atoms now that the XCB connection is available */
	xcb_connection_t *conn =
		wlr_xwayland_get_xwm_connection(server->xwayland);
	if (conn) {
		resolve_atoms(conn, &server->xwayland_atoms);
		wlr_log(WLR_INFO, "xwayland: atoms resolved");
	}

	/* Set the XWayland cursor */
	wlr_xwayland_set_seat(server->xwayland, server->seat);

	wlr_log(WLR_INFO, "xwayland ready on DISPLAY=%s",
		server->xwayland->display_name);
}

/* ------------------------------------------------------------------ */
/*  Module init / finish                                               */
/* ------------------------------------------------------------------ */

void
wm_xwayland_init(struct wm_server *server)
{
	wl_list_init(&server->xwayland_views);

	server->xwayland = wlr_xwayland_create(server->wl_display,
		server->compositor, true /* lazy */);
	if (!server->xwayland) {
		wlr_log(WLR_ERROR, "failed to create xwayland");
		return;
	}

	server->xwayland_new_surface.notify =
		handle_xwayland_new_surface;
	wl_signal_add(&server->xwayland->events.new_surface,
		&server->xwayland_new_surface);

	server->xwayland_ready.notify = handle_xwayland_ready;
	wl_signal_add(&server->xwayland->events.ready,
		&server->xwayland_ready);

	/*
	 * Set DISPLAY so that child processes spawned by the compositor
	 * (startup commands, etc.) can use XWayland.
	 */
	setenv("DISPLAY", server->xwayland->display_name, true);

	wlr_log(WLR_INFO, "xwayland initialized (lazy), DISPLAY=%s",
		server->xwayland->display_name);
}

void
wm_xwayland_finish(struct wm_server *server)
{
	if (server->xwayland) {
		wlr_xwayland_destroy(server->xwayland);
		server->xwayland = NULL;
	}
}

#else /* !WM_HAS_XWAYLAND */

void
wm_xwayland_init(struct wm_server *server)
{
	(void)server;
}

void
wm_xwayland_finish(struct wm_server *server)
{
	(void)server;
}

#endif /* WM_HAS_XWAYLAND */
