/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * icon.h - Application icons for the iconbar
 */

#ifndef WM_ICON_H
#define WM_ICON_H

#include <cairo.h>

struct wlr_buffer;

/*
 * An icon by theme name (hicolor, pixmaps) or absolute path, scaled to
 * size x size. Returns a new reference, or NULL if there is none.
 */
cairo_surface_t *wm_icon_lookup(const char *name, int size);

/* The icon of the desktop file matching an app_id (or X11 class) */
cairo_surface_t *wm_icon_for_app_id(const char *app_id, int size);

/* An icon from client pixels (xdg-toplevel-icon), scaled */
cairo_surface_t *wm_icon_from_buffer(struct wlr_buffer *buffer, int size);

/* Scale an image to fit size x size, centered */
cairo_surface_t *wm_icon_scale(cairo_surface_t *src, int size);

/* Forget cached lookups (icon themes or desktop files changed) */
void wm_icon_cache_clear(void);

#endif /* WM_ICON_H */
