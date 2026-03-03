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

/* --- Helper: get usable area for a view's output --- */

bool
get_view_output_area(struct wm_view *view, struct wlr_box *area)
{
	struct wlr_output *output = wlr_output_layout_output_at(
		view->server->output_layout,
		view->x + 1, view->y + 1);
	if (!output) {
		output = wlr_output_layout_output_at(
			view->server->output_layout,
			view->server->cursor->x,
			view->server->cursor->y);
	}
	if (!output)
		return false;

	struct wm_output *wm_out;
	wl_list_for_each(wm_out, &view->server->outputs, link) {
		if (wm_out->wlr_output == output) {
			if (wm_out->usable_area.width > 0 &&
			    wm_out->usable_area.height > 0) {
				*area = wm_out->usable_area;
			} else {
				wlr_output_layout_get_box(
					view->server->output_layout,
					output, area);
			}
			return true;
		}
	}

	wlr_output_layout_get_box(view->server->output_layout,
		output, area);
	return true;
}

/* --- Toggle maximize --- */

void
wm_view_toggle_maximize(struct wm_view *view)
{
	if (!view->xdg_toplevel || !view->scene_tree->node.enabled)
		return;

	if (view->maximized) {
		/* Restore saved geometry */
		wlr_xdg_toplevel_set_size(view->xdg_toplevel,
			view->saved_geometry.width,
			view->saved_geometry.height);
		wlr_scene_node_set_position(&view->scene_tree->node,
			view->saved_geometry.x, view->saved_geometry.y);
		view->x = view->saved_geometry.x;
		view->y = view->saved_geometry.y;
		view->maximized = false;
	} else {
		/* Save current geometry and maximize */
		wm_view_get_geometry(view, &view->saved_geometry);

		struct wlr_output *wlr_output =
			wlr_output_layout_output_at(
				view->server->output_layout,
				view->server->cursor->x,
				view->server->cursor->y);
		if (wlr_output) {
			struct wlr_box max_box;
			bool use_full = view->server->config &&
				view->server->config->full_maximization;

			if (use_full) {
				wlr_output_layout_get_box(
					view->server->output_layout,
					wlr_output, &max_box);
			} else {
				/* Use usable area (respects toolbar/slit) */
				bool found = false;
				struct wm_output *wm_out;
				wl_list_for_each(wm_out,
					&view->server->outputs, link) {
					if (wm_out->wlr_output == wlr_output &&
					    wm_out->usable_area.width > 0 &&
					    wm_out->usable_area.height > 0) {
						max_box = wm_out->usable_area;
						found = true;
						break;
					}
				}
				if (!found) {
					wlr_output_layout_get_box(
						view->server->output_layout,
						wlr_output, &max_box);
				}
			}

			wlr_xdg_toplevel_set_size(view->xdg_toplevel,
				max_box.width, max_box.height);
			wlr_scene_node_set_position(
				&view->scene_tree->node,
				max_box.x, max_box.y);
			view->x = max_box.x;
			view->y = max_box.y;
		}
		view->maximized = true;
	}

	wlr_xdg_toplevel_set_maximized(view->xdg_toplevel,
		view->maximized);
	wm_foreign_toplevel_set_maximized(view, view->maximized);
}

/* --- Toggle fullscreen --- */

void
wm_view_toggle_fullscreen(struct wm_view *view)
{
	if (!view->xdg_toplevel || !view->scene_tree->node.enabled)
		return;

	if (view->fullscreen) {
		/* Restore from fullscreen */
		wlr_xdg_toplevel_set_size(view->xdg_toplevel,
			view->saved_geometry.width,
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

		struct wlr_output *output =
			wlr_output_layout_output_at(
				view->server->output_layout,
				view->server->cursor->x,
				view->server->cursor->y);
		if (output) {
			struct wlr_box output_box;
			wlr_output_layout_get_box(
				view->server->output_layout,
				output, &output_box);
			wlr_xdg_toplevel_set_size(view->xdg_toplevel,
				output_box.width, output_box.height);
			wlr_scene_node_set_position(
				&view->scene_tree->node,
				output_box.x, output_box.y);
			view->x = output_box.x;
			view->y = output_box.y;
		}
		view->fullscreen = true;

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

	wlr_xdg_toplevel_set_fullscreen(view->xdg_toplevel,
		view->fullscreen);
	wm_foreign_toplevel_set_fullscreen(view, view->fullscreen);
}

/* --- Maximize vertical/horizontal --- */

void
wm_view_maximize_vert(struct wm_view *view)
{
	if (!view) {
		return;
	}

	if (!view->xdg_toplevel || !view->scene_tree->node.enabled)
		return;

	if (view->maximized_vert) {
		/* Restore saved y and height */
		view->y = view->saved_geometry.y;
		wlr_xdg_toplevel_set_size(view->xdg_toplevel,
			0, view->saved_geometry.height);
		wlr_scene_node_set_position(&view->scene_tree->node,
			view->x, view->y);
		view->maximized_vert = false;
	} else {
		/* Save current geometry */
		struct wlr_box geo;
		wm_view_get_geometry(view, &geo);
		view->saved_geometry.y = view->y;
		view->saved_geometry.height = geo.height;

		struct wlr_output *output =
			wlr_output_layout_output_at(
				view->server->output_layout,
				view->server->cursor->x,
				view->server->cursor->y);
		if (output) {
			struct wlr_box output_box;
			wlr_output_layout_get_box(
				view->server->output_layout,
				output, &output_box);
			view->y = output_box.y;
			wlr_xdg_toplevel_set_size(view->xdg_toplevel,
				0, output_box.height);
			wlr_scene_node_set_position(
				&view->scene_tree->node,
				view->x, view->y);
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

	if (!view->xdg_toplevel || !view->scene_tree->node.enabled)
		return;

	if (view->maximized_horiz) {
		/* Restore saved x and width */
		view->x = view->saved_geometry.x;
		wlr_xdg_toplevel_set_size(view->xdg_toplevel,
			view->saved_geometry.width, 0);
		wlr_scene_node_set_position(&view->scene_tree->node,
			view->x, view->y);
		view->maximized_horiz = false;
	} else {
		/* Save current geometry */
		struct wlr_box geo;
		wm_view_get_geometry(view, &geo);
		view->saved_geometry.x = view->x;
		view->saved_geometry.width = geo.width;

		struct wlr_output *output =
			wlr_output_layout_output_at(
				view->server->output_layout,
				view->server->cursor->x,
				view->server->cursor->y);
		if (output) {
			struct wlr_box output_box;
			wlr_output_layout_get_box(
				view->server->output_layout,
				output, &output_box);
			view->x = output_box.x;
			wlr_xdg_toplevel_set_size(view->xdg_toplevel,
				output_box.width, 0);
			wlr_scene_node_set_position(
				&view->scene_tree->node,
				view->x, view->y);
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

	if (!view->xdg_toplevel || !view->scene_tree->node.enabled)
		return;

	if (view->lhalf) {
		/* Toggle off — restore saved geometry */
		view->x = view->saved_geometry.x;
		view->y = view->saved_geometry.y;
		wlr_xdg_toplevel_set_size(view->xdg_toplevel,
			view->saved_geometry.width,
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

	int half_w = area.width / 2;
	view->x = area.x;
	view->y = area.y;
	wlr_xdg_toplevel_set_size(view->xdg_toplevel,
		half_w, area.height);
	wlr_scene_node_set_position(&view->scene_tree->node,
		view->x, view->y);

	view->lhalf = true;
	view->rhalf = false;
	view->maximized = false;
}

void
wm_view_rhalf(struct wm_view *view)
{
	if (!view)
		return;

	if (!view->xdg_toplevel || !view->scene_tree->node.enabled)
		return;

	if (view->rhalf) {
		/* Toggle off — restore saved geometry */
		view->x = view->saved_geometry.x;
		view->y = view->saved_geometry.y;
		wlr_xdg_toplevel_set_size(view->xdg_toplevel,
			view->saved_geometry.width,
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
	view->x = area.x + half_w;
	view->y = area.y;
	wlr_xdg_toplevel_set_size(view->xdg_toplevel,
		half_w, area.height);
	wlr_scene_node_set_position(&view->scene_tree->node,
		view->x, view->y);

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

	if (!view->xdg_toplevel || !view->scene_tree->node.enabled)
		return;

	struct wlr_box geo;
	wm_view_get_geometry(view, &geo);
	int new_w = geo.width + dw;
	int new_h = geo.height + dh;
	if (new_w > 10 && new_h > 10)
		wlr_xdg_toplevel_set_size(view->xdg_toplevel,
			new_w, new_h);
}

/* --- Multi-monitor helpers --- */

void
move_view_to_output(struct wm_view *view, struct wm_output *out)
{
	struct wlr_box area;
	if (out->usable_area.width > 0 &&
	    out->usable_area.height > 0) {
		area = out->usable_area;
	} else {
		wlr_output_layout_get_box(
			view->server->output_layout,
			out->wlr_output, &area);
	}

	struct wlr_box vgeo;
	wm_view_get_geometry(view, &vgeo);
	view->x = area.x + (area.width - vgeo.width) / 2;
	view->y = area.y + (area.height - vgeo.height) / 2;
	wlr_scene_node_set_position(&view->scene_tree->node,
		view->x, view->y);
}

void
wm_view_set_head(struct wm_view *view, int head_index)
{
	if (!view)
		return;

	int idx = 0;
	struct wm_output *out;
	wl_list_for_each(out, &view->server->outputs, link) {
		if (idx == head_index) {
			move_view_to_output(view, out);
			return;
		}
		idx++;
	}
}

void
wm_view_send_to_next_head(struct wm_view *view)
{
	if (!view)
		return;

	struct wlr_output *current = wlr_output_layout_output_at(
		view->server->output_layout,
		view->x + 1, view->y + 1);
	if (!current)
		current = wlr_output_layout_output_at(
			view->server->output_layout,
			view->server->cursor->x,
			view->server->cursor->y);

	struct wm_output *out;
	struct wm_output *first_out = NULL;
	bool found_current = false;

	wl_list_for_each(out, &view->server->outputs, link) {
		if (!first_out)
			first_out = out;
		if (out->wlr_output == current) {
			found_current = true;
			continue;
		}
		if (found_current) {
			move_view_to_output(view, out);
			return;
		}
	}

	/* Wrap around to first output */
	if (first_out)
		move_view_to_output(view, first_out);
}

void
wm_view_send_to_prev_head(struct wm_view *view)
{
	if (!view)
		return;

	struct wlr_output *current = wlr_output_layout_output_at(
		view->server->output_layout,
		view->x + 1, view->y + 1);
	if (!current)
		current = wlr_output_layout_output_at(
			view->server->output_layout,
			view->server->cursor->x,
			view->server->cursor->y);

	struct wm_output *out;
	struct wm_output *prev_out = NULL;
	struct wm_output *last_out = NULL;

	wl_list_for_each(out, &view->server->outputs, link) {
		last_out = out;
		if (out->wlr_output == current) {
			if (prev_out) {
				move_view_to_output(view, prev_out);
				return;
			}
			/* Current is first output, wrap to last */
			break;
		}
		prev_out = out;
	}

	/* Wrap to last output */
	if (last_out)
		move_view_to_output(view, last_out);
}
