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
#include "cursor.h"
#include "xwayland.h"

#ifdef WM_HAS_XWAYLAND

#include <cairo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/util/edges.h>
#include <wlr/util/log.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>

#include "config.h"
#include "decoration.h"
#include "icon.h"
#include "protocols.h"
#include "pseudo_transparency.h"
#include "server.h"
#include "slit.h"
#include "tabgroup.h"
#include "toolbar.h"
#include "view.h"
#include "workspace.h"
#include "xwayland_shape.h"

/* A managed X11 window: a wm_view with an XWayland backend */
struct wm_xwayland_view {
	struct wm_view view;
	struct wm_xshape *shape;	/* its X Shape, which clips the surface */
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
	struct wl_listener set_icon;
	struct wl_listener set_override_redirect;
};

/* An override-redirect X11 window: drawn, never managed */
struct wm_xwayland_unmanaged {
	struct wm_server *server;
	struct wlr_xwayland_surface *xsurface;
	struct wm_xshape *shape;	/* its X Shape, which clips the surface */
	/* what it is drawn in while mapped: wm_xshape_tree(shape) */

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

static void update_icon(struct wm_xwayland_view *xview);
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
	atoms->net_wm_window_type_combo =
		resolve_atom(conn, "_NET_WM_WINDOW_TYPE_COMBO");
	atoms->net_wm_window_type_dnd =
		resolve_atom(conn, "_NET_WM_WINDOW_TYPE_DND");
	atoms->motif_wm_hints = resolve_atom(conn, "_MOTIF_WM_HINTS");
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

/* WM_NORMAL_HINTS as Fluxbox's SizeHints::reset reads them */
static void
xw_size_hints(struct wm_view *view, struct wm_size_hints *h)
{
	const xcb_size_hints_t *s = view->xsurface->size_hints;
	if (!s)
		return;
	if (s->flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE) {
		h->min_w = s->min_width;
		h->min_h = s->min_height;
	}
	if (s->flags & XCB_ICCCM_SIZE_HINT_BASE_SIZE) {
		h->base_w = s->base_width > 0 ? s->base_width : 0;
		h->base_h = s->base_height > 0 ? s->base_height : 0;
		if (!(s->flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE)) {
			h->min_w = h->base_w;
			h->min_h = h->base_h;
		}
	}
	if (s->flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE) {
		h->max_w = s->max_width;
		h->max_h = s->max_height;
	}
	if (s->flags & XCB_ICCCM_SIZE_HINT_P_RESIZE_INC) {
		h->inc_w = s->width_inc;
		h->inc_h = s->height_inc;
	}
	if (s->flags & XCB_ICCCM_SIZE_HINT_P_ASPECT) {
		h->min_aspect_x = s->min_aspect_num;
		h->min_aspect_y = s->min_aspect_den;
		h->max_aspect_x = s->max_aspect_num;
		h->max_aspect_y = s->max_aspect_den;
	}
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
	.size_hints = xw_size_hints,
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
	if (view->mapped) {
		wm_pseudo_update(view);
		wm_cursor_rebase(view->server);
	}
}

/*
 * _MOTIF_WM_HINTS' decorations beyond what wlroots passes on (no title, no
 * border): which buttons the window wants, as Fluxbox reads them.  Bits:
 * all 1, border 2, resize handle 4, title 8, menu 16, minimize 32,
 * maximize 64.
 */
static void
read_motif_buttons(struct wm_view *view)
{
	struct wm_server *server = view->server;
	view->motif_buttons_set = false;
	xcb_connection_t *conn = server->xwayland ?
		wlr_xwayland_get_xwm_connection(server->xwayland) : NULL;
	xcb_atom_t atom = server->xwayland_atoms.motif_wm_hints;
	if (!conn || atom == XCB_ATOM_NONE)
		return;

	xcb_get_property_cookie_t cookie = xcb_get_property(conn, 0,
		view->xsurface->window_id, atom, XCB_GET_PROPERTY_TYPE_ANY,
		0, 5);
	xcb_get_property_reply_t *reply =
		xcb_get_property_reply(conn, cookie, NULL);
	if (!reply)
		return;
	/* flags, functions, decorations, input mode, status */
	if (xcb_get_property_value_length(reply) >= 12) {
		const uint32_t *v = xcb_get_property_value(reply);
		bool has_decorations = v[0] & 2;
		if (has_decorations && !(v[2] & 1)) {
			view->motif_buttons_set = true;
			view->motif_decor = v[2];
		}
	}
	free(reply);
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
	update_icon(xview);

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

	/*
	 * Motif hints say which parts it wants: neither a title nor a border
	 * is no frame; no title only, a border; no border only, a title
	 * (and window types that want no frame at all)
	 */
	bool no_title = xs->decorations &
		WLR_XWAYLAND_SURFACE_DECORATIONS_NO_TITLE;
	bool no_border = xs->decorations &
		WLR_XWAYLAND_SURFACE_DECORATIONS_NO_BORDER;
	view->no_decoration = (no_title && no_border) ||
		cls == WM_XW_SPLASH || cls == WM_XW_POPUP ||
		cls == WM_XW_DESKTOP;
	view->deco_hint = no_title && !no_border ? WM_DECOR_BORDER :
		no_border && !no_title ? WM_DECOR_TINY : WM_DECOR_NORMAL;
	read_motif_buttons(view);
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
	xview->surface_tree = wm_xshape_show(xview->shape, view->scene_tree);
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
	wm_xshape_hide(xview->shape);
	xview->surface_tree = NULL;
}

/*
 * Xwayland commits a window's first buffer as soon as the X window is
 * mapped.  That can be before the window manager has read the window's
 * properties and associated its surface with it, and wlroots doesn't map a
 * surface that already had a buffer when it got its role: it would wait for
 * the next commit, which a window that isn't redrawn (a dialog) never makes.
 * The window stays unmapped, invisible, and its application waits on it.
 */
static void
map_if_already_committed(struct wlr_surface *surface)
{
	if (!surface->mapped && wlr_surface_has_buffer(surface))
		wlr_surface_map(surface);
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
	map_if_already_committed(xs->surface);
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
	wl_list_remove(&xview->set_icon.link);
	wl_list_remove(&xview->set_override_redirect.link);

	view->xsurface->data = NULL;
	wm_xshape_destroy(xview->shape);
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

	/*
	 * Just after it opened, a program (MPlayer...) doesn't move or size
	 * itself over what its apps entry gives it (configureRequestEvent)
	 */
	uint16_t mask = event->mask;
	int width = event->width, height = event->height;
	bool keep_pos, keep_size;
	wm_view_rule_holds_geometry(view, &keep_pos, &keep_size);
	if (keep_pos)
		mask &= ~(XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y);
	if (keep_size) {
		width = xs->width;
		height = xs->height;
	}

	/* ICCCM ConfigureRequest: honour size, and moves (frame follows) */
	int cx, cy;
	content_position(view, &cx, &cy);
	if (mask & (XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y)) {
		int nx = (mask & XCB_CONFIG_WINDOW_X) ? event->x : cx;
		int ny = (mask & XCB_CONFIG_WINDOW_Y) ? event->y : cy;
		wm_view_move(view, view->x + (nx - cx), view->y + (ny - cy));
		content_position(view, &cx, &cy);
	}
	wlr_xwayland_surface_configure(xs, (int16_t)cx, (int16_t)cy,
		clamp_size(width), clamp_size(height));
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

	/*
	 * Or with an xdg-activation token of the user's input, which an X11
	 * client names in _NET_STARTUP_ID (as the protocol says for X11) before
	 * asking: it was given one by whatever the user acted on (a tray
	 * icon), and it is good once.
	 */
	if (wm_activation_token_consume(server, view->xsurface->startup_id))
		allowed = true;
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

/*
 * _NET_WM_ICON: the picture nearest WM_ICON_SIZE that covers it (else the
 * largest) becomes the window's own icon, like a Wayland client's
 * xdg-toplevel-icon.  The pixels are ARGB, not premultiplied.
 */
static void
update_icon(struct wm_xwayland_view *xview)
{
	struct wm_view *view = &xview->view;
	xcb_ewmh_get_wm_icon_reply_t reply;
	if (!wlr_xwayland_surface_fetch_icon(view->xsurface, &reply))
		return;

	const uint32_t *best = NULL;
	uint32_t best_w = 0, best_h = 0;
	xcb_ewmh_wm_icon_iterator_t it = xcb_ewmh_get_wm_icon_iterator(&reply);
	for (; it.rem; xcb_ewmh_get_wm_icon_next(&it)) {
		uint32_t w = it.width, h = it.height;
		if (w == 0 || h == 0 || w > 1024 || h > 1024)
			continue;
		bool covers = w >= WM_ICON_SIZE && h >= WM_ICON_SIZE;
		bool best_covers = best_w >= WM_ICON_SIZE &&
			best_h >= WM_ICON_SIZE;
		if (!best ||
		    (covers && (!best_covers || w * h < best_w * best_h)) ||
		    (!covers && !best_covers && w * h > best_w * best_h)) {
			best = it.data;
			best_w = w;
			best_h = h;
		}
	}

	cairo_surface_t *icon = NULL;
	if (best) {
		cairo_surface_t *src = cairo_image_surface_create(
			CAIRO_FORMAT_ARGB32, (int)best_w, (int)best_h);
		if (cairo_surface_status(src) == CAIRO_STATUS_SUCCESS) {
			cairo_surface_flush(src);
			unsigned char *dst = cairo_image_surface_get_data(src);
			int stride = cairo_image_surface_get_stride(src);
			for (uint32_t y = 0; y < best_h; y++) {
				uint32_t *row = (uint32_t *)(dst +
					(size_t)y * stride);
				for (uint32_t x = 0; x < best_w; x++) {
					uint32_t px = best[y * best_w + x];
					uint32_t a = px >> 24;
					uint32_t r = (((px >> 16) & 0xff) * a +
						127) / 255;
					uint32_t g = (((px >> 8) & 0xff) * a +
						127) / 255;
					uint32_t b = ((px & 0xff) * a + 127) /
						255;
					row[x] = (a << 24) | (r << 16) |
						(g << 8) | b;
				}
			}
			cairo_surface_mark_dirty(src);
			icon = wm_icon_scale(src, WM_ICON_SIZE);
		}
		cairo_surface_destroy(src);
	}
	xcb_ewmh_get_wm_icon_reply_wipe(&reply);

	if (icon) {
		wm_view_set_icon(view, icon, true);
		cairo_surface_destroy(icon);
	}
}

static void
handle_set_icon(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_view *xview =
		wl_container_of(listener, xview, set_icon);
	update_icon(xview);
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
	xview->shape = wm_xshape_create(server, xsurface);

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
	xview->set_icon.notify = handle_set_icon;
	wl_signal_add(&xsurface->events.set_icon, &xview->set_icon);
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

/*
 * A short-lived piece of an application's interface that belongs above
 * whatever is raised (a menu, a tooltip, a drag icon): these stay on top.
 */
static bool
is_popup_window(struct wlr_xwayland_surface *xs,
	const struct wm_xwayland_atoms *atoms)
{
	const xcb_atom_t popups[] = {
		atoms->net_wm_window_type_menu,
		atoms->net_wm_window_type_dropdown_menu,
		atoms->net_wm_window_type_popup_menu,
		atoms->net_wm_window_type_tooltip,
		atoms->net_wm_window_type_notification,
		atoms->net_wm_window_type_combo,
		atoms->net_wm_window_type_dnd,
	};
	for (size_t i = 0; i < sizeof(popups) / sizeof(popups[0]); i++)
		if (popups[i] != XCB_ATOM_NONE && has_window_type(xs, popups[i]))
			return true;
	return false;
}

/* Windows of one application: the same WM_CLASS, as Fluxbox groups by */
static bool
same_application(const struct wlr_xwayland_surface *a,
	const struct wlr_xwayland_surface *b)
{
	return a->class && *a->class && b->class && !strcmp(a->class, b->class) &&
		((!a->instance && !b->instance) ||
		 (a->instance && b->instance && !strcmp(a->instance, b->instance)));
}

/*
 * An override-redirect window is an ordinary one in X's stacking order: it
 * is on top when it appears, and a window the window manager raises after
 * that goes over it.  So a view that is raised goes over the override-
 * redirect windows that are above everything, and sits where it was: Wine
 * and Proton draw their focus overlays as such windows, and a terminal
 * clicked afterwards must cover them.
 *
 * Except that a window of the same application is the one an overlay is
 * for (it shares its WM_CLASS, if not its process): that stays over it,
 * as the application itself raises it over the window again on X.  And a
 * popup (menu, tooltip) is above whatever is raised.
 */
void
wm_xwayland_view_raised(struct wm_view *view)
{
	struct wm_server *server = view->server;
	if (!view->scene_tree || !view->scene_tree->node.parent)
		return;
	/* A window raised where nothing can see it (another workspace) is no
	 * stacking against what is on screen */
	int lx, ly;
	if (!wlr_scene_node_coords(&view->scene_tree->node, &lx, &ly))
		return;
	struct wlr_scene_tree *parent = view->scene_tree->node.parent;
	struct wm_xwayland_unmanaged *u;
	wl_list_for_each(u, &server->xwayland_unmanaged, link) {
		struct wlr_scene_tree *tree = wm_xshape_tree(u->shape);
		if (!tree || is_popup_window(u->xsurface, &server->xwayland_atoms))
			continue;
		bool same_app = view->type == WM_VIEW_XWAYLAND &&
			view->xsurface &&
			same_application(u->xsurface, view->xsurface);
		if (same_app) {
			wlr_scene_node_reparent(&tree->node, parent);
			wlr_scene_node_place_above(&tree->node,
				&view->scene_tree->node);
		} else if (tree->node.parent == server->xwayland_override_tree) {
			wlr_scene_node_reparent(&tree->node, parent);
			wlr_scene_node_place_below(&tree->node,
				&view->scene_tree->node);
		}
	}
}

static void
unmanaged_set_position(struct wm_xwayland_unmanaged *u)
{
	struct wlr_scene_tree *tree = wm_xshape_tree(u->shape);
	if (tree)
		wlr_scene_node_set_position(&tree->node, u->xsurface->x,
			u->xsurface->y);
}

static void
handle_unmanaged_map(struct wl_listener *listener, void *data)
{
	struct wm_xwayland_unmanaged *u = wl_container_of(listener, u, map);
	struct wm_server *server = u->server;
	struct wlr_xwayland_surface *xs = u->xsurface;

	/*
	 * Above everything but the session lock when it appears, as on X:
	 * so a screen locker's window covers the toolbar too, and a menu is
	 * not cut by it.  Windows raised after it go over it
	 * (wm_xwayland_view_raised())
	 */
	if (!wm_xshape_show(u->shape, server->xwayland_override_tree))
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

	wm_xshape_hide(u->shape);

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
	map_if_already_committed(u->xsurface->surface);
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
	wm_xshape_destroy(u->shape);
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
	u->shape = wm_xshape_create(server, xsurface);

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

/* More than a resources file is: it would be a mistake, not a setting */
#define XRESOURCES_MAX (1024 * 1024)

/* The whole of a file in a new buffer (not terminated), or NULL */
static char *
read_resources_file(const char *path, size_t *len)
{
	FILE *file = fopen(path, "r");
	if (!file)
		return NULL;
	char *buf = malloc(XRESOURCES_MAX + 1);
	if (!buf) {
		fclose(file);
		return NULL;
	}
	*len = fread(buf, 1, XRESOURCES_MAX + 1, file);
	bool bad = ferror(file) || *len > XRESOURCES_MAX;
	fclose(file);
	if (bad) {
		free(buf);
		return NULL;
	}
	return buf;
}

void
wm_xwayland_apply_resources(struct wm_server *server)
{
	if (!server->xwayland || !server->config ||
	    !server->config->xwayland_resources)
		return;
	/* None before XWayland has started: it is done when it is ready */
	xcb_connection_t *conn =
		wlr_xwayland_get_xwm_connection(server->xwayland);
	if (!conn)
		return;

	const char *path = server->config->xwayland_resources;
	size_t len = 0;
	char *resources = read_resources_file(path, &len);
	if (!resources) {
		wlr_log(WLR_ERROR, "xwayland: cannot read X resources from %s "
			"(or it is over %d bytes)", path, XRESOURCES_MAX);
		return;
	}
	xcb_screen_t *screen = xcb_setup_roots_iterator(
		xcb_get_setup(conn)).data;
	xcb_change_property(conn, XCB_PROP_MODE_REPLACE, screen->root,
		XCB_ATOM_RESOURCE_MANAGER, XCB_ATOM_STRING, 8, (uint32_t)len,
		resources);
	xcb_flush(conn);
	free(resources);
	wlr_log(WLR_INFO, "xwayland: X resources loaded from %s", path);
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
	wm_xshape_init(server);

	/* What xrdb does in an X session: for the programs started now */
	wm_xwayland_apply_resources(server);

	/* Set the XWayland cursor */
	wlr_xwayland_set_seat(server->xwayland, server->seat);

	/*
	 * The cursor over X11 windows that set none of their own (Tk's,
	 * for one); without an image Xwayland shows them a blank one.
	 */
	struct wlr_xcursor *xcursor = wlr_xcursor_manager_get_xcursor(
		server->cursor_mgr, "default", 1);
	if (!xcursor) {
		wlr_xcursor_manager_load(server->cursor_mgr, 1);
		xcursor = wlr_xcursor_manager_get_xcursor(server->cursor_mgr,
			"default", 1);
	}
	if (xcursor && xcursor->image_count > 0) {
		struct wlr_xcursor_image *image = xcursor->images[0];
		struct wlr_buffer *buffer = wlr_xcursor_image_get_buffer(image);
		if (buffer)
			wlr_xwayland_set_cursor(server->xwayland, buffer,
				(int32_t)image->hotspot_x,
				(int32_t)image->hotspot_y);
	}

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
		wm_xshape_finish(server);
		wl_list_remove(&server->xwayland_new_surface.link);
		wl_list_remove(&server->xwayland_ready.link);
		wlr_xwayland_destroy(server->xwayland);
		server->xwayland = NULL;
	}
}

#else /* !WM_HAS_XWAYLAND */

void
wm_xwayland_view_raised(struct wm_view *view)
{
	(void)view;
}

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

void
wm_xwayland_apply_resources(struct wm_server *server)
{
	(void)server;
}

#endif /* WM_HAS_XWAYLAND */
