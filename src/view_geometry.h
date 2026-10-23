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

/* Pull a window back onto its nearest output if no titlebar is visible */
void wm_view_keep_on_screen(struct wm_view *view);

#endif /* WM_VIEW_GEOMETRY_H */
