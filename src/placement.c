/*
 * fluxland - A Fluxbox-inspired Wayland compositor
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
#include <wlr/util/log.h>

#include "config.h"
#include "output.h"
#include "placement.h"
#include "server.h"
#include "view.h"
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
	wlr_xdg_surface_get_geometry(view->xdg_toplevel->base, &geo);
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
 * ROW_SMART placement: scan left-to-right, top-to-bottom within
 * the usable area to find a gap where the window fits.
 */
static void
place_row_smart(struct wm_server *server, struct wm_view *view,
	struct wlr_box *area)
{
	int w, h;
	view_dims(view, &w, &h);

	int step = 8;
	for (int y = area->y; y + h <= area->y + area->height; y += step) {
		for (int x = area->x; x + w <= area->x + area->width; x += step) {
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
 */
static void
place_col_smart(struct wm_server *server, struct wm_view *view,
	struct wlr_box *area)
{
	int w, h;
	view_dims(view, &w, &h);

	int step = 8;
	for (int x = area->x; x + w <= area->x + area->width; x += step) {
		for (int y = area->y; y + h <= area->y + area->height; y += step) {
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

void
wm_placement_apply(struct wm_server *server, struct wm_view *view)
{
	/* Skip if view already has a position set by rules */
	if (view->x != 0 || view->y != 0)
		return;

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
	}

	wlr_log(WLR_DEBUG, "placement: placed view at (%d, %d) policy=%d",
		view->x, view->y, policy);
}

/*
 * Helper: try to snap a coordinate to a target edge.
 * Returns true and updates *coord if within threshold.
 */
static bool
try_snap(int *coord, int target, int threshold)
{
	int dist = abs(*coord - target);
	if (dist < threshold) {
		*coord = target;
		return true;
	}
	return false;
}

void
wm_snap_edges(struct wm_server *server, struct wm_view *view,
	int *new_x, int *new_y)
{
	if (!server->config || server->config->edge_snap_threshold <= 0)
		return;

	int threshold = server->config->edge_snap_threshold;

	int w, h;
	view_dims(view, &w, &h);

	struct wlr_box area;
	if (!get_cursor_output_area(server, &area))
		return;

	/* Snap to output usable area edges */
	bool snapped_x = false, snapped_y = false;

	/* Left edge */
	if (!snapped_x)
		snapped_x = try_snap(new_x, area.x, threshold);
	/* Right edge */
	if (!snapped_x) {
		int right_target = area.x + area.width - w;
		snapped_x = try_snap(new_x, right_target, threshold);
	}
	/* Top edge */
	if (!snapped_y)
		snapped_y = try_snap(new_y, area.y, threshold);
	/* Bottom edge */
	if (!snapped_y) {
		int bottom_target = area.y + area.height - h;
		snapped_y = try_snap(new_y, bottom_target, threshold);
	}

	/* Snap to other visible window edges on the same workspace */
	struct wm_view *v;
	wl_list_for_each(v, &server->views, link) {
		if (v == view)
			continue;
		if (v->workspace != view->workspace && !v->sticky)
			continue;

		struct wlr_box vbox;
		wm_view_get_geometry(v, &vbox);

		if (!snapped_x) {
			/* Snap left edge of moving view to right edge of other */
			snapped_x = try_snap(new_x, vbox.x + vbox.width,
				threshold);
		}
		if (!snapped_x) {
			/* Snap right edge of moving view to left edge of other */
			int right_snap = vbox.x - w;
			snapped_x = try_snap(new_x, right_snap, threshold);
		}
		if (!snapped_x) {
			/* Snap left edge to left edge of other */
			snapped_x = try_snap(new_x, vbox.x, threshold);
		}
		if (!snapped_x) {
			/* Snap right edge to right edge of other */
			int right_snap = vbox.x + vbox.width - w;
			snapped_x = try_snap(new_x, right_snap, threshold);
		}

		if (!snapped_y) {
			/* Snap top edge of moving view to bottom edge of other */
			snapped_y = try_snap(new_y, vbox.y + vbox.height,
				threshold);
		}
		if (!snapped_y) {
			/* Snap bottom edge of moving view to top edge of other */
			int bottom_snap = vbox.y - h;
			snapped_y = try_snap(new_y, bottom_snap, threshold);
		}
		if (!snapped_y) {
			/* Snap top edge to top edge of other */
			snapped_y = try_snap(new_y, vbox.y, threshold);
		}
		if (!snapped_y) {
			/* Snap bottom edge to bottom edge of other */
			int bottom_snap = vbox.y + vbox.height - h;
			snapped_y = try_snap(new_y, bottom_snap, threshold);
		}

		if (snapped_x && snapped_y)
			break;
	}
}

/* --- Window arrangement actions --- */

/*
 * Collect visible (non-minimized, non-fullscreen) views on the
 * current workspace into a flat array. Returns count.
 */
static int
collect_visible_views(struct wm_server *server, struct wm_view **out,
	int max)
{
	int n = 0;
	struct wm_view *v;
	wl_list_for_each(v, &server->views, link) {
		if (n >= max)
			break;
		if (v->workspace != server->current_workspace && !v->sticky)
			continue;
		if (v->fullscreen)
			continue;
		/* Check if scene node is enabled (not minimized) */
		if (!v->scene_tree->node.enabled)
			continue;
		out[n++] = v;
	}
	return n;
}

/*
 * Move and resize a view, updating both XDG toplevel size and
 * scene node position.
 */
static void
arrange_view(struct wm_view *view, int x, int y, int w, int h)
{
	view->x = x;
	view->y = y;
	wlr_xdg_toplevel_set_size(view->xdg_toplevel, w, h);
	wlr_scene_node_set_position(&view->scene_tree->node, x, y);
}

void
wm_arrange_windows_grid(struct wm_server *server)
{
	struct wlr_box area;
	if (!get_cursor_output_area(server, &area))
		return;

	struct wm_view *views[256];
	int n = collect_visible_views(server, views, 256);
	if (n == 0)
		return;

	int cols = (int)ceil(sqrt((double)n));
	int rows = (int)ceil((double)n / cols);

	int cell_w = area.width / cols;
	int cell_h = area.height / rows;

	for (int i = 0; i < n; i++) {
		int col = i % cols;
		int row = i / cols;
		int x = area.x + col * cell_w;
		int y = area.y + row * cell_h;
		arrange_view(views[i], x, y, cell_w, cell_h);
	}

	wlr_log(WLR_DEBUG, "arranged %d windows in %dx%d grid",
		n, cols, rows);
}

void
wm_arrange_windows_vert(struct wm_server *server)
{
	struct wlr_box area;
	if (!get_cursor_output_area(server, &area))
		return;

	struct wm_view *views[256];
	int n = collect_visible_views(server, views, 256);
	if (n == 0)
		return;

	int col_w = area.width / n;

	for (int i = 0; i < n; i++) {
		int x = area.x + i * col_w;
		arrange_view(views[i], x, area.y, col_w, area.height);
	}

	wlr_log(WLR_DEBUG, "arranged %d windows in vertical columns", n);
}

void
wm_arrange_windows_horiz(struct wm_server *server)
{
	struct wlr_box area;
	if (!get_cursor_output_area(server, &area))
		return;

	struct wm_view *views[256];
	int n = collect_visible_views(server, views, 256);
	if (n == 0)
		return;

	int row_h = area.height / n;

	for (int i = 0; i < n; i++) {
		int y = area.y + i * row_h;
		arrange_view(views[i], area.x, y, area.width, row_h);
	}

	wlr_log(WLR_DEBUG, "arranged %d windows in horizontal rows", n);
}

void
wm_arrange_windows_cascade(struct wm_server *server)
{
	struct wlr_box area;
	if (!get_cursor_output_area(server, &area))
		return;

	struct wm_view *views[256];
	int n = collect_visible_views(server, views, 256);
	if (n == 0)
		return;

	int cx = area.x;
	int cy = area.y;

	/* Default cascade window size: 60% of usable area */
	int win_w = area.width * 60 / 100;
	int win_h = area.height * 60 / 100;

	for (int i = 0; i < n; i++) {
		/* Wrap when reaching screen edge */
		if (cx + win_w > area.x + area.width ||
		    cy + win_h > area.y + area.height) {
			cx = area.x;
			cy = area.y;
		}

		arrange_view(views[i], cx, cy, win_w, win_h);

		cx += CASCADE_STEP_X;
		cy += CASCADE_STEP_Y;
	}

	wlr_log(WLR_DEBUG, "cascaded %d windows", n);
}
