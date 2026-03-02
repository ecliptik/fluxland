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

#endif /* WM_VIEW_GEOMETRY_H */
