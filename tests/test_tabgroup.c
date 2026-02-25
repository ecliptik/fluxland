/*
 * test_tabgroup.c - Unit tests for tab group state machine
 *
 * Includes tabgroup.c directly with stub implementations to avoid
 * needing wlroots/wayland libraries at link time.
 */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- Block real headers via their include guards --- */
#define WAYLAND_SERVER_CORE_H
#define WAYLAND_SERVER_H
#define WAYLAND_UTIL_H
#define WLR_UTIL_LOG_H
#define WLR_TYPES_WLR_SCENE_H
#define WLR_TYPES_WLR_XDG_SHELL_H
#define WM_VIEW_H
#define WM_SERVER_H
#define WM_DECORATION_H
#define WM_STYLE_H
#define WM_RENDER_H

/* --- Stub wayland types --- */

struct wl_list {
	struct wl_list *prev;
	struct wl_list *next;
};

static inline void
wl_list_init(struct wl_list *list)
{
	list->prev = list;
	list->next = list;
}

static inline void
wl_list_insert(struct wl_list *list, struct wl_list *elm)
{
	elm->prev = list;
	elm->next = list->next;
	list->next = elm;
	elm->next->prev = elm;
}

static inline void
wl_list_remove(struct wl_list *elm)
{
	elm->prev->next = elm->next;
	elm->next->prev = elm->prev;
	elm->prev = NULL;
	elm->next = NULL;
}

static inline int
wl_list_empty(const struct wl_list *list)
{
	return list->next == list;
}

#include <stddef.h>
/*
 * In tabgroup.c, wl_container_of is always used with struct wm_view / tab_link.
 * We provide a simplified version that avoids __typeof__ issues in C17
 * when the sample variable is being declared in the same statement.
 */
#define wl_container_of(ptr, sample, member) \
	((void *)((char *)(ptr) - offsetof(struct wm_view, member)))

#define wl_list_for_each_safe(pos, tmp, head, member)			\
	for (pos = wl_container_of((head)->next, pos, member),		\
	     tmp = wl_container_of((pos)->member.next, tmp, member);	\
	     &(pos)->member != (head);					\
	     pos = tmp,							\
	     tmp = wl_container_of((pos)->member.next, tmp, member))

/* --- Stub wlr types --- */

struct wlr_scene_node {
	int enabled;
	int x, y;
};

struct wlr_scene_tree {
	struct wlr_scene_node node;
};

struct wlr_box {
	int x, y, width, height;
};

struct wlr_surface {
	int dummy;
};

struct wlr_xdg_surface {
	struct wlr_surface *surface;
};

struct wlr_xdg_toplevel {
	struct wlr_xdg_surface *base;
	int width, height;
};

/* wlr_log no-op */
#define wlr_log(verb, fmt, ...) ((void)0)
#define WLR_ERROR 1
#define WLR_DEBUG 7

/* --- Stub wm types --- */

struct wm_style { int dummy; };
struct wm_decoration { int dummy; };

struct wm_server {
	struct wm_style *style;
};

struct wm_view;
struct wm_tab_group;
struct wm_animation;

struct wm_view {
	struct wl_list link;
	struct wm_server *server;
	struct wlr_xdg_toplevel *xdg_toplevel;
	struct wlr_scene_tree *scene_tree;
	uint32_t id;
	int x, y;
	struct wlr_box saved_geometry;
	bool maximized, fullscreen;
	bool maximized_vert, maximized_horiz;
	bool lhalf, rhalf;
	bool show_decoration;
	char *title;
	char *app_id;
	void *workspace;
	bool sticky;
	int layer;
	struct wm_tab_group *tab_group;
	struct wl_list tab_link;
	int focus_alpha;
	int unfocus_alpha;
	struct wm_animation *animation;
	struct wm_decoration *decoration;
};

/* --- Stub tracking --- */

static int g_set_enabled_count;
static int g_set_position_count;
static int g_get_geometry_count;
static int g_set_size_count;
static int g_decoration_update_count;
static int g_focus_view_count;
static struct wm_view *g_last_focused_view;

/* --- Stub wlroots functions --- */

static void
wlr_scene_node_set_enabled(struct wlr_scene_node *node, bool enabled)
{
	if (node) {
		node->enabled = enabled ? 1 : 0;
	}
	g_set_enabled_count++;
}

static void
wlr_scene_node_set_position(struct wlr_scene_node *node, int x, int y)
{
	if (node) {
		node->x = x;
		node->y = y;
	}
	g_set_position_count++;
}

static void
wlr_xdg_surface_get_geometry(struct wlr_xdg_surface *surface,
	struct wlr_box *box)
{
	/* Return a fixed geometry for the active view */
	box->x = 0;
	box->y = 0;
	box->width = 800;
	box->height = 600;
	g_get_geometry_count++;
}

static uint32_t
wlr_xdg_toplevel_set_size(struct wlr_xdg_toplevel *toplevel,
	int width, int height)
{
	if (toplevel) {
		toplevel->width = width;
		toplevel->height = height;
	}
	g_set_size_count++;
	return 0;
}

/* --- Stub fluxland functions --- */

static void
wm_decoration_update(struct wm_decoration *decoration,
	struct wm_style *style)
{
	(void)decoration;
	(void)style;
	g_decoration_update_count++;
}

static void
wm_focus_view(struct wm_view *view, struct wlr_surface *surface)
{
	(void)surface;
	g_last_focused_view = view;
	g_focus_view_count++;
}

/* --- Include tabgroup source directly --- */

#include "tabgroup.h"
#include "tabgroup.c"

/* --- Test helpers --- */

static struct wm_server test_server;
static struct wm_style test_style;

/* Scene tree and toplevel per view */
static struct wlr_scene_tree test_trees[8];
static struct wlr_xdg_toplevel test_toplevels[8];
static struct wlr_xdg_surface test_xdg_surfaces[8];
static struct wlr_surface test_surfaces[8];

static void
reset_globals(void)
{
	g_set_enabled_count = 0;
	g_set_position_count = 0;
	g_get_geometry_count = 0;
	g_set_size_count = 0;
	g_decoration_update_count = 0;
	g_focus_view_count = 0;
	g_last_focused_view = NULL;
}

static struct wm_view
make_view(int idx, const char *title)
{
	test_server.style = &test_style;

	/* Wire up xdg surface chain */
	test_surfaces[idx].dummy = 0;
	test_xdg_surfaces[idx].surface = &test_surfaces[idx];
	test_toplevels[idx].base = &test_xdg_surfaces[idx];
	test_toplevels[idx].width = 800;
	test_toplevels[idx].height = 600;
	test_trees[idx].node.enabled = 1;
	test_trees[idx].node.x = 100;
	test_trees[idx].node.y = 100;

	struct wm_view view;
	memset(&view, 0, sizeof(view));
	view.server = &test_server;
	view.xdg_toplevel = &test_toplevels[idx];
	view.scene_tree = &test_trees[idx];
	view.id = idx + 1;
	view.x = 100;
	view.y = 100;
	view.title = (char *)title;
	view.tab_group = NULL;
	view.decoration = NULL;
	wl_list_init(&view.tab_link);
	return view;
}

/* Count views in a group by walking the list */
static int
count_views_in_list(struct wm_tab_group *group)
{
	int count = 0;
	struct wl_list *pos;
	for (pos = group->views.next; pos != &group->views; pos = pos->next) {
		count++;
	}
	return count;
}

/* Get the nth view in the group list (0-based) */
static struct wm_view *
get_view_at(struct wm_tab_group *group, int idx)
{
	struct wl_list *pos = group->views.next;
	for (int i = 0; i < idx && pos != &group->views; i++) {
		pos = pos->next;
	}
	if (pos == &group->views) return NULL;
	struct wm_view *v;
	v = wl_container_of(pos, v, tab_link);
	return v;
}

/* ===== Tests ===== */

static void
test_create_basic(void)
{
	reset_globals();
	struct wm_view v = make_view(0, "view1");

	struct wm_tab_group *group = wm_tab_group_create(&v);
	assert(group != NULL);
	assert(group->count == 1);
	assert(group->active_view == &v);
	assert(group->server == &test_server);
	assert(v.tab_group == group);
	assert(count_views_in_list(group) == 1);

	/* Clean up */
	wm_tab_group_destroy(group);
	printf("  PASS: create_basic\n");
}

static void
test_create_sets_link(void)
{
	reset_globals();
	struct wm_view v = make_view(0, "view1");

	struct wm_tab_group *group = wm_tab_group_create(&v);
	assert(group != NULL);

	/* The view should be linked into the group's list */
	struct wm_view *first = wl_container_of(group->views.next, first, tab_link);
	assert(first == &v);

	wm_tab_group_destroy(group);
	printf("  PASS: create_sets_link\n");
}

static void
test_destroy_null(void)
{
	/* Should not crash */
	wm_tab_group_destroy(NULL);
	printf("  PASS: destroy_null\n");
}

static void
test_destroy_detaches_views(void)
{
	reset_globals();
	struct wm_view v1 = make_view(0, "v1");
	struct wm_view v2 = make_view(1, "v2");

	struct wm_tab_group *group = wm_tab_group_create(&v1);
	wm_tab_group_add(group, &v2);

	assert(v1.tab_group == group);
	assert(v2.tab_group == group);

	wm_tab_group_destroy(group);

	/* Both views should be detached */
	assert(v1.tab_group == NULL);
	assert(v2.tab_group == NULL);
	/* Tab links should be re-initialized (self-referencing) */
	assert(v1.tab_link.next == &v1.tab_link);
	assert(v2.tab_link.next == &v2.tab_link);
	printf("  PASS: destroy_detaches_views\n");
}

static void
test_add_increments_count(void)
{
	reset_globals();
	struct wm_view v1 = make_view(0, "v1");
	struct wm_view v2 = make_view(1, "v2");
	struct wm_view v3 = make_view(2, "v3");

	struct wm_tab_group *group = wm_tab_group_create(&v1);
	assert(group->count == 1);

	wm_tab_group_add(group, &v2);
	assert(group->count == 2);
	assert(v2.tab_group == group);
	assert(count_views_in_list(group) == 2);

	wm_tab_group_add(group, &v3);
	assert(group->count == 3);
	assert(v3.tab_group == group);
	assert(count_views_in_list(group) == 3);

	wm_tab_group_destroy(group);
	printf("  PASS: add_increments_count\n");
}

static void
test_add_hides_new_view(void)
{
	reset_globals();
	struct wm_view v1 = make_view(0, "v1");
	struct wm_view v2 = make_view(1, "v2");

	struct wm_tab_group *group = wm_tab_group_create(&v1);
	reset_globals();

	wm_tab_group_add(group, &v2);

	/* New view should be hidden (set_enabled(false)) */
	assert(v2.scene_tree->node.enabled == 0);

	wm_tab_group_destroy(group);
	printf("  PASS: add_hides_new_view\n");
}

static void
test_add_syncs_geometry(void)
{
	reset_globals();
	struct wm_view v1 = make_view(0, "v1");
	struct wm_view v2 = make_view(1, "v2");
	v2.x = 500;
	v2.y = 500;

	struct wm_tab_group *group = wm_tab_group_create(&v1);
	reset_globals();

	wm_tab_group_add(group, &v2);

	/* v2 should be synced to v1's position */
	assert(v2.x == v1.x);
	assert(v2.y == v1.y);
	/* Size should be set via wlr_xdg_toplevel_set_size */
	assert(g_set_size_count > 0);

	wm_tab_group_destroy(group);
	printf("  PASS: add_syncs_geometry\n");
}

static void
test_add_updates_decoration(void)
{
	reset_globals();
	struct wm_view v1 = make_view(0, "v1");
	struct wm_view v2 = make_view(1, "v2");
	struct wm_decoration deco;
	v1.decoration = &deco;

	struct wm_tab_group *group = wm_tab_group_create(&v1);
	reset_globals();

	wm_tab_group_add(group, &v2);

	/* Decoration should be updated */
	assert(g_decoration_update_count == 1);

	wm_tab_group_destroy(group);
	printf("  PASS: add_updates_decoration\n");
}

static void
test_remove_decrements_count(void)
{
	reset_globals();
	struct wm_view v1 = make_view(0, "v1");
	struct wm_view v2 = make_view(1, "v2");
	struct wm_view v3 = make_view(2, "v3");

	struct wm_tab_group *group = wm_tab_group_create(&v1);
	wm_tab_group_add(group, &v2);
	wm_tab_group_add(group, &v3);
	assert(group->count == 3);

	/* Remove non-active view; group dissolves to 2 but stays */
	wm_tab_group_remove(&v3);
	assert(v3.tab_group == NULL);
	assert(group->count == 2);

	wm_tab_group_destroy(group);
	printf("  PASS: remove_decrements_count\n");
}

static void
test_remove_offsets_position(void)
{
	reset_globals();
	struct wm_view v1 = make_view(0, "v1");
	struct wm_view v2 = make_view(1, "v2");
	struct wm_view v3 = make_view(2, "v3");

	struct wm_tab_group *group = wm_tab_group_create(&v1);
	wm_tab_group_add(group, &v2);
	wm_tab_group_add(group, &v3);

	int old_x = v3.x;
	int old_y = v3.y;
	wm_tab_group_remove(&v3);

	/* Removed view should be offset by 20 pixels */
	assert(v3.x == old_x + 20);
	assert(v3.y == old_y + 20);

	wm_tab_group_destroy(group);
	printf("  PASS: remove_offsets_position\n");
}

static void
test_remove_shows_view(void)
{
	reset_globals();
	struct wm_view v1 = make_view(0, "v1");
	struct wm_view v2 = make_view(1, "v2");
	struct wm_view v3 = make_view(2, "v3");

	struct wm_tab_group *group = wm_tab_group_create(&v1);
	wm_tab_group_add(group, &v2);
	wm_tab_group_add(group, &v3);

	/* v3 is hidden after add */
	assert(v3.scene_tree->node.enabled == 0);

	wm_tab_group_remove(&v3);

	/* Removed view should be visible again */
	assert(v3.scene_tree->node.enabled == 1);

	wm_tab_group_destroy(group);
	printf("  PASS: remove_shows_view\n");
}

static void
test_remove_active_activates_next(void)
{
	reset_globals();
	struct wm_view v1 = make_view(0, "v1");
	struct wm_view v2 = make_view(1, "v2");
	struct wm_view v3 = make_view(2, "v3");

	struct wm_tab_group *group = wm_tab_group_create(&v1);
	wm_tab_group_add(group, &v2);
	wm_tab_group_add(group, &v3);

	/* v1 is active; remove it */
	wm_tab_group_remove(&v1);

	/* The group should have activated the next view from the list.
	 * Since v1 was removed and count went to 2, one of the remaining
	 * views should now be active. The group now dissolves at count==1
	 * only if we remove one more. */
	assert(v1.tab_group == NULL);
	assert(group->count == 2);
	assert(group->active_view != NULL);
	/* active_view should be shown */
	assert(group->active_view->scene_tree->node.enabled == 1);

	wm_tab_group_destroy(group);
	printf("  PASS: remove_active_activates_next\n");
}

static void
test_remove_dissolves_at_one(void)
{
	reset_globals();
	struct wm_view v1 = make_view(0, "v1");
	struct wm_view v2 = make_view(1, "v2");

	struct wm_tab_group *group = wm_tab_group_create(&v1);
	wm_tab_group_add(group, &v2);
	assert(group->count == 2);

	/* Remove v2; group count goes to 1, should auto-dissolve */
	wm_tab_group_remove(&v2);

	/* Both views should now be detached (group dissolved at count=1) */
	assert(v2.tab_group == NULL);
	assert(v1.tab_group == NULL);
	/* v1 should be visible */
	assert(v1.scene_tree->node.enabled == 1);
	printf("  PASS: remove_dissolves_at_one\n");
}

static void
test_remove_no_group(void)
{
	reset_globals();
	struct wm_view v = make_view(0, "v");
	v.tab_group = NULL;

	/* Should be a no-op */
	wm_tab_group_remove(&v);
	assert(v.tab_group == NULL);
	printf("  PASS: remove_no_group\n");
}

static void
test_activate_switches_view(void)
{
	reset_globals();
	struct wm_view v1 = make_view(0, "v1");
	struct wm_view v2 = make_view(1, "v2");

	struct wm_tab_group *group = wm_tab_group_create(&v1);
	wm_tab_group_add(group, &v2);

	assert(group->active_view == &v1);
	assert(v1.scene_tree->node.enabled == 1);
	assert(v2.scene_tree->node.enabled == 0);

	reset_globals();
	wm_tab_group_activate(group, &v2);

	assert(group->active_view == &v2);
	/* Old view hidden, new view shown */
	assert(v1.scene_tree->node.enabled == 0);
	assert(v2.scene_tree->node.enabled == 1);
	/* Focus should be called */
	assert(g_focus_view_count == 1);
	assert(g_last_focused_view == &v2);

	wm_tab_group_destroy(group);
	printf("  PASS: activate_switches_view\n");
}

static void
test_activate_same_is_noop(void)
{
	reset_globals();
	struct wm_view v1 = make_view(0, "v1");
	struct wm_view v2 = make_view(1, "v2");

	struct wm_tab_group *group = wm_tab_group_create(&v1);
	wm_tab_group_add(group, &v2);

	reset_globals();
	wm_tab_group_activate(group, &v1);

	/* Should be a no-op since v1 is already active */
	assert(g_focus_view_count == 0);
	assert(g_set_enabled_count == 0);

	wm_tab_group_destroy(group);
	printf("  PASS: activate_same_is_noop\n");
}

static void
test_activate_syncs_position(void)
{
	reset_globals();
	struct wm_view v1 = make_view(0, "v1");
	struct wm_view v2 = make_view(1, "v2");
	v1.x = 200;
	v1.y = 300;

	struct wm_tab_group *group = wm_tab_group_create(&v1);
	wm_tab_group_add(group, &v2);

	wm_tab_group_activate(group, &v2);

	/* v2 should get v1's position */
	assert(v2.x == 200);
	assert(v2.y == 300);

	wm_tab_group_destroy(group);
	printf("  PASS: activate_syncs_position\n");
}

static void
test_next_cycles_forward(void)
{
	reset_globals();
	struct wm_view v1 = make_view(0, "v1");
	struct wm_view v2 = make_view(1, "v2");
	struct wm_view v3 = make_view(2, "v3");

	struct wm_tab_group *group = wm_tab_group_create(&v1);
	wm_tab_group_add(group, &v2);
	wm_tab_group_add(group, &v3);
	/* List order after inserts: v1 -> v3 -> v2 (add inserts after active) */
	/* Actually: create puts v1 at head. add inserts after active_view.
	 * After add(v2): v1 -> v2 (v2 inserted after v1)
	 * After add(v3): v1 -> v3 -> v2 (v3 inserted after active v1)
	 */

	assert(group->active_view == &v1);

	wm_tab_group_next(group);
	/* Next after v1 in list */
	struct wm_view *active = group->active_view;
	assert(active != &v1);

	wm_tab_group_destroy(group);
	printf("  PASS: next_cycles_forward\n");
}

static void
test_next_wraps_around(void)
{
	reset_globals();
	struct wm_view v1 = make_view(0, "v1");
	struct wm_view v2 = make_view(1, "v2");

	struct wm_tab_group *group = wm_tab_group_create(&v1);
	wm_tab_group_add(group, &v2);

	/* With 2 views, cycling next twice should return to original */
	wm_tab_group_next(group);
	assert(group->active_view == &v2);

	wm_tab_group_next(group);
	assert(group->active_view == &v1);

	wm_tab_group_destroy(group);
	printf("  PASS: next_wraps_around\n");
}

static void
test_next_single_view_noop(void)
{
	reset_globals();
	struct wm_view v1 = make_view(0, "v1");

	struct wm_tab_group *group = wm_tab_group_create(&v1);
	reset_globals();

	wm_tab_group_next(group);

	/* Single view, should be no-op */
	assert(group->active_view == &v1);
	assert(g_focus_view_count == 0);

	wm_tab_group_destroy(group);
	printf("  PASS: next_single_view_noop\n");
}

static void
test_prev_cycles_backward(void)
{
	reset_globals();
	struct wm_view v1 = make_view(0, "v1");
	struct wm_view v2 = make_view(1, "v2");

	struct wm_tab_group *group = wm_tab_group_create(&v1);
	wm_tab_group_add(group, &v2);

	wm_tab_group_prev(group);
	assert(group->active_view == &v2);

	wm_tab_group_prev(group);
	assert(group->active_view == &v1);

	wm_tab_group_destroy(group);
	printf("  PASS: prev_cycles_backward\n");
}

static void
test_prev_single_view_noop(void)
{
	reset_globals();
	struct wm_view v1 = make_view(0, "v1");

	struct wm_tab_group *group = wm_tab_group_create(&v1);
	reset_globals();

	wm_tab_group_prev(group);

	assert(group->active_view == &v1);
	assert(g_focus_view_count == 0);

	wm_tab_group_destroy(group);
	printf("  PASS: prev_single_view_noop\n");
}

static void
test_next_prev_inverse(void)
{
	reset_globals();
	struct wm_view v1 = make_view(0, "v1");
	struct wm_view v2 = make_view(1, "v2");
	struct wm_view v3 = make_view(2, "v3");

	struct wm_tab_group *group = wm_tab_group_create(&v1);
	wm_tab_group_add(group, &v2);
	wm_tab_group_add(group, &v3);

	/* next then prev should return to same view */
	struct wm_view *original = group->active_view;
	wm_tab_group_next(group);
	wm_tab_group_prev(group);
	assert(group->active_view == original);

	wm_tab_group_destroy(group);
	printf("  PASS: next_prev_inverse\n");
}

static void
test_move_right_reorders(void)
{
	reset_globals();
	struct wm_view v1 = make_view(0, "v1");
	struct wm_view v2 = make_view(1, "v2");
	struct wm_view v3 = make_view(2, "v3");

	struct wm_tab_group *group = wm_tab_group_create(&v1);
	wm_tab_group_add(group, &v2);
	wm_tab_group_add(group, &v3);

	/* Record initial order */
	struct wm_view *first_before = get_view_at(group, 0);

	wm_tab_group_move_right(group, first_before);

	/* The moved view should no longer be first */
	struct wm_view *first_after = get_view_at(group, 0);
	assert(first_after != first_before);

	/* List should still have 3 views */
	assert(count_views_in_list(group) == 3);

	wm_tab_group_destroy(group);
	printf("  PASS: move_right_reorders\n");
}

static void
test_move_right_single_noop(void)
{
	reset_globals();
	struct wm_view v1 = make_view(0, "v1");

	struct wm_tab_group *group = wm_tab_group_create(&v1);

	/* Should be no-op with single view */
	wm_tab_group_move_right(group, &v1);
	assert(count_views_in_list(group) == 1);

	wm_tab_group_destroy(group);
	printf("  PASS: move_right_single_noop\n");
}

static void
test_move_left_reorders(void)
{
	reset_globals();
	struct wm_view v1 = make_view(0, "v1");
	struct wm_view v2 = make_view(1, "v2");
	struct wm_view v3 = make_view(2, "v3");

	struct wm_tab_group *group = wm_tab_group_create(&v1);
	wm_tab_group_add(group, &v2);
	wm_tab_group_add(group, &v3);

	struct wm_view *last = get_view_at(group, 2);

	wm_tab_group_move_left(group, last);

	/* The moved view should no longer be last */
	struct wm_view *new_last = get_view_at(group, 2);
	assert(new_last != last);

	assert(count_views_in_list(group) == 3);

	wm_tab_group_destroy(group);
	printf("  PASS: move_left_reorders\n");
}

static void
test_move_left_single_noop(void)
{
	reset_globals();
	struct wm_view v1 = make_view(0, "v1");

	struct wm_tab_group *group = wm_tab_group_create(&v1);

	wm_tab_group_move_left(group, &v1);
	assert(count_views_in_list(group) == 1);

	wm_tab_group_destroy(group);
	printf("  PASS: move_left_single_noop\n");
}

static void
test_move_right_wraps(void)
{
	reset_globals();
	struct wm_view v1 = make_view(0, "v1");
	struct wm_view v2 = make_view(1, "v2");

	struct wm_tab_group *group = wm_tab_group_create(&v1);
	wm_tab_group_add(group, &v2);

	/* Move last view right (should wrap to beginning area) */
	struct wm_view *last = get_view_at(group, 1);
	wm_tab_group_move_right(group, last);

	assert(count_views_in_list(group) == 2);

	wm_tab_group_destroy(group);
	printf("  PASS: move_right_wraps\n");
}

static void
test_move_left_wraps(void)
{
	reset_globals();
	struct wm_view v1 = make_view(0, "v1");
	struct wm_view v2 = make_view(1, "v2");

	struct wm_tab_group *group = wm_tab_group_create(&v1);
	wm_tab_group_add(group, &v2);

	/* Move first view left (should wrap to end area) */
	struct wm_view *first = get_view_at(group, 0);
	wm_tab_group_move_left(group, first);

	assert(count_views_in_list(group) == 2);

	wm_tab_group_destroy(group);
	printf("  PASS: move_left_wraps\n");
}

static void
test_add_view_already_in_group(void)
{
	reset_globals();
	struct wm_view v1 = make_view(0, "v1");
	struct wm_view v2 = make_view(1, "v2");
	struct wm_view v3 = make_view(2, "v3");

	struct wm_tab_group *group1 = wm_tab_group_create(&v1);
	wm_tab_group_add(group1, &v2);
	assert(group1->count == 2);

	/* Create a second group */
	struct wm_tab_group *group2 = wm_tab_group_create(&v3);

	/* Adding v2 (already in group1) to group2 should remove it from group1 first.
	 * But group1 will dissolve since it goes to count=1. */
	wm_tab_group_add(group2, &v2);

	assert(v2.tab_group == group2);
	assert(group2->count == 2);
	/* group1 dissolved (v1 detached) */
	assert(v1.tab_group == NULL);

	wm_tab_group_destroy(group2);
	printf("  PASS: add_view_already_in_group\n");
}

static void
test_remove_last_destroys_group(void)
{
	reset_globals();
	struct wm_view v1 = make_view(0, "v1");

	struct wm_tab_group *group = wm_tab_group_create(&v1);
	(void)group; /* group gets freed inside remove */

	wm_tab_group_remove(&v1);

	/* Group should be destroyed, view detached */
	assert(v1.tab_group == NULL);
	assert(v1.scene_tree->node.enabled == 1);
	printf("  PASS: remove_last_destroys_group\n");
}

static void
test_full_lifecycle(void)
{
	reset_globals();
	struct wm_view v1 = make_view(0, "v1");
	struct wm_view v2 = make_view(1, "v2");
	struct wm_view v3 = make_view(2, "v3");

	/* Create group with v1 */
	struct wm_tab_group *group = wm_tab_group_create(&v1);
	assert(group->count == 1);

	/* Add v2 and v3 */
	wm_tab_group_add(group, &v2);
	wm_tab_group_add(group, &v3);
	assert(group->count == 3);

	/* Cycle through tabs */
	wm_tab_group_next(group);
	struct wm_view *second = group->active_view;
	assert(second != &v1);

	wm_tab_group_next(group);
	wm_tab_group_next(group);
	/* Should wrap back to v1 eventually (3 nexts from v1) */

	/* Activate specific view */
	wm_tab_group_activate(group, &v2);
	assert(group->active_view == &v2);

	/* Remove v2 (active) */
	wm_tab_group_remove(&v2);
	assert(v2.tab_group == NULL);
	assert(group->count == 2);
	assert(group->active_view != NULL);
	assert(group->active_view != &v2);

	/* Remove one more -> dissolves at count=1 */
	struct wm_view *remaining = group->active_view;
	struct wm_view *other = (remaining == &v1) ? &v3 : &v1;
	wm_tab_group_remove(other);

	/* Group should be dissolved */
	assert(other->tab_group == NULL);
	assert(remaining->tab_group == NULL);

	printf("  PASS: full_lifecycle\n");
}

int
main(void)
{
	printf("test_tabgroup:\n");

	test_create_basic();
	test_create_sets_link();
	test_destroy_null();
	test_destroy_detaches_views();
	test_add_increments_count();
	test_add_hides_new_view();
	test_add_syncs_geometry();
	test_add_updates_decoration();
	test_remove_decrements_count();
	test_remove_offsets_position();
	test_remove_shows_view();
	test_remove_active_activates_next();
	test_remove_dissolves_at_one();
	test_remove_no_group();
	test_activate_switches_view();
	test_activate_same_is_noop();
	test_activate_syncs_position();
	test_next_cycles_forward();
	test_next_wraps_around();
	test_next_single_view_noop();
	test_prev_cycles_backward();
	test_prev_single_view_noop();
	test_next_prev_inverse();
	test_move_right_reorders();
	test_move_right_single_noop();
	test_move_left_reorders();
	test_move_left_single_noop();
	test_move_right_wraps();
	test_move_left_wraps();
	test_add_view_already_in_group();
	test_remove_last_destroys_group();
	test_full_lifecycle();

	printf("All tabgroup tests passed.\n");
	return 0;
}
