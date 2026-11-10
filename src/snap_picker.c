/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * snap_picker.c - The layout picker of a window dragged to the top edge
 */

#define _GNU_SOURCE
#include <math.h>
#include <stdlib.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_scene.h>
#include <cairo.h>

#include "config.h"
#include "pixel_buffer.h"
#include "server.h"
#include "snap_picker.h"

bool
wm_snap_picker_geometry(const struct wm_config *config,
	const struct wlr_box *usable, struct wm_snap_picker_geom *geom)
{
	if (!config || !usable || !geom || config->snap_layout_count < 1)
		return false;

	int count = config->snap_layout_count;
	if (count > WM_SNAP_LAYOUTS_MAX)
		count = WM_SNAP_LAYOUTS_MAX;
	int pad = WM_SNAP_PICKER_PAD;
	int tw = config->snap_layout_picker_size;

	/* No wider than the head leaves room for */
	int room = usable->width - 2 * pad - 2 * pad;
	if (tw > room)
		tw = room;
	if (tw < 20)
		tw = 20;
	int th = tw * 10 / 16;

	/* As many in a row as asked for, or as fit, and no more than there are */
	int fit = (room + pad) / (tw + pad);
	int cols = config->snap_layout_picker_columns > 0 ?
		config->snap_layout_picker_columns : fit;
	if (cols > fit)
		cols = fit;
	if (cols < 1)
		cols = 1;
	if (cols > count)
		cols = count;
	int rows = (count + cols - 1) / cols;

	geom->count = count;
	geom->panel.width = cols * tw + (cols - 1) * pad + 2 * pad;
	geom->panel.height = rows * th + (rows - 1) * pad + 2 * pad;
	geom->panel.x = usable->x + (usable->width - geom->panel.width) / 2;
	geom->panel.y = usable->y + config->snap_zone_threshold + 4;

	for (int i = 0; i < count; i++) {
		geom->thumbs[i].x = geom->panel.x + pad + (i % cols) * (tw + pad);
		geom->thumbs[i].y = geom->panel.y + pad + (i / cols) * (th + pad);
		geom->thumbs[i].width = tw;
		geom->thumbs[i].height = th;
	}
	return true;
}

struct wlr_box
wm_snap_picker_cell_box(const struct wlr_box *thumb,
	const struct wm_snap_cell *cell)
{
	int x0 = thumb->x + thumb->width * cell->x / 1000;
	int x1 = thumb->x + thumb->width * (cell->x + cell->w) / 1000;
	int y0 = thumb->y + thumb->height * cell->y / 1000;
	int y1 = thumb->y + thumb->height * (cell->y + cell->h) / 1000;
	return (struct wlr_box){ x0, y0, x1 - x0, y1 - y0 };
}

bool
wm_snap_picker_hit(const struct wm_snap_picker_geom *geom,
	const struct wm_config *config, double x, double y, int *layout,
	int *cell)
{
	for (int i = 0; i < geom->count; i++) {
		const struct wm_snap_layout *l = &config->snap_layouts[i];
		for (int c = 0; c < l->cell_count; c++) {
			struct wlr_box b = wm_snap_picker_cell_box(
				&geom->thumbs[i], &l->cells[c]);
			if (x >= b.x && x < b.x + b.width && y >= b.y &&
			    y < b.y + b.height) {
				*layout = i;
				*cell = c;
				return true;
			}
		}
	}
	return false;
}

bool
wm_snap_picker_contains(const struct wm_snap_picker_geom *geom,
	const struct wlr_box *usable, double x, double y)
{
	return x >= geom->panel.x && x < geom->panel.x + geom->panel.width &&
		y >= usable->y && y < geom->panel.y + geom->panel.height;
}

void
wm_snap_picker_target(const struct wm_config *config,
	const struct wlr_box *usable, int layout, int cell, int out[4],
	struct wlr_box *box)
{
	const struct wm_snap_cell *c = &config->snap_layouts[layout].cells[cell];
	out[0] = c->x;
	out[1] = c->y;
	out[2] = c->w;
	out[3] = c->h;
	box->x = usable->x + usable->width * c->x / 1000;
	box->y = usable->y + usable->height * c->y / 1000;
	box->width = usable->x + usable->width * (c->x + c->w) / 1000 - box->x;
	box->height = usable->y + usable->height * (c->y + c->h) / 1000 -
		box->y;
}

static void
rounded_rect(cairo_t *cr, double x, double y, double w, double h, double r)
{
	cairo_new_sub_path(cr);
	cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2, 0);
	cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2);
	cairo_arc(cr, x + r, y + h - r, r, M_PI / 2, M_PI);
	cairo_arc(cr, x + r, y + r, r, M_PI, 3 * M_PI / 2);
	cairo_close_path(cr);
}

void
snap_picker_destroy(struct wm_server *server)
{
	if (server->snap_picker) {
		wlr_scene_node_destroy(&server->snap_picker->node);
		server->snap_picker = NULL;
	}
	server->snap_picker_shown = false;
}

void
snap_picker_show(struct wm_server *server, const struct wlr_box *usable,
	int layout, int cell)
{
	struct wm_snap_picker_geom geom;
	if (!wm_snap_picker_geometry(server->config, usable, &geom))
		return;

	cairo_surface_t *surf = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, geom.panel.width, geom.panel.height);
	if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surf);
		return;
	}
	cairo_t *cr = cairo_create(surf);

	rounded_rect(cr, 0.5, 0.5, geom.panel.width - 1, geom.panel.height - 1,
		8);
	cairo_set_source_rgba(cr, 0.12, 0.13, 0.16, 0.92);
	cairo_fill_preserve(cr);
	cairo_set_source_rgba(cr, 0.3, 0.5, 0.8, 0.7);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);

	for (int i = 0; i < geom.count; i++) {
		const struct wm_snap_layout *l = &server->config->snap_layouts[i];
		struct wlr_box *t = &geom.thumbs[i];
		double tx = t->x - geom.panel.x, ty = t->y - geom.panel.y;

		cairo_rectangle(cr, tx, ty, t->width, t->height);
		cairo_set_source_rgba(cr, 0.2, 0.22, 0.27, 1.0);
		cairo_fill(cr);

		for (int c = 0; c < l->cell_count; c++) {
			struct wlr_box b = wm_snap_picker_cell_box(t,
				&l->cells[c]);
			/* A gap between neighbouring cells */
			cairo_rectangle(cr, b.x - geom.panel.x + 1.5,
				b.y - geom.panel.y + 1.5, b.width - 3.0,
				b.height - 3.0);
			if (i == layout && c == cell)
				cairo_set_source_rgba(cr, 0.3, 0.5, 0.8, 0.95);
			else
				cairo_set_source_rgba(cr, 0.5, 0.52, 0.58, 0.8);
			cairo_fill(cr);
		}
	}
	cairo_destroy(cr);

	struct wlr_buffer *buf = wlr_buffer_from_cairo(surf);
	if (!buf)
		return;
	if (server->snap_picker)
		wlr_scene_buffer_set_buffer(server->snap_picker, buf);
	else
		server->snap_picker = wlr_scene_buffer_create(
			server->layer_overlay, buf);
	wlr_buffer_drop(buf);
	if (server->snap_picker)
		wlr_scene_node_set_position(&server->snap_picker->node,
			geom.panel.x, geom.panel.y);
	server->snap_picker_shown = true;
}
