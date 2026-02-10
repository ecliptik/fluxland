/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * placement.h - Window placement and edge snapping
 */

#ifndef WM_PLACEMENT_H
#define WM_PLACEMENT_H

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
 * Snap a view to nearby edges during an interactive move.
 * Checks proximity to output usable area edges and to other
 * visible window edges on the same workspace. Modifies new_x
 * and new_y in place if snapping occurs.
 *
 * Uses server->config->edge_snap_threshold; does nothing if
 * the threshold is 0.
 */
void wm_snap_edges(struct wm_server *server, struct wm_view *view,
	int *new_x, int *new_y);

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

#endif /* WM_PLACEMENT_H */
