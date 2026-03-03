/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * pixel_buffer.h - Cairo-to-wlr_buffer bridge
 *
 * Provides a wlr_buffer implementation backed by raw pixel data from a
 * Cairo surface. This lets Cairo-rendered content be passed into the
 * wlroots scene graph.
 */

#ifndef WM_PIXEL_BUFFER_H
#define WM_PIXEL_BUFFER_H

#include <cairo.h>

struct wlr_buffer;

/*
 * Create a wlr_buffer from a Cairo ARGB32 surface.
 * The Cairo surface is consumed (destroyed) by this function.
 * Returns a new wlr_buffer, or NULL on error.
 */
struct wlr_buffer *wlr_buffer_from_cairo(cairo_surface_t *surface);

#endif /* WM_PIXEL_BUFFER_H */
