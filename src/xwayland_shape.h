/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * xwayland_shape.h - The X Shape extension for XWayland windows
 *
 * An X11 window can have a shape: only part of its rectangle is the window
 * (Wine's layered windows, such as Proton's focus overlays, are rings; xeyes
 * is a pair of eyes).  Wayland has no shapes, and XWayland leaves what is
 * outside the shape black, so the compositor must clip the window to its
 * shape itself.
 *
 * The shape is read when a window appears and again whenever the X server
 * says it changed (ShapeNotify, delivered through the XWayland event loop:
 * nothing is polled).  The scene graph clips to one rectangle at a time, so
 * a shaped window is drawn as one clipped copy of its surface per rectangle
 * of the shape.
 */

#ifndef WM_XWAYLAND_SHAPE_H
#define WM_XWAYLAND_SHAPE_H

#include <stdbool.h>
#include <wlr/util/box.h>

struct wm_server;
struct wlr_scene_tree;
struct wlr_xwayland_surface;
struct wm_xshape;

/* The most rectangles of a shape that get a copy each; a shape of more is
 * drawn as the box around it */
#define WM_XSHAPE_PIECES_MAX 32

/*
 * What of a w x h window a shape of n rectangles (relative to the window)
 * leaves: the rectangles clipped to the window, empty ones dropped, in out
 * (up to max).  Returns how many; *whole is set when the window is not cut
 * at all (nothing is to be clipped).  More pieces than max become their
 * bounding box.  Zero pieces and not whole: nothing of the window is left.
 */
int wm_xshape_pieces(const struct wlr_box *rects, int n, int w, int h,
	struct wlr_box *out, int max, bool *whole);

/* Set up the extension on the XWayland event loop; XWayland has started */
void wm_xshape_init(struct wm_server *server);
void wm_xshape_finish(struct wm_server *server);

/* Follow the shape of an XWayland window, from now until destroyed */
struct wm_xshape *wm_xshape_create(struct wm_server *server,
	struct wlr_xwayland_surface *xsurface);
void wm_xshape_destroy(struct wm_xshape *xshape);

/*
 * Draw the window in parent: the tree returned holds the surface, clipped
 * to the shape, and is kept up to date; at the window's origin like the
 * plain surface tree it stands in for.  hide() takes it down (before the
 * surface goes away).
 */
struct wlr_scene_tree *wm_xshape_show(struct wm_xshape *xshape,
	struct wlr_scene_tree *parent);
void wm_xshape_hide(struct wm_xshape *xshape);

/* The tree show() made, or NULL when not shown (it is also gone when its
 * parent tree is destroyed with it still shown) */
struct wlr_scene_tree *wm_xshape_tree(const struct wm_xshape *xshape);

#endif /* WM_XWAYLAND_SHAPE_H */
