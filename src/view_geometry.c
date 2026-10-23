/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * view_geometry.c - View geometry, tiling, maximize/fullscreen, multi-monitor
 */

#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
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
	} else {
		/* Save current geometry and maximize */
		wm_view_get_geometry(view, &view->saved_geometry);

		struct wlr_box max_box;
		if (wm_view_maximize_area(view, &max_box))
			wm_view_set_frame_box(view, &max_box);
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
			wm_view_set_frame_box(view, &frame);
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
			wm_view_set_frame_box(view, &frame);
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
			wm_view_set_frame_box(view, &box);
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
	if (view->maximized_vert || view->maximized_horiz ||
	    view->lhalf || view->rhalf)
		wm_view_set_frame_box(view, &frame);
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
