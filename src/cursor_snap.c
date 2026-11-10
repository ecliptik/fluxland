/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * cursor_snap.c - Snap zones, wireframe, and position overlay helpers
 *
 * Provides snap zone detection and preview rendering for window
 * move operations, wireframe outlines for non-opaque move/resize,
 * and position/size overlay display.
 */

#include <stdlib.h>
#include <string.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <cairo.h>
#include <drm_fourcc.h>

#include "config.h"
#include "cursor_snap.h"
#include "pixel_buffer.h"
#include "server.h"
#include "snap_picker.h"
#include "output.h"
#include "render.h"
#include "style.h"
#include "view.h"

void
position_overlay_destroy(struct wm_server *server)
{
	if (server->position_overlay) {
		wlr_scene_node_destroy(
			&server->position_overlay->node);
		server->position_overlay = NULL;
	}
}

void
position_overlay_update(struct wm_server *server, const char *text)
{
	if (!server->show_position || !server->grabbed_view)
		return;

	struct wm_style *style = server->style;
	struct wm_font font = {
		.family = "sans",
		.size = 11,
		.bold = true,
		.italic = false,
		.shadow_x = 0,
		.shadow_y = 0,
		.shadow_color = {0, 0, 0, 0xFF},
	};
	struct wm_color fg = {0xE0, 0xE0, 0xE0, 0xFF};
	if (style) {
		font = style->toolbar_font;
		fg = style->toolbar_text_color;
	}

	int tw = 0, th = 0;
	cairo_surface_t *text_surf = wm_render_text(text, &font,
		&fg, 300, &tw, &th, WM_JUSTIFY_CENTER, 1.0f);
	if (!text_surf)
		return;

	/* Create background surface with padding */
	int pad = 4;
	int w = tw + 2 * pad;
	int h = th + 2 * pad;
	cairo_surface_t *surf = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, w, h);
	if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surf);
		cairo_surface_destroy(text_surf);
		return;
	}
	cairo_t *cr = cairo_create(surf);
	cairo_set_source_rgba(cr, 0.1, 0.1, 0.1, 0.85);
	cairo_paint(cr);
	cairo_set_source_surface(cr, text_surf, pad,
		(h - th) / 2);
	cairo_paint(cr);
	cairo_destroy(cr);
	cairo_surface_destroy(text_surf);

	struct wlr_buffer *buf = wlr_buffer_from_cairo(surf);
	if (!buf)
		return;

	if (server->position_overlay) {
		wlr_scene_buffer_set_buffer(
			server->position_overlay, buf);
	} else {
		server->position_overlay =
			wlr_scene_buffer_create(
				server->layer_overlay, buf);
	}
	wlr_buffer_drop(buf);

	if (server->position_overlay) {
		/* Position near the top-left corner of the window */
		struct wm_view *view = server->grabbed_view;
		int ox = view->x + 4;
		int oy = view->y - h - 2;
		if (oy < 0)
			oy = view->y + 4;
		wlr_scene_node_set_position(
			&server->position_overlay->node, ox, oy);
	}
}

/* --- Wireframe helpers --- */

#define WIREFRAME_BORDER 2

void
wireframe_show(struct wm_server *server, int x, int y, int w, int h)
{
	if (w < 1) w = 1;
	if (h < 1) h = 1;
	float color[4] = {1.0f, 1.0f, 1.0f, 0.8f};
	int b = WIREFRAME_BORDER;

	if (!server->wireframe_rects[0]) {
		/* Create 4 rect nodes: top, right, bottom, left */
		server->wireframe_rects[0] = wlr_scene_rect_create(
			server->layer_overlay, w, b, color);
		server->wireframe_rects[1] = wlr_scene_rect_create(
			server->layer_overlay, b, h, color);
		server->wireframe_rects[2] = wlr_scene_rect_create(
			server->layer_overlay, w, b, color);
		server->wireframe_rects[3] = wlr_scene_rect_create(
			server->layer_overlay, b, h, color);
		/* Allocation failure: drop the partial set, skip drawing */
		if (!server->wireframe_rects[0] || !server->wireframe_rects[1] ||
		    !server->wireframe_rects[2] || !server->wireframe_rects[3]) {
			for (int i = 0; i < 4; i++) {
				if (server->wireframe_rects[i])
					wlr_scene_node_destroy(
						&server->wireframe_rects[i]->node);
				server->wireframe_rects[i] = NULL;
			}
			return;
		}
	} else {
		/* Resize existing rects */
		wlr_scene_rect_set_size(server->wireframe_rects[0], w, b);
		wlr_scene_rect_set_size(server->wireframe_rects[1], b, h);
		wlr_scene_rect_set_size(server->wireframe_rects[2], w, b);
		wlr_scene_rect_set_size(server->wireframe_rects[3], b, h);
	}

	/* Position: top, right, bottom, left */
	wlr_scene_node_set_position(
		&server->wireframe_rects[0]->node, x, y);
	wlr_scene_node_set_position(
		&server->wireframe_rects[1]->node, x + w - b, y);
	wlr_scene_node_set_position(
		&server->wireframe_rects[2]->node, x, y + h - b);
	wlr_scene_node_set_position(
		&server->wireframe_rects[3]->node, x, y);

	server->wireframe_active = true;
	server->wireframe_x = x;
	server->wireframe_y = y;
	server->wireframe_w = w;
	server->wireframe_h = h;
}

void
wireframe_hide(struct wm_server *server)
{
	for (int i = 0; i < 4; i++) {
		if (server->wireframe_rects[i]) {
			wlr_scene_node_destroy(
				&server->wireframe_rects[i]->node);
			server->wireframe_rects[i] = NULL;
		}
	}
	server->wireframe_active = false;
}

/* --- Window snap zone helpers --- */

/*
 * Detect which snap zone the cursor is in based on its position
 * relative to the output's usable area edges.
 */
enum wm_snap_zone
snap_zone_detect(struct wm_server *server, struct wlr_box *usable)
{
	int threshold = 10;
	if (server->config)
		threshold = server->config->snap_zone_threshold;

	int cx = (int)server->cursor->x;
	int cy = (int)server->cursor->y;

	bool at_left = (cx - usable->x) < threshold;
	bool at_right = (usable->x + usable->width - cx) < threshold;
	bool at_top = (cy - usable->y) < threshold;
	bool at_bottom = (usable->y + usable->height - cy) < threshold;

	/* Corners take priority over edges */
	if (at_left && at_top)
		return WM_SNAP_ZONE_TOPLEFT;
	if (at_right && at_top)
		return WM_SNAP_ZONE_TOPRIGHT;
	if (at_left && at_bottom)
		return WM_SNAP_ZONE_BOTTOMLEFT;
	if (at_right && at_bottom)
		return WM_SNAP_ZONE_BOTTOMRIGHT;

	/* Edges */
	if (at_left)
		return WM_SNAP_ZONE_LEFT_HALF;
	if (at_right)
		return WM_SNAP_ZONE_RIGHT_HALF;
	if (at_top)
		return WM_SNAP_ZONE_MAXIMIZE;
	if (at_bottom)
		return WM_SNAP_ZONE_BOTTOM_HALF;

	return WM_SNAP_ZONE_NONE;
}

/*
 * Calculate the target geometry for a given snap zone
 * within the output's usable area.
 */
struct wlr_box
snap_zone_geometry(enum wm_snap_zone zone, struct wlr_box *usable)
{
	struct wlr_box box = {0};
	int half_w = usable->width / 2;
	int half_h = usable->height / 2;

	switch (zone) {
	case WM_SNAP_ZONE_LEFT_HALF:
		box.x = usable->x;
		box.y = usable->y;
		box.width = half_w;
		box.height = usable->height;
		break;
	case WM_SNAP_ZONE_RIGHT_HALF:
		box.x = usable->x + half_w;
		box.y = usable->y;
		box.width = usable->width - half_w;
		box.height = usable->height;
		break;
	case WM_SNAP_ZONE_MAXIMIZE:
		/* The top edge fills the head, as on Windows and KDE */
		box = *usable;
		break;
	case WM_SNAP_ZONE_BOTTOM_HALF:
		box.x = usable->x;
		box.y = usable->y + half_h;
		box.width = usable->width;
		box.height = usable->height - half_h;
		break;
	case WM_SNAP_ZONE_TOPLEFT:
		box.x = usable->x;
		box.y = usable->y;
		box.width = half_w;
		box.height = half_h;
		break;
	case WM_SNAP_ZONE_TOPRIGHT:
		box.x = usable->x + half_w;
		box.y = usable->y;
		box.width = usable->width - half_w;
		box.height = half_h;
		break;
	case WM_SNAP_ZONE_BOTTOMLEFT:
		box.x = usable->x;
		box.y = usable->y + half_h;
		box.width = half_w;
		box.height = usable->height - half_h;
		break;
	case WM_SNAP_ZONE_BOTTOMRIGHT:
		box.x = usable->x + half_w;
		box.y = usable->y + half_h;
		box.width = usable->width - half_w;
		box.height = usable->height - half_h;
		break;
	default:
		break;
	}
	return box;
}

/*
 * Destroy the snap preview overlay.
 */
static void
preview_clear(struct wm_server *server)
{
	if (server->snap_preview) {
		wlr_scene_node_destroy(&server->snap_preview->node);
		server->snap_preview = NULL;
	}
	server->snap_zone = WM_SNAP_ZONE_NONE;
}

void
snap_preview_destroy(struct wm_server *server)
{
	preview_clear(server);
	snap_picker_destroy(server);
}

/*
 * The preview is above every window but the one being dragged, which stays
 * over it (as on Windows): in the dragged window's own tree, just under it.
 */
static void
snap_preview_stack(struct wm_server *server)
{
	struct wm_view *view = server->grabbed_view;
	if (!server->snap_preview || !view || !view->scene_tree ||
	    !view->scene_tree->node.parent)
		return;
	struct wlr_scene_node *node = &server->snap_preview->node;
	if (node->parent != view->scene_tree->node.parent)
		wlr_scene_node_reparent(node, view->scene_tree->node.parent);
	wlr_scene_node_place_below(node, &view->scene_tree->node);
}

/*
 * Create or update the snap preview overlay showing the target zone.
 * Renders a semi-transparent rectangle with a border outline.
 */
void
snap_preview_update(struct wm_server *server, struct wlr_box *box)
{
	if (box->width < 1 || box->height < 1)
		return;

	/* Create a Cairo surface for the preview */
	cairo_surface_t *surf = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32, box->width, box->height);
	if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surf);
		return;
	}

	cairo_t *cr = cairo_create(surf);

	/* Fill with semi-transparent highlight (30% opacity blue) */
	cairo_set_source_rgba(cr, 0.3, 0.5, 0.8, 0.3);
	cairo_paint(cr);

	/* Draw a 3px border outline */
	double border = 3.0;
	cairo_set_source_rgba(cr, 0.3, 0.5, 0.8, 0.7);
	cairo_set_line_width(cr, border);
	cairo_rectangle(cr, border / 2.0, border / 2.0,
		box->width - border, box->height - border);
	cairo_stroke(cr);

	cairo_destroy(cr);

	struct wlr_buffer *buf = wlr_buffer_from_cairo(surf);
	if (!buf)
		return;

	if (server->snap_preview) {
		wlr_scene_buffer_set_buffer(server->snap_preview, buf);
	} else {
		server->snap_preview = wlr_scene_buffer_create(
			server->layer_overlay, buf);
	}
	wlr_buffer_drop(buf);

	if (server->snap_preview) {
		wlr_scene_node_set_position(
			&server->snap_preview->node, box->x, box->y);
		snap_preview_stack(server);
	}
}

/*
 * Check cursor position against output edges during a window move
 * and show/update/hide the snap preview as appropriate.
 */
void
snap_zone_check(struct wm_server *server)
{
	if (!server->config || !server->config->enable_window_snapping) {
		if (server->snap_zone != WM_SNAP_ZONE_NONE)
			snap_preview_destroy(server);
		return;
	}

	/* Find the output the cursor is on */
	struct wlr_output *wlr_output = wlr_output_layout_output_at(
		server->output_layout, server->cursor->x, server->cursor->y);
	if (!wlr_output) {
		if (server->snap_zone != WM_SNAP_ZONE_NONE)
			snap_preview_destroy(server);
		return;
	}

	/* Find our wm_output wrapper to get usable_area */
	struct wm_output *output;
	struct wlr_box usable = {0};
	bool found = false;
	wl_list_for_each(output, &server->outputs, link) {
		if (output->wlr_output == wlr_output) {
			usable = output->usable_area;
			found = true;
			break;
		}
	}
	if (!found) {
		if (server->snap_zone != WM_SNAP_ZONE_NONE)
			snap_preview_destroy(server);
		return;
	}

	enum wm_snap_zone zone = snap_zone_detect(server, &usable);

	if (server->config->snap_layout_picker) {
		struct wm_snap_picker_geom geom;
		double cx = server->cursor->x, cy = server->cursor->y;
		bool have = wm_snap_picker_geometry(server->config, &usable,
			&geom);
		bool on_head = server->snap_picker_shown &&
			server->snap_picker_usable.x == usable.x &&
			server->snap_picker_usable.y == usable.y;
		bool show = have && (zone == WM_SNAP_ZONE_MAXIMIZE ||
			(on_head && wm_snap_picker_contains(&geom, &usable,
				cx, cy)));
		if (!show) {
			snap_picker_destroy(server);
		} else {
			int layout = -1, cell = -1;
			bool hit = wm_snap_picker_hit(&geom, server->config,
				cx, cy, &layout, &cell);
			if (!hit) {
				layout = cell = -1;
			}
			if (!server->snap_picker_shown ||
			    layout != server->snap_picker_layout ||
			    cell != server->snap_picker_cell) {
				server->snap_picker_layout = layout;
				server->snap_picker_cell = cell;
				server->snap_picker_usable = usable;
				snap_picker_show(server, &usable, layout, cell);
			}
			if (hit) {
				struct wlr_box box;
				wm_snap_picker_target(server->config, &usable,
					layout, cell, server->snap_tile, &box);
				if (server->snap_zone != WM_SNAP_ZONE_TILE ||
				    memcmp(&box, &server->snap_preview_box,
						sizeof(box)) != 0) {
					server->snap_zone = WM_SNAP_ZONE_TILE;
					server->snap_preview_box = box;
					snap_preview_update(server,
						&server->snap_preview_box);
				}
				return;
			}
			if (zone != WM_SNAP_ZONE_MAXIMIZE) {
				/* On the panel, between cells */
				if (server->snap_zone != WM_SNAP_ZONE_NONE)
					preview_clear(server);
				return;
			}
			if (server->snap_zone == WM_SNAP_ZONE_TILE)
				preview_clear(server);
		}
	}

	if (zone == WM_SNAP_ZONE_NONE) {
		if (server->snap_zone != WM_SNAP_ZONE_NONE)
			snap_preview_destroy(server);
		return;
	}

	/* Zone changed — update the preview */
	if ((int)zone != server->snap_zone ||
	    memcmp(&usable, &server->snap_picker_usable, sizeof(usable)) != 0) {
		server->snap_zone = zone;
		server->snap_picker_usable = usable;
		server->snap_preview_box = snap_zone_geometry(zone, &usable);
		snap_preview_update(server, &server->snap_preview_box);
	}
}
