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

#ifdef WM_HAS_PIXBUF
#include <gdk-pixbuf/gdk-pixbuf.h>
#endif

#ifdef WM_PERF_ENABLE
static struct wm_perf_probe perf_render_text;
static bool perf_render_text_inited;
#endif

/* --- Cached measurement context --- */

/*
 * Persistent Pango measurement context backed by a tiny 1×1 Cairo surface.
 * Avoids creating and destroying a full Cairo surface + PangoLayout on
 * every call to wm_measure_text_width() and wm_render_text().
 */
static cairo_surface_t *measure_surface;
static cairo_t *measure_cr;
static PangoLayout *measure_layout;

static void
ensure_measurement_context(void)
{
	if (measure_layout)
		return;
	measure_surface = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, 1, 1);
	measure_cr = cairo_create(measure_surface);
	measure_layout = pango_cairo_create_layout(measure_cr);
}

/* --- RTL detection helper --- */

/*
 * Determine the base text direction by scanning for the first character
 * with a strong directionality.  Uses pango_unichar_direction() which is
 * deprecated in Pango >= 1.44 but has no simple replacement without FriBidi.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
PangoDirection
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
/*
 * Textures are drawn as Fluxbox draws them (FbTk/TextureRender.cc), so
 * a style looks the same: its gradient tables, its bevels (a lighter
 * top and left edge, a darker bottom and right) and its colour steps.
 */

/* Fluxbox's ColorLUT: BRIGHTER_8 and PRE_MULTIPLY_0_75 */
static inline uint8_t
fb_brighter(uint8_t v)
{
	return (uint8_t)(v + ((255 - v) >> 3));
}

static inline uint8_t
fb_darker(uint8_t v)
{
	return (uint8_t)((v * 3) >> 2);
}

static inline uint32_t
fb_brighten(uint32_t argb)
{
	return (argb & 0xFF000000) |
		((uint32_t)fb_brighter((argb >> 16) & 0xFF) << 16) |
		((uint32_t)fb_brighter((argb >> 8) & 0xFF) << 8) |
		fb_brighter(argb & 0xFF);
}

static inline uint32_t
fb_darken(uint32_t argb)
{
	return (argb & 0xFF000000) |
		((uint32_t)fb_darker((argb >> 16) & 0xFF) << 16) |
		((uint32_t)fb_darker((argb >> 8) & 0xFF) << 8) |
		fb_darker(argb & 0xFF);
}

/* prepareLinearTable: n steps from 'from' toward 'to', times scale */
static void
linear_table(uint32_t *t, int n, uint32_t from, uint32_t to, double scale)
{
	double r = (from >> 16) & 0xFF, g = (from >> 8) & 0xFF, b = from & 0xFF;
	double dr = ((double)((to >> 16) & 0xFF) - r) / n;
	double dg = ((double)((to >> 8) & 0xFF) - g) / n;
	double db = ((double)(to & 0xFF) - b) / n;
	for (int i = 0; i < n; i++) {
		t[i] = 0xFF000000 |
			((uint32_t)(scale * (r + i * dr)) << 16) |
			((uint32_t)(scale * (g + i * dg)) << 8) |
			(uint32_t)(scale * (b + i * db));
	}
}

/* prepareMirrorTable: 'from' at both ends, 'to' in the middle */
static void
mirror_table(uint32_t *t, int n, uint32_t from, uint32_t to, double scale)
{
	int half = (n >> 1) + (n & 1);
	linear_table(t, half, from, to, scale);
	for (int i = 0; i < n / 2; i++)
		t[n - 1 - i] = t[i];
}

/* Two tables, each scaled by 0.5, added channel by channel */
static inline uint32_t
add_halves(uint32_t a, uint32_t b)
{
	return 0xFF000000 |
		(((a & 0xFF0000) + (b & 0xFF0000)) & 0xFF0000) |
		(((a & 0xFF00) + (b & 0xFF00)) & 0xFF00) |
		(((a & 0xFF) + (b & 0xFF)) & 0xFF);
}

/* Which side of the line from (0,0) along (ax,ay) a point is on */
static inline int
side(int ax, int ay, int x, int y)
{
	long c = (long)ax * y - (long)ay * x;
	return (c > 0) - (c < 0);
}

static void
render_gradient(cairo_surface_t *surface, int w, int h,
	uint32_t color, uint32_t color_to, enum wm_gradient_type type,
	bool interlaced)
{
	cairo_surface_flush(surface);
	uint32_t *data = (uint32_t *)cairo_image_surface_get_data(surface);
	int stride = cairo_image_surface_get_stride(surface) / 4;
	uint32_t *xt = calloc((size_t)w + (size_t)h, sizeof(uint32_t));
	if (!xt)
		return;
	uint32_t *yt = xt + w;

	switch (type) {
	case WM_GRAD_HORIZONTAL:
		linear_table(xt, w, color, color_to, 1.0);
		break;
	case WM_GRAD_VERTICAL:
		linear_table(yt, h, color, color_to, 1.0);
		break;
	case WM_GRAD_DIAGONAL:
		linear_table(xt, w, color, color_to, 0.5);
		linear_table(yt, h, color, color_to, 0.5);
		break;
	case WM_GRAD_CROSSDIAGONAL:
		linear_table(xt, w, color_to, color, 0.5);
		linear_table(yt, h, color, color_to, 0.5);
		break;
	case WM_GRAD_PYRAMID:
		mirror_table(xt, w, color, color_to, 0.5);
		mirror_table(yt, h, color, color_to, 0.5);
		break;
	case WM_GRAD_RECTANGLE:
	case WM_GRAD_PIPECROSS:
		mirror_table(xt, w, color, color_to, 1.0);
		mirror_table(yt, h, color, color_to, 1.0);
		break;
	case WM_GRAD_ELLIPTIC:
		break;
	}

	double w2 = w / 2.0, h2 = h / 2.0;
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			uint32_t px = 0;
			switch (type) {
			case WM_GRAD_HORIZONTAL:
				px = xt[x];
				break;
			case WM_GRAD_VERTICAL:
				px = yt[y];
				break;
			case WM_GRAD_DIAGONAL:
			case WM_GRAD_CROSSDIAGONAL:
			case WM_GRAD_PYRAMID:
				px = add_halves(xt[x], yt[y]);
				break;
			case WM_GRAD_RECTANGLE:
			case WM_GRAD_PIPECROSS: {
				/* The diagonals cut the area into four:
				 * left and right take the x table */
				int s = side(w - 1, h - 1, x, y) *
					side(w - 1, -(h - 1), x, y - (h - 1));
				bool use_x = type == WM_GRAD_RECTANGLE ?
					s < 0 : s > 0;
				px = use_x ? xt[x] : yt[y];
				break;
			}
			case WM_GRAD_ELLIPTIC: {
				double dx = x - w2, dy = y - h2;
				double d = (dx * dx / (w2 * w2) +
					dy * dy / (h2 * h2)) / 2.0;
				px = 0xFF000000;
				for (int c = 16; c >= 0; c -= 8) {
					double to = (color_to >> c) & 0xFF;
					double from = (color >> c) & 0xFF;
					double v = to - d * (to - from);
					if (v < 0) v = 0;
					if (v > 255) v = 255;
					px |= (uint32_t)v << c;
				}
				break;
			}
			}
			/* Interlaced: even rows lighter, odd rows darker */
			if (interlaced)
				px = (y & 1) ? fb_darken(px) : fb_brighten(px);
			data[y * stride + x] = px;
		}
	}
	free(xt);
	cairo_surface_mark_dirty(surface);
}

/* renderBevel1/renderBevel2: the edges of a gradient, pixel by pixel */
static void
gradient_bevel(cairo_surface_t *surface, int w, int h,
	enum wm_bevel_type bevel)
{
	cairo_surface_flush(surface);
	uint32_t *d = (uint32_t *)cairo_image_surface_get_data(surface);
	int st = cairo_image_surface_get_stride(surface) / 4;
#define PX(x, y) d[(y) * st + (x)]
	if (bevel == WM_BEVEL1 && w > 2 && h > 2) {
		for (int x = 0; x < w; x++)
			PX(x, 0) = fb_brighten(PX(x, 0));
		for (int y = 1; y < h - 1; y++) {
			PX(0, y) = fb_brighten(PX(0, y));
			PX(w - 1, y) = fb_darken(PX(w - 1, y));
		}
		for (int x = 1; x < w; x++)
			PX(x, h - 1) = fb_darken(PX(x, h - 1));
		/* the lower right corner darker again, and the right
		 * end of the row above it */
		PX(w - 1, h - 1) = fb_darken(PX(w - 1, h - 1));
		PX(w - 1, h - 2) = fb_darken(PX(w - 1, h - 2));
	} else if (bevel == WM_BEVEL2 && w > 4 && h > 4) {
		for (int x = 1; x < w - 2; x++)
			PX(x, 1) = fb_brighten(PX(x, 1));
		for (int y = 1; y < h - 2; y++) {
			PX(w - 2, y) = fb_darken(PX(w - 2, y));
			PX(1, y + 1) = fb_brighten(PX(1, y + 1));
		}
		for (int x = 2; x < w - 1; x++)
			PX(x, h - 2) = fb_darken(PX(x, h - 2));
	}
#undef PX
	cairo_surface_mark_dirty(surface);
}

/* invertRGB: the image turned half a turn */
static void
turn_half(cairo_surface_t *surface, int w, int h)
{
	cairo_surface_flush(surface);
	uint32_t *d = (uint32_t *)cairo_image_surface_get_data(surface);
	int st = cairo_image_surface_get_stride(surface) / 4;
	for (int y = 0; y < (h + 1) / 2; y++) {
		int y2 = h - 1 - y;
		int xmax = y == y2 ? w / 2 : w;
		for (int x = 0; x < xmax; x++) {
			uint32_t t = d[y * st + x];
			d[y * st + x] = d[y2 * st + (w - 1 - x)];
			d[y2 * st + (w - 1 - x)] = t;
		}
	}
	cairo_surface_mark_dirty(surface);
}

/* renderSolid: the colour, colorTo every other line when interlaced,
 * and a bevel of lines in the colour made lighter and darker */
static void
render_solid(cairo_surface_t *surface, int w, int h,
	const struct wm_texture *tex, uint32_t color, uint32_t color_to)
{
	cairo_surface_flush(surface);
	uint32_t *d = (uint32_t *)cairo_image_surface_get_data(surface);
	int st = cairo_image_surface_get_stride(surface) / 4;
	for (int y = 0; y < h; y++) {
		uint32_t px = tex->interlaced && !(y & 1) ? color_to : color;
		for (int x = 0; x < w; x++)
			d[y * st + x] = px;
	}

	if (tex->appearance != WM_TEX_FLAT && tex->bevel != WM_BEVEL_NONE) {
		uint32_t hi = fb_brighten(color), lo = fb_darken(color);
		if (tex->appearance == WM_TEX_SUNKEN) {
			uint32_t t = hi;
			hi = lo;
			lo = t;
		}
		int i = tex->bevel == WM_BEVEL2 ? 1 : 0;
		int x0 = i, y0 = i, x1 = w - 1 - i, y1 = h - 1 - i;
		if (x1 > x0 && y1 > y0) {
			/* drawBevelRectangle: bottom and right, then top
			 * and left over their corners */
			for (int x = x0; x <= x1; x++)
				d[y1 * st + x] = lo;
			for (int y = y0; y <= y1; y++)
				d[y * st + x1] = lo;
			for (int x = x0; x <= x1; x++)
				d[y0 * st + x] = hi;
			for (int y = y0; y <= y1; y++)
				d[y * st + x0] = hi;
		}
	}
	cairo_surface_mark_dirty(surface);
}

/* --- Pixmap loading --- */

#ifdef WM_HAS_PIXBUF
/*
 * Convert a GdkPixbuf to a Cairo ARGB32 surface.
 * GdkPixbuf stores pixels in straight-alpha RGBA order, while Cairo uses
 * premultiplied-alpha BGRA (on little-endian).  We do the conversion manually.
 */
static cairo_surface_t *
pixbuf_to_cairo(GdkPixbuf *pixbuf)
{
	int pw = gdk_pixbuf_get_width(pixbuf);
	int ph = gdk_pixbuf_get_height(pixbuf);
	int src_stride = gdk_pixbuf_get_rowstride(pixbuf);
	int n_channels = gdk_pixbuf_get_n_channels(pixbuf);
	const guchar *src_data = gdk_pixbuf_get_pixels(pixbuf);
	bool has_alpha = gdk_pixbuf_get_has_alpha(pixbuf);

	cairo_surface_t *surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, pw, ph);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS)
		return surface;

	cairo_surface_flush(surface);
	uint32_t *dst = (uint32_t *)cairo_image_surface_get_data(surface);
	int dst_stride = cairo_image_surface_get_stride(surface) / 4;

	for (int y = 0; y < ph; y++) {
		const guchar *row = src_data + y * src_stride;
		uint32_t *dst_row = dst + y * dst_stride;
		for (int x = 0; x < pw; x++) {
			uint8_t r = row[x * n_channels + 0];
			uint8_t g = row[x * n_channels + 1];
			uint8_t b = row[x * n_channels + 2];
			uint8_t a = has_alpha ? row[x * n_channels + 3] : 255;

			/* Premultiply alpha for Cairo */
			if (a == 0) {
				dst_row[x] = 0;
			} else if (a == 255) {
				dst_row[x] = (255u << 24) | (r << 16) |
					(g << 8) | b;
			} else {
				dst_row[x] = ((uint32_t)a << 24) |
					((r * a / 255) << 16) |
					((g * a / 255) << 8) |
					(b * a / 255);
			}
		}
	}

	cairo_surface_mark_dirty(surface);
	return surface;
}
#endif

cairo_surface_t *
wm_load_image(const char *path)
{
	if (!path || *path == '\0')
		return NULL;

	/* Fast path: try native Cairo PNG loading first */
	cairo_surface_t *img = cairo_image_surface_create_from_png(path);
	if (cairo_surface_status(img) == CAIRO_STATUS_SUCCESS)
		return img;
	cairo_surface_destroy(img);

#ifdef WM_HAS_PIXBUF
	/* Fallback: use GdkPixbuf for XPM, JPEG, BMP, and other formats */
	GError *err = NULL;
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(path, &err);
	if (!pixbuf) {
		if (err)
			g_error_free(err);
		return NULL;
	}
	img = pixbuf_to_cairo(pixbuf);
	g_object_unref(pixbuf);
	if (cairo_surface_status(img) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(img);
		return NULL;
	}
	return img;
#else
	return NULL;
#endif
}

cairo_surface_t *
wm_load_pixmap(const char *path, int width, int height)
{
	cairo_surface_t *img = wm_load_image(path);
	if (!img)
		return NULL;

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

/* An image repeated over w x h (a Tiled texture) */
static cairo_surface_t *
tile_image(cairo_surface_t *img, int w, int h)
{
	cairo_surface_t *out =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
	cairo_t *cr = cairo_create(out);
	cairo_pattern_t *pat = cairo_pattern_create_for_surface(img);
	cairo_pattern_set_extend(pat, CAIRO_EXTEND_REPEAT);
	cairo_set_source(cr, pat);
	cairo_paint(cr);
	cairo_pattern_destroy(pat);
	cairo_destroy(cr);
	cairo_surface_destroy(img);
	return out;
}

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

	/* A pixmap that loads is drawn whatever the texture is */
	if (texture->pixmap_path) {
		cairo_surface_t *img = texture->tiled ?
			wm_load_image(texture->pixmap_path) :
			wm_load_pixmap(texture->pixmap_path, w, h);
		if (img && texture->tiled)
			img = tile_image(img, w, h);
		if (img)
			return img;
	}

	cairo_surface_t *surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surface);
		return NULL;
	}

	if (texture->fill == WM_TEX_GRADIENT) {
		/* Sunken: the colours swapped and the result turned, so
		 * the bevel falls on the other edges */
		bool sunken = texture->appearance == WM_TEX_SUNKEN;
		bool turn = texture->invert != sunken;
		render_gradient(surface, w, h, sunken ? color_to : color,
			sunken ? color : color_to, texture->gradient,
			texture->interlaced);
		if (texture->appearance != WM_TEX_FLAT)
			gradient_bevel(surface, w, h, texture->bevel);
		if (turn)
			turn_half(surface, w, h);
	} else {
		/* Solid, or a pixmap texture without its image */
		render_solid(surface, w, h, texture, color, color_to);
	}

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
	 * Phase 1: Measure text using the cached measurement context.
	 * This avoids allocating an oversized surface just to determine
	 * the actual rendered extent.
	 */
	ensure_measurement_context();

	PangoFontDescription *desc = pango_font_description_new();
	pango_font_description_set_family(desc,
		font->family ? font->family : "sans");
	pango_font_description_set_size(desc,
		(int)(font->size * scale * PANGO_SCALE + 0.5f));
	if (font->bold)
		pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
	if (font->italic)
		pango_font_description_set_style(desc, PANGO_STYLE_ITALIC);
	pango_layout_set_font_description(measure_layout, desc);
	pango_font_description_free(desc);

	pango_layout_set_text(measure_layout, text, -1);
	pango_layout_set_width(measure_layout,
		scaled_max_width * PANGO_SCALE);
	pango_layout_set_ellipsize(measure_layout, PANGO_ELLIPSIZE_END);
	pango_layout_set_single_paragraph_mode(measure_layout, TRUE);

	/* RTL auto-detection */
	PangoDirection base_dir = find_base_dir(text);
	bool is_rtl = (base_dir == PANGO_DIRECTION_RTL);
	PangoContext *m_ctx = pango_layout_get_context(measure_layout);
	pango_context_set_base_dir(m_ctx,
		is_rtl ? PANGO_DIRECTION_RTL : PANGO_DIRECTION_LTR);
	pango_layout_context_changed(measure_layout);

	(void)justify;
	pango_layout_set_alignment(measure_layout, PANGO_ALIGN_LEFT);

	PangoRectangle ink_rect, logical_rect;
	pango_layout_get_pixel_extents(measure_layout,
		&ink_rect, &logical_rect);

	int text_w = ink_rect.x + ink_rect.width;
	int text_h = logical_rect.height;
	if (text_w <= 0) text_w = 1;
	if (text_h <= 0) text_h = 1;

	int final_w = text_w + shadow_offset_x;
	int final_h = text_h + shadow_offset_y;

	/*
	 * Phase 2: Create a surface at the exact measured size and render.
	 * This eliminates the previous double-surface pattern (oversized
	 * render + crop copy) that allocated ~2x the needed memory.
	 */
	cairo_surface_t *surface = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, final_w, final_h);
	cairo_t *cr = cairo_create(surface);

	PangoLayout *layout = pango_cairo_create_layout(cr);

	/* Re-apply layout properties for rendering context */
	desc = pango_font_description_new();
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
	pango_layout_set_width(layout, scaled_max_width * PANGO_SCALE);
	pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
	pango_layout_set_single_paragraph_mode(layout, TRUE);

	if (is_rtl) {
		PangoContext *pango_ctx = pango_layout_get_context(layout);
		pango_context_set_base_dir(pango_ctx, PANGO_DIRECTION_RTL);
		pango_layout_context_changed(layout);
	}
	pango_layout_set_alignment(layout, PANGO_ALIGN_LEFT);

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

	if (out_width)
		*out_width = final_w;
	if (out_height)
		*out_height = final_h;

#ifdef WM_PERF_ENABLE
	WM_PERF_END(rtext, &perf_render_text);
#endif

	return surface;
}

/* --- Public API: text measurement --- */

int
wm_measure_text_width(const char *text, const struct wm_font *font,
	float scale)
{
	if (!text || !font)
		return 0;

	/*
	 * Use the cached measurement context (1×1 surface + persistent
	 * PangoLayout) instead of creating a fresh 512×N surface per call.
	 * This is called frequently from toolbar layout computation.
	 */
	ensure_measurement_context();

	PangoFontDescription *desc = pango_font_description_new();
	pango_font_description_set_family(desc,
		font->family ? font->family : "sans");
	pango_font_description_set_size(desc,
		(int)(font->size * scale * PANGO_SCALE + 0.5f));
	if (font->bold)
		pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
	if (font->italic)
		pango_font_description_set_style(desc, PANGO_STYLE_ITALIC);
	pango_layout_set_font_description(measure_layout, desc);
	pango_font_description_free(desc);

	pango_layout_set_text(measure_layout, text, -1);
	pango_layout_set_width(measure_layout, -1);
	pango_layout_set_ellipsize(measure_layout, PANGO_ELLIPSIZE_NONE);

	PangoRectangle ink_rect;
	pango_layout_get_pixel_extents(measure_layout, &ink_rect, NULL);

	return ink_rect.x + ink_rect.width;
}

int
wm_font_height(const struct wm_font *font, float scale)
{
	if (!font)
		return 0;
	ensure_measurement_context();

	PangoFontDescription *desc = pango_font_description_new();
	pango_font_description_set_family(desc,
		font->family ? font->family : "sans");
	pango_font_description_set_size(desc,
		(int)(font->size * scale * PANGO_SCALE + 0.5f));
	if (font->bold)
		pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
	if (font->italic)
		pango_font_description_set_style(desc, PANGO_STYLE_ITALIC);
	PangoContext *ctx = pango_layout_get_context(measure_layout);
	PangoFontMetrics *m = pango_context_get_metrics(ctx, desc, NULL);
	pango_font_description_free(desc);
	if (!m)
		return 0;
	int h = PANGO_PIXELS(pango_font_metrics_get_ascent(m) +
		pango_font_metrics_get_descent(m));
	pango_font_metrics_unref(m);
	return h;
}

void
wm_render_cleanup(void)
{
	if (measure_layout) {
		g_object_unref(measure_layout);
		measure_layout = NULL;
	}
	if (measure_cr) {
		cairo_destroy(measure_cr);
		measure_cr = NULL;
	}
	if (measure_surface) {
		cairo_surface_destroy(measure_surface);
		measure_surface = NULL;
	}
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
