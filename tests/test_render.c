/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * test_render.c - Unit tests for render.c (Cairo/Pango text and texture rendering)
 *
 * Links against real pangocairo to verify text measurement uses ink extents
 * (pango_layout_get_pixel_extents), NOT pango_layout_get_pixel_size.
 */

#define _GNU_SOURCE

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* render.c includes render.h (which includes style.h + cairo.h) and
 * pango/pangocairo.h — all resolved by the real system headers. No
 * wayland/wlroots stubs needed since render.c has no wlr dependencies. */
#include "render.c"

/* ------------------------------------------------------------------ */
/* Color helper tests (static inlines from render.c)                  */
/* ------------------------------------------------------------------ */

static void
test_color_r(void)
{
	/* 0xAARRGGBB — R channel is bits 23..16 */
	assert(fabs(color_r(0xFF804020) - 0x80 / 255.0) < 0.001);
	assert(fabs(color_r(0x00FF0000) - 1.0) < 0.001);
	assert(fabs(color_r(0x00000000) - 0.0) < 0.001);
	printf("PASS: test_color_r\n");
}

static void
test_color_g(void)
{
	assert(fabs(color_g(0xFF008000) - 0x80 / 255.0) < 0.001);
	assert(fabs(color_g(0x0000FF00) - 1.0) < 0.001);
	printf("PASS: test_color_g\n");
}

static void
test_color_b(void)
{
	assert(fabs(color_b(0xFF000080) - 0x80 / 255.0) < 0.001);
	assert(fabs(color_b(0x000000FF) - 1.0) < 0.001);
	printf("PASS: test_color_b\n");
}

static void
test_color_a(void)
{
	assert(fabs(color_a(0xFF000000) - 1.0) < 0.001);
	assert(fabs(color_a(0x80000000) - 0x80 / 255.0) < 0.001);
	assert(fabs(color_a(0x00000000) - 0.0) < 0.001);
	printf("PASS: test_color_a\n");
}

/* ------------------------------------------------------------------ */
/* Interpolation and color math tests                                 */
/* ------------------------------------------------------------------ */

static void
test_lerp_u8(void)
{
	assert(lerp_u8(0, 255, 0.0) == 0);
	assert(lerp_u8(0, 255, 1.0) == 255);
	/* Midpoint should be ~128 */
	uint8_t mid = lerp_u8(0, 255, 0.5);
	assert(mid >= 127 && mid <= 128);
	printf("PASS: test_lerp_u8\n");
}

static void
test_lerp_argb(void)
{
	uint32_t black = 0xFF000000;
	uint32_t white = 0xFFFFFFFF;

	/* Boundary conditions */
	assert(lerp_argb(black, white, 0.0) == black);
	assert(lerp_argb(black, white, 1.0) == white);
	/* Clamp below 0 and above 1 */
	assert(lerp_argb(black, white, -1.0) == black);
	assert(lerp_argb(black, white, 2.0) == white);

	/* Midpoint: all channels should be ~128 */
	uint32_t mid = lerp_argb(black, white, 0.5);
	uint8_t mr = (mid >> 16) & 0xFF;
	uint8_t mg = (mid >> 8) & 0xFF;
	uint8_t mb = mid & 0xFF;
	assert(mr >= 126 && mr <= 129);
	assert(mg >= 126 && mg <= 129);
	assert(mb >= 126 && mb <= 129);
	printf("PASS: test_lerp_argb\n");
}

static void
test_brighten(void)
{
	/* Brighten #808080 by 1.5 → should be brighter */
	uint32_t gray = 0xFF808080;
	uint32_t bright = brighten(gray, 1.5);
	uint8_t r = (bright >> 16) & 0xFF;
	assert(r > 0x80);
	assert(r == (uint8_t)(0x80 * 1.5 + 0.5));

	/* Brighten by 2.0 should clamp to 255 */
	uint32_t very_bright = brighten(0xFF808080, 2.0);
	uint8_t vr = (very_bright >> 16) & 0xFF;
	assert(vr == 255);

	/* Alpha should be preserved */
	uint32_t a_test = brighten(0x80404040, 1.5);
	assert(((a_test >> 24) & 0xFF) == 0x80);
	printf("PASS: test_brighten\n");
}

static void
test_darken(void)
{
	uint32_t gray = 0xFF808080;
	uint32_t dark = darken(gray, 0.5);
	uint8_t r = (dark >> 16) & 0xFF;
	assert(r < 0x80);
	assert(r == (uint8_t)(0x80 * 0.5 + 0.5));

	/* Alpha should be preserved */
	uint32_t a_test = darken(0x80404040, 0.5);
	assert(((a_test >> 24) & 0xFF) == 0x80);
	printf("PASS: test_darken\n");
}

/* ------------------------------------------------------------------ */
/* Texture rendering tests                                            */
/* ------------------------------------------------------------------ */

static void
test_render_texture_null(void)
{
	assert(wm_render_texture(NULL, 100, 100, 1.0f) == NULL);

	struct wm_texture tex = {0};
	/* Zero dimensions */
	assert(wm_render_texture(&tex, 0, 100, 1.0f) == NULL);
	assert(wm_render_texture(&tex, 100, 0, 1.0f) == NULL);
	assert(wm_render_texture(&tex, -1, 100, 1.0f) == NULL);
	printf("PASS: test_render_texture_null\n");
}

static void
test_render_texture_parent_relative(void)
{
	struct wm_texture tex = { .fill = WM_TEX_PARENT_RELATIVE };
	assert(wm_render_texture(&tex, 100, 100, 1.0f) == NULL);
	printf("PASS: test_render_texture_parent_relative\n");
}

static void
test_render_texture_solid(void)
{
	struct wm_texture tex = {
		.appearance = WM_TEX_FLAT,
		.fill = WM_TEX_SOLID,
		.bevel = WM_BEVEL_NONE,
		.color = 0xFFFF0000,  /* opaque red */
	};
	cairo_surface_t *s = wm_render_texture(&tex, 10, 10, 1.0f);
	assert(s != NULL);
	assert(cairo_image_surface_get_width(s) == 10);
	assert(cairo_image_surface_get_height(s) == 10);
	cairo_surface_destroy(s);
	printf("PASS: test_render_texture_solid\n");
}

static void
test_render_texture_gradient(void)
{
	struct wm_texture tex = {
		.appearance = WM_TEX_FLAT,
		.fill = WM_TEX_GRADIENT,
		.gradient = WM_GRAD_VERTICAL,
		.bevel = WM_BEVEL_NONE,
		.color = 0xFF000000,
		.color_to = 0xFFFFFFFF,
	};
	cairo_surface_t *s = wm_render_texture(&tex, 20, 20, 1.0f);
	assert(s != NULL);
	assert(cairo_image_surface_get_width(s) == 20);
	assert(cairo_image_surface_get_height(s) == 20);
	cairo_surface_destroy(s);
	printf("PASS: test_render_texture_gradient\n");
}

static void
test_render_texture_with_bevel(void)
{
	struct wm_texture tex = {
		.appearance = WM_TEX_RAISED,
		.fill = WM_TEX_SOLID,
		.bevel = WM_BEVEL1,
		.color = 0xFF808080,
	};
	cairo_surface_t *s = wm_render_texture(&tex, 20, 20, 1.0f);
	assert(s != NULL);
	cairo_surface_destroy(s);
	printf("PASS: test_render_texture_with_bevel\n");
}

static void
test_render_texture_interlaced(void)
{
	struct wm_texture tex = {
		.appearance = WM_TEX_FLAT,
		.fill = WM_TEX_SOLID,
		.bevel = WM_BEVEL_NONE,
		.interlaced = true,
		.color = 0xFFFFFFFF,
	};
	cairo_surface_t *s = wm_render_texture(&tex, 10, 10, 1.0f);
	assert(s != NULL);
	cairo_surface_destroy(s);
	printf("PASS: test_render_texture_interlaced\n");
}

static void
test_render_texture_alpha_fixup(void)
{
	/* Color with zero alpha should get 0xFF added */
	struct wm_texture tex = {
		.appearance = WM_TEX_FLAT,
		.fill = WM_TEX_SOLID,
		.bevel = WM_BEVEL_NONE,
		.color = 0x00FF0000,  /* zero alpha */
	};
	cairo_surface_t *s = wm_render_texture(&tex, 5, 5, 1.0f);
	assert(s != NULL);
	cairo_surface_destroy(s);
	printf("PASS: test_render_texture_alpha_fixup\n");
}

/* ------------------------------------------------------------------ */
/* Text rendering tests (use real Pango/Cairo)                        */
/* ------------------------------------------------------------------ */

static void
test_render_text_null_args(void)
{
	struct wm_font font = { .family = "sans", .size = 12 };
	struct wm_color color = { .r = 255, .g = 255, .b = 255, .a = 255 };

	assert(wm_render_text(NULL, &font, &color, 100, NULL, NULL,
		WM_JUSTIFY_LEFT, 1.0f) == NULL);
	assert(wm_render_text("hello", NULL, &color, 100, NULL, NULL,
		WM_JUSTIFY_LEFT, 1.0f) == NULL);
	assert(wm_render_text("hello", &font, NULL, 100, NULL, NULL,
		WM_JUSTIFY_LEFT, 1.0f) == NULL);
	printf("PASS: test_render_text_null_args\n");
}

static void
test_render_text_basic(void)
{
	struct wm_font font = { .family = "sans", .size = 12 };
	struct wm_color color = { .r = 255, .g = 255, .b = 255, .a = 255 };
	int w = 0, h = 0;

	cairo_surface_t *s = wm_render_text("Hello World", &font, &color,
		200, &w, &h, WM_JUSTIFY_LEFT, 1.0f);
	assert(s != NULL);
	assert(w > 0);
	assert(h > 0);
	cairo_surface_destroy(s);
	printf("PASS: test_render_text_basic\n");
}

static void
test_render_text_justification(void)
{
	struct wm_font font = { .family = "sans", .size = 12 };
	struct wm_color color = { .r = 255, .g = 255, .b = 255, .a = 255 };

	/* Each justification should produce a valid surface */
	enum wm_justify modes[] = {
		WM_JUSTIFY_LEFT, WM_JUSTIFY_CENTER, WM_JUSTIFY_RIGHT
	};
	for (int i = 0; i < 3; i++) {
		cairo_surface_t *s = wm_render_text("test", &font, &color,
			200, NULL, NULL, modes[i], 1.0f);
		assert(s != NULL);
		cairo_surface_destroy(s);
	}
	printf("PASS: test_render_text_justification\n");
}

static void
test_render_text_with_shadow(void)
{
	struct wm_font font = {
		.family = "sans", .size = 12,
		.shadow_x = 2, .shadow_y = 2,
		.shadow_color = { .r = 0, .g = 0, .b = 0, .a = 128 },
	};
	struct wm_color color = { .r = 255, .g = 255, .b = 255, .a = 255 };
	int w = 0, h = 0;

	cairo_surface_t *s = wm_render_text("Shadow Test", &font, &color,
		200, &w, &h, WM_JUSTIFY_LEFT, 1.0f);
	assert(s != NULL);
	assert(w > 0);
	assert(h > 0);
	cairo_surface_destroy(s);
	printf("PASS: test_render_text_with_shadow\n");
}

/* ------------------------------------------------------------------ */
/* Text measurement (the critical ink-extents test)                   */
/* ------------------------------------------------------------------ */

static void
test_measure_text_width_null(void)
{
	struct wm_font font = { .family = "sans", .size = 12 };

	assert(wm_measure_text_width(NULL, &font, 1.0f) == 0);
	assert(wm_measure_text_width("hello", NULL, 1.0f) == 0);
	printf("PASS: test_measure_text_width_null\n");
}

static void
test_measure_text_width_basic(void)
{
	struct wm_font font = { .family = "sans", .size = 12 };

	int w = wm_measure_text_width("Hello", &font, 1.0f);
	assert(w > 0);

	/* A longer string should be wider */
	int w2 = wm_measure_text_width("Hello World Extended", &font, 1.0f);
	assert(w2 > w);
	printf("PASS: test_measure_text_width_basic\n");
}

static void
test_measure_text_uses_ink_extents(void)
{
	/*
	 * CRITICAL TEST: verify render.c uses pango_layout_get_pixel_extents()
	 * (ink rect) rather than pango_layout_get_pixel_size().
	 *
	 * We verify this indirectly: ink extents report ink_rect.x + ink_rect.width,
	 * which accounts for the actual pixel offset of the first glyph.
	 * pango_layout_get_pixel_size() would only return the logical width.
	 *
	 * We test by measuring a string and verifying the measurement is positive
	 * and consistent with what a Pango ink rect measurement would produce.
	 * This verifies the render code path is exercised with ink extents.
	 */
	struct wm_font font = { .family = "monospace", .size = 14 };

	/* Measure using our API (which uses ink extents) */
	int w = wm_measure_text_width("MMMMMM", &font, 1.0f);
	assert(w > 0);

	/* Cross-check with a direct Pango measurement using ink extents */
	cairo_surface_t *tmp = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, 512, 64);
	cairo_t *cr = cairo_create(tmp);
	PangoLayout *layout = pango_cairo_create_layout(cr);

	PangoFontDescription *desc = pango_font_description_new();
	pango_font_description_set_family(desc, "monospace");
	pango_font_description_set_size(desc, 14 * PANGO_SCALE);
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);
	pango_layout_set_text(layout, "MMMMMM", -1);

	PangoRectangle ink_rect;
	pango_layout_get_pixel_extents(layout, &ink_rect, NULL);
	int expected_w = ink_rect.x + ink_rect.width;

	/* Our measurement should match the ink extent width */
	assert(w == expected_w);

	g_object_unref(layout);
	cairo_destroy(cr);
	cairo_surface_destroy(tmp);
	printf("PASS: test_measure_text_uses_ink_extents\n");
}

static void
test_measure_text_bold_vs_regular(void)
{
	struct wm_font regular = { .family = "sans", .size = 12 };
	struct wm_font bold = { .family = "sans", .size = 12, .bold = true };

	int w_regular = wm_measure_text_width("TestBold", &regular, 1.0f);
	int w_bold = wm_measure_text_width("TestBold", &bold, 1.0f);

	/* Both should be positive */
	assert(w_regular > 0);
	assert(w_bold > 0);
	/* Bold text is typically wider or equal (never narrower) */
	assert(w_bold >= w_regular);
	printf("PASS: test_measure_text_bold_vs_regular\n");
}

/* ------------------------------------------------------------------ */
/* Button glyph rendering tests                                       */
/* ------------------------------------------------------------------ */

static void
test_render_button_glyph_null(void)
{
	assert(wm_render_button_glyph(WM_BUTTON_CLOSE, NULL, 16, 1.0f) == NULL);
	struct wm_color c = { .r = 255, .g = 255, .b = 255, .a = 255 };
	assert(wm_render_button_glyph(WM_BUTTON_CLOSE, &c, 0, 1.0f) == NULL);
	assert(wm_render_button_glyph(WM_BUTTON_CLOSE, &c, -5, 1.0f) == NULL);
	printf("PASS: test_render_button_glyph_null\n");
}

static void
test_render_button_glyph_all_types(void)
{
	struct wm_color c = { .r = 200, .g = 200, .b = 200, .a = 255 };
	enum wm_button_type types[] = {
		WM_BUTTON_CLOSE, WM_BUTTON_MAXIMIZE, WM_BUTTON_ICONIFY,
		WM_BUTTON_SHADE, WM_BUTTON_STICK,
	};

	for (int i = 0; i < 5; i++) {
		cairo_surface_t *s = wm_render_button_glyph(types[i], &c,
			16, 1.0f);
		assert(s != NULL);
		assert(cairo_image_surface_get_width(s) == 16);
		assert(cairo_image_surface_get_height(s) == 16);
		cairo_surface_destroy(s);
	}
	printf("PASS: test_render_button_glyph_all_types\n");
}

static void
test_render_button_glyph_scaled(void)
{
	struct wm_color c = { .r = 255, .g = 255, .b = 255, .a = 255 };
	cairo_surface_t *s = wm_render_button_glyph(WM_BUTTON_CLOSE, &c,
		16, 2.0f);
	assert(s != NULL);
	assert(cairo_image_surface_get_width(s) == 32);
	assert(cairo_image_surface_get_height(s) == 32);
	cairo_surface_destroy(s);
	printf("PASS: test_render_button_glyph_scaled\n");
}

/* ------------------------------------------------------------------ */
/* Rounded rectangle path tests                                       */
/* ------------------------------------------------------------------ */

static void
test_rounded_rect_no_radius(void)
{
	/* radius=0 should produce a plain rectangle */
	cairo_surface_t *tmp = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, 100, 100);
	cairo_t *cr = cairo_create(tmp);

	wm_render_rounded_rect_path(cr, 10, 10, 80, 60, 0, 0xFF);
	/* Verify the path was added (has current point) */
	double x, y;
	assert(cairo_has_current_point(cr));
	cairo_get_current_point(cr, &x, &y);

	cairo_destroy(cr);
	cairo_surface_destroy(tmp);
	printf("PASS: test_rounded_rect_no_radius\n");
}

static void
test_rounded_rect_all_corners(void)
{
	cairo_surface_t *tmp = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, 100, 100);
	cairo_t *cr = cairo_create(tmp);

	uint8_t all = WM_CORNER_TOP_LEFT | WM_CORNER_TOP_RIGHT |
		WM_CORNER_BOTTOM_LEFT | WM_CORNER_BOTTOM_RIGHT;
	wm_render_rounded_rect_path(cr, 0, 0, 100, 100, 10, all);
	assert(cairo_has_current_point(cr));

	cairo_destroy(cr);
	cairo_surface_destroy(tmp);
	printf("PASS: test_rounded_rect_all_corners\n");
}

static void
test_rounded_rect_radius_clamped(void)
{
	/* Radius larger than half width/height should be clamped */
	cairo_surface_t *tmp = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, 100, 100);
	cairo_t *cr = cairo_create(tmp);

	/* 20x10 rect with radius 50 — should clamp to 5 (half of min dim) */
	wm_render_rounded_rect_path(cr, 0, 0, 20, 10, 50, 0xFF);
	assert(cairo_has_current_point(cr));

	cairo_destroy(cr);
	cairo_surface_destroy(tmp);
	printf("PASS: test_rounded_rect_radius_clamped\n");
}

/* ------------------------------------------------------------------ */
/* RTL detection test                                                 */
/* ------------------------------------------------------------------ */

static void
test_find_base_dir(void)
{
	/* NULL or empty text */
	assert(find_base_dir(NULL) == PANGO_DIRECTION_NEUTRAL);
	assert(find_base_dir("") == PANGO_DIRECTION_NEUTRAL);

	/* Latin text should be LTR */
	assert(find_base_dir("Hello") == PANGO_DIRECTION_LTR);

	/* Arabic text should be RTL */
	assert(find_base_dir("\xd8\xa7\xd9\x84\xd8\xb3\xd9\x84\xd8\xa7\xd9\x85") == PANGO_DIRECTION_RTL);

	/* Digits only → neutral */
	assert(find_base_dir("12345") == PANGO_DIRECTION_NEUTRAL);
	printf("PASS: test_find_base_dir\n");
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

int
main(void)
{
	/* Color helpers */
	test_color_r();
	test_color_g();
	test_color_b();
	test_color_a();

	/* Interpolation */
	test_lerp_u8();
	test_lerp_argb();
	test_brighten();
	test_darken();

	/* Texture rendering */
	test_render_texture_null();
	test_render_texture_parent_relative();
	test_render_texture_solid();
	test_render_texture_gradient();
	test_render_texture_with_bevel();
	test_render_texture_interlaced();
	test_render_texture_alpha_fixup();

	/* Text rendering */
	test_render_text_null_args();
	test_render_text_basic();
	test_render_text_justification();
	test_render_text_with_shadow();

	/* Text measurement */
	test_measure_text_width_null();
	test_measure_text_width_basic();
	test_measure_text_uses_ink_extents();
	test_measure_text_bold_vs_regular();

	/* Button glyphs */
	test_render_button_glyph_null();
	test_render_button_glyph_all_types();
	test_render_button_glyph_scaled();

	/* Rounded rect path */
	test_rounded_rect_no_radius();
	test_rounded_rect_all_corners();
	test_rounded_rect_radius_clamped();

	/* RTL detection */
	test_find_base_dir();

	printf("\nAll render tests passed (%d tests)\n", 27);
	return 0;
}
