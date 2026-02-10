/*
 * test_placement.c - Unit tests for placement grid math and snap logic
 *
 * The placement module's public API requires a running compositor (server,
 * views, outputs). Instead of testing those directly, we test the underlying
 * math/logic that the placement algorithms rely on: grid layout calculations,
 * edge snap distance, and cascade stepping.
 */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* Cascade step constants (mirrored from placement.c) */
#define CASCADE_STEP_X 30
#define CASCADE_STEP_Y 30

/*
 * Replicate the grid math from wm_arrange_windows_grid:
 *   cols = ceil(sqrt(n))
 *   rows = ceil(n / cols)
 *   cell_w = area_width / cols
 *   cell_h = area_height / rows
 *   For view i: col = i % cols, row = i / cols
 *               x = area_x + col * cell_w
 *               y = area_y + row * cell_h
 */

/* Replicate try_snap from placement.c */
static int
try_snap(int coord, int target, int threshold)
{
	int dist = abs(coord - target);
	if (dist < threshold) {
		return target;
	}
	return coord;
}

/* Test: grid layout with 1 window fills the entire area */
static void
test_grid_1_window(void)
{
	int n = 1;
	int area_x = 0, area_y = 0, area_w = 1920, area_h = 1080;

	int cols = (int)ceil(sqrt((double)n));
	int rows = (int)ceil((double)n / cols);

	assert(cols == 1);
	assert(rows == 1);

	int cell_w = area_w / cols;
	int cell_h = area_h / rows;

	assert(cell_w == 1920);
	assert(cell_h == 1080);

	/* View 0 position */
	int x = area_x + (0 % cols) * cell_w;
	int y = area_y + (0 / cols) * cell_h;
	assert(x == 0);
	assert(y == 0);

	printf("  PASS: test_grid_1_window\n");
}

/* Test: grid layout with 2 windows -> 2 columns, 1 row */
static void
test_grid_2_windows(void)
{
	int n = 2;
	int area_w = 1920, area_h = 1080;

	int cols = (int)ceil(sqrt((double)n));
	int rows = (int)ceil((double)n / cols);

	assert(cols == 2);
	assert(rows == 1);

	int cell_w = area_w / cols;
	int cell_h = area_h / rows;

	assert(cell_w == 960);
	assert(cell_h == 1080);

	printf("  PASS: test_grid_2_windows\n");
}

/* Test: grid layout with 4 windows -> 2x2 grid */
static void
test_grid_4_windows(void)
{
	int n = 4;
	int area_x = 0, area_y = 0, area_w = 1920, area_h = 1080;

	int cols = (int)ceil(sqrt((double)n));
	int rows = (int)ceil((double)n / cols);

	assert(cols == 2);
	assert(rows == 2);

	int cell_w = area_w / cols;
	int cell_h = area_h / rows;

	assert(cell_w == 960);
	assert(cell_h == 540);

	/* Check all 4 positions */
	for (int i = 0; i < n; i++) {
		int col = i % cols;
		int row = i / cols;
		int x = area_x + col * cell_w;
		int y = area_y + row * cell_h;

		if (i == 0) { assert(x == 0 && y == 0); }
		if (i == 1) { assert(x == 960 && y == 0); }
		if (i == 2) { assert(x == 0 && y == 540); }
		if (i == 3) { assert(x == 960 && y == 540); }
	}

	printf("  PASS: test_grid_4_windows\n");
}

/* Test: grid layout with 9 windows -> 3x3 grid */
static void
test_grid_9_windows(void)
{
	int n = 9;
	int area_w = 1800, area_h = 900;

	int cols = (int)ceil(sqrt((double)n));
	int rows = (int)ceil((double)n / cols);

	assert(cols == 3);
	assert(rows == 3);

	int cell_w = area_w / cols;
	int cell_h = area_h / rows;

	assert(cell_w == 600);
	assert(cell_h == 300);

	printf("  PASS: test_grid_9_windows\n");
}

/* Test: grid layout with 10 windows -> 4 cols, 3 rows */
static void
test_grid_10_windows(void)
{
	int n = 10;

	int cols = (int)ceil(sqrt((double)n));
	int rows = (int)ceil((double)n / cols);

	/* ceil(sqrt(10)) = ceil(3.16) = 4 */
	assert(cols == 4);
	/* ceil(10/4) = ceil(2.5) = 3 */
	assert(rows == 3);

	printf("  PASS: test_grid_10_windows\n");
}

/* Test: grid layout with 3 windows -> 2 cols, 2 rows (last cell empty) */
static void
test_grid_3_windows(void)
{
	int n = 3;

	int cols = (int)ceil(sqrt((double)n));
	int rows = (int)ceil((double)n / cols);

	/* ceil(sqrt(3)) = ceil(1.73) = 2 */
	assert(cols == 2);
	/* ceil(3/2) = ceil(1.5) = 2 */
	assert(rows == 2);

	printf("  PASS: test_grid_3_windows\n");
}

/* Test: grid with non-zero origin area */
static void
test_grid_offset_area(void)
{
	int n = 4;
	int area_x = 100, area_y = 50;
	int area_w = 1600, area_h = 900;

	int cols = (int)ceil(sqrt((double)n));
	int rows = (int)ceil((double)n / cols);
	int cell_w = area_w / cols;
	int cell_h = area_h / rows;

	/* View 0: top-left */
	int x0 = area_x + (0 % cols) * cell_w;
	int y0 = area_y + (0 / cols) * cell_h;
	assert(x0 == 100);
	assert(y0 == 50);

	/* View 3: bottom-right */
	int x3 = area_x + (3 % cols) * cell_w;
	int y3 = area_y + (3 / cols) * cell_h;
	assert(x3 == 900);
	assert(y3 == 500);

	printf("  PASS: test_grid_offset_area\n");
}

/* Test: edge snap within threshold snaps to target */
static void
test_snap_within_threshold(void)
{
	int threshold = 10;

	/* coord is 5 away from target -> snaps */
	int result = try_snap(105, 100, threshold);
	assert(result == 100);

	/* coord is 9 away from target -> snaps */
	result = try_snap(91, 100, threshold);
	assert(result == 100);

	/* coord is exactly at target -> snaps */
	result = try_snap(100, 100, threshold);
	assert(result == 100);

	printf("  PASS: test_snap_within_threshold\n");
}

/* Test: edge snap outside threshold doesn't snap */
static void
test_snap_outside_threshold(void)
{
	int threshold = 10;

	/* coord is 10 away -> at boundary, should NOT snap */
	int result = try_snap(110, 100, threshold);
	assert(result == 110);

	/* coord is 15 away -> no snap */
	result = try_snap(115, 100, threshold);
	assert(result == 115);

	/* coord is 100 away -> no snap */
	result = try_snap(200, 100, threshold);
	assert(result == 200);

	printf("  PASS: test_snap_outside_threshold\n");
}

/* Test: snap with zero threshold never snaps */
static void
test_snap_zero_threshold(void)
{
	int result = try_snap(101, 100, 0);
	assert(result == 101);

	/* Even exact match: abs(100-100)=0, 0 < 0 is false */
	result = try_snap(100, 100, 0);
	assert(result == 100); /* same value, trivially */

	printf("  PASS: test_snap_zero_threshold\n");
}

/* Test: snap works with negative coordinates */
static void
test_snap_negative_coords(void)
{
	int threshold = 10;

	int result = try_snap(-5, 0, threshold);
	assert(result == 0);

	result = try_snap(-95, -100, threshold);
	assert(result == -100);

	printf("  PASS: test_snap_negative_coords\n");
}

/* Test: cascade stepping constants */
static void
test_cascade_constants(void)
{
	assert(CASCADE_STEP_X == 30);
	assert(CASCADE_STEP_Y == 30);

	/* Simulate a cascade for 5 windows */
	int cx = 100, cy = 50;
	for (int i = 0; i < 5; i++) {
		int x = cx + i * CASCADE_STEP_X;
		int y = cy + i * CASCADE_STEP_Y;
		assert(x == 100 + i * 30);
		assert(y == 50 + i * 30);
	}

	printf("  PASS: test_cascade_constants\n");
}

/* Test: cascade wrapping logic */
static void
test_cascade_wrapping(void)
{
	int area_x = 0, area_y = 0;
	int area_w = 1920, area_h = 1080;
	int win_w = area_w * 60 / 100;  /* 1152 */
	int win_h = area_h * 60 / 100;  /* 648 */

	/* How many cascades before wrapping? */
	/* x wraps when cx + win_w > area_w, i.e., cx > 768 */
	/* cx starts at 0, grows by 30 each step */
	/* 768 / 30 = 25.6 -> wraps after 26 windows */
	int cx = area_x, cy = area_y;
	int steps_before_wrap = 0;

	for (int i = 0; i < 100; i++) {
		if (cx + win_w > area_x + area_w ||
		    cy + win_h > area_y + area_h) {
			break;
		}
		steps_before_wrap++;
		cx += CASCADE_STEP_X;
		cy += CASCADE_STEP_Y;
	}

	/* cy wraps when cy + win_h > 1080, i.e., cy > 432 */
	/* 432 / 30 = 14.4 -> wraps after 15 steps */
	assert(steps_before_wrap == 15);

	printf("  PASS: test_cascade_wrapping\n");
}

int
main(void)
{
	printf("test_placement:\n");

	test_grid_1_window();
	test_grid_2_windows();
	test_grid_4_windows();
	test_grid_9_windows();
	test_grid_10_windows();
	test_grid_3_windows();
	test_grid_offset_area();
	test_snap_within_threshold();
	test_snap_outside_threshold();
	test_snap_zero_threshold();
	test_snap_negative_coords();
	test_cascade_constants();
	test_cascade_wrapping();

	printf("All placement tests passed.\n");
	return 0;
}
