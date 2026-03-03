/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * menu_render.h - Menu rendering internal API
 *
 * Cairo/Pango rendering functions for menu title, items, and layout
 * computation.  Used by menu.c for display and selection updates.
 */

#ifndef WM_MENU_RENDER_H
#define WM_MENU_RENDER_H

#include <cairo.h>
#include <stdbool.h>
#include <stdint.h>

#include "menu.h"
#include "pixel_buffer.h"
#include "style.h"

/* --- Layout constants --- */

#define MENU_MIN_WIDTH     120
#define MENU_MAX_WIDTH     300
#define MENU_ITEM_PADDING  4
#define MENU_TITLE_PADDING 6
#define MENU_SEPARATOR_HEIGHT 7
#define MENU_ARROW_SIZE    8

/*
 * Convert a wm_color to a float[4] array (RGBA, 0.0-1.0).
 */
void wm_color_to_float4(const struct wm_color *c, float out[4]);

/*
 * Count items and compute menu dimensions (width, height, title_height,
 * item_height, border_width, item_count).
 */
void menu_compute_layout(struct wm_menu *menu, struct wm_style *style);

/*
 * Render the menu title bar to a Cairo surface.
 * Returns NULL on failure.
 */
cairo_surface_t *render_menu_title(struct wm_menu *menu,
	struct wm_style *style);

/*
 * Render all menu items to a single Cairo surface.
 * Returns NULL on failure.
 */
cairo_surface_t *render_menu_items(struct wm_menu *menu,
	struct wm_style *style);

/*
 * Re-render the items buffer for a selection change.
 * Updates the scene graph buffer in-place.
 */
void menu_update_render(struct wm_menu *menu);

#endif /* WM_MENU_RENDER_H */
