/*
 * test_placement.c - Unit tests for window placement and edge snapping
 *
 * Includes placement.c directly with stub implementations to avoid
 * needing wlroots/wayland libraries at link time. Header guards block
 * the real wayland/wlroots headers so our stub types are used instead.
 *
 * Tests the ACTUAL functions in placement.c: cascade, smart, center,
 * under-mouse, snap edges, overlap detection, and arrangement actions.
 */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- Block real headers via their include guards --- */
#define WAYLAND_SERVER_CORE_H
#define WAYLAND_SERVER_H
#define WAYLAND_UTIL_H
#define WLR_UTIL_LOG_H
#define WLR_UTIL_BOX_H
#define WLR_TYPES_WLR_CURSOR_H
#define WLR_TYPES_WLR_OUTPUT_LAYOUT_H
#define WLR_TYPES_WLR_XDG_SHELL_H
#define WLR_TYPES_WLR_SCENE_H
#define WLR_TYPES_WLR_OUTPUT_H
#define WM_CONFIG_H
#define WM_OUTPUT_H
#define WM_PLACEMENT_H
#define WM_SERVER_H
#define WM_VIEW_H
#define WM_WORKSPACE_H

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

#define wl_list_for_each(pos, head, member)				\
	for (pos = (void *)((char *)((head)->next)			\
		- offsetof(__typeof__(*pos), member));			\
	     &(pos)->member != (head);					\
	     pos = (void *)((char *)((pos)->member.next)		\
		- offsetof(__typeof__(*pos), member)))

/* --- Stub wlr types --- */

struct wlr_box {
	int x, y, width, height;
};

struct wlr_output {
	int dummy;
};

struct wlr_output_layout {
	int dummy;
};

struct wlr_surface {
	int dummy;
};

struct wlr_scene_node {
	int enabled;
	int x, y;
};

struct wlr_scene_tree {
	struct wlr_scene_node node;
};

struct wlr_xdg_surface {
	struct wlr_surface *surface;
};

struct wlr_xdg_toplevel {
	struct wlr_xdg_surface *base;
	int width, height;
};

struct wlr_cursor {
	double x, y;
};

/* wlr_log no-op */
#define wlr_log(verb, fmt, ...) ((void)0)
#define WLR_ERROR 1
#define WLR_DEBUG 7

/* --- Stub wm types --- */

/* Forward declarations */
struct wm_view;
struct wm_workspace;
struct wm_tab_group;
struct wm_animation;
struct wm_decoration;
struct wm_config;

enum wm_placement_policy {
	WM_PLACEMENT_ROW_SMART,
	WM_PLACEMENT_COL_SMART,
	WM_PLACEMENT_CASCADE,
	WM_PLACEMENT_UNDER_MOUSE,
	WM_PLACEMENT_ROW_MIN_OVERLAP,
	WM_PLACEMENT_COL_MIN_OVERLAP,
};

struct wm_config {
	enum wm_placement_policy placement_policy;
	bool row_right_to_left;
	bool col_bottom_to_top;
	int edge_snap_threshold;
};

struct wm_output {
	struct wl_list link;
	struct wlr_output *wlr_output;
	struct wlr_box usable_area;
};

struct wm_server {
	struct wl_list views;
	struct wl_list outputs;
	struct wlr_output_layout *output_layout;
	struct wlr_cursor *cursor;
	struct wm_config *config;
	struct wm_workspace *current_workspace;
};

struct wm_workspace {
	int index;
};

struct wm_view {
	struct wl_list link;
	struct wm_server *server;
	struct wlr_xdg_toplevel *xdg_toplevel;
	struct wlr_scene_tree *scene_tree;
	uint32_t id;
	int x, y;
	struct wlr_box saved_geometry;
	bool maximized, fullscreen;
	bool sticky;
	char *title;
	char *app_id;
	struct wm_workspace *workspace;
	struct wm_tab_group *tab_group;
	struct wl_list tab_link;
	struct wm_decoration *decoration;
};

/* --- Stub tracking globals --- */

static int g_set_size_count;
static int g_set_position_count;
static struct wlr_box g_stub_xdg_geo; /* geometry returned by stub */

/* Output returned by wlr_output_layout_output_at */
static struct wlr_output *g_output_at_result;
/* Box returned by wlr_output_layout_get_box */
static struct wlr_box g_layout_box;
/* Geometry returned by wm_view_get_geometry */
static struct wlr_box g_view_geo_override;
static bool g_use_view_geo_override;

/* --- Stub wlroots functions --- */

static struct wlr_output *
wlr_output_layout_output_at(struct wlr_output_layout *layout,
	double lx, double ly)
{
	(void)layout;
	(void)lx;
	(void)ly;
	return g_output_at_result;
}

static void
wlr_output_layout_get_box(struct wlr_output_layout *layout,
	struct wlr_output *output, struct wlr_box *box)
{
	(void)layout;
	(void)output;
	*box = g_layout_box;
}

static void
wlr_xdg_surface_get_geometry(struct wlr_xdg_surface *surface,
	struct wlr_box *box)
{
	(void)surface;
	*box = g_stub_xdg_geo;
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

static void
wlr_scene_node_set_position(struct wlr_scene_node *node, int x, int y)
{
	if (node) {
		node->x = x;
		node->y = y;
	}
	g_set_position_count++;
}

/* --- Stub fluxland functions --- */

static void
wm_view_get_geometry(struct wm_view *view, struct wlr_box *box)
{
	if (g_use_view_geo_override) {
		*box = g_view_geo_override;
		return;
	}
	/* Default: use view position + xdg geometry size */
	box->x = view->x;
	box->y = view->y;
	box->width = g_stub_xdg_geo.width;
	box->height = g_stub_xdg_geo.height;
}

/* Active workspace stub */
static struct wm_workspace *g_active_workspace;

static struct wm_workspace *
wm_workspace_get_active(struct wm_server *server)
{
	(void)server;
	return g_active_workspace;
}

/* --- Include placement source directly --- */

#include "../src/placement.c"

/* --- Test helpers --- */

static struct wm_server test_server;
static struct wm_config test_config;
static struct wlr_output_layout test_layout;
static struct wlr_cursor test_cursor;
static struct wlr_output test_wlr_output;
static struct wm_output test_output;
static struct wm_workspace test_workspace;

/* View + backing objects */
#define MAX_TEST_VIEWS 8
static struct wm_view test_views[MAX_TEST_VIEWS];
static struct wlr_scene_tree test_trees[MAX_TEST_VIEWS];
static struct wlr_xdg_toplevel test_toplevels[MAX_TEST_VIEWS];
static struct wlr_xdg_surface test_xdg_surfaces[MAX_TEST_VIEWS];

static void
reset_globals(void)
{
	g_set_size_count = 0;
	g_set_position_count = 0;
	g_use_view_geo_override = false;
	memset(&g_view_geo_override, 0, sizeof(g_view_geo_override));

	/* Reset static cascade position in placement.c */
	cascade_x = 0;
	cascade_y = 0;
}

static void
setup(void)
{
	reset_globals();

	memset(&test_server, 0, sizeof(test_server));
	memset(&test_config, 0, sizeof(test_config));
	memset(&test_layout, 0, sizeof(test_layout));
	memset(&test_cursor, 0, sizeof(test_cursor));
	memset(&test_wlr_output, 0, sizeof(test_wlr_output));
	memset(&test_output, 0, sizeof(test_output));
	memset(&test_workspace, 0, sizeof(test_workspace));
	memset(test_views, 0, sizeof(test_views));
	memset(test_trees, 0, sizeof(test_trees));
	memset(test_toplevels, 0, sizeof(test_toplevels));
	memset(test_xdg_surfaces, 0, sizeof(test_xdg_surfaces));

	/* Wire up server */
	wl_list_init(&test_server.views);
	wl_list_init(&test_server.outputs);
	test_server.output_layout = &test_layout;
	test_server.cursor = &test_cursor;
	test_server.config = &test_config;
	test_server.current_workspace = &test_workspace;

	/* Config defaults */
	test_config.placement_policy = WM_PLACEMENT_ROW_SMART;
	test_config.row_right_to_left = false;
	test_config.col_bottom_to_top = false;
	test_config.edge_snap_threshold = 10;

	/* Output: 1920x1080 at (0,0) */
	test_output.wlr_output = &test_wlr_output;
	test_output.usable_area = (struct wlr_box){ 0, 0, 1920, 1080 };
	wl_list_insert(&test_server.outputs, &test_output.link);

	/* Layout box fallback */
	g_layout_box = (struct wlr_box){ 0, 0, 1920, 1080 };
	g_output_at_result = &test_wlr_output;

	/* Default XDG geometry: 400x300 */
	g_stub_xdg_geo = (struct wlr_box){ 0, 0, 400, 300 };

	/* Active workspace */
	g_active_workspace = &test_workspace;

	/* Cursor at center of screen */
	test_cursor.x = 960.0;
	test_cursor.y = 540.0;
}

static void
init_view(int idx)
{
	struct wm_view *v = &test_views[idx];
	test_xdg_surfaces[idx].surface = NULL;
	test_toplevels[idx].base = &test_xdg_surfaces[idx];
	test_toplevels[idx].width = 400;
	test_toplevels[idx].height = 300;
	test_trees[idx].node.enabled = 1;
	test_trees[idx].node.x = 0;
	test_trees[idx].node.y = 0;

	v->server = &test_server;
	v->xdg_toplevel = &test_toplevels[idx];
	v->scene_tree = &test_trees[idx];
	v->id = idx + 1;
	v->x = 0;
	v->y = 0;
	v->workspace = &test_workspace;
	v->sticky = false;
	v->fullscreen = false;
}

/* Add a view to the server's view list */
static void
add_view_to_server(int idx)
{
	init_view(idx);
	wl_list_insert(&test_server.views, &test_views[idx].link);
}

/* ===== Tests ===== */

/*
 * Test: place_cascade positions first window and advances
 */
static void
test_cascade_basic(void)
{
	setup();
	init_view(0);
	test_views[0].x = 0;
	test_views[0].y = 0;

	struct wlr_box area = { 0, 0, 1920, 1080 };

	/* cascade_x/y are 0, area starts at 0 too, so 0 is NOT < 0.
	 * The window fits (0+400 <= 1920, 0+300 <= 1080), so no reset.
	 * First window placed at (0,0), then cascade advances to (30,30). */
	place_cascade(&test_server, &test_views[0], &area);

	assert(test_views[0].x == 0);
	assert(test_views[0].y == 0);

	/* Second call: position is now (30,30) */
	init_view(1);
	place_cascade(&test_server, &test_views[1], &area);

	assert(test_views[1].x == CASCADE_STEP_X);
	assert(test_views[1].y == CASCADE_STEP_Y);

	/* Third call: (60,60) */
	init_view(2);
	place_cascade(&test_server, &test_views[2], &area);

	assert(test_views[2].x == 2 * CASCADE_STEP_X);
	assert(test_views[2].y == 2 * CASCADE_STEP_Y);
	printf("  PASS: test_cascade_basic\n");
}

/*
 * Test: cascade wraps back when window would exceed area bounds
 */
static void
test_cascade_wrapping(void)
{
	setup();

	struct wlr_box area = { 0, 0, 1920, 1080 };

	/* Force cascade position near the edge so the next window won't fit */
	cascade_x = 1920 - 400 + 1; /* beyond where a 400px window fits */
	cascade_y = 100;

	init_view(0);
	place_cascade(&test_server, &test_views[0], &area);

	/* Should have wrapped back to STEP offset */
	assert(test_views[0].x == CASCADE_STEP_X);
	assert(test_views[0].y == CASCADE_STEP_Y);
	printf("  PASS: test_cascade_wrapping\n");
}

/*
 * Test: cascade wraps on Y overflow too
 */
static void
test_cascade_wrapping_y(void)
{
	setup();

	struct wlr_box area = { 0, 0, 1920, 1080 };

	/* X fits but Y overflows */
	cascade_x = 100;
	cascade_y = 1080 - 300 + 1; /* beyond where a 300px window fits */

	init_view(0);
	place_cascade(&test_server, &test_views[0], &area);

	assert(test_views[0].x == CASCADE_STEP_X);
	assert(test_views[0].y == CASCADE_STEP_Y);
	printf("  PASS: test_cascade_wrapping_y\n");
}

/*
 * Test: place_under_mouse centers window on cursor position
 */
static void
test_under_mouse_center(void)
{
	setup();
	init_view(0);

	test_cursor.x = 960.0;
	test_cursor.y = 540.0;

	struct wlr_box area = { 0, 0, 1920, 1080 };
	place_under_mouse(&test_server, &test_views[0], &area);

	/* Centered on cursor: 960 - 200 = 760, 540 - 150 = 390 */
	assert(test_views[0].x == 760);
	assert(test_views[0].y == 390);
	printf("  PASS: test_under_mouse_center\n");
}

/*
 * Test: place_under_mouse clamps to left/top edge
 */
static void
test_under_mouse_clamp_topleft(void)
{
	setup();
	init_view(0);

	test_cursor.x = 10.0;
	test_cursor.y = 10.0;

	struct wlr_box area = { 0, 0, 1920, 1080 };
	place_under_mouse(&test_server, &test_views[0], &area);

	/* Would be 10-200=-190 and 10-150=-140, clamped to 0 */
	assert(test_views[0].x == 0);
	assert(test_views[0].y == 0);
	printf("  PASS: test_under_mouse_clamp_topleft\n");
}

/*
 * Test: place_under_mouse clamps to right/bottom edge
 */
static void
test_under_mouse_clamp_bottomright(void)
{
	setup();
	init_view(0);

	test_cursor.x = 1910.0;
	test_cursor.y = 1070.0;

	struct wlr_box area = { 0, 0, 1920, 1080 };
	place_under_mouse(&test_server, &test_views[0], &area);

	/* x = 1910 - 200 = 1710, but 1710 + 400 = 2110 > 1920, so clamped */
	assert(test_views[0].x == 1920 - 400);
	assert(test_views[0].y == 1080 - 300);
	printf("  PASS: test_under_mouse_clamp_bottomright\n");
}

/*
 * Test: overlaps_any returns false when no other views exist
 */
static void
test_overlaps_any_empty(void)
{
	setup();
	init_view(0);
	/* No views in server list */

	bool result = overlaps_any(&test_server, &test_views[0],
		100, 100, 400, 300);
	assert(!result);
	printf("  PASS: test_overlaps_any_empty\n");
}

/*
 * Test: overlaps_any detects collision with existing view
 */
static void
test_overlaps_any_collision(void)
{
	setup();

	/* Add a view at (100, 100) with size 400x300 */
	add_view_to_server(0);
	test_views[0].x = 100;
	test_views[0].y = 100;

	init_view(1);
	test_views[1].workspace = &test_workspace;

	/* Test overlapping position */
	bool result = overlaps_any(&test_server, &test_views[1],
		200, 200, 400, 300);
	assert(result);

	/* Test non-overlapping position (completely to the right) */
	result = overlaps_any(&test_server, &test_views[1],
		500, 100, 400, 300);
	assert(!result);

	printf("  PASS: test_overlaps_any_collision\n");
}

/*
 * Test: overlaps_any skips views on different workspaces (non-sticky)
 */
static void
test_overlaps_any_different_workspace(void)
{
	setup();

	struct wm_workspace other_ws;
	memset(&other_ws, 0, sizeof(other_ws));
	other_ws.index = 1;

	add_view_to_server(0);
	test_views[0].x = 100;
	test_views[0].y = 100;
	test_views[0].workspace = &other_ws; /* different workspace */

	init_view(1);
	test_views[1].workspace = &test_workspace;

	/* Should NOT overlap because view 0 is on a different workspace */
	bool result = overlaps_any(&test_server, &test_views[1],
		100, 100, 400, 300);
	assert(!result);

	/* But if view 0 is sticky, it should overlap */
	test_views[0].sticky = true;
	result = overlaps_any(&test_server, &test_views[1],
		100, 100, 400, 300);
	assert(result);

	printf("  PASS: test_overlaps_any_different_workspace\n");
}

/*
 * Test: overlap_area computes correct intersection area
 */
static void
test_overlap_area_calculation(void)
{
	setup();

	/* Place a 400x300 view at (0,0) */
	add_view_to_server(0);
	test_views[0].x = 0;
	test_views[0].y = 0;

	init_view(1);
	test_views[1].workspace = &test_workspace;

	/* Candidate at (200,150) should overlap by 200*150 = 30000 */
	int area = overlap_area(&test_server, &test_views[1],
		200, 150, 400, 300);
	assert(area == 200 * 150);

	/* No overlap at all (completely to the right) */
	area = overlap_area(&test_server, &test_views[1],
		400, 0, 400, 300);
	assert(area == 0);

	printf("  PASS: test_overlap_area_calculation\n");
}

/*
 * Test: place_row_smart finds gap when space exists
 */
static void
test_row_smart_finds_gap(void)
{
	setup();

	/* Put an existing view at (0,0) */
	add_view_to_server(0);
	test_views[0].x = 0;
	test_views[0].y = 0;

	init_view(1);
	test_views[1].workspace = &test_workspace;

	struct wlr_box area = { 0, 0, 1920, 1080 };
	place_row_smart(&test_server, &test_views[1], &area);

	/* Should find a non-overlapping position */
	bool collision = overlaps_any(&test_server, &test_views[1],
		test_views[1].x, test_views[1].y, 400, 300);
	assert(!collision);

	/* Position should be within the usable area */
	assert(test_views[1].x >= area.x);
	assert(test_views[1].y >= area.y);
	assert(test_views[1].x + 400 <= area.x + area.width);
	assert(test_views[1].y + 300 <= area.y + area.height);

	printf("  PASS: test_row_smart_finds_gap\n");
}

/*
 * Test: place_row_smart on empty workspace places at origin
 */
static void
test_row_smart_empty_workspace(void)
{
	setup();
	init_view(0);
	test_views[0].workspace = &test_workspace;

	struct wlr_box area = { 0, 0, 1920, 1080 };
	place_row_smart(&test_server, &test_views[0], &area);

	/* With no other windows, first scan position (0,0) should work */
	assert(test_views[0].x == 0);
	assert(test_views[0].y == 0);
	printf("  PASS: test_row_smart_empty_workspace\n");
}

/*
 * Test: place_col_smart scans column-major (x outer, y inner)
 */
static void
test_col_smart_finds_gap(void)
{
	setup();

	/* Fill origin with a view */
	add_view_to_server(0);
	test_views[0].x = 0;
	test_views[0].y = 0;

	init_view(1);
	test_views[1].workspace = &test_workspace;

	struct wlr_box area = { 0, 0, 1920, 1080 };
	place_col_smart(&test_server, &test_views[1], &area);

	/* Should find gap without overlapping view 0 */
	bool collision = overlaps_any(&test_server, &test_views[1],
		test_views[1].x, test_views[1].y, 400, 300);
	assert(!collision);

	printf("  PASS: test_col_smart_finds_gap\n");
}

/*
 * Test: place_row_min_overlap with no other views places at start
 */
static void
test_row_min_overlap_empty(void)
{
	setup();
	init_view(0);
	test_views[0].workspace = &test_workspace;

	struct wlr_box area = { 0, 0, 1920, 1080 };
	place_row_min_overlap(&test_server, &test_views[0], &area);

	/* No other views, so first scanned position (0,0) has zero overlap */
	assert(test_views[0].x == 0);
	assert(test_views[0].y == 0);
	printf("  PASS: test_row_min_overlap_empty\n");
}

/*
 * Test: place_col_min_overlap places at minimum overlap position
 */
static void
test_col_min_overlap_avoids_existing(void)
{
	setup();

	add_view_to_server(0);
	test_views[0].x = 0;
	test_views[0].y = 0;

	init_view(1);
	test_views[1].workspace = &test_workspace;

	struct wlr_box area = { 0, 0, 1920, 1080 };
	place_col_min_overlap(&test_server, &test_views[1], &area);

	/* With a large area and only one existing window, should find a
	 * zero-overlap position */
	int ov = overlap_area(&test_server, &test_views[1],
		test_views[1].x, test_views[1].y, 400, 300);
	assert(ov == 0);
	printf("  PASS: test_col_min_overlap_avoids_existing\n");
}

/*
 * Test: try_snap snaps coordinate within threshold
 */
static void
test_try_snap_within(void)
{
	setup();
	int coord = 105;
	bool snapped = try_snap(&coord, 100, 10);

	assert(snapped);
	assert(coord == 100);
	printf("  PASS: test_try_snap_within\n");
}

/*
 * Test: try_snap does not snap when outside threshold
 */
static void
test_try_snap_outside(void)
{
	setup();
	int coord = 115;
	bool snapped = try_snap(&coord, 100, 10);

	assert(!snapped);
	assert(coord == 115); /* unchanged */
	printf("  PASS: test_try_snap_outside\n");
}

/*
 * Test: try_snap at exact boundary (distance == threshold is NOT a snap)
 */
static void
test_try_snap_boundary(void)
{
	setup();
	int coord = 110;
	bool snapped = try_snap(&coord, 100, 10);

	/* abs(110-100) = 10, 10 < 10 is false */
	assert(!snapped);
	assert(coord == 110);
	printf("  PASS: test_try_snap_boundary\n");
}

/*
 * Test: wm_snap_edges snaps to output left/top edges
 */
static void
test_snap_edges_output_left_top(void)
{
	setup();
	init_view(0);

	/* View near left edge */
	int new_x = 5;
	int new_y = 3;

	wm_snap_edges(&test_server, &test_views[0], &new_x, &new_y);

	/* Should snap to 0 (left edge and top edge) */
	assert(new_x == 0);
	assert(new_y == 0);
	printf("  PASS: test_snap_edges_output_left_top\n");
}

/*
 * Test: wm_snap_edges snaps to output right/bottom edges
 */
static void
test_snap_edges_output_right_bottom(void)
{
	setup();
	init_view(0);

	/* View near right edge: area width 1920, window width 400 */
	/* Right target = 0 + 1920 - 400 = 1520 */
	int new_x = 1515;
	/* Bottom target = 0 + 1080 - 300 = 780 */
	int new_y = 775;

	wm_snap_edges(&test_server, &test_views[0], &new_x, &new_y);

	assert(new_x == 1520);
	assert(new_y == 780);
	printf("  PASS: test_snap_edges_output_right_bottom\n");
}

/*
 * Test: wm_snap_edges does nothing when threshold is 0
 */
static void
test_snap_edges_disabled(void)
{
	setup();
	test_config.edge_snap_threshold = 0;
	init_view(0);

	int new_x = 5;
	int new_y = 5;

	wm_snap_edges(&test_server, &test_views[0], &new_x, &new_y);

	assert(new_x == 5);
	assert(new_y == 5);
	printf("  PASS: test_snap_edges_disabled\n");
}

/*
 * Test: wm_snap_edges snaps to other window edges
 */
static void
test_snap_edges_to_window(void)
{
	setup();

	/* Existing view at (500, 200) with 400x300 geometry */
	add_view_to_server(0);
	test_views[0].x = 500;
	test_views[0].y = 200;

	init_view(1);
	test_views[1].workspace = &test_workspace;

	/* Place new view so its left edge is near the right edge of view 0.
	 * Right edge of view 0 = 500 + 400 = 900.
	 * Moving view's x = 903 (within threshold 10). */
	int new_x = 903;
	int new_y = 600; /* far from any snap target on Y */

	wm_snap_edges(&test_server, &test_views[1], &new_x, &new_y);

	/* Should snap X to 900 (left edge snaps to right edge of view 0) */
	assert(new_x == 900);
	printf("  PASS: test_snap_edges_to_window\n");
}

/*
 * Test: wm_placement_apply skips if view already positioned
 */
static void
test_placement_apply_skip_positioned(void)
{
	setup();
	init_view(0);
	test_views[0].x = 100;
	test_views[0].y = 200;

	wm_placement_apply(&test_server, &test_views[0]);

	/* Position should be unchanged (rules already set it) */
	assert(test_views[0].x == 100);
	assert(test_views[0].y == 200);
	printf("  PASS: test_placement_apply_skip_positioned\n");
}

/*
 * Test: wm_placement_apply dispatches to cascade strategy
 */
static void
test_placement_apply_dispatch_cascade(void)
{
	setup();
	test_config.placement_policy = WM_PLACEMENT_CASCADE;

	/* Use offset usable area so cascade triggers the reset branch
	 * (cascade_x=0 < area.x=100), placing at (130, 80). */
	test_output.usable_area = (struct wlr_box){ 100, 50, 1600, 900 };

	init_view(0);
	test_views[0].x = 0;
	test_views[0].y = 0;

	wm_placement_apply(&test_server, &test_views[0]);

	assert(test_views[0].x == 100 + CASCADE_STEP_X);
	assert(test_views[0].y == 50 + CASCADE_STEP_Y);
	printf("  PASS: test_placement_apply_dispatch_cascade\n");
}

/*
 * Test: wm_placement_apply dispatches under-mouse strategy
 */
static void
test_placement_apply_dispatch_under_mouse(void)
{
	setup();
	init_view(0);
	test_views[0].x = 0;
	test_views[0].y = 0;
	test_config.placement_policy = WM_PLACEMENT_UNDER_MOUSE;
	test_cursor.x = 960.0;
	test_cursor.y = 540.0;

	wm_placement_apply(&test_server, &test_views[0]);

	assert(test_views[0].x == 760);
	assert(test_views[0].y == 390);
	printf("  PASS: test_placement_apply_dispatch_under_mouse\n");
}

/*
 * Test: wm_placement_apply returns early when no output found
 */
static void
test_placement_apply_no_output(void)
{
	setup();
	init_view(0);
	test_views[0].x = 0;
	test_views[0].y = 0;
	g_output_at_result = NULL; /* no output under cursor */

	wm_placement_apply(&test_server, &test_views[0]);

	/* Should remain at (0,0) since no output was found */
	assert(test_views[0].x == 0);
	assert(test_views[0].y == 0);
	printf("  PASS: test_placement_apply_no_output\n");
}

/*
 * Test: view_dims returns actual geometry when valid
 */
static void
test_view_dims_normal(void)
{
	setup();
	init_view(0);
	g_stub_xdg_geo = (struct wlr_box){ 0, 0, 800, 600 };

	int w, h;
	view_dims(&test_views[0], &w, &h);

	assert(w == 800);
	assert(h == 600);
	printf("  PASS: test_view_dims_normal\n");
}

/*
 * Test: view_dims returns default 200x200 for zero-size geometry
 */
static void
test_view_dims_zero_size(void)
{
	setup();
	init_view(0);
	g_stub_xdg_geo = (struct wlr_box){ 0, 0, 0, 0 };

	int w, h;
	view_dims(&test_views[0], &w, &h);

	assert(w == 200);
	assert(h == 200);
	printf("  PASS: test_view_dims_zero_size\n");
}

/*
 * Test: place_under_mouse with window larger than output clamps properly
 */
static void
test_under_mouse_window_larger_than_output(void)
{
	setup();
	init_view(0);

	/* Window is 2000x1200, larger than 1920x1080 output */
	g_stub_xdg_geo = (struct wlr_box){ 0, 0, 2000, 1200 };

	test_cursor.x = 960.0;
	test_cursor.y = 540.0;

	struct wlr_box area = { 0, 0, 1920, 1080 };
	place_under_mouse(&test_server, &test_views[0], &area);

	/* x = 960-1000 = -40, clamped to 0,
	 * but then 0+2000 > 1920, so clamped to 1920-2000 = -80.
	 * The clamping logic does: first clamp to left (x=0),
	 * then clamp right: 0+2000>1920 -> x = 1920-2000 = -80.
	 * This is the correct behavior for oversized windows. */
	assert(test_views[0].x == 1920 - 2000);
	assert(test_views[0].y == 1080 - 1200);
	printf("  PASS: test_under_mouse_window_larger_than_output\n");
}

/*
 * Test: get_cursor_output_area falls back to layout box when
 * usable_area is zero
 */
static void
test_get_cursor_output_area_fallback(void)
{
	setup();

	/* Set usable_area to zero-size to trigger fallback */
	test_output.usable_area = (struct wlr_box){ 0, 0, 0, 0 };
	g_layout_box = (struct wlr_box){ 100, 50, 1600, 900 };

	struct wlr_box area;
	bool found = get_cursor_output_area(&test_server, &area);

	assert(found);
	assert(area.x == 100);
	assert(area.y == 50);
	assert(area.width == 1600);
	assert(area.height == 900);
	printf("  PASS: test_get_cursor_output_area_fallback\n");
}

/*
 * Test: get_cursor_output_area returns usable_area when valid
 */
static void
test_get_cursor_output_area_usable(void)
{
	setup();

	test_output.usable_area = (struct wlr_box){ 0, 30, 1920, 1050 };

	struct wlr_box area;
	bool found = get_cursor_output_area(&test_server, &area);

	assert(found);
	assert(area.x == 0);
	assert(area.y == 30);
	assert(area.width == 1920);
	assert(area.height == 1050);
	printf("  PASS: test_get_cursor_output_area_usable\n");
}

/*
 * Test: get_cursor_output_area returns false with no output
 */
static void
test_get_cursor_output_area_no_output(void)
{
	setup();
	g_output_at_result = NULL;

	struct wlr_box area;
	bool found = get_cursor_output_area(&test_server, &area);

	assert(!found);
	printf("  PASS: test_get_cursor_output_area_no_output\n");
}

/*
 * Test: place_row_smart with right-to-left config
 */
static void
test_row_smart_rtl(void)
{
	setup();
	test_config.row_right_to_left = true;

	init_view(0);
	test_views[0].workspace = &test_workspace;

	struct wlr_box area = { 0, 0, 1920, 1080 };
	place_row_smart(&test_server, &test_views[0], &area);

	/* RTL: should start scanning from the right side */
	/* x_start = 0 + 1920 - 400 = 1520 */
	assert(test_views[0].x == 1520);
	assert(test_views[0].y == 0);
	printf("  PASS: test_row_smart_rtl\n");
}

/*
 * Test: place_col_smart with bottom-to-top config
 */
static void
test_col_smart_btt(void)
{
	setup();
	test_config.col_bottom_to_top = true;

	init_view(0);
	test_views[0].workspace = &test_workspace;

	struct wlr_box area = { 0, 0, 1920, 1080 };
	place_col_smart(&test_server, &test_views[0], &area);

	/* BTT: should start scanning from the bottom */
	/* y_start = 0 + 1080 - 300 = 780 */
	assert(test_views[0].x == 0);
	assert(test_views[0].y == 780);
	printf("  PASS: test_col_smart_btt\n");
}

/*
 * Test: cascade with offset area (non-zero origin) triggers reset
 */
static void
test_cascade_offset_area(void)
{
	setup();
	init_view(0);

	struct wlr_box area = { 100, 50, 1600, 900 };

	/* cascade_x=0 < area.x=100, so the reset branch fires.
	 * Resets to (100+30, 50+30) = (130, 80) */
	place_cascade(&test_server, &test_views[0], &area);

	assert(test_views[0].x == 100 + CASCADE_STEP_X);
	assert(test_views[0].y == 50 + CASCADE_STEP_Y);

	/* Second window: cascade advances to (160, 110) */
	init_view(1);
	place_cascade(&test_server, &test_views[1], &area);

	assert(test_views[1].x == 100 + 2 * CASCADE_STEP_X);
	assert(test_views[1].y == 50 + 2 * CASCADE_STEP_Y);
	printf("  PASS: test_cascade_offset_area\n");
}

/*
 * Test: wm_placement_apply with NULL config defaults to ROW_SMART
 */
static void
test_placement_apply_null_config(void)
{
	setup();
	test_server.config = NULL;
	init_view(0);
	test_views[0].x = 0;
	test_views[0].y = 0;

	wm_placement_apply(&test_server, &test_views[0]);

	/* Should use default ROW_SMART and place at (0,0) on empty workspace */
	assert(test_views[0].x == 0);
	assert(test_views[0].y == 0);
	printf("  PASS: test_placement_apply_null_config\n");
}

/*
 * Test: wm_snap_edges does nothing with NULL config
 */
static void
test_snap_edges_null_config(void)
{
	setup();
	test_server.config = NULL;
	init_view(0);

	int new_x = 5;
	int new_y = 5;
	wm_snap_edges(&test_server, &test_views[0], &new_x, &new_y);

	/* Should be unchanged */
	assert(new_x == 5);
	assert(new_y == 5);
	printf("  PASS: test_snap_edges_null_config\n");
}

int
main(void)
{
	printf("test_placement:\n");

	/* Static helpers */
	test_view_dims_normal();
	test_view_dims_zero_size();
	test_get_cursor_output_area_usable();
	test_get_cursor_output_area_fallback();
	test_get_cursor_output_area_no_output();

	/* Overlap detection */
	test_overlaps_any_empty();
	test_overlaps_any_collision();
	test_overlaps_any_different_workspace();
	test_overlap_area_calculation();

	/* try_snap */
	test_try_snap_within();
	test_try_snap_outside();
	test_try_snap_boundary();

	/* Placement strategies */
	test_cascade_basic();
	test_cascade_wrapping();
	test_cascade_wrapping_y();
	test_cascade_offset_area();
	test_under_mouse_center();
	test_under_mouse_clamp_topleft();
	test_under_mouse_clamp_bottomright();
	test_under_mouse_window_larger_than_output();
	test_row_smart_empty_workspace();
	test_row_smart_finds_gap();
	test_row_smart_rtl();
	test_col_smart_finds_gap();
	test_col_smart_btt();
	test_row_min_overlap_empty();
	test_col_min_overlap_avoids_existing();

	/* Edge snapping */
	test_snap_edges_output_left_top();
	test_snap_edges_output_right_bottom();
	test_snap_edges_disabled();
	test_snap_edges_to_window();
	test_snap_edges_null_config();

	/* Dispatch / main entry */
	test_placement_apply_skip_positioned();
	test_placement_apply_dispatch_cascade();
	test_placement_apply_dispatch_under_mouse();
	test_placement_apply_no_output();
	test_placement_apply_null_config();

	printf("All placement tests passed.\n");
	return 0;
}
