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

/*
 * Arrange all visible windows on the current workspace in a grid layout.
 * cols = ceil(sqrt(n)), rows = ceil(n/cols), divides usable area into cells.
 */
void wm_arrange_windows_grid(struct wm_server *server);

/*
 * Arrange all visible windows in equal-width vertical columns.
 */
void wm_arrange_windows_vert(struct wm_server *server);

/*
 * Arrange all visible windows in equal-height horizontal rows.
 */
void wm_arrange_windows_horiz(struct wm_server *server);

/*
 * Cascade all visible windows with a 30px x,y offset each.
 */
void wm_arrange_windows_cascade(struct wm_server *server);

/*
 * Master-stack layouts: first visible view gets 50% of screen (master),
 * remaining views stack equally in the other 50%.
 */
void wm_arrange_windows_stack_left(struct wm_server *server);
void wm_arrange_windows_stack_right(struct wm_server *server);
void wm_arrange_windows_stack_top(struct wm_server *server);
void wm_arrange_windows_stack_bottom(struct wm_server *server);

#endif /* WM_PLACEMENT_H */
