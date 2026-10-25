/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * render.h - Cairo-based texture, gradient, text, and button glyph renderer
 *
 * Renders Fluxbox textures (gradients, solids, bevels, pixmaps) to Cairo
 * surfaces for use in window decorations, menus, and toolbar.
 */

#ifndef WM_RENDER_H
#define WM_RENDER_H

#include "style.h"
#include <cairo.h>
#include <pango/pango.h>
#include <stdbool.h>

/* --- Button types for glyph rendering --- */

enum wm_button_type {
	WM_BUTTON_CLOSE,
	WM_BUTTON_MAXIMIZE,
	WM_BUTTON_ICONIFY,
	WM_BUTTON_SHADE,
	WM_BUTTON_STICK,
	WM_BUTTON_MENUICON,	/* the window menu */
	WM_BUTTON_LHALF,	/* to the left half of the head */
	WM_BUTTON_RHALF,
};

/* --- Texture rendering --- */

/*
 * Render a texture to a Cairo image surface.
 *
 * Returns a newly created CAIRO_FORMAT_ARGB32 surface of the given
 * dimensions (in device pixels, i.e. width*scale x height*scale).
 * Returns NULL for ParentRelative textures or on error.
 *
 * Caller is responsible for calling cairo_surface_destroy() on the result.
 */
cairo_surface_t *wm_render_texture(const struct wm_texture *texture,
	int width, int height, float scale);

/* --- Text rendering --- */

/*
 * Render text using Pango/Cairo.
 *
 * Returns a Cairo surface containing the rendered text with optional shadow.
 * If out_width/out_height are non-NULL, the actual rendered size is stored.
 * Text is truncated with ellipsis if wider than max_width.
 *
 * Caller is responsible for calling cairo_surface_destroy() on the result.
 */
cairo_surface_t *wm_render_text(const char *text, const struct wm_font *font,
	const struct wm_color *color, int max_width, int *out_width,
	int *out_height, enum wm_justify justify, float scale);

/* --- Text measurement --- */

/*
 * Measure the pixel width of text using Pango without rendering.
 *
 * Returns the natural (unconstrained) width in pixels that the given
 * text would occupy when rendered with the specified font.
 */
int wm_measure_text_width(const char *text, const struct wm_font *font,
	float scale);

/* A font's height (ascent and descent) in pixels, as Fluxbox's
 * Font::height() */
int wm_font_height(const struct wm_font *font, float scale);

/* --- Pixmap loading --- */

/* An image file at its own size (PNG; JPEG, XPM... with GdkPixbuf) */
cairo_surface_t *wm_load_image(const char *path);

/*
 * Load an image file (PNG, XPM, JPEG, BMP) and scale it to the given
 * dimensions.  Returns a Cairo surface or NULL on error.
 *
 * Caller is responsible for calling cairo_surface_destroy() on the result.
 */
cairo_surface_t *wm_load_pixmap(const char *path, int width, int height);

/* --- Button glyph rendering --- */

/*
 * A titlebar button's picture as Fluxbox's WinButton draws it, in
 * pic_color on a clear w x h surface; toggled is a stuck window for
 * Stick, a shaded one for Shade.
 *
 * Caller is responsible for calling cairo_surface_destroy() on the result.
 */
cairo_surface_t *wm_render_button_glyph(enum wm_button_type type,
	bool toggled, const struct wm_color *pic_color, int w, int h);

/* --- RTL text direction detection --- */

/*
 * Determine the base text direction by scanning for the first character
 * with a strong directionality. Returns PANGO_DIRECTION_LTR,
 * PANGO_DIRECTION_RTL, or PANGO_DIRECTION_NEUTRAL.
 */
PangoDirection find_base_dir(const char *text);

/* --- Cleanup --- */

/*
 * Free the cached measurement context used by text rendering.
 * Call during compositor shutdown.
 */
void wm_render_cleanup(void);

/* --- Rounded rectangle path --- */

/*
 * Add a rounded rectangle path to a Cairo context.
 *
 * The corners bitmask uses WM_CORNER_* flags from style.h to select
 * which corners are rounded. A radius of 0 produces a normal rectangle.
 */
/*
 * Fluxbox's round corners (FbTk::Shape's 8x8 masks): cut out of a
 * surface drawn at (x, y) in a frame_w x frame_h frame the corner pixels
 * that frame loses, for the corners set (WM_CORNER_*)
 */
void wm_cut_corners(cairo_surface_t *surface, int x, int y, int frame_w,
	int frame_h, uint8_t corners);

void wm_render_rounded_rect_path(cairo_t *cr, double x, double y,
	double width, double height, double radius, uint8_t corners);

#endif /* WM_RENDER_H */
