/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * wallpaper.h - Native per-workspace wallpaper support
 *
 * Renders wallpaper images (PNG) as scene buffers in the background layer.
 * Each workspace can have its own wallpaper, with a default fallback.
 * Supports stretch, center, tile, and fill display modes.
 */

#ifndef WM_WALLPAPER_H
#define WM_WALLPAPER_H

#include <stdbool.h>

struct wm_server;

/* Opaque wallpaper manager */
struct wm_wallpaper;

/*
 * Initialize the wallpaper system.
 * Creates per-workspace background scene nodes under layer_background.
 * Loads wallpaper images from config paths.
 * Returns NULL if no wallpapers are configured or on error.
 */
struct wm_wallpaper *wm_wallpaper_create(struct wm_server *server);

/*
 * Destroy wallpaper manager and free all resources.
 * Safe to call with NULL.
 */
void wm_wallpaper_destroy(struct wm_wallpaper *wp);

/*
 * Switch the visible wallpaper to match the given workspace index.
 * Called from wm_workspace_switch().
 */
void wm_wallpaper_switch(struct wm_wallpaper *wp, int workspace_index);

/*
 * Reload wallpaper images from config (called on reconfigure).
 */
void wm_wallpaper_reload(struct wm_wallpaper *wp);

#endif /* WM_WALLPAPER_H */
