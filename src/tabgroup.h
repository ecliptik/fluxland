/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * tabgroup.h - Window tabbing/grouping (Fluxbox-style tab groups)
 *
 * Multiple windows can be combined into a single decoration frame with
 * tabs in the titlebar.  Only the active tab's surface is visible.
 * All views in a group share position and size.
 */

#ifndef WM_TABGROUP_H
#define WM_TABGROUP_H

#include <wayland-server-core.h>

struct wm_view;
struct wm_server;

struct wm_tab_group {
	struct wl_list views;         /* wm_view.tab_link */
	struct wm_view *active_view;  /* currently visible/focused tab */
	int count;                    /* number of views in group */
	struct wm_server *server;
};

/*
 * Create a new tab group containing a single view.
 * The view must not already belong to a tab group.
 * Returns the new group, or NULL on allocation failure.
 */
struct wm_tab_group *wm_tab_group_create(struct wm_view *view);

/*
 * Destroy a tab group.  All remaining views are detached first
 * (their tab_group pointers are cleared).
 */
void wm_tab_group_destroy(struct wm_tab_group *group);

/*
 * Add a view to an existing tab group.
 * The view is appended after the current active tab.
 * Its surface is hidden and it is resized/repositioned to match
 * the group's geometry.
 */
void wm_tab_group_add(struct wm_tab_group *group, struct wm_view *view);

/*
 * Remove a view from its tab group (DetachClient).
 * The view's surface is made visible and it gets a slight position
 * offset so it doesn't perfectly overlap.  If the group becomes
 * empty, it is destroyed.  If the removed view was the active tab,
 * the next tab becomes active.
 */
void wm_tab_group_remove(struct wm_view *view);

/*
 * Switch the active (visible) tab in the group.
 * Hides the old active surface, shows the new one, and updates
 * keyboard focus.
 */
void wm_tab_group_activate(struct wm_tab_group *group,
	struct wm_view *view);

/*
 * Cycle to the next tab in the group (wraps around).
 */
void wm_tab_group_next(struct wm_tab_group *group);

/*
 * Cycle to the previous tab in the group (wraps around).
 */
void wm_tab_group_prev(struct wm_tab_group *group);

/*
 * Move a tab one position to the right in the tab order.
 * Wraps around to the beginning.
 */
void wm_tab_group_move_right(struct wm_tab_group *group,
	struct wm_view *view);

/*
 * Move a tab one position to the left in the tab order.
 * Wraps around to the end.
 */
void wm_tab_group_move_left(struct wm_tab_group *group,
	struct wm_view *view);

/*
 * Merge two tab groups.  All views from `other` are appended to
 * `target`.  The `other` group is destroyed afterward.
 */
void wm_tab_group_merge(struct wm_tab_group *target,
	struct wm_tab_group *other);

/*
 * Get the tab index (0-based) of a view within its group.
 * Returns -1 if the view is not in a group.
 */
int wm_tab_group_index_of(struct wm_tab_group *group,
	struct wm_view *view);

#endif /* WM_TABGROUP_H */
