/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * placement.h - Window placement and edge snapping
 */

#ifndef WM_PLACEMENT_H
#define WM_PLACEMENT_H

#include <stdint.h>

struct wm_server;
struct wm_view;

/*
 * Apply automatic window placement to a newly mapped view.
 * Uses the configured placement policy (ROW_SMART, COL_SMART,
 * CASCADE, or UNDER_MOUSE) to position the view within the
 * usable area of the output under the cursor.
 */
void wm_placement_apply(struct wm_server *server, struct wm_view *view);

/*
 * Snap a window being moved to (*new_x, *new_y), its frame's top left,
 * as Fluxbox's doSnapping: to the nearest edge within edgeSnapThreshold
 * of the heads (usable areas and whole) and the other windows on the
 * workspace.  Nothing when the threshold is 0.
 */
void wm_snap_edges(struct wm_server *server, struct wm_view *view,
	int *new_x, int *new_y);

/*
 * Snap the dragged corner of a resize (edges: WLR_EDGE_*) the same way,
 * within edgeResizeSnapThreshold: x, y the frame's top left, w, h the
 * client's size, all changed in place.
 */
void wm_snap_resize(struct wm_server *server, struct wm_view *view,
	uint32_t edges, int *x, int *y, int *w, int *h);

/* Fluxbox's ArrangeWindows methods */
enum wm_arrange_method {
	WM_ARRANGE_GRID,          /* ArrangeWindows: more cells the long way */
	WM_ARRANGE_VERTICAL,      /* ArrangeWindowsVertical */
	WM_ARRANGE_HORIZONTAL,    /* ArrangeWindowsHorizontal */
	WM_ARRANGE_STACK_LEFT,    /* the others on the left, ...: */
	WM_ARRANGE_STACK_RIGHT,
	WM_ARRANGE_STACK_TOP,
	WM_ARRANGE_STACK_BOTTOM,
};

/*
 * ArrangeWindows* [pattern]: the shown windows of this workspace on the
 * pointer's head (that match) in a grid of near-square cells over its
 * usable area, each cell taking the window nearest it, shaded ones down
 * the sides; the stacked methods keep the focused window big in half of
 * it (ArrangeWindowsCmd)
 */
void wm_arrange_windows(struct wm_server *server,
	enum wm_arrange_method method, const char *pattern);

/* The same without a pattern */
void wm_arrange_windows_grid(struct wm_server *server);
void wm_arrange_windows_vert(struct wm_server *server);
void wm_arrange_windows_horiz(struct wm_server *server);
void wm_arrange_windows_stack_left(struct wm_server *server);
void wm_arrange_windows_stack_right(struct wm_server *server);
void wm_arrange_windows_stack_top(struct wm_server *server);
void wm_arrange_windows_stack_bottom(struct wm_server *server);

/* Unclutter [pattern]: those windows placed again where they overlap
 * least (MinOverlapPlacement) */
void wm_unclutter(struct wm_server *server, const char *pattern);

/* CascadeWindows [pattern] (fluxland's): a step down and right each */
void wm_cascade_windows(struct wm_server *server, const char *pattern);
void wm_arrange_windows_cascade(struct wm_server *server);

#endif /* WM_PLACEMENT_H */
