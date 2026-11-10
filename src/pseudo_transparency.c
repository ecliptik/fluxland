/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * pseudo_transparency.c - A translucent window shows the desktop behind it
 */

#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_scene.h>

#include "config.h"
#include "pseudo_transparency.h"
#include "view_geometry.h"
#include "server.h"
#include "view.h"

#define PSEUDO_SOURCES_MAX 64

/* Which state of the desktop the copies under windows are of: it moves on
 * whenever the wallpaper (or the outputs, or a setting) does */
static unsigned background_generation = 1;

/* A piece of the desktop under a window */
struct pseudo_piece {
	bool is_rect;
	struct wlr_box box;		/* layout coordinates */
	struct wlr_buffer *buffer;
	struct wlr_fbox src;		/* of the buffer */
	enum wlr_scale_filter_mode filter;
	float opacity;
	float color[4];
};

bool
wm_pseudo_clip(const struct wlr_box *window, const struct wlr_box *node,
	const struct wlr_fbox *src, struct wlr_box *inter,
	struct wlr_fbox *inter_src)
{
	if (node->width < 1 || node->height < 1)
		return false;
	int x0 = window->x > node->x ? window->x : node->x;
	int y0 = window->y > node->y ? window->y : node->y;
	int x1 = window->x + window->width < node->x + node->width ?
		window->x + window->width : node->x + node->width;
	int y1 = window->y + window->height < node->y + node->height ?
		window->y + window->height : node->y + node->height;
	if (x1 <= x0 || y1 <= y0)
		return false;

	inter->x = x0;
	inter->y = y0;
	inter->width = x1 - x0;
	inter->height = y1 - y0;
	if (inter_src && src) {
		/* The buffer is stretched over the node: the same share of
		 * it is what is inside */
		double sx = src->width / node->width;
		double sy = src->height / node->height;
		inter_src->x = src->x + (x0 - node->x) * sx;
		inter_src->y = src->y + (y0 - node->y) * sy;
		inter_src->width = inter->width * sx;
		inter_src->height = inter->height * sy;
	}
	return true;
}

bool
wm_view_pseudo_transparent(const struct wm_view *view)
{
	if (view->pseudo_rule == WM_PSEUDO_YES)
		return true;
	if (view->pseudo_rule == WM_PSEUDO_NO)
		return false;
	return view->server->config &&
		view->server->config->pseudo_transparency;
}

/* Collect what the background layer draws, bottom to top */
static void
collect(struct wlr_scene_node *node, const struct wlr_box *window,
	struct pseudo_piece *pieces, int *count)
{
	if (!node->enabled || *count >= PSEUDO_SOURCES_MAX)
		return;

	if (node->type == WLR_SCENE_NODE_TREE) {
		struct wlr_scene_tree *tree = wlr_scene_tree_from_node(node);
		struct wlr_scene_node *child;
		wl_list_for_each(child, &tree->children, link)
			collect(child, window, pieces, count);
		return;
	}

	int lx, ly;
	if (!wlr_scene_node_coords(node, &lx, &ly))
		return;

	struct pseudo_piece *p = &pieces[*count];
	if (node->type == WLR_SCENE_NODE_RECT) {
		struct wlr_scene_rect *rect = wlr_scene_rect_from_node(node);
		struct wlr_box box = { lx, ly, rect->width, rect->height };
		if (!wm_pseudo_clip(window, &box, NULL, &p->box, NULL))
			return;
		p->is_rect = true;
		for (int i = 0; i < 4; i++)
			p->color[i] = rect->color[i];
		(*count)++;
		return;
	}
	if (node->type != WLR_SCENE_NODE_BUFFER)
		return;

	struct wlr_scene_buffer *sb = wlr_scene_buffer_from_node(node);
	/* A rotated buffer would need its crop turned; wallpapers are not */
	if (!sb->buffer || sb->transform != WL_OUTPUT_TRANSFORM_NORMAL)
		return;
	struct wlr_fbox src = sb->src_box;
	if (src.width <= 0 || src.height <= 0)
		src = (struct wlr_fbox){ 0, 0, sb->buffer->width,
			sb->buffer->height };
	struct wlr_box box = { lx, ly,
		sb->dst_width > 0 ? sb->dst_width : (int)src.width,
		sb->dst_height > 0 ? sb->dst_height : (int)src.height };
	if (!wm_pseudo_clip(window, &box, &src, &p->box, &p->src))
		return;
	p->is_rect = false;
	p->buffer = sb->buffer;
	p->filter = sb->filter_mode;
	p->opacity = sb->opacity;
	(*count)++;
}

/* The desktop is not to be picked: what is below a window's own content */
static bool
never_accepts_input(struct wlr_scene_buffer *buffer, double *sx, double *sy)
{
	return false;
}

static void
destroy_pieces(struct wm_view *view)
{
	if (view->pseudo_tree) {
		wlr_scene_node_destroy(&view->pseudo_tree->node);
		view->pseudo_tree = NULL;
	}
	view->pseudo_generation = 0;
}

void
wm_pseudo_update(struct wm_view *view)
{
	if (!view || !view->scene_tree)
		return;
	if (!view->mapped || !wm_view_surface(view) ||
	    !wm_view_pseudo_transparent(view)) {
		destroy_pieces(view);
		return;
	}

	struct wlr_box geo = {0};
	wm_view_content_geometry(view, &geo);
	/* The content is inside the frame: after its titlebar and borders */
	int top, bottom, left, right;
	wm_view_frame_extents(view, &top, &bottom, &left, &right);
	struct wlr_box window = { view->x + left, view->y + top, geo.width,
		geo.height };

	/* Nothing has moved, and the desktop is as it was: the copy is right */
	if (view->pseudo_tree &&
	    view->pseudo_generation == background_generation &&
	    memcmp(&window, &view->pseudo_box, sizeof(window)) == 0)
		return;
	view->pseudo_box = window;
	view->pseudo_generation = background_generation;

	struct pseudo_piece pieces[PSEUDO_SOURCES_MAX];
	int count = 0;
	collect(&view->server->layer_background->node, &window, pieces,
		&count);

	/* The same pieces as before, only moved? Then move them */
	bool same = view->pseudo_tree != NULL;
	int existing = 0;
	if (same) {
		struct wlr_scene_node *node;
		wl_list_for_each(node, &view->pseudo_tree->children, link) {
			bool is_rect = node->type == WLR_SCENE_NODE_RECT;
			if (existing >= count ||
			    pieces[existing].is_rect != is_rect) {
				same = false;
				break;
			}
			existing++;
		}
		if (same && existing != count)
			same = false;
	}
	if (!same) {
		destroy_pieces(view);
		view->pseudo_tree = wlr_scene_tree_create(view->scene_tree);
		if (!view->pseudo_tree)
			return;
		view->pseudo_tree->node.data = NULL;
		for (int i = 0; i < count; i++) {
			struct pseudo_piece *p = &pieces[i];
			if (p->is_rect) {
				wlr_scene_rect_create(view->pseudo_tree, 1, 1,
					p->color);
			} else {
				struct wlr_scene_buffer *sb =
					wlr_scene_buffer_create(
						view->pseudo_tree, p->buffer);
				if (!sb) {
					destroy_pieces(view);
					return;
				}
				sb->point_accepts_input = never_accepts_input;
			}
		}
		/* Under everything else of the window */
		wlr_scene_node_lower_to_bottom(&view->pseudo_tree->node);
	}

	int i = 0;
	struct wlr_scene_node *node;
	wl_list_for_each(node, &view->pseudo_tree->children, link) {
		struct pseudo_piece *p = &pieces[i++];
		wlr_scene_node_set_position(node, p->box.x - view->x,
			p->box.y - view->y);
		if (p->is_rect) {
			struct wlr_scene_rect *rect =
				wlr_scene_rect_from_node(node);
			wlr_scene_rect_set_size(rect, p->box.width,
				p->box.height);
			wlr_scene_rect_set_color(rect, p->color);
		} else {
			struct wlr_scene_buffer *sb =
				wlr_scene_buffer_from_node(node);
			if (sb->buffer != p->buffer)
				wlr_scene_buffer_set_buffer(sb, p->buffer);
			wlr_scene_buffer_set_source_box(sb, &p->src);
			wlr_scene_buffer_set_dest_size(sb, p->box.width,
				p->box.height);
			wlr_scene_buffer_set_filter_mode(sb, p->filter);
			wlr_scene_buffer_set_opacity(sb, p->opacity);
		}
	}
}

void
wm_pseudo_update_all(struct wm_server *server)
{
	background_generation++;
	struct wm_view *view;
	wl_list_for_each(view, &server->views, link)
		wm_pseudo_update(view);
}

void
wm_pseudo_sync(struct wm_server *server)
{
	struct wm_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (view->pseudo_tree || wm_view_pseudo_transparent(view))
			wm_pseudo_update(view);
	}
}
