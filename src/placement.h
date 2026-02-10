/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
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

#endif /* WM_PLACEMENT_H */
