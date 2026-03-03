/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * render.c - Cairo-based texture, gradient, text, and button glyph renderer
 *
 * Implements all 8 Fluxbox gradient types plus solid fills, bevel effects,
 * interlacing, pixmap loading, Pango text rendering, and button glyphs.
 */

#define _GNU_SOURCE

#include "perf.h"
#include "render.h"
#include "style.h"
#include <cairo.h>
#include <math.h>
#include <pango/pangocairo.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifdef WM_PERF_ENABLE
static struct wm_perf_probe perf_render_text;
static bool perf_render_text_inited;
#endif

/* --- RTL detection helper --- */

/*
 * Determine the base text direction by scanning for the first character
 * with a strong directionality.  Uses pango_unichar_direction() which is
 * deprecated in Pango >= 1.44 but has no simple replacement without FriBidi.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
static PangoDirection
find_base_dir(const char *text)
{
	if (!text)
		return PANGO_DIRECTION_NEUTRAL;
	const char *p = text;
	while (*p) {
		gunichar ch = g_utf8_get_char(p);
		PangoDirection dir = pango_unichar_direction(ch);
		if (dir == PANGO_DIRECTION_LTR ||
		    dir == PANGO_DIRECTION_RTL)
			return dir;
		p = g_utf8_next_char(p);
	}
	return PANGO_DIRECTION_NEUTRAL;
}
#pragma GCC diagnostic pop

/* --- Color helpers --- */

static inline double
color_r(uint32_t argb)
{
	return ((argb >> 16) & 0xFF) / 255.0;
}

static inline double
color_g(uint32_t argb)
{
	return ((argb >> 8) & 0xFF) / 255.0;
}

static inline double
color_b(uint32_t argb)
{
	return (argb & 0xFF) / 255.0;
}

static inline double
color_a(uint32_t argb)
{
	return ((argb >> 24) & 0xFF) / 255.0;
}

/* Linearly interpolate between two color channel values */
static inline uint8_t
lerp_u8(uint8_t a, uint8_t b, double t)
{
	return (uint8_t)(a + (b - a) * t + 0.5);
}

/* Interpolate full ARGB color */
static inline uint32_t
lerp_argb(uint32_t c1, uint32_t c2, double t)
{
	if (t <= 0.0) return c1;
	if (t >= 1.0) return c2;
	uint8_t r = lerp_u8((c1 >> 16) & 0xFF, (c2 >> 16) & 0xFF, t);
	uint8_t g = lerp_u8((c1 >> 8) & 0xFF, (c2 >> 8) & 0xFF, t);
	uint8_t b = lerp_u8(c1 & 0xFF, c2 & 0xFF, t);
	uint8_t a = lerp_u8((c1 >> 24) & 0xFF, (c2 >> 24) & 0xFF, t);
	return ((uint32_t)a << 24) | ((uint32_t)r << 16) |
		((uint32_t)g << 8) | (uint32_t)b;
}

/* Brighten a color by a factor (for bevel highlights) */
static inline uint32_t
brighten(uint32_t argb, double factor)
{
	int r = ((argb >> 16) & 0xFF);
	int g = ((argb >> 8) & 0xFF);
	int b = (argb & 0xFF);
	uint8_t a = (argb >> 24) & 0xFF;

	r = (int)(r * factor + 0.5);
	g = (int)(g * factor + 0.5);
	b = (int)(b * factor + 0.5);

	if (r > 255) r = 255;
	if (g > 255) g = 255;
	if (b > 255) b = 255;

	return ((uint32_t)a << 24) | ((uint32_t)r << 16) |
		((uint32_t)g << 8) | (uint32_t)b;
}

/* Darken a color by a factor (for bevel shadows) */
static inline uint32_t
darken(uint32_t argb, double factor)
{
	int r = ((argb >> 16) & 0xFF);
	int g = ((argb >> 8) & 0xFF);
	int b = (argb & 0xFF);
	uint8_t a = (argb >> 24) & 0xFF;

	r = (int)(r * factor + 0.5);
	g = (int)(g * factor + 0.5);
	b = (int)(b * factor + 0.5);

	if (r < 0) r = 0;
	if (g < 0) g = 0;
	if (b < 0) b = 0;

	return ((uint32_t)a << 24) | ((uint32_t)r << 16) |
		((uint32_t)g << 8) | (uint32_t)b;
}

/* Set a pixel in premultiplied ARGB32 surface data */
static inline void
set_pixel(uint32_t *data, int stride, int x, int y,
	int w, int h, uint32_t argb)
{
	if (x < 0 || x >= w || y < 0 || y >= h)
		return;
	/* Cairo ARGB32 is premultiplied and in native byte order */
	uint8_t a = (argb >> 24) & 0xFF;
	uint8_t r = (argb >> 16) & 0xFF;
	uint8_t g = (argb >> 8) & 0xFF;
	uint8_t b = argb & 0xFF;

	if (a < 255) {
		r = (r * a) / 255;
		g = (g * a) / 255;
		b = (b * a) / 255;
	}

	data[y * (stride / 4) + x] =
		((uint32_t)a << 24) | ((uint32_t)r << 16) |
		((uint32_t)g << 8) | (uint32_t)b;
}

/* --- Gradient rendering --- */

/*
 * Render pixel-by-pixel gradients directly to surface data.
 * This handles all 8 Fluxbox gradient types.
 */
static void
render_gradient(cairo_surface_t *surface, int w, int h,
	uint32_t color, uint32_t color_to, enum wm_gradient_type type)
{
	cairo_surface_flush(surface);
	uint32_t *data = (uint32_t *)cairo_image_surface_get_data(surface);
	int stride = cairo_image_surface_get_stride(surface);

	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			double t = 0.0;

			switch (type) {
			case WM_GRAD_HORIZONTAL:
				t = (w > 1) ? (double)x / (w - 1) : 0.0;
				break;

			case WM_GRAD_VERTICAL:
				t = (h > 1) ? (double)y / (h - 1) : 0.0;
				break;

			case WM_GRAD_DIAGONAL:
				t = (w + h > 2) ?
					(double)(x + y) / (w + h - 2) : 0.0;
				break;

			case WM_GRAD_CROSSDIAGONAL:
				t = (w + h > 2) ?
					(double)((w - 1 - x) + y) / (w + h - 2)
					: 0.0;
				break;

			case WM_GRAD_PIPECROSS: {
				/* Crossed pipes: distance from center axes */
				double dx = (w > 1) ?
					fabs((double)x / (w - 1) - 0.5) * 2.0
					: 0.0;
				double dy = (h > 1) ?
					fabs((double)y / (h - 1) - 0.5) * 2.0
					: 0.0;
				t = fmin(dx, dy);
				break;
			}

			case WM_GRAD_ELLIPTIC: {
				/* Radial from center */
				double cx = (w - 1) / 2.0;
				double cy = (h - 1) / 2.0;
				double dx = (cx > 0) ? (x - cx) / cx : 0.0;
				double dy = (cy > 0) ? (y - cy) / cy : 0.0;
				t = sqrt(dx * dx + dy * dy);
				if (t > 1.0) t = 1.0;
				break;
			}

			case WM_GRAD_RECTANGLE: {
				/* Rectangular distance from center */
				double dx = (w > 1) ?
					fabs((double)x / (w - 1) - 0.5) * 2.0
					: 0.0;
				double dy = (h > 1) ?
					fabs((double)y / (h - 1) - 0.5) * 2.0
					: 0.0;
				t = fmax(dx, dy);
				break;
			}

			case WM_GRAD_PYRAMID: {
				/* Pyramidal: product of horizontal and
				 * vertical distance factors */
				double dx = (w > 1) ?
					1.0 - fabs((double)x / (w - 1) - 0.5) * 2.0
					: 1.0;
				double dy = (h > 1) ?
					1.0 - fabs((double)y / (h - 1) - 0.5) * 2.0
					: 1.0;
				t = 1.0 - dx * dy;
				break;
			}
			}

			set_pixel(data, stride, x, y, w, h,
				lerp_argb(color, color_to, t));
		}
	}

	cairo_surface_mark_dirty(surface);
}

/* --- Solid fill --- */

static void
render_solid(cairo_t *cr, int w, int h, uint32_t color)
{
	cairo_set_source_rgba(cr, color_r(color), color_g(color),
		color_b(color), color_a(color));
	cairo_rectangle(cr, 0, 0, w, h);
	cairo_fill(cr);
}

/* --- Interlace effect --- */

static void
apply_interlace(cairo_surface_t *surface, int w, int h)
{
	cairo_surface_flush(surface);
	uint32_t *data = (uint32_t *)cairo_image_surface_get_data(surface);
	int stride = cairo_image_surface_get_stride(surface);

	for (int y = 1; y < h; y += 2) {
		for (int x = 0; x < w; x++) {
			uint32_t px = data[y * (stride / 4) + x];
			/* Darken by ~25%: multiply RGB by 0.75 */
			data[y * (stride / 4) + x] = darken(px, 0.75);
		}
	}

	cairo_surface_mark_dirty(surface);
}

/* --- Bevel effect --- */

static void
apply_bevel(cairo_surface_t *surface, int w, int h,
	enum wm_texture_appearance appearance, enum wm_bevel_type bevel,
	uint32_t base_color)
{
	if (bevel == WM_BEVEL_NONE)
		return;

	cairo_t *cr = cairo_create(surface);

	int inset = (bevel == WM_BEVEL2) ? 1 : 0;
	int x0 = inset;
	int y0 = inset;
	int x1 = w - 1 - inset;
	int y1 = h - 1 - inset;

	if (x1 <= x0 || y1 <= y0) {
		cairo_destroy(cr);
		return;
	}

	/* Determine light and shadow colors based on appearance */
	uint32_t light, shadow;
	if (appearance == WM_TEX_RAISED) {
		light = brighten(base_color, 1.6);
		shadow = darken(base_color, 0.5);
	} else {
		/* Sunken: swap light and shadow */
		light = darken(base_color, 0.5);
		shadow = brighten(base_color, 1.6);
	}

	/* Top edge (light) */
	cairo_set_source_rgba(cr, color_r(light), color_g(light),
		color_b(light), 0.6);
	cairo_set_line_width(cr, 1.0);
	cairo_move_to(cr, x0 + 0.5, y0 + 0.5);
	cairo_line_to(cr, x1 + 0.5, y0 + 0.5);
	cairo_stroke(cr);

	/* Left edge (light) */
	cairo_move_to(cr, x0 + 0.5, y0 + 0.5);
	cairo_line_to(cr, x0 + 0.5, y1 + 0.5);
	cairo_stroke(cr);

	/* Bottom edge (shadow) */
	cairo_set_source_rgba(cr, color_r(shadow), color_g(shadow),
		color_b(shadow), 0.6);
	cairo_move_to(cr, x0 + 0.5, y1 + 0.5);
	cairo_line_to(cr, x1 + 0.5, y1 + 0.5);
	cairo_stroke(cr);

	/* Right edge (shadow) */
	cairo_move_to(cr, x1 + 0.5, y0 + 0.5);
	cairo_line_to(cr, x1 + 0.5, y1 + 0.5);
	cairo_stroke(cr);

	cairo_destroy(cr);
}

/* --- Pixmap loading --- */

static cairo_surface_t *
load_pixmap(const char *path, int width, int height)
{
	if (!path || *path == '\0')
		return NULL;

	cairo_surface_t *img = cairo_image_surface_create_from_png(path);
	if (cairo_surface_status(img) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(img);
		return NULL;
	}

	/* Scale to fill the requested dimensions */
	int img_w = cairo_image_surface_get_width(img);
	int img_h = cairo_image_surface_get_height(img);

	if (img_w == width && img_h == height)
		return img;

	/* Create scaled version */
	cairo_surface_t *scaled =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	cairo_t *cr = cairo_create(scaled);
	cairo_scale(cr, (double)width / img_w, (double)height / img_h);
	cairo_set_source_surface(cr, img, 0, 0);
	cairo_paint(cr);
	cairo_destroy(cr);
	cairo_surface_destroy(img);

	return scaled;
}

/* --- Public API: texture rendering --- */

cairo_surface_t *
wm_render_texture(const struct wm_texture *texture, int width, int height,
	float scale)
{
	if (!texture || width <= 0 || height <= 0)
		return NULL;

	/* ParentRelative: signal to caller to use parent */
	if (texture->fill == WM_TEX_PARENT_RELATIVE)
		return NULL;

	int w = (int)(width * scale + 0.5f);
	int h = (int)(height * scale + 0.5f);
	if (w <= 0) w = 1;
	if (h <= 0) h = 1;

	/* Ensure alpha channel is set if color has none */
	uint32_t color = texture->color;
	uint32_t color_to = texture->color_to;
	if ((color >> 24) == 0)
		color |= 0xFF000000;
	if ((color_to >> 24) == 0)
		color_to |= 0xFF000000;

	/* Pixmap fill */
	if (texture->fill == WM_TEX_PIXMAP)
		return load_pixmap(texture->pixmap_path, w, h);

	cairo_surface_t *surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surface);
		return NULL;
	}

	if (texture->fill == WM_TEX_GRADIENT) {
		render_gradient(surface, w, h, color, color_to,
			texture->gradient);
	} else {
		/* Solid fill */
		cairo_t *cr = cairo_create(surface);
		render_solid(cr, w, h, color);
		cairo_destroy(cr);
	}

	/* Apply interlace before bevel so bevel lines aren't interlaced */
	if (texture->interlaced)
		apply_interlace(surface, w, h);

	/* Apply bevel */
	if (texture->appearance != WM_TEX_FLAT)
		apply_bevel(surface, w, h, texture->appearance,
			texture->bevel, color);

	return surface;
}

/* --- Public API: text rendering --- */

cairo_surface_t *
wm_render_text(const char *text, const struct wm_font *font,
	const struct wm_color *color, int max_width, int *out_width,
	int *out_height, enum wm_justify justify, float scale)
{
	if (!text || !font || !color)
		return NULL;

#ifdef WM_PERF_ENABLE
	if (!perf_render_text_inited) {
		wm_perf_probe_init(&perf_render_text, "render_text");
		perf_render_text_inited = true;
	}
	WM_PERF_BEGIN(rtext);
#endif

	int scaled_max_width = (int)(max_width * scale + 0.5f);
	if (scaled_max_width <= 0)
		scaled_max_width = 1;

	int shadow_offset_x = abs(font->shadow_x);
	int shadow_offset_y = abs(font->shadow_y);

	/*
	 * Allocate the surface at max_width and render the text directly.
	 * pango_layout_get_pixel_size() can underestimate the actual
	 * rendered width, so we size the surface to the layout constraint
	 * (max_width) to avoid clipping.
	 */
	int surf_w = scaled_max_width + shadow_offset_x;
	int est_h = (int)(font->size * scale * 3 + 0.5f) +
		shadow_offset_y;
	if (est_h < 32) est_h = 32;

	cairo_surface_t *surface = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, surf_w, est_h);
	cairo_t *cr = cairo_create(surface);

	PangoLayout *layout = pango_cairo_create_layout(cr);

	/* Build font description */
	PangoFontDescription *desc = pango_font_description_new();
	pango_font_description_set_family(desc,
		font->family ? font->family : "sans");
	pango_font_description_set_size(desc,
		(int)(font->size * scale * PANGO_SCALE + 0.5f));
	if (font->bold)
		pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
	if (font->italic)
		pango_font_description_set_style(desc, PANGO_STYLE_ITALIC);
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);

	/* Set text and constraints */
	pango_layout_set_text(layout, text, -1);
	pango_layout_set_width(layout, scaled_max_width * PANGO_SCALE);
	pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
	pango_layout_set_single_paragraph_mode(layout, TRUE);

	/* RTL auto-detection: detect base direction of the text and
	 * set Pango context direction accordingly so that RTL scripts
	 * (Arabic, Hebrew, etc.) render correctly. */
	PangoDirection base_dir = find_base_dir(text);
	bool is_rtl = (base_dir == PANGO_DIRECTION_RTL);
	if (is_rtl) {
		PangoContext *pango_ctx = pango_layout_get_context(layout);
		pango_context_set_base_dir(pango_ctx, PANGO_DIRECTION_RTL);
		pango_layout_context_changed(layout);
	}

	/*
	 * Always render left-aligned so the returned surface is
	 * tightly cropped to the ink extents.  Callers handle
	 * centering / right-alignment using the returned width.
	 * RTL base direction is already set above for correct
	 * glyph ordering and ellipsis placement.
	 */
	(void)justify;
	pango_layout_set_alignment(layout, PANGO_ALIGN_LEFT);

	/*
	 * Use ink extents for accurate rendered bounds.  The ink rectangle
	 * reflects the actual pixels drawn, which can exceed the logical
	 * size returned by pango_layout_get_pixel_size().
	 */
	PangoRectangle ink_rect, logical_rect;
	pango_layout_get_pixel_extents(layout, &ink_rect, &logical_rect);

	int text_w = ink_rect.x + ink_rect.width;
	int text_h = logical_rect.height;
	if (text_w <= 0) text_w = 1;
	if (text_h <= 0) text_h = 1;

	/* Final surface: ink width + shadow, logical height + shadow */
	int final_w = text_w + shadow_offset_x;
	int final_h = text_h + shadow_offset_y;

	/* Draw shadow if enabled */
	if (font->shadow_x != 0 || font->shadow_y != 0) {
		double sx = (font->shadow_x > 0) ? font->shadow_x : 0;
		double sy = (font->shadow_y > 0) ? font->shadow_y : 0;
		cairo_move_to(cr, sx, sy);
		cairo_set_source_rgba(cr,
			font->shadow_color.r / 255.0,
			font->shadow_color.g / 255.0,
			font->shadow_color.b / 255.0,
			font->shadow_color.a / 255.0);
		pango_cairo_show_layout(cr, layout);
	}

	/* Draw halo outline (between shadow and main text) */
	double tx = (font->shadow_x < 0) ? -font->shadow_x : 0;
	double ty = (font->shadow_y < 0) ? -font->shadow_y : 0;

	if (font->halo) {
		cairo_save(cr);
		cairo_move_to(cr, tx, ty);
		cairo_set_source_rgba(cr,
			font->halo_color.r / 255.0,
			font->halo_color.g / 255.0,
			font->halo_color.b / 255.0,
			font->halo_color.a / 255.0);
		cairo_set_line_width(cr, 2.0 * scale);
		pango_cairo_layout_path(cr, layout);
		cairo_stroke(cr);
		cairo_restore(cr);
	}

	/* Draw text */
	cairo_move_to(cr, tx, ty);
	cairo_set_source_rgba(cr, color->r / 255.0, color->g / 255.0,
		color->b / 255.0, color->a / 255.0);
	pango_cairo_show_layout(cr, layout);

	g_object_unref(layout);
	cairo_destroy(cr);

	/*
	 * Crop to the actual rendered extent so the caller gets a
	 * tightly-fitting surface for compositing.
	 */
	if (final_w > surf_w) final_w = surf_w;
	if (final_h > est_h) final_h = est_h;
	cairo_surface_t *cropped = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, final_w, final_h);
	cairo_t *crop_cr = cairo_create(cropped);
	cairo_set_source_surface(crop_cr, surface, 0, 0);
	cairo_paint(crop_cr);
	cairo_destroy(crop_cr);
	cairo_surface_destroy(surface);

	if (out_width)
		*out_width = final_w;
	if (out_height)
		*out_height = final_h;

#ifdef WM_PERF_ENABLE
	WM_PERF_END(rtext, &perf_render_text);
#endif

	return cropped;
}

/* --- Public API: text measurement --- */

int
wm_measure_text_width(const char *text, const struct wm_font *font,
	float scale)
{
	if (!text || !font)
		return 0;

	/*
	 * Use a reasonably sized surface for consistent font metrics
	 * and ink extents for accurate width measurement.
	 */
	int est_h = (int)(font->size * scale * 3 + 0.5f);
	if (est_h < 32) est_h = 32;
	cairo_surface_t *tmp =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 512, est_h);
	cairo_t *cr = cairo_create(tmp);
	PangoLayout *layout = pango_cairo_create_layout(cr);

	PangoFontDescription *desc = pango_font_description_new();
	pango_font_description_set_family(desc,
		font->family ? font->family : "sans");
	pango_font_description_set_size(desc,
		(int)(font->size * scale * PANGO_SCALE + 0.5f));
	if (font->bold)
		pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
	if (font->italic)
		pango_font_description_set_style(desc, PANGO_STYLE_ITALIC);
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);

	pango_layout_set_text(layout, text, -1);

	PangoRectangle ink_rect;
	pango_layout_get_pixel_extents(layout, &ink_rect, NULL);
	int text_w = ink_rect.x + ink_rect.width;

	g_object_unref(layout);
	cairo_destroy(cr);
	cairo_surface_destroy(tmp);

	return text_w;
}

/* --- Public API: button glyph rendering --- */

cairo_surface_t *
wm_render_button_glyph(enum wm_button_type type,
	const struct wm_color *pic_color, int size, float scale)
{
	if (!pic_color || size <= 0)
		return NULL;

	int s = (int)(size * scale + 0.5f);
	if (s <= 0) s = 1;

	cairo_surface_t *surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, s, s);
	cairo_t *cr = cairo_create(surface);

	cairo_set_source_rgba(cr, pic_color->r / 255.0,
		pic_color->g / 255.0, pic_color->b / 255.0,
		pic_color->a / 255.0);
	cairo_set_line_width(cr, fmax(1.0, s / 10.0));
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

	/* Margin around the glyph */
	double m = s * 0.2;

	switch (type) {
	case WM_BUTTON_CLOSE:
		/* X shape */
		cairo_move_to(cr, m, m);
		cairo_line_to(cr, s - m, s - m);
		cairo_stroke(cr);
		cairo_move_to(cr, s - m, m);
		cairo_line_to(cr, m, s - m);
		cairo_stroke(cr);
		break;

	case WM_BUTTON_MAXIMIZE:
		/* Square outline */
		cairo_rectangle(cr, m, m, s - 2 * m, s - 2 * m);
		cairo_stroke(cr);
		break;

	case WM_BUTTON_ICONIFY:
		/* Horizontal line at bottom */
		cairo_move_to(cr, m, s - m);
		cairo_line_to(cr, s - m, s - m);
		cairo_stroke(cr);
		break;

	case WM_BUTTON_SHADE: {
		/* Triangle pointing up */
		double cx = s / 2.0;
		cairo_move_to(cr, m, s - m);
		cairo_line_to(cr, cx, m);
		cairo_line_to(cr, s - m, s - m);
		cairo_close_path(cr);
		cairo_fill(cr);
		break;
	}

	case WM_BUTTON_STICK: {
		/* Small filled circle (dot) */
		double cx = s / 2.0;
		double cy = s / 2.0;
		double r = (s - 2 * m) / 3.0;
		cairo_arc(cr, cx, cy, r, 0, 2 * M_PI);
		cairo_fill(cr);
		break;
	}
	}

	cairo_destroy(cr);
	return surface;
}

void
wm_render_rounded_rect_path(cairo_t *cr, double x, double y,
	double width, double height, double radius, uint8_t corners)
{
	if (radius <= 0 || corners == 0) {
		cairo_rectangle(cr, x, y, width, height);
		return;
	}

	double r = radius;
	if (r > width / 2.0) r = width / 2.0;
	if (r > height / 2.0) r = height / 2.0;

	cairo_new_sub_path(cr);

	/* Top-left corner */
	if (corners & WM_CORNER_TOP_LEFT) {
		cairo_arc(cr, x + r, y + r, r, M_PI, 3.0 * M_PI / 2.0);
	} else {
		cairo_move_to(cr, x, y);
	}

	/* Top-right corner */
	if (corners & WM_CORNER_TOP_RIGHT) {
		cairo_arc(cr, x + width - r, y + r, r,
			3.0 * M_PI / 2.0, 2.0 * M_PI);
	} else {
		cairo_line_to(cr, x + width, y);
	}

	/* Bottom-right corner */
	if (corners & WM_CORNER_BOTTOM_RIGHT) {
		cairo_arc(cr, x + width - r, y + height - r, r,
			0, M_PI / 2.0);
	} else {
		cairo_line_to(cr, x + width, y + height);
	}

	/* Bottom-left corner */
	if (corners & WM_CORNER_BOTTOM_LEFT) {
		cairo_arc(cr, x + r, y + height - r, r,
			M_PI / 2.0, M_PI);
	} else {
		cairo_line_to(cr, x, y + height);
	}

	cairo_close_path(cr);
}
