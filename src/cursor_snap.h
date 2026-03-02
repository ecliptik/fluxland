/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * cursor_snap.h - Snap zones, wireframe, and position overlay helpers
 */

#ifndef WM_CURSOR_SNAP_H
#define WM_CURSOR_SNAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <cairo.h>
#include <wlr/types/wlr_buffer.h>

#include "config.h"

struct wm_server;
struct wlr_box;

/* Convert a Cairo surface to a wlr_buffer (transfers ownership of surface) */
struct wlr_buffer *cursor_wlr_buffer_from_cairo(cairo_surface_t *surface);

/* Position overlay (shows window coordinates during move/resize) */
void position_overlay_destroy(struct wm_server *server);
void position_overlay_update(struct wm_server *server, const char *text);

/* Wireframe outline (shown during non-opaque move/resize) */
void wireframe_show(struct wm_server *server, int x, int y, int w, int h);
void wireframe_hide(struct wm_server *server);

/* Snap zone detection and preview */
enum wm_snap_zone snap_zone_detect(struct wm_server *server,
	struct wlr_box *usable);
struct wlr_box snap_zone_geometry(enum wm_snap_zone zone,
	struct wlr_box *usable);
void snap_preview_destroy(struct wm_server *server);
void snap_preview_update(struct wm_server *server, struct wlr_box *box);
void snap_zone_check(struct wm_server *server);

#endif /* WM_CURSOR_SNAP_H */
