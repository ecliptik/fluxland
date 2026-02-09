/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
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
#include <stdbool.h>

/* --- Button types for glyph rendering --- */

enum wm_button_type {
	WM_BUTTON_CLOSE,
	WM_BUTTON_MAXIMIZE,
	WM_BUTTON_ICONIFY,
	WM_BUTTON_SHADE,
	WM_BUTTON_STICK,
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

/* --- Button glyph rendering --- */

/*
 * Render a button glyph (close X, maximize square, etc.).
 *
 * Returns a Cairo surface of size*scale x size*scale containing the glyph
 * drawn in the given color.
 *
 * Caller is responsible for calling cairo_surface_destroy() on the result.
 */
cairo_surface_t *wm_render_button_glyph(enum wm_button_type type,
	const struct wm_color *pic_color, int size, float scale);

#endif /* WM_RENDER_H */
