/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * xwayland.c - XWayland bridge for X11 application support
 *
 * Managed X11 windows are ordinary wm_views with an XWayland backend:
 * they get decorations, rules, workspaces, tabs, the iconbar, taskbar
 * handles and every window command, exactly like xdg-shell windows.
 * Override-redirect windows (menus, tooltips, drag icons) are drawn
 * above the windows at the position the client asks for and never
 * managed.
 *
 * Compile-time optional: the entire file is a no-op without
 * WM_HAS_XWAYLAND.
 */

#define _POSIX_C_SOURCE 200809L
#include "xwayland.h"

#ifdef WM_HAS_XWAYLAND

#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/edges.h>
#include <wlr/util/log.h>
#include <xcb/xcb_icccm.h>

#include "decoration.h"
#include "server.h"
#include "slit.h"
#include "tabgroup.h"
#include "toolbar.h"
#include "view.h"
#include "workspace.h"

/* A managed X11 window: a wm_view with an XWayland backend */
struct wm_xwayland_view {
	struct wm_view view;
	struct wlr_scene_tree *surface_tree; /* while associated */

	struct wl_listener destroy;
	struct wl_listener associate;
	struct wl_listener dissociate;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener commit;
	struct wl_listener request_configure;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_maximize;
	struct wl_listener request_fullscreen;
	struct wl_listener request_minimize;
	struct wl_listener request_activate;
	struct wl_listener set_title;
	struct wl_listener set_class;
	struct wl_listener set_role;
	struct wl_listener set_hints;
	struct wl_listener set_override_redirect;
};

/* An override-redirect X11 window: drawn, never managed */
struct wm_xwayland_unmanaged {
	struct wm_server *server;
	struct wlr_xwayland_surface *xsurface;
	struct wlr_scene_tree *scene_tree; /* while mapped */

	struct wl_listener destroy;
	struct wl_listener associate;
	struct wl_listener dissociate;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener request_configure;
	struct wl_listener set_geometry;
	struct wl_listener set_override_redirect;

	struct wl_list link; /* wm_server.xwayland_unmanaged */
};

static void create_managed(struct wm_server *server,
	struct wlr_xwayland_surface *xsurface);
static void create_unmanaged(struct wm_server *server,
	struct wlr_xwayland_surface *xsurface);

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

static bool
has_window_type(struct wlr_xwayland_surface *xsurface, xcb_atom_t type)
{
	if (type == XCB_ATOM_NONE)
		return false;
	for (size_t i = 0; i < xsurface->window_type_len; i++) {
		if (xsurface->window_type[i] == type)
			return true;
	}
	return false;
}

enum wm_xw_window_class
wm_xwayland_classify(struct wlr_xwayland_surface *xsurface,
	const struct wm_xwayland_atoms *atoms)
{
	if (xsurface->override_redirect)
		return WM_XW_UNMANAGED;

	if (has_window_type(xsurface, atoms->net_wm_window_type_dock))
		return WM_XW_DOCK;
	if (has_window_type(xsurface, atoms->net_wm_window_type_desktop))
		return WM_XW_DESKTOP;
	if (has_window_type(xsurface, atoms->net_wm_window_type_tooltip) ||
	    has_window_type(xsurface,
		    atoms->net_wm_window_type_notification) ||
	    has_window_type(xsurface,
		    atoms->net_wm_window_type_popup_menu) ||
	    has_window_type(xsurface,
		    atoms->net_wm_window_type_dropdown_menu))
		return WM_XW_POPUP;
	if (has_window_type(xsurface, atoms->net_wm_window_type_splash))
		return WM_XW_SPLASH;
	if (has_window_type(xsurface, atoms->net_wm_window_type_dialog) ||
	    has_window_type(xsurface, atoms->net_wm_window_type_utility) ||
	    has_window_type(xsurface, atoms->net_wm_window_type_toolbar) ||
	    has_window_type(xsurface, atoms->net_wm_window_type_menu) ||
	    xsurface->parent || xsurface->modal)
		return WM_XW_FLOATING;
	return WM_XW_MANAGED;
}

/*
 * Classic dockapps use the withdrawn initial state (WM_HINTS); some
 * (e.g. gkrellm -w) also claim _NET_WM_WINDOW_TYPE_NORMAL.
 */
static bool
is_withdrawn_dockapp(struct wlr_xwayland_surface *xsurface)
{
	return xsurface->hints &&
		(xsurface->hints->flags & XCB_ICCCM_WM_HINT_STATE) &&
		xsurface->hints->initial_state == XCB_ICCCM_WM_STATE_WITHDRAWN;
}

/* ------------------------------------------------------------------ */
/*  Backend operations                                                 */
/* ------------------------------------------------------------------ */

static struct wm_xwayland_view *
xview_from_view(struct wm_view *view)
{
	return wl_container_of(view, (struct wm_xwayland_view *)NULL, view);
}

static struct wlr_surface *
xw_surface(struct wm_view *view)
{
	return view->xsurface->surface;
}

static void
xw_content_geometry(struct wm_view *view, struct wlr_box *geo)
{
	geo->x = 0;
	geo->y = 0;
	geo->width = view->xsurface->width;
	geo->height = view->xsurface->height;
}

/* Layout position of the client area (inside the decorations) */
static void
content_position(struct wm_view *view, int *x, int *y)
{
	struct wm_xwayland_view *xview = xview_from_view(view);
	*x = view->x;
	*y = view->y;
	if (xview->surface_tree &&
	    !wlr_scene_node_coords(&xview->surface_tree->node, x, y)) {
		/* Disabled (unmapped/minimized): use the frame offset */
		*x = view->x + xview->surface_tree->node.x;
		*y = view->y + xview->surface_tree->node.y;
	}
}

static uint16_t
clamp_size(int v)
{
	if (v < 1)
		return 1;
	if (v > 16384)
		return 16384;
	return (uint16_t)v;
}

static void
xw_set_size(struct wm_view *view, int width, int height)
{
	struct wlr_xwayland_surface *xs = view->xsurface;
	int x, y;
	content_position(view, &x, &y);
	if (width <= 0)
		width = xs->width;
	if (height <= 0)
		height = xs->height;
	wlr_xwayland_surface_configure(xs, (int16_t)x, (int16_t)y,
		clamp_size(width), clamp_size(height));
}

/* X11 clients must know where they are (for their own popups) */
static void
xw_moved(struct wm_view *view)
{
	struct wlr_xwayland_surface *xs = view->xsurface;
	int x, y;
	content_position(view, &x, &y);
	if (x != xs->x || y != xs->y)
		wlr_xwayland_surface_configure(xs, (int16_t)x, (int16_t)y,
			xs->width, xs->height);
}

static void
xw_set_activated(struct wm_view *view, bool activated)
{
	struct wlr_xwayland_surface *xs = view->xsurface;
	wlr_xwayland_surface_activate(xs, activated);
	if (activated)
		wlr_xwayland_surface_restack(xs, NULL, XCB_STACK_MODE_ABOVE);
}

static void
xw_set_maximized(struct wm_view *view, bool maximized)
{
	wlr_xwayland_surface_set_maximized(view->xsurface, maximized, maximized);
}

static void
xw_set_fullscreen(struct wm_view *view, bool fullscreen)
{
	wlr_xwayland_surface_set_fullscreen(view->xsurface, fullscreen);
}

static void
xw_set_tiled(struct wm_view *view, uint32_t edges)
{
	/* X11 has no tiled hint */
	(void)view;
	(void)edges;
}

static void
xw_close(struct wm_view *view)
{
	wlr_xwayland_surface_close(view->xsurface);
}

static struct wl_client *
xw_client(struct wm_view *view)
{
	/* All X11 windows share Xwayland's connection */
	(void)view;
	return NULL;
}

static pid_t
xw_pid(struct wm_view *view)
{
	return view->xsurface->pid;
}

static struct wm_view *
xw_parent(struct wm_view *view)
{
	struct wlr_xwayland_surface *parent = view->xsurface->parent;
	if (!parent)
		return NULL;
	struct wm_view *v;
	wl_list_for_each(v, &view->server->views, link) {
		if (v->type == WM_VIEW_XWAYLAND && v->xsurface == parent)
			return v;
	}
	return NULL;
}

static void
xw_set_minimized(struct wm_view *view, bool minimized)
{
	wlr_xwayland_surface_set_minimized(view->xsurface, minimized);
}

/* A text property of the X window (@NAME patterns, SetXProp) */
static bool
xw_get_xprop(struct wm_view *view, const char *name, char *buf, size_t len)
{
	xcb_connection_t *conn = view->server->xwayland ?
		wlr_xwayland_get_xwm_connection(view->server->xwayland) : NULL;
	if (!conn)
		return false;
	xcb_atom_t atom = resolve_atom(conn, name);
	if (atom == XCB_ATOM_NONE)
		return false;
	xcb_get_property_cookie_t cookie = xcb_get_property(conn, 0,
		view->xsurface->window_id, atom, XCB_GET_PROPERTY_TYPE_ANY, 0,
		(uint32_t)(len / 4));
	xcb_get_property_reply_t *reply =
		xcb_get_property_reply(conn, cookie, NULL);
	if (!reply)
		return false;
	bool found = reply->type != XCB_ATOM_NONE && reply->format == 8;
	if (found) {
		int n = xcb_get_property_value_length(reply);
		if ((size_t)n >= len)
			n = (int)len - 1;
		memcpy(buf, xcb_get_property_value(reply), (size_t)n);
		buf[n] = '\0';
	}
	free(reply);
	return found;
}

static void
xw_set_xprop(struct wm_view *view, const char *name, const char *value)
{
	xcb_connection_t *conn = view->server->xwayland ?
		wlr_xwayland_get_xwm_connection(view->server->xwayland) : NULL;
	if (!conn)
		return;
	xcb_atom_t atom = resolve_atom(conn, name);
	xcb_atom_t utf8 = resolve_atom(conn, "UTF8_STRING");
	if (atom == XCB_ATOM_NONE || utf8 == XCB_ATOM_NONE)
		return;
	xcb_change_property(conn, XCB_PROP_MODE_REPLACE,
		view->xsurface->window_id, atom, utf8, 8,
		(uint32_t)strlen(value), value);
	xcb_flush(conn);
}

static const struct wm_view_impl xwayland_view_impl = {
	.surface = xw_surface,
	.content_geometry = xw_content_geometry,
	.set_size = xw_set_size,
	.moved = xw_moved,
	.set_activated = xw_set_activated,
	.set_maximized = xw_set_maximized,
	.set_fullscreen = xw_set_fullscreen,
	.set_tiled = xw_set_tiled,
	.close = xw_close,
	.client = xw_client,
	.pid = xw_pid,
	.parent = xw_parent,
	.set_minimized = xw_set_minimized,
	.get_xprop = xw_get_xprop,
	.set_xprop = xw_set_xprop,
};

/* ------------------------------------------------------------------ */
/*  Managed windows: lifecycle                                         */
/* ------------------------------------------------------------------ */

static void
refresh_metadata(struct wm_xwayland_view *xview)
{
	struct wm_view *view = &xview->view;
	struct wlr_xwayland_surface *xs = view->xsurface;

	wm_view_set_title(view, xs->title);
	free(view->instance);
	view->instance = xs->instance ? strdup(xs->instance) : NULL;
	free(view->role);
	view->role = xs->role ? strdup(xs->role) : NULL;
	wm_view_set_app_id(view, xs->class);
}

static void
update_urgency(struct wm_view *view)
{
	struct wlr_xwayland_surface *xs = view->xsurface;
	bool urgent = xs->hints &&
		(xs->hints->flags & XCB_ICCCM_WM_HINT_X_URGENCY);
	if (urgent == view->urgent)
		return;
	wm_view_set_urgent(view, urgent);
}

static void
handle_commit(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_view *xview =
		wl_container_of(listener, xview, commit);
	struct wm_view *view = &xview->view;

	/* The client changed size: refit the frame */
	if (view->decoration && view->server->style)
		wm_decoration_refresh_geometry(view->decoration,
			view->server->style);
}

static void
handle_map(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_view *xview =
		wl_container_of(listener, xview, map);
	struct wm_view *view = &xview->view;
	struct wm_server *server = view->server;
	struct wlr_xwayland_surface *xs = view->xsurface;

	refresh_metadata(xview);
	update_urgency(view);

	enum wm_xw_window_class cls =
		wm_xwayland_classify(xs, &server->xwayland_atoms);

	/* Dockapps go to the slit, which manages them from here on */
	if (server->slit && (cls == WM_XW_DOCK || is_withdrawn_dockapp(xs))) {
		wlr_log(WLR_INFO, "xwayland: docking \"%s\" [%s] in the slit",
			view->title ? view->title : "",
			view->app_id ? view->app_id : "");
		view->mapped = true;
		view->docked = true;
		wm_slit_add_client(server->slit, xs);
		return;
	}

	/* Motif hints / window types that want no frame or focus */
	view->no_decoration = (xs->decorations &
		(WLR_XWAYLAND_SURFACE_DECORATIONS_NO_BORDER |
		 WLR_XWAYLAND_SURFACE_DECORATIONS_NO_TITLE)) ||
		cls == WM_XW_SPLASH || cls == WM_XW_POPUP ||
		cls == WM_XW_DESKTOP;
	view->no_focus = cls == WM_XW_POPUP ||
		wlr_xwayland_surface_icccm_input_model(xs) ==
		WLR_ICCCM_INPUT_MODEL_NONE;
	if (cls == WM_XW_POPUP || cls == WM_XW_SPLASH ||
	    cls == WM_XW_DESKTOP) {
		view->focus_hidden = true;
		view->icon_hidden = true;
	}
	if (cls == WM_XW_POPUP)
		view->layer = WM_LAYER_ABOVE;
	if (cls == WM_XW_DESKTOP) {
		view->layer = WM_LAYER_DESKTOP;
		view->sticky = true;
	}

	/* The client area goes inside the decoration frame */
	xview->surface_tree = wlr_scene_subsurface_tree_create(
		view->scene_tree, xs->surface);
	if (!xview->surface_tree) {
		wlr_log(WLR_ERROR, "%s",
			"xwayland: failed to create surface tree");
		return;
	}

	/*
	 * Keep a client-chosen position (USPosition/PPosition, e.g.
	 * restored geometry); otherwise placement decides.
	 */
	view->x = 0;
	view->y = 0;
	if (xs->size_hints && (xs->size_hints->flags &
	    (XCB_ICCCM_SIZE_HINT_US_POSITION |
	     XCB_ICCCM_SIZE_HINT_P_POSITION))) {
		view->x = xs->x;
		view->y = xs->y;
	}

	xview->commit.notify = handle_commit;
	wl_signal_add(&xs->surface->events.commit, &xview->commit);

	wm_view_map_common(view, xs->maximized_vert && xs->maximized_horz,
		xs->fullscreen);

	/* The frame moved the client area; tell the client where */
	xw_moved(view);
}

static void
handle_unmap(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_view *xview =
		wl_container_of(listener, xview, unmap);
	struct wm_view *view = &xview->view;

	if (view->docked) {
		/* The slit handles its own clients */
		view->mapped = false;
		view->docked = false;
		return;
	}
	if (!xview->surface_tree)
		return;

	wl_list_remove(&xview->commit.link);
	wm_view_unmap_common(view);

	/*
	 * X11 windows can be remapped (withdraw + map), unlike xdg
	 * toplevels: drop the surface tree, decorations and tab group.
	 */
	if (view->decoration) {
		wm_decoration_destroy(view->decoration);
		view->decoration = NULL;
	}
	if (view->tab_group)
		wm_tab_group_remove(view);
	wlr_scene_node_destroy(&xview->surface_tree->node);
	xview->surface_tree = NULL;
}

static void
handle_associate(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_view *xview =
		wl_container_of(listener, xview, associate);
	struct wlr_xwayland_surface *xs = xview->view.xsurface;

	xview->map.notify = handle_map;
	wl_signal_add(&xs->surface->events.map, &xview->map);
	xview->unmap.notify = handle_unmap;
	wl_signal_add(&xs->surface->events.unmap, &xview->unmap);
}

static void
handle_dissociate(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_view *xview =
		wl_container_of(listener, xview, dissociate);
	wl_list_remove(&xview->map.link);
	wl_list_remove(&xview->unmap.link);
}

static void
destroy_managed(struct wm_xwayland_view *xview)
{
	struct wm_view *view = &xview->view;

	if (view->xsurface->surface) {
		/* Still associated: dissociate wasn't emitted */
		wl_list_remove(&xview->map.link);
		wl_list_remove(&xview->unmap.link);
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
	wl_list_remove(&xview->set_role.link);
	wl_list_remove(&xview->set_hints.link);
	wl_list_remove(&xview->set_override_redirect.link);

	view->xsurface->data = NULL;
	wm_view_destroy_common(view);
	free(xview);
}

static void
handle_destroy(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_view *xview =
		wl_container_of(listener, xview, destroy);
	destroy_managed(xview);
}

/* ------------------------------------------------------------------ */
/*  Managed windows: requests and property changes                     */
/* ------------------------------------------------------------------ */

static void
handle_request_configure(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_view *xview =
		wl_container_of(listener, xview, request_configure);
	struct wm_view *view = &xview->view;
	struct wlr_xwayland_surface_configure_event *event = data;
	struct wlr_xwayland_surface *xs = view->xsurface;

	if (!view->mapped) {
		/* Before map the client chooses freely */
		wlr_xwayland_surface_configure(xs, event->x, event->y,
			clamp_size(event->width), clamp_size(event->height));
		return;
	}

	/* Maximized/fullscreen windows keep the geometry we gave them */
	if (view->maximized || view->fullscreen) {
		xw_set_size(view, xs->width, xs->height);
		return;
	}

	/* ICCCM ConfigureRequest: honour size, and moves (frame follows) */
	int cx, cy;
	content_position(view, &cx, &cy);
	if (event->mask & (XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y)) {
		int nx = (event->mask & XCB_CONFIG_WINDOW_X) ? event->x : cx;
		int ny = (event->mask & XCB_CONFIG_WINDOW_Y) ? event->y : cy;
		wm_view_move(view, view->x + (nx - cx), view->y + (ny - cy));
		content_position(view, &cx, &cy);
	}
	wlr_xwayland_surface_configure(xs, (int16_t)cx, (int16_t)cy,
		clamp_size(event->width), clamp_size(event->height));
}

static void
handle_request_move(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_view *xview =
		wl_container_of(listener, xview, request_move);
	if (xview->view.mapped)
		wm_view_begin_interactive(&xview->view, WM_CURSOR_MOVE, 0);
}

static void
handle_request_resize(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_view *xview =
		wl_container_of(listener, xview, request_resize);
	struct wlr_xwayland_resize_event *event = data;
	if (xview->view.mapped)
		wm_view_begin_interactive(&xview->view, WM_CURSOR_RESIZE,
			event->edges);
}

static void
handle_request_maximize(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_view *xview =
		wl_container_of(listener, xview, request_maximize);
	struct wm_view *view = &xview->view;
	struct wlr_xwayland_surface *xs = view->xsurface;

	if (!view->mapped)
		return; /* applied at map from xs->maximized_* */
	bool want = xs->maximized_vert && xs->maximized_horz;
	if (want != view->maximized)
		wm_view_toggle_maximize(view);
	else
		wm_view_set_maximized_state(view, view->maximized);
}

static void
handle_request_fullscreen(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_view *xview =
		wl_container_of(listener, xview, request_fullscreen);
	struct wm_view *view = &xview->view;

	if (!view->mapped)
		return; /* applied at map from xs->fullscreen */
	if (view->xsurface->fullscreen != view->fullscreen)
		wm_view_toggle_fullscreen(view);
	else
		wm_view_set_fullscreen_state(view, view->fullscreen);
}

static void
handle_request_minimize(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_view *xview =
		wl_container_of(listener, xview, request_minimize);
	struct wlr_xwayland_minimize_event *event = data;
	struct wm_view *view = &xview->view;

	if (!view->mapped)
		return;
	if (event->minimize)
		wm_view_minimize(view);
	else
		wm_view_unminimize(view);
}

static void
handle_request_activate(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_view *xview =
		wl_container_of(listener, xview, request_activate);
	struct wm_view *view = &xview->view;
	struct wm_server *server = view->server;

	/*
	 * Focus stealing prevention: an X11 client may move focus between
	 * its own windows, or take it when nothing is focused. Anything
	 * else only marks the window as demanding attention.
	 */
	struct wm_view *focused = server->focused_view;
	bool allowed = !focused || (focused->type == WM_VIEW_XWAYLAND &&
		focused->xsurface->pid == view->xsurface->pid);
	wm_view_request_activate(view, allowed);
}

static void
handle_set_title(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_view *xview =
		wl_container_of(listener, xview, set_title);
	wm_view_set_title(&xview->view, xview->view.xsurface->title);
}

static void
handle_set_class(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_view *xview =
		wl_container_of(listener, xview, set_class);
	struct wm_view *view = &xview->view;
	free(view->instance);
	view->instance = view->xsurface->instance ?
		strdup(view->xsurface->instance) : NULL;
	wm_view_set_app_id(view, view->xsurface->class);
}

static void
handle_set_role(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_view *xview =
		wl_container_of(listener, xview, set_role);
	struct wm_view *view = &xview->view;
	free(view->role);
	view->role = view->xsurface->role ?
		strdup(view->xsurface->role) : NULL;
}

static void
handle_set_hints(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_view *xview =
		wl_container_of(listener, xview, set_hints);
	update_urgency(&xview->view);
}

static void
handle_managed_set_override_redirect(struct wl_listener *listener,
	void *data)
{
	struct wm_xwayland_view *xview =
		wl_container_of(listener, xview, set_override_redirect);
	struct wlr_xwayland_surface *xs = xview->view.xsurface;
	struct wm_server *server = xview->view.server;

	/* Became override-redirect: hand it over to the unmanaged path */
	bool mapped = xs->surface && xs->surface->mapped;
	if (mapped)
		handle_unmap(&xview->unmap, NULL);
	destroy_managed(xview);
	create_unmanaged(server, xs);
	if (mapped) {
		struct wm_xwayland_unmanaged *u = xs->data;
		if (u)
			u->map.notify(&u->map, NULL);
	}
}

static void
create_managed(struct wm_server *server,
	struct wlr_xwayland_surface *xsurface)
{
	struct wm_xwayland_view *xview = calloc(1, sizeof(*xview));
	if (!xview) {
		wlr_log(WLR_ERROR, "%s", "xwayland: allocation failed");
		return;
	}
	struct wm_view *view = &xview->view;
	view->type = WM_VIEW_XWAYLAND;
	view->impl = &xwayland_view_impl;
	view->xsurface = xsurface;
	if (!wm_view_init_common(view, server)) {
		free(xview);
		return;
	}
	xsurface->data = xview;

	xview->destroy.notify = handle_destroy;
	wl_signal_add(&xsurface->events.destroy, &xview->destroy);
	xview->associate.notify = handle_associate;
	wl_signal_add(&xsurface->events.associate, &xview->associate);
	xview->dissociate.notify = handle_dissociate;
	wl_signal_add(&xsurface->events.dissociate, &xview->dissociate);
	xview->request_configure.notify = handle_request_configure;
	wl_signal_add(&xsurface->events.request_configure,
		&xview->request_configure);
	xview->request_move.notify = handle_request_move;
	wl_signal_add(&xsurface->events.request_move, &xview->request_move);
	xview->request_resize.notify = handle_request_resize;
	wl_signal_add(&xsurface->events.request_resize,
		&xview->request_resize);
	xview->request_maximize.notify = handle_request_maximize;
	wl_signal_add(&xsurface->events.request_maximize,
		&xview->request_maximize);
	xview->request_fullscreen.notify = handle_request_fullscreen;
	wl_signal_add(&xsurface->events.request_fullscreen,
		&xview->request_fullscreen);
	xview->request_minimize.notify = handle_request_minimize;
	wl_signal_add(&xsurface->events.request_minimize,
		&xview->request_minimize);
	xview->request_activate.notify = handle_request_activate;
	wl_signal_add(&xsurface->events.request_activate,
		&xview->request_activate);
	xview->set_title.notify = handle_set_title;
	wl_signal_add(&xsurface->events.set_title, &xview->set_title);
	xview->set_class.notify = handle_set_class;
	wl_signal_add(&xsurface->events.set_class, &xview->set_class);
	xview->set_role.notify = handle_set_role;
	wl_signal_add(&xsurface->events.set_role, &xview->set_role);
	xview->set_hints.notify = handle_set_hints;
	wl_signal_add(&xsurface->events.set_hints, &xview->set_hints);
	xview->set_override_redirect.notify =
		handle_managed_set_override_redirect;
	wl_signal_add(&xsurface->events.set_override_redirect,
		&xview->set_override_redirect);

	/* Already associated (e.g. converted from override-redirect) */
	if (xsurface->surface)
		handle_associate(&xview->associate, NULL);
}

/* ------------------------------------------------------------------ */
/*  Override-redirect windows                                          */
/* ------------------------------------------------------------------ */

static void
unmanaged_set_position(struct wm_xwayland_unmanaged *u)
{
	if (u->scene_tree)
		wlr_scene_node_set_position(&u->scene_tree->node,
			u->xsurface->x, u->xsurface->y);
}

static void
handle_unmanaged_map(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_unmanaged *u = wl_container_of(listener, u, map);
	struct wm_server *server = u->server;
	struct wlr_xwayland_surface *xs = u->xsurface;

	/* Above all windows, like xdg popups */
	u->scene_tree = wlr_scene_subsurface_tree_create(
		server->xdg_popup_tree, xs->surface);
	if (!u->scene_tree)
		return;
	unmanaged_set_position(u);

	/* Menus of the focused app may take the keyboard (ICCCM) */
	if (wlr_xwayland_surface_override_redirect_wants_focus(xs)) {
		struct wlr_keyboard *kb = wlr_seat_get_keyboard(server->seat);
		wlr_seat_keyboard_notify_enter(server->seat, xs->surface,
			kb ? kb->keycodes : NULL, kb ? kb->num_keycodes : 0,
			kb ? &kb->modifiers : NULL);
	}
}

static void
handle_unmanaged_unmap(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_unmanaged *u =
		wl_container_of(listener, u, unmap);
	struct wm_server *server = u->server;

	if (u->scene_tree) {
		wlr_scene_node_destroy(&u->scene_tree->node);
		u->scene_tree = NULL;
	}

	/* Give the keyboard back to the focused window */
	if (server->seat->keyboard_state.focused_surface ==
	    u->xsurface->surface) {
		struct wm_view *focused = server->focused_view;
		if (focused) {
			struct wlr_keyboard *kb =
				wlr_seat_get_keyboard(server->seat);
			wlr_seat_keyboard_notify_enter(server->seat,
				wm_view_surface(focused),
				kb ? kb->keycodes : NULL,
				kb ? kb->num_keycodes : 0,
				kb ? &kb->modifiers : NULL);
		} else {
			wlr_seat_keyboard_notify_clear_focus(server->seat);
		}
	}
}

static void
handle_unmanaged_associate(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_unmanaged *u =
		wl_container_of(listener, u, associate);
	u->map.notify = handle_unmanaged_map;
	wl_signal_add(&u->xsurface->surface->events.map, &u->map);
	u->unmap.notify = handle_unmanaged_unmap;
	wl_signal_add(&u->xsurface->surface->events.unmap, &u->unmap);
}

static void
handle_unmanaged_dissociate(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_unmanaged *u =
		wl_container_of(listener, u, dissociate);
	wl_list_remove(&u->map.link);
	wl_list_remove(&u->unmap.link);
}

static void
handle_unmanaged_request_configure(struct wl_listener *listener,
	void *data)
{
	struct wm_xwayland_unmanaged *u =
		wl_container_of(listener, u, request_configure);
	struct wlr_xwayland_surface_configure_event *event = data;
	wlr_xwayland_surface_configure(u->xsurface, event->x, event->y,
		clamp_size(event->width), clamp_size(event->height));
	unmanaged_set_position(u);
}

static void
handle_unmanaged_set_geometry(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_unmanaged *u =
		wl_container_of(listener, u, set_geometry);
	unmanaged_set_position(u);
}

static void
destroy_unmanaged(struct wm_xwayland_unmanaged *u)
{
	if (u->xsurface->surface) {
		wl_list_remove(&u->map.link);
		wl_list_remove(&u->unmap.link);
	}
	if (u->scene_tree)
		wlr_scene_node_destroy(&u->scene_tree->node);
	wl_list_remove(&u->destroy.link);
	wl_list_remove(&u->associate.link);
	wl_list_remove(&u->dissociate.link);
	wl_list_remove(&u->request_configure.link);
	wl_list_remove(&u->set_geometry.link);
	wl_list_remove(&u->set_override_redirect.link);
	wl_list_remove(&u->link);
	u->xsurface->data = NULL;
	free(u);
}

static void
handle_unmanaged_destroy(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_unmanaged *u =
		wl_container_of(listener, u, destroy);
	destroy_unmanaged(u);
}

static void
handle_unmanaged_set_override_redirect(struct wl_listener *listener,
	void *data)
{
	struct wm_xwayland_unmanaged *u =
		wl_container_of(listener, u, set_override_redirect);
	struct wlr_xwayland_surface *xs = u->xsurface;
	struct wm_server *server = u->server;

	/* No longer override-redirect: manage it */
	bool mapped = xs->surface && xs->surface->mapped;
	if (mapped)
		handle_unmanaged_unmap(&u->unmap, NULL);
	destroy_unmanaged(u);
	create_managed(server, xs);
	if (mapped) {
		struct wm_xwayland_view *xview = xs->data;
		if (xview)
			handle_map(&xview->map, NULL);
	}
}

static void
create_unmanaged(struct wm_server *server,
	struct wlr_xwayland_surface *xsurface)
{
	struct wm_xwayland_unmanaged *u = calloc(1, sizeof(*u));
	if (!u) {
		wlr_log(WLR_ERROR, "%s", "xwayland: allocation failed");
		return;
	}
	u->server = server;
	u->xsurface = xsurface;
	xsurface->data = u;

	u->destroy.notify = handle_unmanaged_destroy;
	wl_signal_add(&xsurface->events.destroy, &u->destroy);
	u->associate.notify = handle_unmanaged_associate;
	wl_signal_add(&xsurface->events.associate, &u->associate);
	u->dissociate.notify = handle_unmanaged_dissociate;
	wl_signal_add(&xsurface->events.dissociate, &u->dissociate);
	u->request_configure.notify = handle_unmanaged_request_configure;
	wl_signal_add(&xsurface->events.request_configure,
		&u->request_configure);
	u->set_geometry.notify = handle_unmanaged_set_geometry;
	wl_signal_add(&xsurface->events.set_geometry, &u->set_geometry);
	u->set_override_redirect.notify =
		handle_unmanaged_set_override_redirect;
	wl_signal_add(&xsurface->events.set_override_redirect,
		&u->set_override_redirect);
	wl_list_insert(&server->xwayland_unmanaged, &u->link);

	if (xsurface->surface)
		handle_unmanaged_associate(&u->associate, NULL);
}

/* ------------------------------------------------------------------ */
/*  Server hooks                                                       */
/* ------------------------------------------------------------------ */

static void
handle_xwayland_new_surface(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, xwayland_new_surface);
	struct wlr_xwayland_surface *xsurface = data;

	wlr_log(WLR_DEBUG, "xwayland: new surface window_id=%u "
		"override_redirect=%d",
		xsurface->window_id, xsurface->override_redirect);

	if (xsurface->override_redirect)
		create_unmanaged(server, xsurface);
	else
		create_managed(server, xsurface);
}

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
		wlr_log(WLR_INFO, "%s", "xwayland: atoms resolved");
	}

	/* Set the XWayland cursor */
	wlr_xwayland_set_seat(server->xwayland, server->seat);

	wlr_log(WLR_INFO, "xwayland ready on DISPLAY=%s",
		server->xwayland->display_name);
}

void
wm_xwayland_sync_positions(struct wm_server *server)
{
	struct wm_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (view->type == WM_VIEW_XWAYLAND && view->mapped &&
		    !view->docked)
			xw_moved(view);
	}
}

void
wm_xwayland_init(struct wm_server *server)
{
	wl_list_init(&server->xwayland_unmanaged);

	server->xwayland = wlr_xwayland_create(server->wl_display,
		server->compositor, true /* lazy */);
	if (!server->xwayland) {
		wlr_log(WLR_ERROR, "%s", "failed to create xwayland");
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
		wl_list_remove(&server->xwayland_new_surface.link);
		wl_list_remove(&server->xwayland_ready.link);
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

void
wm_xwayland_sync_positions(struct wm_server *server)
{
	(void)server;
}

#endif /* WM_HAS_XWAYLAND */
