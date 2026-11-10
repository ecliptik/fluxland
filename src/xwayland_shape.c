/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * xwayland_shape.c - The X Shape extension for XWayland windows
 */

#ifdef WM_HAS_XWAYLAND

#include <stdlib.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>
#include <wlr/xwayland.h>
#include <xcb/shape.h>
#include <xcb/xcb.h>

#include "server.h"
#include "xwayland_shape.h"

struct wm_xshape {
	struct wm_server *server;
	struct wlr_xwayland_surface *xsurface;
	struct wl_list link;		/* shapes */

	/* The window's bounding shape, relative to it; none when unshaped */
	struct wlr_box *rects;
	int nrects;
	bool shaped;

	struct wlr_scene_tree *tree;	/* while shown: the pieces' parent */
	int width, height;		/* what the pieces were made for */

	struct wl_listener set_geometry;
	struct wl_listener tree_destroy;
};

/* All of them, to find the one a ShapeNotify is for */
static struct wl_list shapes;	/* set up by the first window */
static uint8_t shape_first_event;	/* of the extension, 0 when absent */

int
wm_xshape_pieces(const struct wlr_box *rects, int n, int w, int h,
	struct wlr_box *out, int max, bool *whole)
{
	*whole = true;
	if (n <= 0 || w < 1 || h < 1)
		return 0;

	int count = 0, total = 0;
	long area = 0;
	int bx0 = w, by0 = h, bx1 = 0, by1 = 0;
	for (int i = 0; i < n; i++) {
		int x0 = rects[i].x < 0 ? 0 : rects[i].x;
		int y0 = rects[i].y < 0 ? 0 : rects[i].y;
		int x1 = rects[i].x + rects[i].width;
		int y1 = rects[i].y + rects[i].height;
		if (x1 > w)
			x1 = w;
		if (y1 > h)
			y1 = h;
		if (x1 <= x0 || y1 <= y0)
			continue;
		area += (long)(x1 - x0) * (y1 - y0);
		if (x0 < bx0)
			bx0 = x0;
		if (y0 < by0)
			by0 = y0;
		if (x1 > bx1)
			bx1 = x1;
		if (y1 > by1)
			by1 = y1;
		if (count < max)
			out[count++] = (struct wlr_box){ x0, y0, x1 - x0,
				y1 - y0 };
		total++;
	}
	if (total == 0) {
		*whole = false;		/* nothing of the window is left */
		return 0;
	}
	/* The rectangles of a shape do not overlap: if they add up to the
	 * window, it is not cut at all */
	if (area == (long)w * h)
		return 0;

	*whole = false;
	if (total > max) {
		/* Too many to draw one by one: the box around them all */
		if (max < 1)
			return 0;
		out[0] = (struct wlr_box){ bx0, by0, bx1 - bx0, by1 - by0 };
		return 1;
	}
	return count;
}

static xcb_connection_t *
connection(struct wm_server *server)
{
	return server->xwayland ?
		wlr_xwayland_get_xwm_connection(server->xwayland) : NULL;
}

/* Read the shape the X server has for the window now */
static void
fetch_shape(struct wm_xshape *xshape)
{
	free(xshape->rects);
	xshape->rects = NULL;
	xshape->nrects = 0;
	xshape->shaped = false;

	xcb_connection_t *conn = connection(xshape->server);
	if (!conn || !shape_first_event)
		return;
	xcb_window_t window = xshape->xsurface->window_id;
	xcb_shape_query_extents_cookie_t extents =
		xcb_shape_query_extents(conn, window);
	xcb_shape_get_rectangles_cookie_t rectangles =
		xcb_shape_get_rectangles(conn, window, XCB_SHAPE_SK_BOUNDING);
	xcb_shape_query_extents_reply_t *e =
		xcb_shape_query_extents_reply(conn, extents, NULL);
	xcb_shape_get_rectangles_reply_t *r =
		xcb_shape_get_rectangles_reply(conn, rectangles, NULL);

	if (e && r && e->bounding_shaped) {
		int n = xcb_shape_get_rectangles_rectangles_length(r);
		xcb_rectangle_t *rc = xcb_shape_get_rectangles_rectangles(r);
		xshape->rects = calloc(n > 0 ? n : 1, sizeof(*xshape->rects));
		if (xshape->rects) {
			for (int i = 0; i < n; i++)
				xshape->rects[i] = (struct wlr_box){ rc[i].x,
					rc[i].y, rc[i].width, rc[i].height };
			xshape->nrects = n;
			xshape->shaped = true;
		}
	}
	free(e);
	free(r);
}

static void
clear_pieces(struct wm_xshape *xshape)
{
	struct wlr_scene_node *node, *tmp;
	wl_list_for_each_safe(node, tmp, &xshape->tree->children, link)
		wlr_scene_node_destroy(node);
}

/* Draw the window as its shape now says */
static void
rebuild(struct wm_xshape *xshape)
{
	if (!xshape->tree)
		return;
	clear_pieces(xshape);
	struct wlr_xwayland_surface *xs = xshape->xsurface;
	if (!xs->surface)
		return;
	xshape->width = xs->width;
	xshape->height = xs->height;

	struct wlr_box pieces[WM_XSHAPE_PIECES_MAX];
	bool whole = true;
	int n = 0;
	if (xshape->shaped)
		n = wm_xshape_pieces(xshape->rects, xshape->nrects, xs->width,
			xs->height, pieces, WM_XSHAPE_PIECES_MAX, &whole);
	if (whole) {
		wlr_scene_subsurface_tree_create(xshape->tree, xs->surface);
		return;
	}
	for (int i = 0; i < n; i++) {
		struct wlr_scene_tree *piece = wlr_scene_subsurface_tree_create(
			xshape->tree, xs->surface);
		if (piece)
			wlr_scene_subsurface_tree_set_clip(&piece->node,
				&pieces[i]);
	}
}

static void
handle_set_geometry(struct wl_listener *listener, void *data)
{
	struct wm_xshape *xshape =
		wl_container_of(listener, xshape, set_geometry);
	/* The shape is clipped to the window: it follows its size */
	if (xshape->tree && (xshape->width != xshape->xsurface->width ||
	    xshape->height != xshape->xsurface->height))
		rebuild(xshape);
}

/* The tree went with the one it was in (a workspace that was removed) */
static void
handle_tree_destroy(struct wl_listener *listener, void *data)
{
	struct wm_xshape *xshape =
		wl_container_of(listener, xshape, tree_destroy);
	wl_list_remove(&xshape->tree_destroy.link);
	xshape->tree = NULL;
}

struct wm_xshape *
wm_xshape_create(struct wm_server *server,
	struct wlr_xwayland_surface *xsurface)
{
	struct wm_xshape *xshape = calloc(1, sizeof(*xshape));
	if (!xshape)
		return NULL;
	xshape->server = server;
	xshape->xsurface = xsurface;
	if (!shapes.next)
		wl_list_init(&shapes);
	wl_list_insert(&shapes, &xshape->link);

	xcb_connection_t *conn = connection(server);
	if (conn && shape_first_event) {
		/* ShapeNotify for this window from now on, and what it is */
		xcb_shape_select_input(conn, xsurface->window_id, 1);
		xcb_flush(conn);
		fetch_shape(xshape);
	}
	xshape->set_geometry.notify = handle_set_geometry;
	wl_signal_add(&xsurface->events.set_geometry, &xshape->set_geometry);
	return xshape;
}

void
wm_xshape_destroy(struct wm_xshape *xshape)
{
	if (!xshape)
		return;
	wm_xshape_hide(xshape);
	wl_list_remove(&xshape->set_geometry.link);
	wl_list_remove(&xshape->link);
	free(xshape->rects);
	free(xshape);
}

struct wlr_scene_tree *
wm_xshape_show(struct wm_xshape *xshape, struct wlr_scene_tree *parent)
{
	wm_xshape_hide(xshape);
	xshape->tree = wlr_scene_tree_create(parent);
	if (xshape->tree) {
		xshape->tree_destroy.notify = handle_tree_destroy;
		wl_signal_add(&xshape->tree->node.events.destroy,
			&xshape->tree_destroy);
		rebuild(xshape);
	}
	return xshape->tree;
}

void
wm_xshape_hide(struct wm_xshape *xshape)
{
	if (xshape->tree) {
		wl_list_remove(&xshape->tree_destroy.link);
		wlr_scene_node_destroy(&xshape->tree->node);
		xshape->tree = NULL;
	}
}

struct wlr_scene_tree *
wm_xshape_tree(const struct wm_xshape *xshape)
{
	return xshape ? xshape->tree : NULL;
}

/* An X event the XWayland event loop got: the extension's, if it is */
static bool
handle_x_event(struct wlr_xwayland *wlr_xwayland, xcb_generic_event_t *event)
{
	if (!shape_first_event ||
	    (event->response_type & 0x7f) != shape_first_event + XCB_SHAPE_NOTIFY)
		return false;
	xcb_shape_notify_event_t *notify = (xcb_shape_notify_event_t *)event;
	if (notify->shape_kind != XCB_SHAPE_SK_BOUNDING)
		return true;

	if (!shapes.next)
		return true;
	struct wm_xshape *xshape;
	wl_list_for_each(xshape, &shapes, link) {
		if (xshape->xsurface->window_id != notify->affected_window)
			continue;
		fetch_shape(xshape);
		rebuild(xshape);
		break;
	}
	return true;
}

void
wm_xshape_init(struct wm_server *server)
{
	xcb_connection_t *conn = connection(server);
	if (!conn)
		return;
	const xcb_query_extension_reply_t *ext =
		xcb_get_extension_data(conn, &xcb_shape_id);
	if (!ext || !ext->present) {
		wlr_log(WLR_INFO, "%s",
			"xwayland: no Shape extension; shaped windows stay boxes");
		return;
	}
	shape_first_event = ext->first_event;
	server->xwayland->user_event_handler = handle_x_event;
}

void
wm_xshape_finish(struct wm_server *server)
{
	if (server->xwayland)
		server->xwayland->user_event_handler = NULL;
	shape_first_event = 0;
}

#endif /* WM_HAS_XWAYLAND */
