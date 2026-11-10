/*
 * test_snap_picker.c - The layout picker's geometry and hit-testing
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "snap_picker.h"

/* The drawing is not under test */
struct wlr_buffer *wlr_buffer_from_cairo(void *surface);
struct wlr_buffer *wlr_buffer_from_cairo(void *surface)
	{ (void)surface; return NULL; }

static struct wm_config
two_columns(void)
{
	struct wm_config c;
	memset(&c, 0, sizeof(c));
	c.snap_zone_threshold = 10;
	c.snap_layout_picker_size = 120;
	c.snap_layout_count = 1;
	c.snap_layouts[0].cell_count = 2;
	c.snap_layouts[0].cells[0] = (struct wm_snap_cell){ 0, 0, 500, 1000 };
	c.snap_layouts[0].cells[1] = (struct wm_snap_cell){ 500, 0, 500, 1000 };
	return c;
}

static void
test_geometry_centred_below_the_zone(void)
{
	struct wm_config c = two_columns();
	struct wlr_box usable = { 1920, 0, 1920, 1040 };
	struct wm_snap_picker_geom g;
	assert(wm_snap_picker_geometry(&c, &usable, &g));
	assert(g.count == 1);
	assert(g.thumbs[0].width == 120 && g.thumbs[0].height == 75);
	assert(g.panel.width == 120 + 2 * WM_SNAP_PICKER_PAD);
	assert(g.panel.x == 1920 + (1920 - g.panel.width) / 2);
	assert(g.panel.y > usable.y + c.snap_zone_threshold);
	printf("  PASS: test_geometry_centred_below_the_zone\n");
}

static void
test_geometry_wraps_on_a_narrow_head(void)
{
	struct wm_config c = two_columns();
	c.snap_layout_count = 7;
	for (int i = 1; i < 7; i++)
		c.snap_layouts[i] = c.snap_layouts[0];
	struct wlr_box usable = { 0, 0, 400, 600 };
	struct wm_snap_picker_geom g;
	assert(wm_snap_picker_geometry(&c, &usable, &g));
	assert(g.count == 7);
	assert(g.panel.x >= 0 && g.panel.x + g.panel.width <= 400);
	/* More than one row, none outside the panel */
	assert(g.thumbs[6].y > g.thumbs[0].y);
	for (int i = 0; i < 7; i++) {
		assert(g.thumbs[i].x >= g.panel.x);
		assert(g.thumbs[i].x + g.thumbs[i].width <=
			g.panel.x + g.panel.width);
		assert(g.thumbs[i].y + g.thumbs[i].height <=
			g.panel.y + g.panel.height);
	}

	/* A head narrower than a thumbnail shrinks it */
	struct wlr_box tiny = { 0, 0, 100, 600 };
	assert(wm_snap_picker_geometry(&c, &tiny, &g));
	assert(g.panel.x >= 0 && g.panel.x + g.panel.width <= 100);
	printf("  PASS: test_geometry_wraps_on_a_narrow_head\n");
}

static void
test_rows_of_the_columns_asked_for(void)
{
	struct wm_config c = two_columns();
	c.snap_layout_count = 8;
	for (int i = 1; i < 8; i++)
		c.snap_layouts[i] = c.snap_layouts[0];
	struct wlr_box usable = { 0, 0, 1920, 1080 };
	struct wm_snap_picker_geom g;

	/* four to a row: two rows of four, though the head has room for eight */
	c.snap_layout_picker_columns = 4;
	assert(wm_snap_picker_geometry(&c, &usable, &g));
	assert(g.thumbs[3].y == g.thumbs[0].y);
	assert(g.thumbs[4].y > g.thumbs[0].y);
	assert(g.thumbs[4].x == g.thumbs[0].x);
	assert(g.thumbs[7].y == g.thumbs[4].y);
	/* the second row is under the first, in the same columns */
	for (int i = 0; i < 4; i++)
		assert(g.thumbs[i + 4].x == g.thumbs[i].x);
	/* the panel is centred, and as wide as four */
	assert(g.panel.width == 4 * 120 + 3 * WM_SNAP_PICKER_PAD +
		2 * WM_SNAP_PICKER_PAD);
	assert(g.panel.x == (1920 - g.panel.width) / 2);

	/* none asked for: as many as fit, here all eight in a row */
	c.snap_layout_picker_columns = 0;
	assert(wm_snap_picker_geometry(&c, &usable, &g));
	assert(g.thumbs[7].y == g.thumbs[0].y);

	/* more than fit: as many as do */
	c.snap_layout_picker_columns = 8;
	struct wlr_box narrow = { 0, 0, 600, 600 };
	assert(wm_snap_picker_geometry(&c, &narrow, &g));
	assert(g.thumbs[7].y > g.thumbs[0].y);
	assert(g.panel.x >= 0 && g.panel.x + g.panel.width <= 600);
	printf("  PASS: test_rows_of_the_columns_asked_for\n");
}

static void
test_nothing_to_show(void)
{
	struct wm_config c = two_columns();
	c.snap_layout_count = 0;
	struct wlr_box usable = { 0, 0, 800, 600 };
	struct wm_snap_picker_geom g;
	assert(!wm_snap_picker_geometry(&c, &usable, &g));
	printf("  PASS: test_nothing_to_show\n");
}

static void
test_hit_and_target(void)
{
	struct wm_config c = two_columns();
	struct wlr_box usable = { 0, 0, 1000, 800 };
	struct wm_snap_picker_geom g;
	assert(wm_snap_picker_geometry(&c, &usable, &g));

	int layout = -1, cell = -1;
	struct wlr_box t = g.thumbs[0];
	assert(wm_snap_picker_hit(&g, &c, t.x + 5, t.y + 5, &layout, &cell));
	assert(layout == 0 && cell == 0);
	assert(wm_snap_picker_hit(&g, &c, t.x + t.width - 5, t.y + t.height - 5,
		&layout, &cell));
	assert(layout == 0 && cell == 1);
	/* The panel's padding is no cell */
	assert(!wm_snap_picker_hit(&g, &c, g.panel.x + 2, g.panel.y + 2,
		&layout, &cell));
	assert(!wm_snap_picker_hit(&g, &c, t.x + t.width, t.y, &layout, &cell));

	/* The panel, and the way to it from the top of the head */
	assert(wm_snap_picker_contains(&g, &usable, t.x + 5, 0));
	assert(wm_snap_picker_contains(&g, &usable, t.x + 5, t.y + 5));
	assert(!wm_snap_picker_contains(&g, &usable, 5, 5));
	assert(!wm_snap_picker_contains(&g, &usable, t.x + 5,
		g.panel.y + g.panel.height));

	int out[4];
	struct wlr_box box;
	wm_snap_picker_target(&c, &usable, 0, 1, out, &box);
	assert(out[0] == 500 && out[1] == 0 && out[2] == 500 && out[3] == 1000);
	assert(box.x == 500 && box.y == 0 && box.width == 500 &&
		box.height == 800);
	printf("  PASS: test_hit_and_target\n");
}

int
main(void)
{
	printf("test_snap_picker:\n");
	test_geometry_centred_below_the_zone();
	test_geometry_wraps_on_a_narrow_head();
	test_rows_of_the_columns_asked_for();
	test_nothing_to_show();
	test_hit_and_target();
	printf("All snap_picker tests passed.\n");
	return 0;
}
