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

/* Test: invalid color formats - should return black (0,0,0) */
static void
test_color_invalid_formats(void)
{
	struct wm_color c;

	/* Invalid hex chars */
	c = style_parse_color("#GGG");
	assert(c.r == 0 && c.g == 0 && c.b == 0);

	/* Too many hex digits (not matching either sscanf pattern) */
	c = style_parse_color("#ZZZZZ");
	assert(c.r == 0 && c.g == 0 && c.b == 0);

	/* Hash with no valid digits */
	c = style_parse_color("#");
	assert(c.r == 0 && c.g == 0 && c.b == 0);

	/* rgb: with invalid format */
	c = style_parse_color("rgb:ZZ/AA/BB");
	/* sscanf %x will fail on ZZ so returns black */
	assert(c.r == 0 && c.g == 0 && c.b == 0);

	/* Unrecognized named color */
	c = style_parse_color("magenta");
	assert(c.r == 0 && c.g == 0 && c.b == 0);

	/* Leading whitespace should be skipped */
	c = style_parse_color("  #FF0000");
	assert(c.r == 255 && c.g == 0 && c.b == 0);

	printf("  PASS: test_color_invalid_formats\n");
}

/* Test: font with 0 size uses default */
static void
test_font_zero_size(void)
{
	struct wm_font f = style_parse_font("mono-0");
	assert(strcmp(f.family, "mono") == 0);
	/* Size 0 fails the fsize > 0 check, so defaults to 10 */
	assert(f.size == 10);
	free(f.family);

	printf("  PASS: test_font_zero_size\n");
}

/* Test: font with no size (just family and style) */
static void
test_font_no_size(void)
{
	/* Family only, no dash-number, should use default size */
	struct wm_font f = style_parse_font("monospace:bold");
	assert(strcmp(f.family, "monospace") == 0);
	assert(f.size == 10); /* default */
	assert(f.bold == true);
	free(f.family);

	printf("  PASS: test_font_no_size\n");
}

/* Test: font with huge size is clamped */
static void
test_font_huge_size(void)
{
	struct wm_font f = style_parse_font("serif-999");
	assert(strcmp(f.family, "serif") == 0);
	/* 999 > MAX_FONT_SIZE (200), so defaults to 10 */
	assert(f.size == 10);
	free(f.family);

	printf("  PASS: test_font_huge_size\n");
}

/* Test: font with halo flag */
static void
test_font_halo(void)
{
	struct wm_font f = style_parse_font("sans-12:halo");
	assert(strcmp(f.family, "sans") == 0);
	assert(f.size == 12);
	assert(f.halo == true);
	free(f.family);

	printf("  PASS: test_font_halo\n");
}

/* Test: style_load with NULL arguments */
static void
test_style_load_null(void)
{
	struct wm_style *s = style_create();
	assert(s != NULL);

	int ret = style_load(s, NULL);
	assert(ret == -1);

	ret = style_load(NULL, TEST_STYLE);
	assert(ret == -1);

	ret = style_load(NULL, NULL);
	assert(ret == -1);

	style_destroy(s);
	printf("  PASS: test_style_load_null\n");
}

/* Test: style_load with empty file */
static void
test_style_load_empty_file(void)
{
	write_file(TEST_STYLE, "");

	struct wm_style *s = style_create();
	int ret = style_load(s, TEST_STYLE);
	assert(ret == 0);

	/* Defaults should still be intact */
	assert(s->window_border_width == 1);
	assert(s->window_bevel_width == 1);
	assert(s->window_handle_width == 6);
	assert(s->window_label_focus_font.size == 10);

	style_destroy(s);
	printf("  PASS: test_style_load_empty_file\n");
}

/* Test: unfocus textures and colors */
static void
test_unfocus_textures(void)
{
	write_file(TEST_STYLE,
		"window.title.unfocus: Flat Solid\n"
		"window.title.unfocus.color: #AABBCC\n"
		"window.label.unfocus: Sunken Gradient Horizontal\n"
		"window.label.unfocus.textColor: #112233\n"
		"window.button.unfocus: Flat Gradient Vertical\n"
		"window.button.unfocus.picColor: #445566\n"
		"window.handle.unfocus: Raised Solid\n"
		"window.grip.unfocus: Flat Solid\n"
		"window.frame.unfocusColor: #778899\n"
	);

	struct wm_style *s = style_create();
	int ret = style_load(s, TEST_STYLE);
	assert(ret == 0);

	assert(s->window_title_unfocus.appearance == WM_TEX_FLAT);
	assert(s->window_title_unfocus.fill == WM_TEX_SOLID);
	struct wm_color tc = wm_argb_to_color(s->window_title_unfocus.color);
	assert(tc.r == 0xAA && tc.g == 0xBB && tc.b == 0xCC);

	assert(s->window_label_unfocus.appearance == WM_TEX_SUNKEN);
	assert(s->window_label_unfocus.fill == WM_TEX_GRADIENT);
	assert(s->window_label_unfocus_text_color.r == 0x11);
	assert(s->window_label_unfocus_text_color.g == 0x22);
	assert(s->window_label_unfocus_text_color.b == 0x33);

	assert(s->window_button_unfocus.appearance == WM_TEX_FLAT);
	assert(s->window_button_unfocus_pic_color.r == 0x44);

	assert(s->window_handle_unfocus.appearance == WM_TEX_RAISED);
	assert(s->window_grip_unfocus.appearance == WM_TEX_FLAT);

	assert(s->window_frame_unfocus_color.r == 0x77);
	assert(s->window_frame_unfocus_color.g == 0x88);
	assert(s->window_frame_unfocus_color.b == 0x99);

	style_destroy(s);
	printf("  PASS: test_unfocus_textures\n");
}

/* Test: menu frame texture and colors */
static void
test_menu_frame_textures(void)
{
	write_file(TEST_STYLE,
		"menu.frame: Raised Gradient Diagonal\n"
		"menu.frame.color: #334455\n"
		"menu.frame.colorTo: #112233\n"
		"menu.frame.textColor: #DDEEFF\n"
		"menu.frame.font: monospace-11:italic\n"
		"menu.hilite: Sunken Solid\n"
		"menu.hilite.color: #FF0000\n"
		"menu.hilite.textColor: #FFFFFF\n"
		"menu.borderColor: #000000\n"
		"menu.borderWidth: 2\n"
		"menu.bullet: square\n"
		"menu.bullet.position: Left\n"
		"menu.itemHeight: 24\n"
		"menu.titleHeight: 28\n"
	);

	struct wm_style *s = style_create();
	int ret = style_load(s, TEST_STYLE);
	assert(ret == 0);

	assert(s->menu_frame.appearance == WM_TEX_RAISED);
	assert(s->menu_frame.fill == WM_TEX_GRADIENT);
	assert(s->menu_frame.gradient == WM_GRAD_DIAGONAL);

	struct wm_color fc = wm_argb_to_color(s->menu_frame.color);
	assert(fc.r == 0x33 && fc.g == 0x44 && fc.b == 0x55);

	assert(s->menu_frame_text_color.r == 0xDD);
	assert(s->menu_frame_text_color.g == 0xEE);
	assert(s->menu_frame_text_color.b == 0xFF);

	assert(s->menu_frame_font.italic == true);
	assert(strcmp(s->menu_frame_font.family, "monospace") == 0);
	assert(s->menu_frame_font.size == 11);

	assert(s->menu_hilite.appearance == WM_TEX_SUNKEN);
	assert(s->menu_hilite.fill == WM_TEX_SOLID);
	assert(s->menu_hilite_text_color.r == 0xFF);

	assert(s->menu_border_color.r == 0 && s->menu_border_color.g == 0);
	assert(s->menu_border_width == 2);

	assert(s->menu_bullet != NULL);
	assert(strcmp(s->menu_bullet, "square") == 0);
	assert(s->menu_bullet_position == WM_JUSTIFY_LEFT);

	assert(s->menu_item_height == 24);
	assert(s->menu_title_height == 28);

	style_destroy(s);
	printf("  PASS: test_menu_frame_textures\n");
}

/* Test: ARGB conversion with alpha=0 */
static void
test_argb_alpha_zero(void)
{
	struct wm_color c = { .r = 0x12, .g = 0x34, .b = 0x56, .a = 0x00 };
	uint32_t argb = wm_color_to_argb(c);
	assert(argb == 0x00123456);

	struct wm_color c2 = wm_argb_to_color(argb);
	assert(c2.r == 0x12 && c2.g == 0x34 && c2.b == 0x56 && c2.a == 0x00);

	/* Fully transparent black */
	struct wm_color zero = { .r = 0, .g = 0, .b = 0, .a = 0 };
	assert(wm_color_to_argb(zero) == 0x00000000);

	printf("  PASS: test_argb_alpha_zero\n");
}

/* Test: focus border width and color */
static void
test_focus_border(void)
{
	write_file(TEST_STYLE,
		"window.focus.border.width: 3\n"
		"window.focus.border.color: #88C0D0\n"
	);

	struct wm_style *s = style_create();
	int ret = style_load(s, TEST_STYLE);
	assert(ret == 0);

	assert(s->window_focus_border_width == 3);
	assert(s->window_focus_border_color.r == 0x88);
	assert(s->window_focus_border_color.g == 0xC0);
	assert(s->window_focus_border_color.b == 0xD0);

	style_destroy(s);
	printf("  PASS: test_focus_border\n");
}

/* Test: window round corners */
static void
test_round_corners(void)
{
	write_file(TEST_STYLE,
		"window.roundCorners: TopLeft TopRight BottomLeft BottomRight\n"
	);

	struct wm_style *s = style_create();
	int ret = style_load(s, TEST_STYLE);
	assert(ret == 0);

	assert(s->window_round_corners & WM_CORNER_TOP_LEFT);
	assert(s->window_round_corners & WM_CORNER_TOP_RIGHT);
	assert(s->window_round_corners & WM_CORNER_BOTTOM_LEFT);
	assert(s->window_round_corners & WM_CORNER_BOTTOM_RIGHT);

	style_destroy(s);
	printf("  PASS: test_round_corners\n");
}

/* Test: huge borderWidth in style file */
static void
test_huge_border_width(void)
{
	write_file(TEST_STYLE,
		"window.borderWidth: 999\n"
		"window.bevelWidth: 500\n"
		"window.handleWidth: 1000\n"
	);

	struct wm_style *s = style_create();
	int ret = style_load(s, TEST_STYLE);
	assert(ret == 0);

	/* style_get_int just parses; no clamping in style.c */
	assert(s->window_border_width == 999);
	assert(s->window_bevel_width == 500);
	assert(s->window_handle_width == 1000);

	style_destroy(s);
	printf("  PASS: test_huge_border_width\n");
}

/* Test: button pressed texture and picColor */
static void
test_button_pressed(void)
{
	write_file(TEST_STYLE,
		"window.button.pressed: Sunken Solid\n"
		"window.button.pressed.color: #AA0000\n"
		"window.button.pressed.picColor: #00FF00\n"
	);

	struct wm_style *s = style_create();
	int ret = style_load(s, TEST_STYLE);
	assert(ret == 0);

	assert(s->window_button_pressed.appearance == WM_TEX_SUNKEN);
	assert(s->window_button_pressed.fill == WM_TEX_SOLID);
	assert(s->window_button_pressed_pic_color.r == 0x00);
	assert(s->window_button_pressed_pic_color.g == 0xFF);
	assert(s->window_button_pressed_pic_color.b == 0x00);

	style_destroy(s);
	printf("  PASS: test_button_pressed\n");
}

/* Test: slit texture and border */
static void
test_slit_style(void)
{
	write_file(TEST_STYLE,
		"slit: Raised Gradient Horizontal\n"
		"slit.borderColor: #FF0000\n"
		"slit.borderWidth: 3\n"
	);

	struct wm_style *s = style_create();
	int ret = style_load(s, TEST_STYLE);
	assert(ret == 0);

	assert(s->slit_texture.appearance == WM_TEX_RAISED);
	assert(s->slit_texture.fill == WM_TEX_GRADIENT);
	assert(s->slit_texture.gradient == WM_GRAD_HORIZONTAL);
	assert(s->slit_border_color.r == 0xFF);
	assert(s->slit_border_width == 3);

	style_destroy(s);
	printf("  PASS: test_slit_style\n");
}

/* Test: window justify variants */
static void
test_window_justify(void)
{
	/* Right justify */
	write_file(TEST_STYLE, "window.justify: Right\n");
	struct wm_style *s = style_create();
	style_load(s, TEST_STYLE);
	assert(s->window_justify == WM_JUSTIFY_RIGHT);
	style_destroy(s);

	/* Left justify */
	write_file(TEST_STYLE, "window.justify: Left\n");
	s = style_create();
	style_load(s, TEST_STYLE);
	assert(s->window_justify == WM_JUSTIFY_LEFT);
	style_destroy(s);

	/* Centre (British spelling) */
	write_file(TEST_STYLE, "window.justify: Centre\n");
	s = style_create();
	style_load(s, TEST_STYLE);
	assert(s->window_justify == WM_JUSTIFY_CENTER);
	style_destroy(s);

	printf("  PASS: test_window_justify\n");
}

/* Test: menu title justify */
static void
test_menu_title_justify(void)
{
	write_file(TEST_STYLE,
		"menu.title.justify: Left\n");
	struct wm_style *s = style_create();
	style_load(s, TEST_STYLE);
	assert(s->menu_title_justify == WM_JUSTIFY_LEFT);
	style_destroy(s);

	write_file(TEST_STYLE,
		"menu.title.justify: Right\n");
	s = style_create();
	style_load(s, TEST_STYLE);
	assert(s->menu_title_justify == WM_JUSTIFY_RIGHT);
	style_destroy(s);

	printf("  PASS: test_menu_title_justify\n");
}

/* Test: texture pixmap type */
static void
test_texture_pixmap(void)
{
	struct wm_texture t = style_parse_texture("Flat Pixmap");
	assert(t.appearance == WM_TEX_FLAT);
	assert(t.fill == WM_TEX_PIXMAP);

	printf("  PASS: test_texture_pixmap\n");
}

/* Test: parent_relative alternate spelling */
static void
test_texture_parent_relative_alt(void)
{
	struct wm_texture t = style_parse_texture("parent_relative");
	assert(t.fill == WM_TEX_PARENT_RELATIVE);
	assert(t.appearance == WM_TEX_FLAT);
	assert(t.bevel == WM_BEVEL_NONE);

	printf("  PASS: test_texture_parent_relative_alt\n");
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

	/* New coverage tests */
	test_color_invalid_formats();
	test_font_zero_size();
	test_font_no_size();
	test_font_huge_size();
	test_font_halo();
	test_style_load_null();
	test_style_load_empty_file();
	test_unfocus_textures();
	test_menu_frame_textures();
	test_argb_alpha_zero();
	test_focus_border();
	test_round_corners();
	test_huge_border_width();
	test_button_pressed();
	test_slit_style();
	test_window_justify();
	test_menu_title_justify();
	test_texture_pixmap();
	test_texture_parent_relative_alt();

	cleanup();
	printf("All style tests passed.\n");
	return 0;
}
