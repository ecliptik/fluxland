/*
 * test_pseudo_transparency.c - What of the desktop is under a window
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "config.h"
#include "pseudo_transparency.h"
#include "server.h"
#include "view.h"

/* The scene graph work is checked end to end by the UI tests */
struct wlr_surface *wm_view_surface(struct wm_view *view)
	{ (void)view; return NULL; }
void wm_view_content_geometry(struct wm_view *view, struct wlr_box *box)
	{ (void)view; (void)box; }
void wm_view_frame_extents(struct wm_view *view, int *top, int *bottom,
	int *left, int *right)
	{ (void)view; *top = *bottom = *left = *right = 0; }

static int
near(double a, double b)
{
	return fabs(a - b) < 1e-6;
}

static void
test_clip_inside_and_outside(void)
{
	struct wlr_box window = { 100, 50, 200, 100 };
	struct wlr_box node = { 0, 0, 1920, 1080 };
	struct wlr_fbox src = { 0, 0, 1920, 1080 };
	struct wlr_box inter;
	struct wlr_fbox isrc;

	/* A window inside a wallpaper: its own area of it, 1:1 */
	assert(wm_pseudo_clip(&window, &node, &src, &inter, &isrc));
	assert(inter.x == 100 && inter.y == 50);
	assert(inter.width == 200 && inter.height == 100);
	assert(near(isrc.x, 100) && near(isrc.y, 50));
	assert(near(isrc.width, 200) && near(isrc.height, 100));

	/* Nothing of a wallpaper elsewhere (another head) */
	struct wlr_box other = { 1920, 0, 1920, 1080 };
	assert(!wm_pseudo_clip(&window, &other, &src, &inter, &isrc));

	/* Touching is not overlapping */
	struct wlr_box touching = { 300, 50, 100, 100 };
	assert(!wm_pseudo_clip(&window, &touching, &src, &inter, &isrc));
	printf("  PASS: test_clip_inside_and_outside\n");
}

static void
test_clip_on_the_edge_of_a_head(void)
{
	/* A window across two heads: the part of each wallpaper under it */
	struct wlr_box window = { 1800, 100, 400, 300 };
	struct wlr_box left = { 0, 0, 1920, 1080 };
	struct wlr_box right = { 1920, 0, 1920, 1080 };
	struct wlr_fbox src = { 0, 0, 1920, 1080 };
	struct wlr_box inter;
	struct wlr_fbox isrc;

	assert(wm_pseudo_clip(&window, &left, &src, &inter, &isrc));
	assert(inter.x == 1800 && inter.width == 120);
	assert(near(isrc.x, 1800) && near(isrc.width, 120));

	assert(wm_pseudo_clip(&window, &right, &src, &inter, &isrc));
	assert(inter.x == 1920 && inter.width == 280);
	/* the right head's wallpaper starts at its own left edge */
	assert(near(isrc.x, 0) && near(isrc.width, 280));
	printf("  PASS: test_clip_on_the_edge_of_a_head\n");
}

static void
test_clip_of_a_scaled_wallpaper(void)
{
	/* A 3840x2160 image shown on a 1920x1080 head: a window sees twice
	 * as many image pixels as it has, from twice as far in */
	struct wlr_box window = { 100, 50, 200, 100 };
	struct wlr_box node = { 0, 0, 1920, 1080 };
	struct wlr_fbox src = { 0, 0, 3840, 2160 };
	struct wlr_box inter;
	struct wlr_fbox isrc;

	assert(wm_pseudo_clip(&window, &node, &src, &inter, &isrc));
	assert(near(isrc.x, 200) && near(isrc.y, 100));
	assert(near(isrc.width, 400) && near(isrc.height, 200));

	/* A cropped one (a source box) and a head not at the origin */
	struct wlr_box node2 = { 1920, 0, 1920, 1080 };
	struct wlr_fbox src2 = { 480, 270, 960, 540 };
	struct wlr_box w2 = { 2020, 50, 200, 100 };
	assert(wm_pseudo_clip(&w2, &node2, &src2, &inter, &isrc));
	assert(near(isrc.x, 480 + 100 * 0.5) && near(isrc.y, 270 + 50 * 0.5));
	assert(near(isrc.width, 100) && near(isrc.height, 50));
	printf("  PASS: test_clip_of_a_scaled_wallpaper\n");
}

static void
test_rule_and_global(void)
{
	struct wm_config config = { 0 };
	struct wm_server server = { 0 };
	struct wm_view view = { 0 };
	server.config = &config;
	view.server = &server;

	assert(!wm_view_pseudo_transparent(&view));
	config.pseudo_transparency = true;
	assert(wm_view_pseudo_transparent(&view));
	/* the window's own say wins either way */
	view.pseudo_rule = WM_PSEUDO_NO;
	assert(!wm_view_pseudo_transparent(&view));
	config.pseudo_transparency = false;
	view.pseudo_rule = WM_PSEUDO_YES;
	assert(wm_view_pseudo_transparent(&view));
	printf("  PASS: test_rule_and_global\n");
}

int
main(void)
{
	printf("test_pseudo_transparency:\n");
	test_clip_inside_and_outside();
	test_clip_on_the_edge_of_a_head();
	test_clip_of_a_scaled_wallpaper();
	test_rule_and_global();
	printf("All pseudo_transparency tests passed.\n");
	return 0;
}
