/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * view_geometry.h - View geometry, tiling, and multi-monitor helpers
 */

#ifndef WM_VIEW_GEOMETRY_H
#define WM_VIEW_GEOMETRY_H

#include <stdbool.h>

struct wm_view;
struct wm_output;
struct wlr_box;

/* Toggle maximize on/off for the given view. */
void wm_view_toggle_maximize(struct wm_view *view);

/* Toggle fullscreen on/off for the given view. */
void wm_view_toggle_fullscreen(struct wm_view *view);

/* Get the usable area for the output containing the view. */
bool get_view_output_area(struct wm_view *view, struct wlr_box *area);

/* Move and center a view on the given output. */
void move_view_to_output(struct wm_view *view, struct wm_output *out);

/* Decoration extents around the client (0 when undecorated/fullscreen) */
void wm_view_frame_extents(struct wm_view *view, int *top, int *bottom,
	int *left, int *right);

/* The window's outer box: client plus decorations, in layout coords */
void wm_view_frame_box(struct wm_view *view, struct wlr_box *box);

/* Move and resize so the whole frame (decorations too) fills box */
void wm_view_set_frame_box(struct wm_view *view, const struct wlr_box *box);

/*
 * Area a maximized window fills on its own output: the usable area, or
 * the whole output with session.screen0.fullMaximization.
 */
bool wm_view_maximize_area(struct wm_view *view, struct wlr_box *area);

/*
 * Re-apply maximized / fullscreen / half / vert / horiz geometry on the
 * window's current output (after the output or its usable area changed).
 */
void wm_view_refit(struct wm_view *view);

/* wm_view_refit() for every mapped window */
void wm_view_refit_all(struct wm_server *server);

/*
 * The window is no longer maximized, vertically or horizontally maximized,
 * or tiled to a half: for one the user is resizing by hand, or snapping to
 * a zone.  It stays where it is, as big as it is (Fluxbox's
 * disableMaximization), so that undoing the state later returns to what the
 * user made, not to what it was before.  Fullscreen is left alone.
 */
void wm_view_clear_maximized(struct wm_view *view);

/*
 * A window in one of those states (or fullscreen) that was moved by hand
 * keeps it, on the head it was dropped on: it is fitted to that head again
 * (the same one: back where it was), and what undoing the state returns it
 * to is moved onto that head too.  old_head_x/y is the layout position of
 * the head it was moved from.
 */
void wm_view_settle_on_head(struct wm_view *view, int old_head_x,
	int old_head_y);

/*
 * Tile a window to a cell of its head, given in permille of the head's usable
 * area (0..1000: x, y, and the width and height).  What undoing it returns
 * to is remembered unless the window was in such a state already; the cell
 * goes with the window to another head (wm_view_refit()); the same cell again
 * puts it back.  Nothing for a cell that is empty or outside the head.
 */
void wm_view_tile(struct wm_view *view, int x, int y, int w, int h);

/*
 * SnapStep left|right|up|down, as the Windows key + arrow: the next place
 * for the window on its way in that direction.  Left and right snap to a
 * half, then onto the next head in that direction (on its other half), and
 * undo a snap to the other side; up maximizes, or makes a half a top
 * quarter; down restores a maximized window, makes a half a bottom quarter
 * and a top quarter a half, and minimizes any other.  False for a direction
 * that is none.
 */
bool wm_view_snap_step(struct wm_server *server, struct wm_view *view,
	const char *direction);

/*
 * A maximized window dragged by hand is pulled off its maximize: it
 * is no longer maximized and goes back to the size it had before, with
 * the pointer (px, py) in the same place across it and as far down.  The
 * offset of the pointer in the new frame comes back in grab_x / grab_y.
 * False (nothing done) when it is not maximized, or it has no size to go
 * back to.
 */
bool wm_view_pull_restore(struct wm_view *view, double px, double py,
	double *grab_x, double *grab_y);

/*
 * Dropped in the maximize snap zone of a head (area is its usable area):
 * the window is maximized there.  A window that was maximized before keeps
 * the geometry it was restored to (prev_saved), moved onto the head as
 * from old_head_x/y; else what it was when dropped is.
 */
void wm_view_snap_maximize(struct wm_view *view, const struct wlr_box *area,
	const struct wlr_box *prev_saved, int old_head_x, int old_head_y);

/*
 * Drop a window dragged onto a cell of the layout picker on a head (its
 * usable area; cell is x y w h in permille): it is moved onto the head and tiled there, and keeps the
 * geometry it is to be restored to (prev_saved, as for wm_view_snap_maximize())
 * when it was maximized or tiled already.
 */
void wm_view_snap_tile(struct wm_view *view, const struct wlr_box *area,
	const int cell[4], const struct wlr_box *prev_saved, int old_head_x,
	int old_head_y);

/* Pull a window back onto its nearest output if no titlebar is visible */
void wm_view_keep_on_screen(struct wm_view *view);

#endif /* WM_VIEW_GEOMETRY_H */
