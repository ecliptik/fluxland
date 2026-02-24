/*
 * test_style.c - Unit tests for style/theme parser
 */

#define _POSIX_C_SOURCE 200809L

#include "style.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST_DIR "/tmp/fluxland-test/wm-test-style"
#define TEST_STYLE TEST_DIR "/theme"

static void
setup(void)
{
	mkdir("/tmp/fluxland-test", 0755);
	mkdir(TEST_DIR, 0755);
}

static void
cleanup(void)
{
	unlink(TEST_STYLE);
	rmdir(TEST_DIR);
}

static void
write_file(const char *path, const char *contents)
{
	FILE *f = fopen(path, "w");
	assert(f);
	fputs(contents, f);
	fclose(f);
}

/* Test: style_create returns valid defaults, style_destroy frees cleanly */
static void
test_create_destroy(void)
{
	struct wm_style *s = style_create();
	assert(s != NULL);

	/* Check default dimensions */
	assert(s->window_border_width == 1);
	assert(s->window_bevel_width == 1);
	assert(s->window_handle_width == 6);
	assert(s->window_title_height == 0);
	assert(s->menu_border_width == 1);

	/* Check default justification */
	assert(s->window_justify == WM_JUSTIFY_LEFT);
	assert(s->menu_title_justify == WM_JUSTIFY_CENTER);

	/* Check default font */
	assert(s->window_label_focus_font.family != NULL);
	assert(strcmp(s->window_label_focus_font.family, "sans") == 0);
	assert(s->window_label_focus_font.size == 10);
	assert(s->window_label_focus_font.bold == false);
	assert(s->window_label_focus_font.italic == false);

	/* Default textures should be Raised Gradient Vertical */
	assert(s->window_title_focus.appearance == WM_TEX_RAISED);
	assert(s->window_title_focus.fill == WM_TEX_GRADIENT);
	assert(s->window_title_focus.gradient == WM_GRAD_VERTICAL);
	assert(s->window_title_focus.bevel == WM_BEVEL1);

	style_destroy(s);
	printf("  PASS: test_create_destroy\n");
}

/* Test: style_destroy(NULL) is safe */
static void
test_destroy_null(void)
{
	style_destroy(NULL);
	printf("  PASS: test_destroy_null\n");
}

/* Test: color parsing - #RRGGBB */
static void
test_color_hex(void)
{
	struct wm_color c;

	c = style_parse_color("#FF0000");
	assert(c.r == 255 && c.g == 0 && c.b == 0 && c.a == 255);

	c = style_parse_color("#00FF00");
	assert(c.r == 0 && c.g == 255 && c.b == 0);

	c = style_parse_color("#0000FF");
	assert(c.r == 0 && c.g == 0 && c.b == 255);

	c = style_parse_color("#1A2B3C");
	assert(c.r == 0x1A && c.g == 0x2B && c.b == 0x3C);

	printf("  PASS: test_color_hex\n");
}

/* Test: color parsing - short #RGB form */
static void
test_color_short_hex(void)
{
	struct wm_color c = style_parse_color("#F00");
	assert(c.r == 255 && c.g == 0 && c.b == 0);

	c = style_parse_color("#ABC");
	assert(c.r == 0xAA && c.g == 0xBB && c.b == 0xCC);

	printf("  PASS: test_color_short_hex\n");
}

/* Test: color parsing - named colors */
static void
test_color_named(void)
{
	struct wm_color c;

	c = style_parse_color("white");
	assert(c.r == 255 && c.g == 255 && c.b == 255);

	c = style_parse_color("black");
	assert(c.r == 0 && c.g == 0 && c.b == 0);

	c = style_parse_color("red");
	assert(c.r == 255 && c.g == 0 && c.b == 0);

	c = style_parse_color("green");
	assert(c.r == 0 && c.g == 255 && c.b == 0);

	c = style_parse_color("blue");
	assert(c.r == 0 && c.g == 0 && c.b == 255);

	c = style_parse_color("grey");
	assert(c.r == 190 && c.g == 190 && c.b == 190);

	c = style_parse_color("gray");
	assert(c.r == 190 && c.g == 190 && c.b == 190);

	printf("  PASS: test_color_named\n");
}

/* Test: color parsing - NULL and empty */
static void
test_color_null_empty(void)
{
	struct wm_color c;

	c = style_parse_color(NULL);
	assert(c.r == 0 && c.g == 0 && c.b == 0);

	c = style_parse_color("");
	assert(c.r == 0 && c.g == 0 && c.b == 0);

	printf("  PASS: test_color_null_empty\n");
}

/* Test: color parsing - rgb:R/G/B format */
static void
test_color_rgb_format(void)
{
	struct wm_color c = style_parse_color("rgb:FF/00/FF");
	assert(c.r == 255 && c.g == 0 && c.b == 255);

	printf("  PASS: test_color_rgb_format\n");
}

/* Test: texture parsing - Flat Solid */
static void
test_texture_flat_solid(void)
{
	struct wm_texture t = style_parse_texture("Flat Solid");
	assert(t.appearance == WM_TEX_FLAT);
	assert(t.fill == WM_TEX_SOLID);
	assert(t.bevel == WM_BEVEL_NONE); /* flat => no bevel by default */

	printf("  PASS: test_texture_flat_solid\n");
}

/* Test: texture parsing - Raised Gradient Vertical */
static void
test_texture_raised_gradient(void)
{
	struct wm_texture t = style_parse_texture("Raised Gradient Vertical");
	assert(t.appearance == WM_TEX_RAISED);
	assert(t.fill == WM_TEX_GRADIENT);
	assert(t.gradient == WM_GRAD_VERTICAL);
	assert(t.bevel == WM_BEVEL1);

	printf("  PASS: test_texture_raised_gradient\n");
}

/* Test: texture parsing - Sunken Gradient Horizontal */
static void
test_texture_sunken(void)
{
	struct wm_texture t = style_parse_texture("Sunken Gradient Horizontal");
	assert(t.appearance == WM_TEX_SUNKEN);
	assert(t.fill == WM_TEX_GRADIENT);
	assert(t.gradient == WM_GRAD_HORIZONTAL);

	printf("  PASS: test_texture_sunken\n");
}

/* Test: texture parsing - gradient types */
static void
test_texture_gradient_types(void)
{
	struct wm_texture t;

	t = style_parse_texture("Flat Gradient Diagonal");
	assert(t.gradient == WM_GRAD_DIAGONAL);

	t = style_parse_texture("Flat Gradient CrossDiagonal");
	assert(t.gradient == WM_GRAD_CROSSDIAGONAL);

	t = style_parse_texture("Flat Gradient PipeCross");
	assert(t.gradient == WM_GRAD_PIPECROSS);

	t = style_parse_texture("Flat Gradient Elliptic");
	assert(t.gradient == WM_GRAD_ELLIPTIC);

	t = style_parse_texture("Flat Gradient Rectangle");
	assert(t.gradient == WM_GRAD_RECTANGLE);

	t = style_parse_texture("Flat Gradient Pyramid");
	assert(t.gradient == WM_GRAD_PYRAMID);

	printf("  PASS: test_texture_gradient_types\n");
}

/* Test: texture parsing - interlaced flag */
static void
test_texture_interlaced(void)
{
	struct wm_texture t = style_parse_texture("Raised Gradient Vertical Interlaced");
	assert(t.interlaced == true);
	assert(t.appearance == WM_TEX_RAISED);
	assert(t.fill == WM_TEX_GRADIENT);

	printf("  PASS: test_texture_interlaced\n");
}

/* Test: texture parsing - bevel types */
static void
test_texture_bevel(void)
{
	struct wm_texture t;

	t = style_parse_texture("Raised Solid Bevel1");
	assert(t.bevel == WM_BEVEL1);

	t = style_parse_texture("Raised Solid Bevel2");
	assert(t.bevel == WM_BEVEL2);

	/* Flat with explicit bevel */
	t = style_parse_texture("Flat Solid Bevel1");
	assert(t.bevel == WM_BEVEL1);

	printf("  PASS: test_texture_bevel\n");
}

/* Test: texture parsing - ParentRelative */
static void
test_texture_parent_relative(void)
{
	struct wm_texture t = style_parse_texture("ParentRelative");
	assert(t.fill == WM_TEX_PARENT_RELATIVE);
	assert(t.appearance == WM_TEX_FLAT);
	assert(t.bevel == WM_BEVEL_NONE);

	printf("  PASS: test_texture_parent_relative\n");
}

/* Test: texture parsing - NULL and empty */
static void
test_texture_null_empty(void)
{
	struct wm_texture t;

	t = style_parse_texture(NULL);
	/* Should return default */
	assert(t.appearance == WM_TEX_RAISED);
	assert(t.fill == WM_TEX_SOLID);

	t = style_parse_texture("");
	assert(t.appearance == WM_TEX_RAISED);

	printf("  PASS: test_texture_null_empty\n");
}

/* Test: font parsing */
static void
test_font_parsing(void)
{
	struct wm_font f;

	f = style_parse_font("monospace-12:bold");
	assert(f.family != NULL);
	assert(strcmp(f.family, "monospace") == 0);
	assert(f.size == 12);
	assert(f.bold == true);
	assert(f.italic == false);
	free(f.family);

	f = style_parse_font("serif-8:italic");
	assert(strcmp(f.family, "serif") == 0);
	assert(f.size == 8);
	assert(f.bold == false);
	assert(f.italic == true);
	free(f.family);

	f = style_parse_font("DejaVu Sans-14:bold:italic");
	assert(f.size == 14);
	assert(f.bold == true);
	assert(f.italic == true);
	free(f.family);

	/* Just a family name, no size or style */
	f = style_parse_font("sans");
	assert(strcmp(f.family, "sans") == 0);
	assert(f.size == 10); /* default */
	assert(f.bold == false);
	free(f.family);

	/* NULL */
	f = style_parse_font(NULL);
	assert(strcmp(f.family, "sans") == 0);
	assert(f.size == 10);
	free(f.family);

	printf("  PASS: test_font_parsing\n");
}

/* Test: style_load from file */
static void
test_load_style_file(void)
{
	write_file(TEST_STYLE,
		"window.title.focus: Raised Gradient Horizontal\n"
		"window.title.focus.color: #445566\n"
		"window.title.focus.colorTo: #112233\n"
		"window.label.focus.textColor: #FFFFFF\n"
		"window.borderWidth: 2\n"
		"window.bevelWidth: 3\n"
		"window.handleWidth: 8\n"
		"window.title.height: 22\n"
		"menu.title: Flat Solid\n"
		"menu.title.textColor: #AABBCC\n"
		"menu.borderWidth: 0\n"
		"window.label.focus.font: monospace-11:bold\n"
		"window.justify: Center\n"
	);

	struct wm_style *s = style_create();
	assert(s != NULL);

	int ret = style_load(s, TEST_STYLE);
	assert(ret == 0);

	/* Check loaded values */
	assert(s->window_title_focus.appearance == WM_TEX_RAISED);
	assert(s->window_title_focus.fill == WM_TEX_GRADIENT);
	assert(s->window_title_focus.gradient == WM_GRAD_HORIZONTAL);

	/* Check title color */
	struct wm_color tc = wm_argb_to_color(s->window_title_focus.color);
	assert(tc.r == 0x44 && tc.g == 0x55 && tc.b == 0x66);

	/* Check text color */
	assert(s->window_label_focus_text_color.r == 0xFF);
	assert(s->window_label_focus_text_color.g == 0xFF);
	assert(s->window_label_focus_text_color.b == 0xFF);

	/* Check dimensions */
	assert(s->window_border_width == 2);
	assert(s->window_bevel_width == 3);
	assert(s->window_handle_width == 8);
	assert(s->window_title_height == 22);
	assert(s->menu_border_width == 0);

	/* Check menu title */
	assert(s->menu_title.appearance == WM_TEX_FLAT);
	assert(s->menu_title.fill == WM_TEX_SOLID);
	assert(s->menu_title_text_color.r == 0xAA);
	assert(s->menu_title_text_color.g == 0xBB);
	assert(s->menu_title_text_color.b == 0xCC);

	/* Check font */
	assert(s->window_label_focus_font.family != NULL);
	assert(strcmp(s->window_label_focus_font.family, "monospace") == 0);
	assert(s->window_label_focus_font.size == 11);
	assert(s->window_label_focus_font.bold == true);

	/* Check justify */
	assert(s->window_justify == WM_JUSTIFY_CENTER);

	style_destroy(s);
	printf("  PASS: test_load_style_file\n");
}

/* Test: style_load with nonexistent file returns error */
static void
test_load_nonexistent(void)
{
	struct wm_style *s = style_create();
	int ret = style_load(s, "/tmp/fluxland-test/nonexistent_style_file");
	assert(ret == -1);
	style_destroy(s);
	printf("  PASS: test_load_nonexistent\n");
}

/* Test: ARGB conversion round-trip */
static void
test_argb_conversion(void)
{
	struct wm_color c = { .r = 0x12, .g = 0x34, .b = 0x56, .a = 0xFF };
	uint32_t argb = wm_color_to_argb(c);
	assert(argb == 0xFF123456);

	struct wm_color c2 = wm_argb_to_color(argb);
	assert(c2.r == 0x12 && c2.g == 0x34 && c2.b == 0x56 && c2.a == 0xFF);

	printf("  PASS: test_argb_conversion\n");
}

/* Test: toolbar style loading */
static void
test_toolbar_style(void)
{
	write_file(TEST_STYLE,
		"toolbar: Flat Solid\n"
		"toolbar.textColor: #EEFFEE\n"
		"toolbar.font: terminus-9\n"
	);

	struct wm_style *s = style_create();
	style_load(s, TEST_STYLE);

	assert(s->toolbar_texture.appearance == WM_TEX_FLAT);
	assert(s->toolbar_texture.fill == WM_TEX_SOLID);
	assert(s->toolbar_text_color.r == 0xEE);
	assert(s->toolbar_text_color.g == 0xFF);
	assert(s->toolbar_text_color.b == 0xEE);
	assert(strcmp(s->toolbar_font.family, "terminus") == 0);
	assert(s->toolbar_font.size == 9);

	style_destroy(s);
	printf("  PASS: test_toolbar_style\n");
}

int
main(void)
{
	printf("test_style:\n");
	setup();

	test_create_destroy();
	test_destroy_null();
	test_color_hex();
	test_color_short_hex();
	test_color_named();
	test_color_null_empty();
	test_color_rgb_format();
	test_texture_flat_solid();
	test_texture_raised_gradient();
	test_texture_sunken();
	test_texture_gradient_types();
	test_texture_interlaced();
	test_texture_bevel();
	test_texture_parent_relative();
	test_texture_null_empty();
	test_font_parsing();
	test_load_style_file();
	test_load_nonexistent();
	test_argb_conversion();
	test_toolbar_style();

	cleanup();
	printf("All style tests passed.\n");
	return 0;
}
