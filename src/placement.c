/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * placement.c - Window placement and edge snapping
 *
 * Implements Fluxbox-style window placement policies and edge
 * snapping during interactive move.
 */

#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/box.h>
#include <wlr/util/edges.h>
#include <wlr/util/log.h>

#include "client_pattern.h"
#include "config.h"
#include "output.h"
#include "placement.h"
#include "server.h"
#include "view.h"
#include "view_geometry.h"
#include "workspace.h"

/* Cascade offset in pixels */
#define CASCADE_STEP_X 30
#define CASCADE_STEP_Y 30

/* Static cascade position tracking */
static int cascade_x = 0;
static int cascade_y = 0;

/*
 * Get the usable area of the output containing the cursor.
 * Returns false if no output is found.
 */
static bool
get_cursor_output_area(struct wm_server *server, struct wlr_box *area)
{
	struct wlr_output *wlr_output = wlr_output_layout_output_at(
		server->output_layout, server->cursor->x, server->cursor->y);
	if (!wlr_output)
		return false;

	/* Find our wm_output wrapper */
	struct wm_output *output;
	wl_list_for_each(output, &server->outputs, link) {
		if (output->wlr_output == wlr_output) {
			/* If usable_area has zero size, fall back to output box */
			if (output->usable_area.width > 0 &&
			    output->usable_area.height > 0) {
				*area = output->usable_area;
			} else {
				wlr_output_layout_get_box(
					server->output_layout,
					wlr_output, area);
			}
			return true;
		}
	}

	/* Fallback: use output layout box */
	wlr_output_layout_get_box(server->output_layout, wlr_output, area);
	return true;
}

/*
 * Get the dimensions of a view (width and height of XDG geometry).
 */
static void
view_dims(struct wm_view *view, int *w, int *h)
{
	struct wlr_box geo;
	wm_view_content_geometry(view, &geo);
	*w = geo.width > 0 ? geo.width : 200;
	*h = geo.height > 0 ? geo.height : 200;
}

/*
 * Check whether placing a window at (x, y) with size (w, h) would
 * overlap any existing visible view on the same workspace.
 */
static bool
overlaps_any(struct wm_server *server, struct wm_view *skip,
	int x, int y, int w, int h)
{
	struct wm_view *v;
	wl_list_for_each(v, &server->views, link) {
		if (v == skip)
			continue;
		if (v->workspace != skip->workspace && !v->sticky)
			continue;

		struct wlr_box vbox;
		wm_view_get_geometry(v, &vbox);

		/* AABB overlap test */
		if (x < vbox.x + vbox.width && x + w > vbox.x &&
		    y < vbox.y + vbox.height && y + h > vbox.y) {
			return true;
		}
	}
	return false;
}

/*
 * Calculate total overlap area between a candidate position and all
 * existing visible views on the same workspace.
 */
static int
overlap_area(struct wm_server *server, struct wm_view *skip,
	int x, int y, int w, int h)
{
	int total = 0;
	struct wm_view *v;
	wl_list_for_each(v, &server->views, link) {
		if (v == skip)
			continue;
		if (v->workspace != skip->workspace && !v->sticky)
			continue;

		struct wlr_box vbox;
		wm_view_get_geometry(v, &vbox);

		/* Calculate overlap rectangle */
		int ox1 = x > vbox.x ? x : vbox.x;
		int oy1 = y > vbox.y ? y : vbox.y;
		int ox2 = (x + w) < (vbox.x + vbox.width)
			? (x + w) : (vbox.x + vbox.width);
		int oy2 = (y + h) < (vbox.y + vbox.height)
			? (y + h) : (vbox.y + vbox.height);

		if (ox2 > ox1 && oy2 > oy1)
			total += (ox2 - ox1) * (oy2 - oy1);
	}
	return total;
}

/*
 * ROW_SMART placement: scan left-to-right, top-to-bottom within
 * the usable area to find a gap where the window fits.
 * Respects row_right_to_left and col_bottom_to_top direction config.
 */
static void
place_row_smart(struct wm_server *server, struct wm_view *view,
	struct wlr_box *area)
{
	int w, h;
	view_dims(view, &w, &h);

	bool rtl = server->config && server->config->row_right_to_left;
	bool btt = server->config && server->config->col_bottom_to_top;

	int step = 8;
	int y_start = btt ? area->y + area->height - h : area->y;
	int y_end = btt ? area->y - 1 : area->y + area->height - h + 1;
	int y_step = btt ? -step : step;

	int x_start = rtl ? area->x + area->width - w : area->x;
	int x_end = rtl ? area->x - 1 : area->x + area->width - w + 1;
	int x_step = rtl ? -step : step;

	for (int y = y_start; btt ? (y >= y_end) : (y < y_end); y += y_step) {
		for (int x = x_start; rtl ? (x >= x_end) : (x < x_end); x += x_step) {
			if (!overlaps_any(server, view, x, y, w, h)) {
				view->x = x;
				view->y = y;
				return;
			}
		}
	}

	/* No gap found — fall back to cascade */
	view->x = area->x + CASCADE_STEP_X;
	view->y = area->y + CASCADE_STEP_Y;
}

/*
 * COL_SMART placement: scan top-to-bottom, left-to-right within
 * the usable area to find a gap where the window fits.
 * Respects row_right_to_left and col_bottom_to_top direction config.
 */
static void
place_col_smart(struct wm_server *server, struct wm_view *view,
	struct wlr_box *area)
{
	int w, h;
	view_dims(view, &w, &h);

	bool rtl = server->config && server->config->row_right_to_left;
	bool btt = server->config && server->config->col_bottom_to_top;

	int step = 8;
	int x_start = rtl ? area->x + area->width - w : area->x;
	int x_end = rtl ? area->x - 1 : area->x + area->width - w + 1;
	int x_step = rtl ? -step : step;

	int y_start = btt ? area->y + area->height - h : area->y;
	int y_end = btt ? area->y - 1 : area->y + area->height - h + 1;
	int y_step = btt ? -step : step;

	for (int x = x_start; rtl ? (x >= x_end) : (x < x_end); x += x_step) {
		for (int y = y_start; btt ? (y >= y_end) : (y < y_end); y += y_step) {
			if (!overlaps_any(server, view, x, y, w, h)) {
				view->x = x;
				view->y = y;
				return;
			}
		}
	}

	/* No gap found — fall back to cascade */
	view->x = area->x + CASCADE_STEP_X;
	view->y = area->y + CASCADE_STEP_Y;
}

/*
 * ROW_MIN_OVERLAP placement: scan row-major and pick the position
 * with minimum total overlap area with existing windows.
 */
static void
place_row_min_overlap(struct wm_server *server, struct wm_view *view,
	struct wlr_box *area)
{
	int w, h;
	view_dims(view, &w, &h);

	bool rtl = server->config && server->config->row_right_to_left;
	bool btt = server->config && server->config->col_bottom_to_top;

	int step = 16; /* coarser step for scoring (performance) */
	int best_x = area->x, best_y = area->y;
	int best_score = -1;

	int y_start = btt ? area->y + area->height - h : area->y;
	int y_end = btt ? area->y - 1 : area->y + area->height - h + 1;
	int y_step = btt ? -step : step;

	int x_start = rtl ? area->x + area->width - w : area->x;
	int x_end = rtl ? area->x - 1 : area->x + area->width - w + 1;
	int x_step = rtl ? -step : step;

	for (int y = y_start; btt ? (y >= y_end) : (y < y_end); y += y_step) {
		for (int x = x_start; rtl ? (x >= x_end) : (x < x_end); x += x_step) {
			int score = overlap_area(server, view, x, y, w, h);
			if (score == 0) {
				/* Perfect — no overlap */
				view->x = x;
				view->y = y;
				return;
			}
			if (best_score < 0 || score < best_score) {
				best_score = score;
				best_x = x;
				best_y = y;
			}
		}
	}

	view->x = best_x;
	view->y = best_y;
}

/*
 * COL_MIN_OVERLAP placement: scan column-major and pick the position
 * with minimum total overlap area with existing windows.
 */
static void
place_col_min_overlap(struct wm_server *server, struct wm_view *view,
	struct wlr_box *area)
{
	int w, h;
	view_dims(view, &w, &h);

	bool rtl = server->config && server->config->row_right_to_left;
	bool btt = server->config && server->config->col_bottom_to_top;

	int step = 16;
	int best_x = area->x, best_y = area->y;
	int best_score = -1;

	int x_start = rtl ? area->x + area->width - w : area->x;
	int x_end = rtl ? area->x - 1 : area->x + area->width - w + 1;
	int x_step = rtl ? -step : step;

	int y_start = btt ? area->y + area->height - h : area->y;
	int y_end = btt ? area->y - 1 : area->y + area->height - h + 1;
	int y_step = btt ? -step : step;

	for (int x = x_start; rtl ? (x >= x_end) : (x < x_end); x += x_step) {
		for (int y = y_start; btt ? (y >= y_end) : (y < y_end); y += y_step) {
			int score = overlap_area(server, view, x, y, w, h);
			if (score == 0) {
				view->x = x;
				view->y = y;
				return;
			}
			if (best_score < 0 || score < best_score) {
				best_score = score;
				best_x = x;
				best_y = y;
			}
		}
	}

	view->x = best_x;
	view->y = best_y;
}

/*
 * CASCADE placement: place at an offset from the previous window,
 * wrapping when hitting screen edges.
 */
static void
place_cascade(struct wm_server *server, struct wm_view *view,
	struct wlr_box *area)
{
	int w, h;
	view_dims(view, &w, &h);

	/* Initialize cascade position if first time or wrap needed */
	if (cascade_x < area->x || cascade_y < area->y ||
	    cascade_x + w > area->x + area->width ||
	    cascade_y + h > area->y + area->height) {
		cascade_x = area->x + CASCADE_STEP_X;
		cascade_y = area->y + CASCADE_STEP_Y;
	}

	view->x = cascade_x;
	view->y = cascade_y;

	cascade_x += CASCADE_STEP_X;
	cascade_y += CASCADE_STEP_Y;
}

/*
 * UNDER_MOUSE placement: center the window on the cursor position,
 * clamped to the usable area.
 */
static void
place_under_mouse(struct wm_server *server, struct wm_view *view,
	struct wlr_box *area)
{
	int w, h;
	view_dims(view, &w, &h);

	int x = (int)server->cursor->x - w / 2;
	int y = (int)server->cursor->y - h / 2;

	/* Clamp to usable area */
	if (x < area->x)
		x = area->x;
	if (y < area->y)
		y = area->y;
	if (x + w > area->x + area->width)
		x = area->x + area->width - w;
	if (y + h > area->y + area->height)
		y = area->y + area->height - h;

	view->x = x;
	view->y = y;
}

/* Center a dialog over its parent window, kept on the parent's output */
static void
place_over_parent(struct wm_view *view, struct wm_view *parent)
{
	struct wlr_box pf, vf, area;
	wm_view_frame_box(parent, &pf);
	wm_view_frame_box(view, &vf);
	int x = pf.x + (pf.width - vf.width) / 2;
	int y = pf.y + (pf.height - vf.height) / 2;
	if (get_view_output_area(parent, &area)) {
		if (x + vf.width > area.x + area.width)
			x = area.x + area.width - vf.width;
		if (y + vf.height > area.y + area.height)
			y = area.y + area.height - vf.height;
		if (x < area.x)
			x = area.x;
		if (y < area.y)
			y = area.y;
	}
	view->x = x;
	view->y = y;
}

void
wm_placement_apply(struct wm_server *server, struct wm_view *view)
{
	/* Skip if view already has a position set by rules */
	if (view->rule_placed || view->x != 0 || view->y != 0)
		return;

	/* Dialogs open centered over the window they belong to */
	struct wm_view *parent = wm_view_parent(view);
	if (parent && parent != view && parent->mapped) {
		place_over_parent(view, parent);
		return;
	}

	struct wlr_box area;
	if (!get_cursor_output_area(server, &area)) {
		wlr_log(WLR_DEBUG, "%s", "placement: no output found, using default");
		return;
	}

	enum wm_placement_policy policy = WM_PLACEMENT_ROW_SMART;
	if (server->config)
		policy = server->config->placement_policy;

	switch (policy) {
	case WM_PLACEMENT_ROW_SMART:
		place_row_smart(server, view, &area);
		break;
	case WM_PLACEMENT_COL_SMART:
		place_col_smart(server, view, &area);
		break;
	case WM_PLACEMENT_CASCADE:
		place_cascade(server, view, &area);
		break;
	case WM_PLACEMENT_UNDER_MOUSE:
		place_under_mouse(server, view, &area);
		break;
	case WM_PLACEMENT_ROW_MIN_OVERLAP:
		place_row_min_overlap(server, view, &area);
		break;
	case WM_PLACEMENT_COL_MIN_OVERLAP:
		place_col_min_overlap(server, view, &area);
		break;
	case WM_PLACEMENT_AUTOTAB:
		/* Not tabbed (nothing focused): placed as by default */
		place_row_smart(server, view, &area);
		break;
	}

	wlr_log(WLR_DEBUG, "placement: placed view at (%d, %d) policy=%d",
		view->x, view->y, policy);
}

/*
 * FluxboxWindow.cc's snapToWindow: the nearest of the box's edges the
 * window is next to, closer than the limits so far.  A resize doesn't
 * snap its far edge to the far edge.
 */
static void
snap_to_box(int *xlimit, int *ylimit, int left, int right, int top,
	int bottom, const struct wlr_box *o, bool resize)
{
	int oleft = o->x, oright = o->x + o->width;
	int otop = o->y, obottom = o->y + o->height;
	if (top <= obottom && bottom >= otop) {
		if (abs(left - oleft) < abs(*xlimit))
			*xlimit = -(left - oleft);
		if (abs(right - oleft) < abs(*xlimit))
			*xlimit = -(right - oleft);
		if (abs(left - oright) < abs(*xlimit))
			*xlimit = -(left - oright);
		if (!resize && abs(right - oright) < abs(*xlimit))
			*xlimit = -(right - oright);
	}
	if (left <= oright && right >= oleft) {
		if (abs(top - otop) < abs(*ylimit))
			*ylimit = -(top - otop);
		if (abs(bottom - otop) < abs(*ylimit))
			*ylimit = -(bottom - otop);
		if (abs(top - obottom) < abs(*ylimit))
			*ylimit = -(top - obottom);
		if (!resize && abs(bottom - obottom) < abs(*ylimit))
			*ylimit = -(bottom - obottom);
	}
}

/*
 * FluxboxWindow::doSnapping: (x, y), the top left of a w x h frame (the
 * dragged corner for a resize), to the nearest edge within the threshold
 * of the heads (their usable areas, then whole) and the other windows on
 * the workspace.
 */
static void
do_snapping(struct wm_server *server, struct wm_view *view, int *x, int *y,
	int w, int h, bool resize)
{
	if (!server->config)
		return;
	int threshold = resize ? server->config->edge_resize_snap_threshold :
		server->config->edge_snap_threshold;
	if (threshold <= 0)
		return;
	int dx = threshold + 1, dy = threshold + 1;
	int left = *x, top = *y, right = *x + w, bottom = *y + h;

	struct wm_output *out;
	wl_list_for_each(out, &server->outputs, link)
		snap_to_box(&dx, &dy, left, right, top, bottom,
			&out->usable_area, resize);
	wl_list_for_each(out, &server->outputs, link) {
		struct wlr_box full;
		wm_output_layout_box(out, &full);
		snap_to_box(&dx, &dy, left, right, top, bottom, &full, resize);
	}

	struct wm_workspace *ws = wm_workspace_get_active(server);
	struct wm_view *v;
	wl_list_for_each(v, &server->views, link) {
		if (v == view || !v->mapped || !v->scene_tree->node.enabled)
			continue;
		if (v->workspace != ws && !v->sticky)
			continue;
		struct wlr_box fb;
		wm_view_frame_box(v, &fb);
		snap_to_box(&dx, &dy, left, right, top, bottom, &fb, resize);
	}

	if (dx <= threshold)
		*x += dx;
	if (dy <= threshold)
		*y += dy;
}

void
wm_snap_edges(struct wm_server *server, struct wm_view *view,
	int *new_x, int *new_y)
{
	struct wlr_box fb;
	wm_view_frame_box(view, &fb);
	do_snapping(server, view, new_x, new_y, fb.width, fb.height, false);
}

void
wm_snap_resize(struct wm_server *server, struct wm_view *view,
	uint32_t edges, int *x, int *y, int *w, int *h)
{
	/* Only a corner snaps (FluxboxWindow::motionNotifyEvent) */
	if (!(edges & (WLR_EDGE_LEFT | WLR_EDGE_RIGHT)) ||
	    !(edges & (WLR_EDGE_TOP | WLR_EDGE_BOTTOM)))
		return;
	int t, b, l, r;
	wm_view_frame_extents(view, &t, &b, &l, &r);
	struct wlr_box cur;
	wm_view_frame_box(view, &cur);
	int fl = *x, ft = *y;
	int fr = *x + *w + l + r, fb = *y + *h + t + b;
	int tx = edges & WLR_EDGE_LEFT ? fl : fr;
	int ty = edges & WLR_EDGE_TOP ? ft : fb;
	do_snapping(server, view, &tx, &ty, cur.width, cur.height, true);
	if (edges & WLR_EDGE_LEFT)
		fl = tx;
	else
		fr = tx;
	if (edges & WLR_EDGE_TOP)
		ft = ty;
	else
		fb = ty;
	*x = fl;
	*y = ft;
	*w = fr - fl - l - r;
	*h = fb - ft - t - b;
}

/* --- Window arrangement actions --- */

/*
 * Move and resize a view, updating both XDG toplevel size and
 * scene node position.
 */
static void
arrange_view(struct wm_view *view, int x, int y, int w, int h)
{
	/* Tell client it's being tiled so it fills exact size without rounding */
	wm_view_set_tiled_state(view, WLR_EDGE_TOP | WLR_EDGE_BOTTOM |
		WLR_EDGE_LEFT | WLR_EDGE_RIGHT);
	/* The cell holds the whole frame, decorations included */
	struct wlr_box cell = {x, y, w, h < 1 ? 1 : h};
	if (cell.width < 1)
		cell.width = 1;
	wm_view_set_frame_box(view, &cell);
}

/* The output a view is on, by the middle of its frame */
static struct wlr_output *
view_head(struct wm_server *server, struct wm_view *view)
{
	struct wlr_box fb;
	wm_view_frame_box(view, &fb);
	return wlr_output_layout_output_at(server->output_layout,
		fb.x + fb.width / 2.0, fb.y + fb.height / 2.0);
}

/*
 * The windows ArrangeWindows works on (Workspace::windowList, in the
 * order they came): of this workspace, shown, matching the pattern, on
 * the pointer's head (or on none)
 */
static int
collect_arrange_views(struct wm_server *server, const char *pattern,
	struct wm_view **out, int max)
{
	struct wm_window_list_spec spec;
	if (!wm_window_list_parse(pattern, &spec))
		return 0;
	struct wlr_output *head = wlr_output_layout_output_at(
		server->output_layout, server->cursor->x, server->cursor->y);
	struct wm_workspace *ws = wm_workspace_get_active(server);
	int n = 0;
	struct wm_view *v;
	wl_list_for_each(v, &server->views, link) {
		if (n >= max)
			break;
		if (!v->mapped || v->docked || v->fullscreen ||
		    !v->scene_tree->node.enabled)
			continue;
		if (v->workspace != ws && !v->sticky)
			continue;
		struct wlr_output *at = view_head(server, v);
		if (at && head && at != head)
			continue;
		if (spec.pattern && !wm_client_pattern_match(server,
		    spec.pattern, v))
			continue;
		/* By when they came */
		int i = n++;
		while (i > 0 && out[i - 1]->id > v->id) {
			out[i] = out[i - 1];
			i--;
		}
		out[i] = v;
	}
	wm_window_list_finish(&spec);
	return n;
}

/* ArrangeWindowsCmd: a grid of near-square cells, each taking the window
 * nearest it; the stacked ones keep the focused window big beside it */
void
wm_arrange_windows(struct wm_server *server, enum wm_arrange_method method,
	const char *pattern)
{
	struct wlr_box area;
	if (!get_cursor_output_area(server, &area))
		return;
	struct wm_view *all[256], *normal[256], *shaded[256];
	int count = collect_arrange_views(server, pattern, all, 256);
	if (count == 0)
		return;

	bool stacked = method >= WM_ARRANGE_STACK_LEFT;
	struct wm_view *main_win = NULL;
	int nn = 0, ns = 0;
	for (int i = 0; i < count; i++) {
		struct wm_view *v = all[i];
		if (stacked && v == server->focused_view)
			main_win = v;
		else if (v->decoration && v->decoration->shaded)
			shaded[ns++] = v;
		else
			normal[nn++] = v;
	}
	/* No focused one: the last */
	if (stacked && !main_win && nn > 0)
		main_win = normal[--nn];
	int win_count = nn;
	if (win_count == 0) {
		if (!main_win)
			return;
		win_count = 1;
	}

	int x_offs = area.x, y_offs = area.y;
	int max_width = area.width, max_height = area.height;
	if (method == WM_ARRANGE_STACK_LEFT ||
	    method == WM_ARRANGE_STACK_RIGHT)
		max_width /= 2;
	if (method == WM_ARRANGE_STACK_TOP ||
	    method == WM_ARRANGE_STACK_BOTTOM)
		max_height /= 2;

	/* As many rows as columns, more of them the long way */
	int cols = (int)sqrt((double)win_count);
	if (cols < 1)
		cols = 1;
	int rows = (int)(0.99 + (double)win_count / cols);
	if (method == WM_ARRANGE_VERTICAL ||
	    (method == WM_ARRANGE_GRID && max_width < max_height)) {
		int t = cols;
		cols = rows;
		rows = t;
	}
	if (method == WM_ARRANGE_STACK_RIGHT)
		x_offs += max_width;
	else if (method == WM_ARRANGE_STACK_BOTTOM)
		y_offs += max_height;
	const int orig_x_offs = x_offs, orig_y_offs = y_offs;

	/* The shaded ones down the sides, as they are */
	for (int i = 0; i < ns; i++) {
		struct wlr_box fb;
		wm_view_frame_box(shaded[i], &fb);
		int x = (i & 1) ? x_offs : area.x + area.width - fb.width;
		wm_view_move(shaded[i], x, y_offs);
		y_offs += fb.height;
		max_height -= fb.height;
	}

	int cal_width = max_width / cols, cal_height = max_height / rows;
	for (int r = 0; r < rows; r++) {
		x_offs = orig_x_offs;
		for (int c = 0; c < cols && nn > 0; c++) {
			/* The window nearest the cell (Fluxbox's own sums) */
			int cell_cx = x_offs + (x_offs + cal_width) / 2;
			int cell_cy = y_offs + (y_offs + cal_height) / 2;
			int closest = 0;
			unsigned long long best = ~0ULL;
			for (int i = 0; i < nn; i++) {
				struct wlr_box fb;
				wm_view_frame_box(normal[i], &fb);
				long long dx = fb.x + (fb.x + fb.width / 2) -
					cell_cx;
				long long dy = fb.y + (fb.y + fb.height / 2) -
					cell_cy;
				unsigned long long d = (unsigned long long)
					(dx * dx + dy * dy);
				if (d < best) {
					best = d;
					closest = i;
				}
			}
			int w = cal_width, h = cal_height;
			/* The last gets the rest of the row */
			if (nn == 1) {
				w = area.x + area.width - x_offs;
				if (method == WM_ARRANGE_STACK_LEFT)
					w -= max_width;
			}
			arrange_view(normal[closest], x_offs, y_offs, w, h);
			/* The rest in their order */
			for (int i = closest; i + 1 < nn; i++)
				normal[i] = normal[i + 1];
			nn--;
			x_offs += cal_width;
		}
		y_offs += cal_height;
	}

	if (main_win) {
		switch (method) {
		case WM_ARRANGE_STACK_LEFT:
			arrange_view(main_win, area.x + max_width, orig_y_offs,
				max_width, max_height);
			break;
		case WM_ARRANGE_STACK_RIGHT:
			arrange_view(main_win, area.x, area.y, max_width,
				max_height);
			break;
		case WM_ARRANGE_STACK_TOP:
			arrange_view(main_win, area.x, area.y + max_height,
				max_width, max_height);
			break;
		case WM_ARRANGE_STACK_BOTTOM:
			arrange_view(main_win, area.x, area.y, max_width,
				max_height);
			break;
		default:
			break;
		}
	}
}

void
wm_arrange_windows_grid(struct wm_server *server)
{
	wm_arrange_windows(server, WM_ARRANGE_GRID, NULL);
}

void
wm_arrange_windows_vert(struct wm_server *server)
{
	wm_arrange_windows(server, WM_ARRANGE_VERTICAL, NULL);
}

void
wm_arrange_windows_horiz(struct wm_server *server)
{
	wm_arrange_windows(server, WM_ARRANGE_HORIZONTAL, NULL);
}

void
wm_arrange_windows_stack_left(struct wm_server *server)
{
	wm_arrange_windows(server, WM_ARRANGE_STACK_LEFT, NULL);
}

void
wm_arrange_windows_stack_right(struct wm_server *server)
{
	wm_arrange_windows(server, WM_ARRANGE_STACK_RIGHT, NULL);
}

void
wm_arrange_windows_stack_top(struct wm_server *server)
{
	wm_arrange_windows(server, WM_ARRANGE_STACK_TOP, NULL);
}

void
wm_arrange_windows_stack_bottom(struct wm_server *server)
{
	wm_arrange_windows(server, WM_ARRANGE_STACK_BOTTOM, NULL);
}

/* CascadeWindows (fluxland's): each a step down and right of the last,
 * 60% of the screen, from the top left again when they reach its edge */
void
wm_cascade_windows(struct wm_server *server, const char *pattern)
{
	struct wlr_box area;
	if (!get_cursor_output_area(server, &area))
		return;
	struct wm_view *views[256];
	int n = collect_arrange_views(server, pattern, views, 256);
	int cx = area.x, cy = area.y;
	int win_w = area.width * 60 / 100;
	int win_h = area.height * 60 / 100;
	for (int i = 0; i < n; i++) {
		if (cx + win_w > area.x + area.width ||
		    cy + win_h > area.y + area.height) {
			cx = area.x;
			cy = area.y;
		}
		arrange_view(views[i], cx, cy, win_w, win_h);
		cx += CASCADE_STEP_X;
		cy += CASCADE_STEP_Y;
	}
}

void
wm_arrange_windows_cascade(struct wm_server *server)
{
	wm_cascade_windows(server, NULL);
}

/* Unclutter [pattern]: the windows ArrangeWindows would take, out of the
 * way and then placed again where they overlap least (UnclutterCmd) */
void
wm_unclutter(struct wm_server *server, const char *pattern)
{
	struct wlr_box area;
	if (!get_cursor_output_area(server, &area))
		return;
	struct wm_view *views[256];
	int n = collect_arrange_views(server, pattern, views, 256);
	for (int i = 0; i < n; i++) {
		struct wlr_box fb;
		wm_view_frame_box(views[i], &fb);
		views[i]->x = -fb.width - area.width;
		views[i]->y = -fb.height - area.height;
	}
	bool cols = server->config && server->config->placement_policy ==
		WM_PLACEMENT_COL_MIN_OVERLAP;
	for (int i = 0; i < n; i++) {
		if (cols)
			place_col_min_overlap(server, views[i], &area);
		else
			place_row_min_overlap(server, views[i], &area);
		wm_view_move(views[i], views[i]->x, views[i]->y);
	}
}
