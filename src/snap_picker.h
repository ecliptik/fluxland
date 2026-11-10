/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * snap_picker.h - The layout picker shown when a window is dragged to the
 * top edge: thumbnails of the configured layouts, each cell a place to drop
 * the window on
 */

#ifndef WM_SNAP_PICKER_H
#define WM_SNAP_PICKER_H

#include <wlr/util/box.h>

#include "config.h"

struct wm_server;

#define WM_SNAP_PICKER_PAD 10	/* around the thumbnails, and between them */

/* Where the panel and the thumbnails of each layout fall (layout
 * coordinates) */
struct wm_snap_picker_geom {
	struct wlr_box panel;
	int count;
	struct wlr_box thumbs[WM_SNAP_LAYOUTS_MAX];
};

/*
 * Lay the layouts of config out on a head with the given usable area, the
 * panel centred at the top just below the snap zone: thumbnails of
 * config->snap_layout_picker_size wide (16:10), config->snap_layout_picker_
 * columns to a row (or as many as the head is wide enough for).  False when
 * there is nothing to show.
 */
bool wm_snap_picker_geometry(const struct wm_config *config,
	const struct wlr_box *usable, struct wm_snap_picker_geom *geom);

/* The box of a cell of a layout, inside its thumbnail */
struct wlr_box wm_snap_picker_cell_box(const struct wlr_box *thumb,
	const struct wm_snap_cell *cell);

/* The cell under (x, y): its layout and index; false for none */
bool wm_snap_picker_hit(const struct wm_snap_picker_geom *geom,
	const struct wm_config *config, double x, double y, int *layout,
	int *cell);

/* Whether (x, y) is in the panel, or between it and the top of the head
 * (where the pointer comes from) */
bool wm_snap_picker_contains(const struct wm_snap_picker_geom *geom,
	const struct wlr_box *usable, double x, double y);

/* What a drop on a cell tiles: its permille cell (x y w h), and the box it
 * falls in on a head with that usable area (the pixel edges
 * wm_view_tile() uses) */
void wm_snap_picker_target(const struct wm_config *config,
	const struct wlr_box *usable, int layout, int cell, int out[4],
	struct wlr_box *box);

/* Show or update the picker on a head, with a cell highlighted (layout < 0
 * for none) */
void snap_picker_show(struct wm_server *server, const struct wlr_box *usable,
	int layout, int cell);
void snap_picker_destroy(struct wm_server *server);

#endif /* WM_SNAP_PICKER_H */
