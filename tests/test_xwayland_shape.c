/*
 * test_xwayland_shape.c - What a window's X Shape leaves of it
 */

#include <assert.h>
#include <stdio.h>

#include "xwayland_shape.h"

/* the rest of xwayland_shape.c is checked end to end by the UI tests, on
 * a real XWayland */

static void
test_unshaped_is_whole(void)
{
	struct wlr_box out[8];
	bool whole = false;
	assert(wm_xshape_pieces(NULL, 0, 85, 33, out, 8, &whole) == 0);
	assert(whole);
	printf("  PASS: test_unshaped_is_whole\n");
}

static void
test_a_shape_that_covers_the_window_is_whole(void)
{
	struct wlr_box out[8];
	bool whole = false;
	struct wlr_box full = { 0, 0, 85, 33 };
	assert(wm_xshape_pieces(&full, 1, 85, 33, out, 8, &whole) == 0);
	assert(whole);

	/* bigger than the window: clipped to it, still all of it */
	struct wlr_box big = { -10, -10, 500, 500 };
	assert(wm_xshape_pieces(&big, 1, 85, 33, out, 8, &whole) == 0);
	assert(whole);

	/* in bands that add up to it */
	struct wlr_box bands[2] = { { 0, 0, 85, 10 }, { 0, 10, 85, 23 } };
	assert(wm_xshape_pieces(bands, 2, 85, 33, out, 8, &whole) == 0);
	assert(whole);
	printf("  PASS: test_a_shape_that_covers_the_window_is_whole\n");
}

static void
test_a_ring_is_four_pieces(void)
{
	struct wlr_box ring[4] = {
		{ 0, 0, 85, 5 }, { 0, 5, 5, 23 }, { 80, 5, 5, 23 },
		{ 0, 28, 85, 5 },
	};
	struct wlr_box out[8];
	bool whole = true;
	assert(wm_xshape_pieces(ring, 4, 85, 33, out, 8, &whole) == 4);
	assert(!whole);
	assert(out[1].x == 0 && out[1].y == 5 && out[1].width == 5);
	printf("  PASS: test_a_ring_is_four_pieces\n");
}

static void
test_wines_overlay_is_clipped_to_the_window(void)
{
	/* what Wine set on an 85x33 window: rectangles reaching to 128 */
	struct wlr_box wine[4] = {
		{ 0, 0, 128, 5 }, { 0, 5, 5, 23 }, { 80, 5, 48, 23 },
		{ 0, 28, 128, 100 },
	};
	struct wlr_box out[8];
	bool whole = true;
	assert(wm_xshape_pieces(wine, 4, 85, 33, out, 8, &whole) == 4);
	assert(!whole);
	assert(out[0].width == 85 && out[0].height == 5);
	assert(out[2].x == 80 && out[2].width == 5);	/* 80..128 -> 80..85 */
	assert(out[3].y == 28 && out[3].height == 5);	/* 28..128 -> 28..33 */
	printf("  PASS: test_wines_overlay_is_clipped_to_the_window\n");
}

static void
test_rectangles_outside_the_window_are_dropped(void)
{
	struct wlr_box shape[3] = {
		{ 100, 0, 20, 20 },	/* right of it */
		{ 10, 10, 20, 10 },
		{ 0, 40, 85, 5 },	/* below it */
	};
	struct wlr_box out[8];
	bool whole = true;
	assert(wm_xshape_pieces(shape, 3, 85, 33, out, 8, &whole) == 1);
	assert(!whole && out[0].x == 10 && out[0].width == 20);

	/* nothing inside: nothing of the window is there */
	struct wlr_box nothing = { 200, 200, 10, 10 };
	assert(wm_xshape_pieces(&nothing, 1, 85, 33, out, 8, &whole) == 0);
	assert(!whole);
	printf("  PASS: test_rectangles_outside_the_window_are_dropped\n");
}

static void
test_too_many_pieces_become_their_box(void)
{
	/* a staircase of 10 steps, with room for 4 pieces */
	struct wlr_box steps[10];
	for (int i = 0; i < 10; i++)
		steps[i] = (struct wlr_box){ i * 8, i * 3, 8, 3 };
	struct wlr_box out[4];
	bool whole = true;
	assert(wm_xshape_pieces(steps, 10, 85, 33, out, 4, &whole) == 1);
	assert(!whole);
	assert(out[0].x == 0 && out[0].y == 0);
	assert(out[0].width == 80 && out[0].height == 30);

	/* exactly as many as fit are kept */
	assert(wm_xshape_pieces(steps, 4, 85, 33, out, 4, &whole) == 4);
	printf("  PASS: test_too_many_pieces_become_their_box\n");
}

int
main(void)
{
	printf("test_xwayland_shape:\n");
	test_unshaped_is_whole();
	test_a_shape_that_covers_the_window_is_whole();
	test_a_ring_is_four_pieces();
	test_wines_overlay_is_clipped_to_the_window();
	test_rectangles_outside_the_window_are_dropped();
	test_too_many_pieces_become_their_box();
	printf("All xwayland_shape tests passed.\n");
	return 0;
}
