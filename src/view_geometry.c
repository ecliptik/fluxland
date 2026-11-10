/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * view_geometry.c - View geometry, tiling, maximize/fullscreen, multi-monitor
 */

#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>

#include "config.h"
#include "decoration.h"
#include "foreign_toplevel.h"
#include "output.h"
#include "server.h"
#include "view.h"
#include "view_geometry.h"
#include "workspace.h"

/* --- Helpers --- */

void
wm_view_frame_extents(struct wm_view *view, int *top, int *bottom,
	int *left, int *right)
{
	*top = *bottom = *left = *right = 0;
	if (view->decoration && view->show_decoration && !view->fullscreen)
		wm_decoration_get_extents(view->decoration,
			top, bottom, left, right);
}

void
wm_view_set_frame_box(struct wm_view *view, const struct wlr_box *box)
{
	int top, bottom, left, right;
	wm_view_frame_extents(view, &top, &bottom, &left, &right);
	int w = box->width - left - right;
	int h = box->height - top - bottom;
	wm_view_move(view, box->x, box->y);
	wm_view_set_size(view, w > 1 ? w : 1, h > 1 ? h : 1);
}

void
wm_view_frame_box(struct wm_view *view, struct wlr_box *box)
{
	int top, bottom, left, right;
	wm_view_frame_extents(view, &top, &bottom, &left, &right);
	struct wlr_box geo;
	wm_view_get_geometry(view, &geo);
	box->x = view->x;
	box->y = view->y;
	box->width = geo.width + left + right;
	box->height = geo.height + top + bottom;
}

/* --- Size hints (Fluxbox's SizeHints, WindowState.cc) --- */

void
wm_view_size_hints(struct wm_view *view, struct wm_size_hints *hints)
{
	memset(hints, 0, sizeof(*hints));
	hints->inc_w = hints->inc_h = 1;
	if (view->ignore_size_hints || !view->impl || !view->impl->size_hints)
		return;
	view->impl->size_hints(view, hints);
	if (hints->inc_w < 1)
		hints->inc_w = 1;
	if (hints->inc_h < 1)
		hints->inc_h = 1;
}

static unsigned
increase_to_multiple(unsigned val, unsigned inc)
{
	return val % inc ? val + inc - val % inc : val;
}

static unsigned
decrease_to_multiple(unsigned val, unsigned inc)
{
	return val % inc ? val - val % inc : val;
}

static void
closest_point_to_aspect(unsigned *x, unsigned *y, unsigned ax, unsigned ay)
{
	double u = (double)(*x * ax + *y * ay) / (double)(ax * ax + ay * ay);
	*x = (unsigned)(u * ax);
	*y = (unsigned)(u * ay);
}

void
wm_size_hints_apply(const struct wm_size_hints *hints, int *width,
	int *height, bool make_fit)
{
	const struct wm_size_hints *s = hints;
	unsigned inc_w = s->inc_w > 0 ? (unsigned)s->inc_w : 1;
	unsigned inc_h = s->inc_h > 0 ? (unsigned)s->inc_h : 1;
	unsigned base_w = s->base_w > 0 ? (unsigned)s->base_w : 0;
	unsigned base_h = s->base_h > 0 ? (unsigned)s->base_h : 0;
	unsigned in_w = *width > 0 ? (unsigned)*width : 1;
	unsigned in_h = *height > 0 ? (unsigned)*height : 1;
	unsigned min_w = s->min_w > 0 ? (unsigned)s->min_w : 0;
	unsigned min_h = s->min_h > 0 ? (unsigned)s->min_h : 0;
	unsigned max_wh = s->max_w > 0 ? (unsigned)s->max_w : 0;
	unsigned max_hh = s->max_h > 0 ? (unsigned)s->max_h : 0;
	unsigned mnx = s->min_aspect_x, mny = s->min_aspect_y;
	unsigned mxx = s->max_aspect_x, mxy = s->max_aspect_y;

	/* The aspect ratios go on the size over the base */
	unsigned w = in_w > base_w ? in_w - base_w : 0;
	unsigned h = in_h > base_h ? in_h - base_h : 0;
	if (mny > 0 && w * mny < mnx * h) {
		closest_point_to_aspect(&w, &h, mnx, mny);
		w = increase_to_multiple(w, inc_w);
		h = decrease_to_multiple(h, inc_h);
	} else if (mxx > 0 && w * mxy > mxx * h) {
		closest_point_to_aspect(&w, &h, mxx, mxy);
		w = decrease_to_multiple(w, inc_w);
		h = increase_to_multiple(h, inc_h);
	}

	if (w + base_w < min_w) {
		w = increase_to_multiple(min_w - base_w, inc_w);
		if (mxx > 0 && w * mxy > mxx * h)
			h = increase_to_multiple(w * mxy / mxx, inc_h);
	}
	if (h + base_h < min_h) {
		h = increase_to_multiple(min_h - base_h, inc_h);
		if (mny > 0 && w * mny < mnx * h)
			w = increase_to_multiple(h * mnx / mny, inc_w);
	}

	unsigned max_w = make_fit && (in_w < max_wh || max_wh == 0) ?
		in_w : max_wh;
	unsigned max_h = make_fit && (in_h < max_hh || max_hh == 0) ?
		in_h : max_hh;
	if (max_w > 0 && w + base_w > max_w)
		w = max_w > base_w ? max_w - base_w : 0;
	if (max_h > 0 && h + base_h > max_h)
		h = max_h > base_h ? max_h - base_h : 0;

	w = decrease_to_multiple(w, inc_w);
	h = decrease_to_multiple(h, inc_h);

	if (mny > 0 && w * mny < mnx * h)
		h = decrease_to_multiple(w * mny / mnx, inc_h);
	if (mxx > 0 && w * mxy > mxx * h)
		w = decrease_to_multiple(h * mxx / mxy, inc_w);

	*width = (int)(w + base_w);
	*height = (int)(h + base_h);
}

void
wm_size_hints_display(const struct wm_size_hints *hints, int width,
	int height, int *dw, int *dh)
{
	int inc_w = hints->inc_w > 0 ? hints->inc_w : 1;
	int inc_h = hints->inc_h > 0 ? hints->inc_h : 1;
	*dw = (width - hints->base_w) / inc_w;
	*dh = (height - hints->base_h) / inc_h;
}

void
wm_view_initial_size(struct wm_view *view, bool fullscreen, int *width,
	int *height)
{
	*width = *height = 0;
	struct wlr_box box;
	if (fullscreen) {
		struct wm_output *output = wm_output_for_view(view);
		if (!output)
			return;
		wm_output_layout_box(output, &box);
		*width = box.width;
		*height = box.height;
		return;
	}
	wm_view_ensure_decoration(view);
	if (!wm_view_maximize_area(view, &box))
		return;
	int top, bottom, left, right;
	wm_view_frame_extents(view, &top, &bottom, &left, &right);
	*width = box.width - left - right;
	*height = box.height - top - bottom;
	if (*width < 1 || *height < 1)
		*width = *height = 0;
}

/*
 * A maximized window's frame box: the area, or with maxIgnoreIncrement
 * off what of it the client's size hints allow (FbWinFrame::applyState)
 */
static void
set_maximized_box(struct wm_view *view, const struct wlr_box *area)
{
	struct wlr_box box = *area;
	struct wm_config *cfg = view->server->config;
	if (cfg && !cfg->max_ignore_increment) {
		int top, bottom, left, right;
		wm_view_frame_extents(view, &top, &bottom, &left, &right);
		int w = box.width - left - right, h = box.height - top - bottom;
		struct wm_size_hints hints;
		wm_view_size_hints(view, &hints);
		wm_size_hints_apply(&hints, &w, &h, true);
		box.width = w + left + right;
		box.height = h + top + bottom;
	}
	wm_view_set_frame_box(view, &box);
}

bool
wm_view_maximize_area(struct wm_view *view, struct wlr_box *area)
{
	struct wm_output *output = wm_output_for_view(view);
	if (!output)
		return false;
	if (view->server->config && view->server->config->full_maximization)
		wm_output_layout_box(output, area);
	else
		*area = output->usable_area;
	/* tabs.maxOver: the frame fills it, its external tabs outside
	 * (FbWinFrame::applyState) */
	if (view->server->config && view->server->config->tabs_max_over &&
	    view->decoration && view->show_decoration && !view->fullscreen) {
		int x, y, w, h;
		wm_decoration_tab_offsets(view->decoration, &x, &y, &w, &h);
		area->x -= x;
		area->y -= y;
		area->width += w;
		area->height += h;
	}
	return !wlr_box_empty(area);
}

bool
get_view_output_area(struct wm_view *view, struct wlr_box *area)
{
	struct wm_output *output = wm_output_for_view(view);
	if (!output)
		return false;
	*area = output->usable_area;
	if (wlr_box_empty(area))
		wm_output_layout_box(output, area);
	return !wlr_box_empty(area);
}

/* --- Toggle maximize --- */

void
wm_view_toggle_maximize(struct wm_view *view)
{
	/* Mapped, not necessarily shown yet (map applies pending states) */
	if (!wm_view_surface(view) || !view->mapped)
		return;

	if (view->maximized) {
		/* Restore saved geometry */
		wm_view_set_size(view, view->saved_geometry.width,
			view->saved_geometry.height);
		wlr_scene_node_set_position(&view->scene_tree->node,
			view->saved_geometry.x, view->saved_geometry.y);
		view->x = view->saved_geometry.x;
		view->y = view->saved_geometry.y;
		view->maximized = false;
		view->snap_return.active = false;
	} else {
		/* Save current geometry and maximize */
		wm_view_get_geometry(view, &view->saved_geometry);

		struct wlr_box max_box;
		if (wm_view_maximize_area(view, &max_box))
			set_maximized_box(view, &max_box);
		view->maximized = true;
	}

	wm_view_set_maximized_state(view, view->maximized);
	wm_foreign_toplevel_set_maximized(view, view->maximized);
}

/* --- Toggle fullscreen --- */

void
wm_view_toggle_fullscreen(struct wm_view *view)
{
	if (!wm_view_surface(view) || !view->mapped)
		return;

	if (view->fullscreen) {
		/* Restore from fullscreen */
		wm_view_set_size(view, view->saved_geometry.width,
			view->saved_geometry.height);
		wlr_scene_node_set_position(&view->scene_tree->node,
			view->saved_geometry.x, view->saved_geometry.y);
		view->x = view->saved_geometry.x;
		view->y = view->saved_geometry.y;
		view->fullscreen = false;

		/* Reparent back from overlay to workspace/sticky tree */
		struct wlr_scene_tree *parent;
		if (view->sticky) {
			parent = view->server->sticky_tree;
		} else if (view->workspace) {
			parent = view->workspace->tree;
		} else {
			parent = view->server->view_tree;
		}
		wlr_scene_node_reparent(&view->scene_tree->node, parent);

		/* Show decorations and reposition XDG surface */
		if (view->decoration) {
			wlr_scene_node_set_enabled(
				&view->decoration->tree->node, true);
			int top, bottom, left, right;
			wm_decoration_get_extents(view->decoration,
				&top, &bottom, &left, &right);
			struct wlr_scene_node *child;
			wl_list_for_each(child,
				&view->scene_tree->children, link) {
				if (child !=
					&view->decoration->tree->node) {
					wlr_scene_node_set_position(
						child, left, top);
					break;
				}
			}
		}
	} else {
		/* Enter fullscreen */
		if (!view->maximized) {
			wm_view_get_geometry(view, &view->saved_geometry);
		}

		struct wm_output *output = wm_output_for_view(view);
		view->fullscreen = true;
		if (output) {
			struct wlr_box output_box;
			wm_output_layout_box(output, &output_box);
			wm_view_set_frame_box(view, &output_box);
		}

		/* Hide decorations and move XDG surface to (0,0) */
		if (view->decoration) {
			wlr_scene_node_set_enabled(
				&view->decoration->tree->node, false);
			struct wlr_scene_node *child;
			wl_list_for_each(child,
				&view->scene_tree->children, link) {
				if (child !=
					&view->decoration->tree->node) {
					wlr_scene_node_set_position(
						child, 0, 0);
					break;
				}
			}
		}

		/* Reparent to overlay layer (above toolbar) */
		wlr_scene_node_reparent(&view->scene_tree->node,
			view->server->layer_overlay);
	}

	wm_view_set_fullscreen_state(view, view->fullscreen);
	wm_foreign_toplevel_set_fullscreen(view, view->fullscreen);
}

/* --- Maximize vertical/horizontal --- */

void
wm_view_maximize_vert(struct wm_view *view)
{
	if (!view) {
		return;
	}

	if (!wm_view_surface(view) || !view->scene_tree->node.enabled)
		return;

	if (view->maximized_vert) {
		/* Restore saved y and height */
		struct wlr_box cur;
		wm_view_get_geometry(view, &cur);
		wm_view_move(view, view->x, view->saved_geometry.y);
		wm_view_set_size(view, cur.width, view->saved_geometry.height);
		view->maximized_vert = false;
	} else {
		/* Save current geometry */
		struct wlr_box geo;
		wm_view_get_geometry(view, &geo);
		view->saved_geometry.y = view->y;
		view->saved_geometry.height = geo.height;

		struct wlr_box area, frame;
		if (wm_view_maximize_area(view, &area)) {
			wm_view_frame_box(view, &frame);
			frame.y = area.y;
			frame.height = area.height;
			set_maximized_box(view, &frame);
		}
		view->maximized_vert = true;
	}
}

void
wm_view_maximize_horiz(struct wm_view *view)
{
	if (!view) {
		return;
	}

	if (!wm_view_surface(view) || !view->scene_tree->node.enabled)
		return;

	if (view->maximized_horiz) {
		/* Restore saved x and width */
		struct wlr_box cur;
		wm_view_get_geometry(view, &cur);
		wm_view_move(view, view->saved_geometry.x, view->y);
		wm_view_set_size(view, view->saved_geometry.width, cur.height);
		view->maximized_horiz = false;
	} else {
		/* Save current geometry */
		struct wlr_box geo;
		wm_view_get_geometry(view, &geo);
		view->saved_geometry.x = view->x;
		view->saved_geometry.width = geo.width;

		struct wlr_box area, frame;
		if (wm_view_maximize_area(view, &area)) {
			wm_view_frame_box(view, &frame);
			frame.x = area.x;
			frame.width = area.width;
			set_maximized_box(view, &frame);
		}
		view->maximized_horiz = true;
	}
}

/* --- LHalf / RHalf (half-screen tiling) --- */

void
wm_view_lhalf(struct wm_view *view)
{
	if (!view)
		return;

	if (!wm_view_surface(view) || !view->scene_tree->node.enabled)
		return;

	if (view->lhalf) {
		/* Toggle off — restore saved geometry */
		view->x = view->saved_geometry.x;
		view->y = view->saved_geometry.y;
		wm_view_set_size(view, view->saved_geometry.width,
			view->saved_geometry.height);
		wlr_scene_node_set_position(&view->scene_tree->node,
			view->x, view->y);
		view->lhalf = false;
		return;
	}

	/* Save geometry if not already in a tiled/maximized state */
	if (!view->maximized && !view->rhalf)
		wm_view_get_geometry(view, &view->saved_geometry);

	struct wlr_box area;
	if (!get_view_output_area(view, &area))
		return;

	area.width /= 2;
	wm_view_set_frame_box(view, &area);

	view->lhalf = true;
	view->rhalf = false;
	view->maximized = false;
}

void
wm_view_rhalf(struct wm_view *view)
{
	if (!view)
		return;

	if (!wm_view_surface(view) || !view->scene_tree->node.enabled)
		return;

	if (view->rhalf) {
		/* Toggle off — restore saved geometry */
		view->x = view->saved_geometry.x;
		view->y = view->saved_geometry.y;
		wm_view_set_size(view, view->saved_geometry.width,
			view->saved_geometry.height);
		wlr_scene_node_set_position(&view->scene_tree->node,
			view->x, view->y);
		view->rhalf = false;
		return;
	}

	/* Save geometry if not already in a tiled/maximized state */
	if (!view->maximized && !view->lhalf)
		wm_view_get_geometry(view, &view->saved_geometry);

	struct wlr_box area;
	if (!get_view_output_area(view, &area))
		return;

	int half_w = area.width / 2;
	area.x += half_w;
	area.width -= half_w;
	wm_view_set_frame_box(view, &area);

	view->rhalf = true;
	view->lhalf = false;
	view->maximized = false;
}

/* --- Relative resize --- */

void
wm_view_resize_by(struct wm_view *view, int dw, int dh)
{
	if (!view)
		return;

	if (!wm_view_surface(view) || !view->scene_tree->node.enabled)
		return;

	struct wlr_box geo;
	wm_view_get_geometry(view, &geo);
	int new_w = geo.width + dw;
	int new_h = geo.height + dh;
	if (new_w > 10 && new_h > 10)
		wm_view_set_size(view, new_w, new_h);
}

/* --- Multi-monitor helpers --- */

void
move_view_to_output(struct wm_view *view, struct wm_output *out)
{
	struct wlr_box area;
	if (!wlr_box_empty(&out->usable_area))
		area = out->usable_area;
	else
		wm_output_layout_box(out, &area);

	struct wlr_box frame;
	wm_view_frame_box(view, &frame);
	wm_view_move(view, area.x + (area.width - frame.width) / 2,
		area.y + (area.height - frame.height) / 2);
	/* Maximized and fullscreen windows fill the new head */
	wm_view_refit(view);
}

void
wm_view_clear_maximized(struct wm_view *view)
{
	bool was_maximized = view->maximized;
	view->maximized = false;
	view->maximized_vert = false;
	view->maximized_horiz = false;
	view->lhalf = false;
	view->rhalf = false;
	view->tile.active = false;
	view->snap_return.active = false;
	/* Only a full maximize is a state the client knows of; the size it
	 * has stays the one it was given */
	if (was_maximized) {
		wm_view_set_maximized_state(view, false);
		wm_foreign_toplevel_set_maximized(view, false);
	}
}

/* A cell's frame: its edges are where the permille boundaries fall, the same
 * pixel for the two cells either side of one */
static void
tile_frame(const struct wm_tile *tile, const struct wlr_box *area,
	struct wlr_box *frame);

/* How near the corner of a snapped window has to be to where it is snapped
 * to for it to count as there, in pixels */
#define SNAP_SLACK 5

/* Whether a half or tiled window is still where it was put: dragging it
 * away is the user's own doing, which is not undone */
static bool
snapped_in_place(struct wm_view *view)
{
	struct wm_tile cell = view->tile;
	if (view->lhalf)
		cell = (struct wm_tile){ true, 0, 0, 500, 1000 };
	else if (view->rhalf)
		cell = (struct wm_tile){ true, 500, 0, 500, 1000 };
	else if (!view->tile.active)
		return true;

	struct wlr_box area, want, frame;
	if (!wm_view_maximize_area(view, &area))
		return true;
	tile_frame(&cell, &area, &want);
	wm_view_frame_box(view, &frame);
	return abs(frame.x - want.x) <= SNAP_SLACK &&
		abs(frame.y - want.y) <= SNAP_SLACK;
}

void
wm_view_settle_on_head(struct wm_view *view, int old_head_x, int old_head_y)
{
	if (!view || !(view->maximized || view->maximized_vert ||
	    view->maximized_horiz || view->lhalf || view->rhalf ||
	    view->tile.active || view->fullscreen))
		return;
	/* A half or a tile dragged elsewhere stays where it is dropped, free
	 * of its snap; a maximized window goes with its head */
	if (!view->maximized && !view->maximized_vert &&
	    !view->maximized_horiz && !view->fullscreen &&
	    !snapped_in_place(view)) {
		wm_view_clear_maximized(view);
		return;
	}
	struct wm_output *output = wm_output_for_view(view);
	if (output) {
		struct wlr_box head;
		wm_output_layout_box(output, &head);
		view->saved_geometry.x += head.x - old_head_x;
		view->saved_geometry.y += head.y - old_head_y;
	}
	wm_view_refit(view);
}

static void
tile_frame(const struct wm_tile *tile, const struct wlr_box *area,
	struct wlr_box *frame)
{
	int x0 = area->x + (int)((long)area->width * tile->x / 1000);
	int x1 = area->x + (int)((long)area->width * (tile->x + tile->w) /
		1000);
	int y0 = area->y + (int)((long)area->height * tile->y / 1000);
	int y1 = area->y + (int)((long)area->height * (tile->y + tile->h) /
		1000);
	frame->x = x0;
	frame->y = y0;
	frame->width = x1 - x0;
	frame->height = y1 - y0;
}

void
wm_view_tile(struct wm_view *view, int x, int y, int w, int h)
{
	if (!view || w < 1 || h < 1 || x < 0 || y < 0 || x + w > 1000 ||
	    y + h > 1000)
		return;
	if (!wm_view_surface(view) || !view->scene_tree->node.enabled ||
	    view->fullscreen)
		return;

	if (view->tile.active && view->tile.x == x && view->tile.y == y &&
	    view->tile.w == w && view->tile.h == h) {
		/* The same cell again: back to what it was */
		view->tile.active = false;
		view->x = view->saved_geometry.x;
		view->y = view->saved_geometry.y;
		wm_view_set_size(view, view->saved_geometry.width,
			view->saved_geometry.height);
		wlr_scene_node_set_position(&view->scene_tree->node,
			view->x, view->y);
		return;
	}

	struct wlr_box area, frame;
	if (!wm_view_maximize_area(view, &area))
		return;

	bool in_state = view->maximized || view->maximized_vert ||
		view->maximized_horiz || view->lhalf || view->rhalf ||
		view->tile.active;
	if (!in_state)
		wm_view_get_geometry(view, &view->saved_geometry);
	wm_view_clear_maximized(view);

	view->tile = (struct wm_tile){ true, x, y, w, h };
	tile_frame(&view->tile, &area, &frame);
	wm_view_set_frame_box(view, &frame);
}

bool
wm_view_pull_restore(struct wm_view *view, double px, double py,
	double *grab_x, double *grab_y)
{
	if (!view || !view->maximized || view->saved_geometry.width <= 0 ||
	    view->saved_geometry.height <= 0)
		return false;

	struct wlr_box frame;
	wm_view_frame_box(view, &frame);
	double across = frame.width > 0 ? (px - frame.x) / frame.width : 0.5;
	double down = py - frame.y;

	struct wlr_box saved = view->saved_geometry;
	wm_view_clear_maximized(view);
	wm_view_set_size(view, saved.width, saved.height);

	int top, bottom, left, right;
	wm_view_frame_extents(view, &top, &bottom, &left, &right);
	int new_w = saved.width + left + right;
	int new_h = saved.height + top + bottom;
	if (down > new_h - 1)
		down = new_h - 1;
	wm_view_move(view, (int)(px - across * new_w), (int)(py - down));
	*grab_x = px - view->x;
	*grab_y = py - view->y;
	return true;
}

void
wm_view_snap_maximize(struct wm_view *view, const struct wlr_box *area,
	const struct wlr_box *prev_saved, int old_head_x, int old_head_y)
{
	/* On the head first, where it can be maximized from */
	struct wlr_box frame;
	wm_view_frame_box(view, &frame);
	int x = view->x;
	if (x + frame.width > area->x + area->width)
		x = area->x + area->width - frame.width;
	if (x < area->x)
		x = area->x;
	wm_view_move(view, x, area->y);
	wm_view_toggle_maximize(view);

	if (prev_saved && view->maximized) {
		struct wm_output *output = wm_output_for_view(view);
		struct wlr_box head = { 0 };
		if (output)
			wm_output_layout_box(output, &head);
		view->saved_geometry = *prev_saved;
		view->saved_geometry.x += head.x - old_head_x;
		view->saved_geometry.y += head.y - old_head_y;
	}
}

/* Where a window is in the SnapStep ladder: which side (-1 left, 1 right,
 * 0 neither) and row (-1 top, 1 bottom, 0 the whole height) of its head it
 * holds, as a half or a quarter */
static void
snap_position(const struct wm_view *view, int *side, int *row)
{
	*side = *row = 0;
	if (view->lhalf)
		*side = -1;
	else if (view->rhalf)
		*side = 1;
	else if (view->tile.active && view->tile.w == 500) {
		if (view->tile.x == 0)
			*side = -1;
		else if (view->tile.x == 500)
			*side = 1;
		if (*side && view->tile.h == 500)
			*row = view->tile.y == 0 ? -1 : 1;
		else if (*side && view->tile.h != 1000)
			*side = 0;
	}
}

/* Back to the size the window had before it was snapped, on the head it is
 * on now: where it was is on another head if it was snapped there since,
 * and it goes to the same place relative to this head's size */
static void
snap_undo(struct wm_view *view)
{
	struct wm_output *here = wm_output_for_view(view);
	struct wlr_box saved = view->saved_geometry;
	if (view->lhalf)
		wm_view_lhalf(view);
	else if (view->rhalf)
		wm_view_rhalf(view);
	else if (view->tile.active)
		wm_view_tile(view, view->tile.x, view->tile.y, view->tile.w,
			view->tile.h);

	struct wm_output *was = wm_output_for_view(view);
	if (!here || !was || here == was)
		return;
	struct wlr_box to, from, frame;
	wm_output_layout_box(here, &to);
	wm_output_layout_box(was, &from);
	if (wlr_box_empty(&to) || wlr_box_empty(&from))
		return;
	wm_view_frame_box(view, &frame);
	/* The client has not answered the resize yet: the frame it will have
	 * is the saved size with the decorations around it */
	int top, bottom, left, right;
	wm_view_frame_extents(view, &top, &bottom, &left, &right);
	frame.width = saved.width + left + right;
	frame.height = saved.height + top + bottom;
	int x = to.x + (int)((long)(frame.x - from.x) * to.width / from.width);
	int y = to.y + (int)((long)(frame.y - from.y) * to.height / from.height);
	if (x + frame.width > to.x + to.width)
		x = to.x + to.width - frame.width;
	if (y + frame.height > to.y + to.height)
		y = to.y + to.height - frame.height;
	if (x < to.x)
		x = to.x;
	if (y < to.y)
		y = to.y;
	wm_view_move(view, x, y);
}

/* The half (row 0) or quarter of a side of the head */
static void
snap_to(struct wm_view *view, int side, int row)
{
	int h = row ? 500 : 1000;
	wm_view_tile(view, side < 0 ? 0 : 500, row > 0 ? 500 : 0, 500, h);
}

/* The head in a direction from the window's, if there is one */
static struct wm_output *
adjacent_head(struct wm_view *view, int side)
{
	struct wm_output *cur = wm_output_for_view(view);
	if (!cur)
		return NULL;
	struct wlr_box box;
	wm_output_layout_box(cur, &box);
	/* The nearest head on that side that shares some of its height */
	struct wm_output *best = NULL;
	int best_gap = 0;
	struct wm_output *out;
	wl_list_for_each(out, &view->server->outputs, link) {
		struct wlr_box o;
		if (out == cur)
			continue;
		wm_output_layout_box(out, &o);
		if (wlr_box_empty(&o) || o.y >= box.y + box.height ||
		    o.y + o.height <= box.y)
			continue;
		int gap = side < 0 ? box.x - (o.x + o.width) :
			o.x - (box.x + box.width);
		if (gap < 0)
			continue;
		if (!best || gap < best_gap) {
			best = out;
			best_gap = gap;
		}
	}
	if (best)
		return best;

	/*
	 * None that way: round to the other end, as Windows does (right of the
	 * rightmost head is the leftmost, and the other way): the head that is
	 * furthest the other way among those sharing some height.  The same
	 * head, when there is no other, is no wrap.
	 */
	struct wm_output *far = NULL;
	int far_edge = 0;
	wl_list_for_each(out, &view->server->outputs, link) {
		struct wlr_box o;
		if (out == cur)
			continue;
		wm_output_layout_box(out, &o);
		if (wlr_box_empty(&o) || o.y >= box.y + box.height ||
		    o.y + o.height <= box.y)
			continue;
		int edge = side < 0 ? o.x + o.width : o.x;
		if (!far || (side < 0 ? edge > far_edge : edge < far_edge)) {
			far = out;
			far_edge = edge;
		}
	}
	return far;
}

/*
 * A window that sits exactly where a half or quarter of its head is, though
 * it was put there by hand (dragged away from a snap and back, say), is
 * taken for snapped: its next step is the one a snapped window takes.  Where
 * it was before is not known then, and what it was told earlier would be
 * stale after a drag, so putting it back goes to a window two thirds of the
 * head, in the middle of it.  Its place must match to a few pixels (it is
 * only so by being dropped there, or snapped); its size only to a character
 * cell, as a terminal cannot be exactly half a head.
 */
#define SNAP_ADOPT_SLACK SNAP_SLACK
static void
snap_adopt(struct wm_view *view)
{
	struct wlr_box area, frame;
	if (!wm_view_maximize_area(view, &area))
		return;
	wm_view_frame_box(view, &frame);

	static const int cells[6][4] = {
		{ 0, 0, 500, 1000 }, { 500, 0, 500, 1000 },
		{ 0, 0, 500, 500 }, { 500, 0, 500, 500 },
		{ 0, 500, 500, 500 }, { 500, 500, 500, 500 },
	};
	for (int i = 0; i < 6; i++) {
		int x = area.x + area.width * cells[i][0] / 1000;
		int y = area.y + area.height * cells[i][1] / 1000;
		int w = area.x + area.width * (cells[i][0] + cells[i][2]) / 1000 - x;
		int h = area.y + area.height * (cells[i][1] + cells[i][3]) / 1000 - y;
		/* A terminal's size comes in character cells */
		if (abs(frame.x - x) > SNAP_ADOPT_SLACK ||
		    abs(frame.y - y) > SNAP_ADOPT_SLACK ||
		    abs(frame.width - w) > 48 || abs(frame.height - h) > 48)
			continue;

		struct wlr_box *saved = &view->saved_geometry;
		saved->width = area.width * 2 / 3;
		saved->height = area.height * 2 / 3;
		saved->x = area.x + (area.width - saved->width) / 2;
		saved->y = area.y + (area.height - saved->height) / 2;
		view->tile = (struct wm_tile){ true, cells[i][0], cells[i][1],
			cells[i][2], cells[i][3] };
		return;
	}
}

/* Maximize a window that holds a corner, remembering the corner: down goes
 * back to it, and the size from before the snap stays what it goes back to */
static void
snap_maximize_from_cell(struct wm_view *view)
{
	struct wm_tile cell = view->tile;
	struct wlr_box saved = view->saved_geometry;
	wm_view_clear_maximized(view);
	wm_view_toggle_maximize(view);
	view->saved_geometry = saved;
	if (view->maximized)
		view->snap_return = cell;
}

bool
wm_view_snap_step(struct wm_server *server, struct wm_view *view,
	const char *direction)
{
	if (!direction)
		return false;
	int dx = 0, dy = 0;
	if (!strcmp(direction, "left"))
		dx = -1;
	else if (!strcmp(direction, "right"))
		dx = 1;
	else if (!strcmp(direction, "up"))
		dy = -1;
	else if (!strcmp(direction, "down"))
		dy = 1;
	else
		return false;

	/*
	 * Up, while the keys that went down for a minimize are still down, takes
	 * the minimize back.  Any other key then is about that window too, which
	 * is gone from sight: nothing is done, and certainly not to the window
	 * that has the focus now, until the modifier is let go
	 */
	if (dy < 0) {
		if (wm_snap_step_restore(server))
			return true;
	} else if (wm_snap_step_pending(server)) {
		return true;
	}
	if (!view || !wm_view_surface(view) || !view->scene_tree->node.enabled ||
	    view->fullscreen)
		return true;

	if (!view->maximized && !view->maximized_vert &&
	    !view->maximized_horiz && !view->lhalf && !view->rhalf &&
	    !view->tile.active)
		snap_adopt(view);

	int side, row;
	snap_position(view, &side, &row);
	bool plain = !view->maximized && !view->maximized_vert &&
		!view->maximized_horiz && !view->lhalf && !view->rhalf &&
		!view->tile.active;

	if (dx) {
		if (side == dx) {
			/* At the edge: on to the next head, on its other half */
			struct wm_output *next = adjacent_head(view, dx);
			if (next) {
				move_view_to_output(view, next);
				snap_to(view, -dx, row);
			}
		} else if (side == -dx && row == 0) {
			snap_undo(view);
		} else {
			snap_to(view, dx, side ? row : 0);
		}
	} else if (dy < 0) {
		if (plain)
			wm_view_toggle_maximize(view);
		else if (side && row == -1)
			snap_maximize_from_cell(view);
		else if (side && row >= 0)
			snap_to(view, side, row - 1);
	} else {
		if (view->maximized) {
			if (view->snap_return.active) {
				/* Up undone: back to the corner it was in */
				struct wm_tile cell = view->snap_return;
				struct wlr_box saved = view->saved_geometry;
				wm_view_clear_maximized(view);
				wm_view_tile(view, cell.x, cell.y, cell.w,
					cell.h);
				view->saved_geometry = saved;
			} else {
				wm_view_toggle_maximize(view);
			}
		} else if (side && row == 0) {
			snap_to(view, side, row + 1);
		} else if (side && row < 0) {
			snap_to(view, side, row + 1);
		} else if (plain || (side && row > 0)) {
			/* Not snapped, or in a bottom corner: down goes no
			 * further than out of sight, which is undone by up */
			wm_view_minimize(view);
			wm_snap_step_minimized(view);
		}
	}
	return true;
}

void
wm_view_snap_tile(struct wm_view *view, const struct wlr_box *area,
	const int cell[4], const struct wlr_box *prev_saved, int old_head_x,
	int old_head_y)
{
	struct wlr_box frame;
	wm_view_frame_box(view, &frame);
	int x = view->x;
	if (x + frame.width > area->x + area->width)
		x = area->x + area->width - frame.width;
	if (x < area->x)
		x = area->x;
	wm_view_move(view, x, area->y);
	wm_view_tile(view, cell[0], cell[1], cell[2], cell[3]);

	if (prev_saved && view->tile.active) {
		struct wm_output *output = wm_output_for_view(view);
		struct wlr_box head = { 0 };
		if (output)
			wm_output_layout_box(output, &head);
		view->saved_geometry = *prev_saved;
		view->saved_geometry.x += head.x - old_head_x;
		view->saved_geometry.y += head.y - old_head_y;
	}
}

void
wm_view_refit(struct wm_view *view)
{
	struct wm_output *output = wm_output_for_view(view);
	if (!output)
		return;

	struct wlr_box box, frame;
	if (view->fullscreen) {
		wm_output_layout_box(output, &box);
		wm_view_set_frame_box(view, &box);
		return;
	}
	if (view->maximized) {
		if (wm_view_maximize_area(view, &box))
			set_maximized_box(view, &box);
		return;
	}
	if (!wm_view_maximize_area(view, &box))
		return;
	wm_view_frame_box(view, &frame);
	if (view->maximized_vert) {
		frame.y = box.y;
		frame.height = box.height;
	}
	if (view->maximized_horiz) {
		frame.x = box.x;
		frame.width = box.width;
	}
	if (view->lhalf || view->rhalf) {
		int half_w = box.width / 2;
		frame = box;
		frame.width = view->lhalf ? half_w : box.width - half_w;
		if (view->rhalf)
			frame.x += half_w;
	}
	if (view->tile.active)
		tile_frame(&view->tile, &box, &frame);
	if (view->maximized_vert || view->maximized_horiz ||
	    view->lhalf || view->rhalf || view->tile.active)
		wm_view_set_frame_box(view, &frame);
}

void
wm_view_refit_all(struct wm_server *server)
{
	struct wm_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (view->mapped && !view->docked)
			wm_view_refit(view);
	}
}

void
wm_view_keep_on_screen(struct wm_view *view)
{
	struct wm_server *server = view->server;
	struct wlr_box frame;
	wm_view_frame_box(view, &frame);

	/* Fine while some of the titlebar is on an output */
	int top, bottom, left, right;
	wm_view_frame_extents(view, &top, &bottom, &left, &right);
	int grab_h = top > 0 ? top : 10;
	for (int x = frame.x; x < frame.x + frame.width; x += 8) {
		for (int y = frame.y; y < frame.y + grab_h; y += 4) {
			if (wlr_output_layout_output_at(server->output_layout,
					x, y))
				return;
		}
	}

	struct wm_output *output = wm_output_for_view(view);
	if (!output)
		return;
	struct wlr_box area;
	if (!get_view_output_area(view, &area))
		return;
	int x = frame.x, y = frame.y;
	if (x + frame.width > area.x + area.width)
		x = area.x + area.width - frame.width;
	if (x < area.x)
		x = area.x;
	if (y + frame.height > area.y + area.height)
		y = area.y + area.height - frame.height;
	if (y < area.y)
		y = area.y;
	wm_view_move(view, x, y);
}

void
wm_view_set_head(struct wm_view *view, int head)
{
	if (!view)
		return;
	/* Like Fluxbox: heads count from 1; others are ignored */
	struct wm_output *out = wm_output_head(view->server, head);
	if (out)
		move_view_to_output(view, out);
}

void
wm_view_send_to_head_offset(struct wm_view *view, int offset)
{
	if (!view)
		return;
	int count = wm_output_head_count(view->server);
	if (count < 2)
		return;
	struct wm_output *cur = wm_output_for_view(view);
	int head = cur ? wm_output_head_number(cur) : 0;
	if (head < 1)
		head = 1;
	/* Wraps around in both directions */
	int target = ((head - 1 + offset) % count + count) % count + 1;
	if (target != head)
		wm_view_set_head(view, target);
}
