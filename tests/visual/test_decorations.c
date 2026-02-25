/*
 * test_decorations.c - Visual regression test for decoration rendering
 *
 * Renders decoration elements (textures, gradients, button glyphs, text)
 * directly to Cairo surfaces using the render/style API and compares
 * against reference PNG images.  This catches rendering regressions
 * without requiring a running compositor.
 *
 * On first run (when reference images don't exist), the test saves
 * the rendered output as new references and passes.  Subsequent runs
 * compare against those references.
 */

#define _POSIX_C_SOURCE 200809L

#include "image_compare.h"

#include <cairo.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Include the style and render headers */
#include "style.h"
#include "render.h"

/* ------------------------------------------------------------------ */
/* Test infrastructure                                                 */
/* ------------------------------------------------------------------ */

static int tests_run;
static int tests_passed;
static int tests_failed;

/* Directory for reference images */
static char ref_dir[1024];
static char diff_dir[1024];

#define TEST_START(name) \
	do { \
		tests_run++; \
		printf("  [%d] %s ... ", tests_run, (name)); \
		fflush(stdout); \
	} while (0)

#define TEST_PASS() \
	do { \
		tests_passed++; \
		printf("PASS\n"); \
	} while (0)

#define TEST_FAIL(reason) \
	do { \
		tests_failed++; \
		printf("FAIL: %s\n", (reason)); \
	} while (0)

/* Default comparison tolerance: 2% pixel difference, 3 per-channel */
#define DEFAULT_TOLERANCE 2.0
#define DEFAULT_THRESHOLD 3

static void
setup(void)
{
	const char *base = "/tmp/fluxland-test";
	mkdir(base, 0755);

	snprintf(ref_dir, sizeof(ref_dir), "%s/visual-ref", base);
	mkdir(ref_dir, 0755);

	snprintf(diff_dir, sizeof(diff_dir), "%s/visual-diff", base);
	mkdir(diff_dir, 0755);
}

/* ------------------------------------------------------------------ */
/* Test: solid texture rendering                                       */
/* ------------------------------------------------------------------ */

static void
test_solid_texture(void)
{
	TEST_START("solid_texture");

	struct wm_texture tex = {
		.appearance = WM_TEX_FLAT,
		.fill = WM_TEX_SOLID,
		.gradient = WM_GRAD_VERTICAL,
		.bevel = WM_BEVEL_NONE,
		.interlaced = false,
		.color = 0xFF3b4252,
		.color_to = 0xFF3b4252,
		.pixmap_path = NULL,
	};

	cairo_surface_t *surface = wm_render_texture(&tex, 200, 30, 1.0f);
	if (!surface) {
		TEST_FAIL("wm_render_texture returned NULL");
		return;
	}

	char ref_path[1024];
	char diff_path[1024];
	snprintf(ref_path, sizeof(ref_path), "%s/solid_texture.png", ref_dir);
	snprintf(diff_path, sizeof(diff_path), "%s/solid_texture_diff.png",
		 diff_dir);

	struct image_compare_result result = image_compare_with_reference(
		surface, ref_path, diff_path, DEFAULT_TOLERANCE,
		DEFAULT_THRESHOLD);

	if (result.match) {
		TEST_PASS();
	} else {
		char msg[256];
		snprintf(msg, sizeof(msg),
			 "%.2f%% pixels differ (max channel diff: %d)",
			 result.diff_percent, result.max_channel_diff);
		TEST_FAIL(msg);
	}

	cairo_surface_destroy(surface);
}

/* ------------------------------------------------------------------ */
/* Test: gradient texture rendering                                    */
/* ------------------------------------------------------------------ */

static void
test_gradient_texture(void)
{
	TEST_START("gradient_texture");

	struct wm_texture tex = {
		.appearance = WM_TEX_RAISED,
		.fill = WM_TEX_GRADIENT,
		.gradient = WM_GRAD_VERTICAL,
		.bevel = WM_BEVEL1,
		.interlaced = false,
		.color = 0xFF3b4252,
		.color_to = 0xFF2e3440,
		.pixmap_path = NULL,
	};

	cairo_surface_t *surface = wm_render_texture(&tex, 200, 30, 1.0f);
	if (!surface) {
		TEST_FAIL("wm_render_texture returned NULL");
		return;
	}

	char ref_path[1024];
	char diff_path[1024];
	snprintf(ref_path, sizeof(ref_path), "%s/gradient_texture.png",
		 ref_dir);
	snprintf(diff_path, sizeof(diff_path), "%s/gradient_texture_diff.png",
		 diff_dir);

	struct image_compare_result result = image_compare_with_reference(
		surface, ref_path, diff_path, DEFAULT_TOLERANCE,
		DEFAULT_THRESHOLD);

	if (result.match) {
		TEST_PASS();
	} else {
		char msg[256];
		snprintf(msg, sizeof(msg),
			 "%.2f%% pixels differ (max channel diff: %d)",
			 result.diff_percent, result.max_channel_diff);
		TEST_FAIL(msg);
	}

	cairo_surface_destroy(surface);
}

/* ------------------------------------------------------------------ */
/* Test: horizontal gradient                                           */
/* ------------------------------------------------------------------ */

static void
test_horizontal_gradient(void)
{
	TEST_START("horizontal_gradient");

	struct wm_texture tex = {
		.appearance = WM_TEX_FLAT,
		.fill = WM_TEX_GRADIENT,
		.gradient = WM_GRAD_HORIZONTAL,
		.bevel = WM_BEVEL_NONE,
		.interlaced = false,
		.color = 0xFFFF0000,
		.color_to = 0xFF0000FF,
		.pixmap_path = NULL,
	};

	cairo_surface_t *surface = wm_render_texture(&tex, 200, 30, 1.0f);
	if (!surface) {
		TEST_FAIL("wm_render_texture returned NULL");
		return;
	}

	char ref_path[1024];
	char diff_path[1024];
	snprintf(ref_path, sizeof(ref_path), "%s/horizontal_gradient.png",
		 ref_dir);
	snprintf(diff_path, sizeof(diff_path),
		 "%s/horizontal_gradient_diff.png", diff_dir);

	struct image_compare_result result = image_compare_with_reference(
		surface, ref_path, diff_path, DEFAULT_TOLERANCE,
		DEFAULT_THRESHOLD);

	if (result.match) {
		TEST_PASS();
	} else {
		char msg[256];
		snprintf(msg, sizeof(msg),
			 "%.2f%% pixels differ (max channel diff: %d)",
			 result.diff_percent, result.max_channel_diff);
		TEST_FAIL(msg);
	}

	cairo_surface_destroy(surface);
}

/* ------------------------------------------------------------------ */
/* Test: sunken bevel texture                                          */
/* ------------------------------------------------------------------ */

static void
test_sunken_texture(void)
{
	TEST_START("sunken_texture");

	struct wm_texture tex = {
		.appearance = WM_TEX_SUNKEN,
		.fill = WM_TEX_SOLID,
		.gradient = WM_GRAD_VERTICAL,
		.bevel = WM_BEVEL1,
		.interlaced = false,
		.color = 0xFF555555,
		.color_to = 0xFF555555,
		.pixmap_path = NULL,
	};

	cairo_surface_t *surface = wm_render_texture(&tex, 200, 30, 1.0f);
	if (!surface) {
		TEST_FAIL("wm_render_texture returned NULL");
		return;
	}

	char ref_path[1024];
	char diff_path[1024];
	snprintf(ref_path, sizeof(ref_path), "%s/sunken_texture.png",
		 ref_dir);
	snprintf(diff_path, sizeof(diff_path), "%s/sunken_texture_diff.png",
		 diff_dir);

	struct image_compare_result result = image_compare_with_reference(
		surface, ref_path, diff_path, DEFAULT_TOLERANCE,
		DEFAULT_THRESHOLD);

	if (result.match) {
		TEST_PASS();
	} else {
		char msg[256];
		snprintf(msg, sizeof(msg),
			 "%.2f%% pixels differ (max channel diff: %d)",
			 result.diff_percent, result.max_channel_diff);
		TEST_FAIL(msg);
	}

	cairo_surface_destroy(surface);
}

/* ------------------------------------------------------------------ */
/* Test: interlaced texture                                            */
/* ------------------------------------------------------------------ */

static void
test_interlaced_texture(void)
{
	TEST_START("interlaced_texture");

	struct wm_texture tex = {
		.appearance = WM_TEX_RAISED,
		.fill = WM_TEX_GRADIENT,
		.gradient = WM_GRAD_VERTICAL,
		.bevel = WM_BEVEL1,
		.interlaced = true,
		.color = 0xFF5e81ac,
		.color_to = 0xFF2e3440,
		.pixmap_path = NULL,
	};

	cairo_surface_t *surface = wm_render_texture(&tex, 200, 30, 1.0f);
	if (!surface) {
		TEST_FAIL("wm_render_texture returned NULL");
		return;
	}

	char ref_path[1024];
	char diff_path[1024];
	snprintf(ref_path, sizeof(ref_path), "%s/interlaced_texture.png",
		 ref_dir);
	snprintf(diff_path, sizeof(diff_path),
		 "%s/interlaced_texture_diff.png", diff_dir);

	struct image_compare_result result = image_compare_with_reference(
		surface, ref_path, diff_path, DEFAULT_TOLERANCE,
		DEFAULT_THRESHOLD);

	if (result.match) {
		TEST_PASS();
	} else {
		char msg[256];
		snprintf(msg, sizeof(msg),
			 "%.2f%% pixels differ (max channel diff: %d)",
			 result.diff_percent, result.max_channel_diff);
		TEST_FAIL(msg);
	}

	cairo_surface_destroy(surface);
}

/* ------------------------------------------------------------------ */
/* Test: button glyph rendering                                        */
/* ------------------------------------------------------------------ */

static void
test_button_glyphs(void)
{
	TEST_START("button_glyphs");

	/* Render all five button types onto a single composite surface */
	int btn_size = 16;
	int padding = 4;
	int total_width = 5 * btn_size + 4 * padding;

	cairo_surface_t *composite = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, total_width, btn_size);
	if (cairo_surface_status(composite) != CAIRO_STATUS_SUCCESS) {
		TEST_FAIL("failed to create composite surface");
		cairo_surface_destroy(composite);
		return;
	}

	cairo_t *cr = cairo_create(composite);

	/* Dark background */
	cairo_set_source_rgba(cr, 0.23, 0.26, 0.32, 1.0);
	cairo_paint(cr);

	struct wm_color pic_color = { .r = 0xd8, .g = 0xde, .b = 0xe9,
				      .a = 0xFF };

	enum wm_button_type types[] = {
		WM_BUTTON_CLOSE, WM_BUTTON_MAXIMIZE, WM_BUTTON_ICONIFY,
		WM_BUTTON_SHADE, WM_BUTTON_STICK,
	};

	bool all_ok = true;
	for (int i = 0; i < 5; i++) {
		cairo_surface_t *glyph = wm_render_button_glyph(
			types[i], &pic_color, btn_size, 1.0f);
		if (!glyph) {
			all_ok = false;
			continue;
		}
		int x = i * (btn_size + padding);
		cairo_set_source_surface(cr, glyph, x, 0);
		cairo_paint(cr);
		cairo_surface_destroy(glyph);
	}

	cairo_destroy(cr);

	if (!all_ok) {
		TEST_FAIL("some button glyphs returned NULL");
		cairo_surface_destroy(composite);
		return;
	}

	char ref_path[1024];
	char diff_path[1024];
	snprintf(ref_path, sizeof(ref_path), "%s/button_glyphs.png",
		 ref_dir);
	snprintf(diff_path, sizeof(diff_path), "%s/button_glyphs_diff.png",
		 diff_dir);

	struct image_compare_result result = image_compare_with_reference(
		composite, ref_path, diff_path, DEFAULT_TOLERANCE,
		DEFAULT_THRESHOLD);

	if (result.match) {
		TEST_PASS();
	} else {
		char msg[256];
		snprintf(msg, sizeof(msg),
			 "%.2f%% pixels differ (max channel diff: %d)",
			 result.diff_percent, result.max_channel_diff);
		TEST_FAIL(msg);
	}

	cairo_surface_destroy(composite);
}

/* ------------------------------------------------------------------ */
/* Test: text rendering                                                */
/* ------------------------------------------------------------------ */

static void
test_text_rendering(void)
{
	TEST_START("text_rendering");

	struct wm_font font = {
		.family = "sans",
		.size = 11,
		.bold = true,
		.italic = false,
		.shadow_x = 0,
		.shadow_y = 0,
		.shadow_color = { .r = 0, .g = 0, .b = 0, .a = 0xFF },
		.halo = false,
		.halo_color = { .r = 0, .g = 0, .b = 0, .a = 0xFF },
	};

	struct wm_color color = {
		.r = 0xEC, .g = 0xEF, .b = 0xF4, .a = 0xFF
	};

	int tw = 0, th = 0;
	cairo_surface_t *text_surface = wm_render_text(
		"Test Window Title", &font, &color, 200, &tw, &th,
		WM_JUSTIFY_CENTER, 1.0f);

	if (!text_surface) {
		TEST_FAIL("wm_render_text returned NULL");
		return;
	}

	/* Compose text onto a dark background for comparison */
	int width = 200;
	int height = th > 0 ? th + 4 : 24;
	cairo_surface_t *composite = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, width, height);
	cairo_t *cr = cairo_create(composite);

	cairo_set_source_rgba(cr, 0.23, 0.26, 0.32, 1.0);
	cairo_paint(cr);

	int text_y = (height - th) / 2;
	int text_x = (width - tw) / 2;
	cairo_set_source_surface(cr, text_surface, text_x, text_y);
	cairo_paint(cr);
	cairo_destroy(cr);

	char ref_path[1024];
	char diff_path[1024];
	snprintf(ref_path, sizeof(ref_path), "%s/text_rendering.png",
		 ref_dir);
	snprintf(diff_path, sizeof(diff_path), "%s/text_rendering_diff.png",
		 diff_dir);

	struct image_compare_result result = image_compare_with_reference(
		composite, ref_path, diff_path, DEFAULT_TOLERANCE,
		DEFAULT_THRESHOLD);

	if (result.match) {
		TEST_PASS();
	} else {
		char msg[256];
		snprintf(msg, sizeof(msg),
			 "%.2f%% pixels differ (max channel diff: %d)",
			 result.diff_percent, result.max_channel_diff);
		TEST_FAIL(msg);
	}

	cairo_surface_destroy(text_surface);
	cairo_surface_destroy(composite);
}

/* ------------------------------------------------------------------ */
/* Test: full titlebar composition                                     */
/* ------------------------------------------------------------------ */

static void
test_titlebar_composition(void)
{
	TEST_START("titlebar_composition");

	int width = 300;
	int height = 22;

	/* Render titlebar background */
	struct wm_texture title_tex = {
		.appearance = WM_TEX_RAISED,
		.fill = WM_TEX_GRADIENT,
		.gradient = WM_GRAD_VERTICAL,
		.bevel = WM_BEVEL1,
		.interlaced = false,
		.color = 0xFF3b4252,
		.color_to = 0xFF2e3440,
		.pixmap_path = NULL,
	};

	cairo_surface_t *bg = wm_render_texture(&title_tex, width, height,
						1.0f);
	if (!bg) {
		TEST_FAIL("failed to render titlebar background");
		return;
	}

	/* Create composite */
	cairo_surface_t *composite = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, width, height);
	cairo_t *cr = cairo_create(composite);

	/* Paint background */
	cairo_set_source_surface(cr, bg, 0, 0);
	cairo_paint(cr);
	cairo_surface_destroy(bg);

	/* Render and composite a button glyph */
	struct wm_color pic_color = {
		.r = 0xd8, .g = 0xde, .b = 0xe9, .a = 0xFF
	};
	cairo_surface_t *close_glyph = wm_render_button_glyph(
		WM_BUTTON_CLOSE, &pic_color, 12, 1.0f);
	if (close_glyph) {
		cairo_set_source_surface(cr, close_glyph,
					 width - 16, (height - 12) / 2);
		cairo_paint(cr);
		cairo_surface_destroy(close_glyph);
	}

	/* Render and composite title text */
	struct wm_font font = {
		.family = "sans",
		.size = 11,
		.bold = true,
		.italic = false,
		.shadow_x = 0,
		.shadow_y = 0,
		.shadow_color = { .r = 0, .g = 0, .b = 0, .a = 0xFF },
		.halo = false,
		.halo_color = { .r = 0, .g = 0, .b = 0, .a = 0xFF },
	};
	struct wm_color text_color = {
		.r = 0xEC, .g = 0xEF, .b = 0xF4, .a = 0xFF
	};

	int tw = 0, th = 0;
	cairo_surface_t *text = wm_render_text(
		"fluxland", &font, &text_color, width - 40, &tw, &th,
		WM_JUSTIFY_CENTER, 1.0f);
	if (text) {
		int text_x = (width - tw) / 2;
		int text_y = (height - th) / 2;
		cairo_set_source_surface(cr, text, text_x, text_y);
		cairo_paint(cr);
		cairo_surface_destroy(text);
	}

	cairo_destroy(cr);

	char ref_path[1024];
	char diff_path[1024];
	snprintf(ref_path, sizeof(ref_path), "%s/titlebar_composition.png",
		 ref_dir);
	snprintf(diff_path, sizeof(diff_path),
		 "%s/titlebar_composition_diff.png", diff_dir);

	struct image_compare_result result = image_compare_with_reference(
		composite, ref_path, diff_path, DEFAULT_TOLERANCE,
		DEFAULT_THRESHOLD);

	if (result.match) {
		TEST_PASS();
	} else {
		char msg[256];
		snprintf(msg, sizeof(msg),
			 "%.2f%% pixels differ (max channel diff: %d)",
			 result.diff_percent, result.max_channel_diff);
		TEST_FAIL(msg);
	}

	cairo_surface_destroy(composite);
}

/* ------------------------------------------------------------------ */
/* Test: image_compare_surfaces identity check                         */
/* ------------------------------------------------------------------ */

static void
test_image_compare_identity(void)
{
	TEST_START("image_compare_identity");

	/* An image should be identical to itself */
	cairo_surface_t *surface = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, 64, 64);
	cairo_t *cr = cairo_create(surface);
	cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 1.0);
	cairo_paint(cr);
	cairo_destroy(cr);

	struct image_compare_result result = image_compare_surfaces(
		surface, surface, 0.0, 0);

	if (result.match && result.diff_pixels == 0) {
		TEST_PASS();
	} else {
		char msg[128];
		snprintf(msg, sizeof(msg),
			 "identity comparison failed: %d diff pixels",
			 result.diff_pixels);
		TEST_FAIL(msg);
	}

	cairo_surface_destroy(surface);
}

/* ------------------------------------------------------------------ */
/* Test: image_compare_surfaces difference detection                   */
/* ------------------------------------------------------------------ */

static void
test_image_compare_difference(void)
{
	TEST_START("image_compare_difference");

	/* Two completely different images should fail comparison */
	cairo_surface_t *red = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, 64, 64);
	cairo_t *cr1 = cairo_create(red);
	cairo_set_source_rgba(cr1, 1.0, 0.0, 0.0, 1.0);
	cairo_paint(cr1);
	cairo_destroy(cr1);

	cairo_surface_t *blue = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, 64, 64);
	cairo_t *cr2 = cairo_create(blue);
	cairo_set_source_rgba(cr2, 0.0, 0.0, 1.0, 1.0);
	cairo_paint(cr2);
	cairo_destroy(cr2);

	struct image_compare_result result = image_compare_surfaces(
		red, blue, 1.0, 2);

	if (!result.match && result.diff_percent > 99.0) {
		TEST_PASS();
	} else {
		char msg[128];
		snprintf(msg, sizeof(msg),
			 "expected mismatch but got match=%d diff=%.2f%%",
			 result.match, result.diff_percent);
		TEST_FAIL(msg);
	}

	cairo_surface_destroy(red);
	cairo_surface_destroy(blue);
}

/* ------------------------------------------------------------------ */
/* Test: image_compare_surfaces dimension mismatch                     */
/* ------------------------------------------------------------------ */

static void
test_image_compare_size_mismatch(void)
{
	TEST_START("image_compare_size_mismatch");

	cairo_surface_t *small = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, 32, 32);
	cairo_surface_t *large = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, 64, 64);

	struct image_compare_result result = image_compare_surfaces(
		small, large, 100.0, 255);

	if (!result.match) {
		TEST_PASS();
	} else {
		TEST_FAIL("expected mismatch for different-sized images");
	}

	cairo_surface_destroy(small);
	cairo_surface_destroy(large);
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

int
main(void)
{
	printf("test_decorations (visual):\n");

	setup();

	/* Image comparison library tests */
	test_image_compare_identity();
	test_image_compare_difference();
	test_image_compare_size_mismatch();

	/* Rendering tests (compare against reference images) */
	test_solid_texture();
	test_gradient_texture();
	test_horizontal_gradient();
	test_sunken_texture();
	test_interlaced_texture();
	test_button_glyphs();
	test_text_rendering();
	test_titlebar_composition();

	printf("  Results: %d/%d passed", tests_passed, tests_run);
	if (tests_failed > 0)
		printf(", %d FAILED", tests_failed);
	printf("\n");

	return tests_failed > 0 ? 1 : 0;
}
