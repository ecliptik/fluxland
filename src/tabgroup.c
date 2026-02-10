/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * tabgroup.c - Window tabbing/grouping implementation
 */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>

#include "server.h"
#include "tabgroup.h"
#include "view.h"

/*
 * Synchronize a view's position and size to match the group's active
 * view.  Used when adding a view to a group or when the group is
 * resized/moved.
 */
static void
sync_view_to_group(struct wm_tab_group *group, struct wm_view *view)
{
	struct wm_view *active = group->active_view;
	if (!active || active == view) {
		return;
	}

	/* Match position */
	view->x = active->x;
	view->y = active->y;
	wlr_scene_node_set_position(&view->scene_tree->node,
		view->x, view->y);

	/* Match size */
	struct wlr_box geo;
	wlr_xdg_surface_get_geometry(active->xdg_toplevel->base, &geo);
	wlr_xdg_toplevel_set_size(view->xdg_toplevel,
		geo.width, geo.height);
}

/*
 * Show or hide a view's XDG surface within the scene graph.
 * We walk the scene tree children to find the xdg_surface subtree
 * (the second child, after the decoration tree once decorations exist).
 *
 * For now we toggle the entire scene_tree visibility of non-active
 * tabs and rely on the active tab being the only one enabled.
 */
static void
set_view_surface_visible(struct wm_view *view, bool visible)
{
	/*
	 * We iterate children of view->scene_tree.  Child nodes that
	 * represent the XDG surface (not decoration nodes) need toggling.
	 * For simplicity, we walk all children.  The decoration subtree
	 * is shared (rendered based on the active tab), so we only need
	 * to control the surface node.
	 *
	 * Implementation: each view's scene_tree has its xdg surface as
	 * the first (and currently only) child created by
	 * wlr_scene_xdg_surface_create().  When decorations are added,
	 * the decoration tree will be a sibling.  We need to hide only
	 * non-active views' surface subtrees while keeping the shared
	 * decoration tree visible for the group frame.
	 *
	 * For the initial implementation (before decoration integration),
	 * we simply toggle the entire view scene tree.  The decoration
	 * engineer will refine this during integration.
	 */
	wlr_scene_node_set_enabled(&view->scene_tree->node, visible);
}

struct wm_tab_group *
wm_tab_group_create(struct wm_view *view)
{
	assert(view);
	assert(!view->tab_group);

	struct wm_tab_group *group = calloc(1, sizeof(*group));
	if (!group) {
		wlr_log(WLR_ERROR, "%s", "tab group allocation failed");
		return NULL;
	}

	wl_list_init(&group->views);
	group->server = view->server;
	group->active_view = view;
	group->count = 1;

	view->tab_group = group;
	wl_list_insert(&group->views, &view->tab_link);

	wlr_log(WLR_DEBUG, "created tab group for view '%s'",
		view->title ? view->title : "(untitled)");

	return group;
}

void
wm_tab_group_destroy(struct wm_tab_group *group)
{
	if (!group) {
		return;
	}

	/* Detach all views without destroying the group recursively */
	struct wm_view *view, *tmp;
	wl_list_for_each_safe(view, tmp, &group->views, tab_link) {
		wl_list_remove(&view->tab_link);
		wl_list_init(&view->tab_link);
		view->tab_group = NULL;
		/* Make the surface visible again */
		set_view_surface_visible(view, true);
	}

	wlr_log(WLR_DEBUG, "%s", "destroyed tab group");
	free(group);
}

void
wm_tab_group_add(struct wm_tab_group *group, struct wm_view *view)
{
	assert(group);
	assert(view);

	/* If the view is already in a group, remove it first */
	if (view->tab_group) {
		wm_tab_group_remove(view);
	}

	/* Insert after the active view in the list */
	wl_list_insert(&group->active_view->tab_link, &view->tab_link);
	view->tab_group = group;
	group->count++;

	/* Sync geometry and hide the surface (only active tab is visible) */
	sync_view_to_group(group, view);
	set_view_surface_visible(view, false);

	wlr_log(WLR_DEBUG, "added view '%s' to tab group (count=%d)",
		view->title ? view->title : "(untitled)", group->count);
}

void
wm_tab_group_remove(struct wm_view *view)
{
	assert(view);

	struct wm_tab_group *group = view->tab_group;
	if (!group) {
		return;
	}

	bool was_active = (group->active_view == view);

	/* Unlink from the group */
	wl_list_remove(&view->tab_link);
	wl_list_init(&view->tab_link);
	view->tab_group = NULL;
	group->count--;

	/* Make the detached view visible */
	set_view_surface_visible(view, true);

	/* Offset the detached view slightly so it doesn't overlap exactly */
	view->x += 20;
	view->y += 20;
	wlr_scene_node_set_position(&view->scene_tree->node,
		view->x, view->y);

	if (group->count == 0) {
		/* Group is now empty, destroy it */
		group->active_view = NULL;
		wm_tab_group_destroy(group);
		return;
	}

	if (was_active) {
		/* Activate the next view in the list */
		struct wm_view *next = wl_container_of(
			group->views.next, next, tab_link);
		group->active_view = next;
		set_view_surface_visible(next, true);
	}

	/* If only one view remains, dissolve the group */
	if (group->count == 1) {
		struct wm_view *last = wl_container_of(
			group->views.next, last, tab_link);
		wl_list_remove(&last->tab_link);
		wl_list_init(&last->tab_link);
		last->tab_group = NULL;
		set_view_surface_visible(last, true);
		free(group);
		wlr_log(WLR_DEBUG, "%s", "dissolved single-view tab group");
		return;
	}

	wlr_log(WLR_DEBUG, "removed view from tab group (count=%d)",
		group->count);
}

void
wm_tab_group_activate(struct wm_tab_group *group, struct wm_view *view)
{
	assert(group);
	assert(view);
	assert(view->tab_group == group);

	if (group->active_view == view) {
		return;
	}

	/* Hide the old active tab */
	struct wm_view *old = group->active_view;
	if (old) {
		set_view_surface_visible(old, false);
	}

	/* Show the new active tab */
	group->active_view = view;
	set_view_surface_visible(view, true);

	/* Sync position in case something drifted */
	if (old) {
		view->x = old->x;
		view->y = old->y;
		wlr_scene_node_set_position(&view->scene_tree->node,
			view->x, view->y);
	}

	/* Focus the newly active view */
	wm_focus_view(view, view->xdg_toplevel->base->surface);

	wlr_log(WLR_DEBUG, "activated tab '%s'",
		view->title ? view->title : "(untitled)");
}

void
wm_tab_group_next(struct wm_tab_group *group)
{
	assert(group);
	if (group->count <= 1) {
		return;
	}

	struct wm_view *active = group->active_view;
	struct wl_list *next_link = active->tab_link.next;

	/* Wrap around: if we're at the end of the list, go to the first */
	if (next_link == &group->views) {
		next_link = group->views.next;
	}

	struct wm_view *next = wl_container_of(next_link, next, tab_link);
	wm_tab_group_activate(group, next);
}

void
wm_tab_group_prev(struct wm_tab_group *group)
{
	assert(group);
	if (group->count <= 1) {
		return;
	}

	struct wm_view *active = group->active_view;
	struct wl_list *prev_link = active->tab_link.prev;

	/* Wrap around: if we're at the beginning, go to the last */
	if (prev_link == &group->views) {
		prev_link = group->views.prev;
	}

	struct wm_view *prev = wl_container_of(prev_link, prev, tab_link);
	wm_tab_group_activate(group, prev);
}

void
wm_tab_group_move_right(struct wm_tab_group *group, struct wm_view *view)
{
	assert(group);
	assert(view);
	assert(view->tab_group == group);

	if (group->count <= 1) {
		return;
	}

	struct wl_list *next_link = view->tab_link.next;

	/* Wrap: if at end, move to beginning */
	if (next_link == &group->views) {
		next_link = group->views.next;
	}

	/* Swap by removing and reinserting after the next element */
	wl_list_remove(&view->tab_link);
	wl_list_insert(next_link, &view->tab_link);

	wlr_log(WLR_DEBUG, "moved tab '%s' right",
		view->title ? view->title : "(untitled)");
}

void
wm_tab_group_move_left(struct wm_tab_group *group, struct wm_view *view)
{
	assert(group);
	assert(view);
	assert(view->tab_group == group);

	if (group->count <= 1) {
		return;
	}

	struct wl_list *prev_link = view->tab_link.prev;

	/* Wrap: if at beginning, move to end */
	if (prev_link == &group->views) {
		prev_link = group->views.prev;
	}

	/* Swap by removing and reinserting before the previous element */
	wl_list_remove(&view->tab_link);
	wl_list_insert(prev_link->prev, &view->tab_link);

	wlr_log(WLR_DEBUG, "moved tab '%s' left",
		view->title ? view->title : "(untitled)");
}

void
wm_tab_group_merge(struct wm_tab_group *target,
	struct wm_tab_group *other)
{
	assert(target);
	assert(other);
	assert(target != other);

	struct wm_view *view, *tmp;
	wl_list_for_each_safe(view, tmp, &other->views, tab_link) {
		wl_list_remove(&view->tab_link);
		view->tab_group = target;
		wl_list_insert(target->views.prev, &view->tab_link);
		target->count++;

		/* Sync geometry and hide */
		sync_view_to_group(target, view);
		set_view_surface_visible(view, false);
	}

	wlr_log(WLR_DEBUG, "merged tab groups (new count=%d)",
		target->count);

	other->count = 0;
	other->active_view = NULL;
	free(other);
}

int
wm_tab_group_index_of(struct wm_tab_group *group, struct wm_view *view)
{
	if (!group || !view || view->tab_group != group) {
		return -1;
	}

	int idx = 0;
	struct wm_view *v;
	wl_list_for_each(v, &group->views, tab_link) {
		if (v == view) {
			return idx;
		}
		idx++;
	}

	return -1;
}
