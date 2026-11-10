/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * view_internal.c - windows the compositor draws itself (view_internal.h)
 */

#define _GNU_SOURCE
#include "view_internal.h"

#include <stdlib.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>

#include "decoration.h"
#include "output.h"
#include "server.h"
#include "view.h"

struct wm_internal_view {
	struct wm_view view;
	struct wlr_scene_buffer *content;
	int width, height;
	const struct wm_internal_ops *ops;
	void *data;
	bool dying;
};

static const struct wm_view_impl internal_view_impl;

static struct wm_internal_view *
internal_of(struct wm_view *view)
{
	if (!view || view->impl != &internal_view_impl)
		return NULL;
	return (struct wm_internal_view *)view;	/* view is the first member */
}

static struct wlr_surface *
internal_surface(struct wm_view *view)
{
	(void)view;
	return NULL;
}

static void
internal_content_geometry(struct wm_view *view, struct wlr_box *geo)
{
	struct wm_internal_view *iv = internal_of(view);
	*geo = (struct wlr_box){ 0, 0, iv->width, iv->height };
}

/* Its size is its own: min and max are the same */
static void
internal_size_hints(struct wm_view *view, struct wm_size_hints *h)
{
	struct wm_internal_view *iv = internal_of(view);
	h->min_w = h->max_w = iv->width;
	h->min_h = h->max_h = iv->height;
}

static void
internal_set_size(struct wm_view *view, int width, int height)
{
	(void)view; (void)width; (void)height;
}

static void
internal_set_bool(struct wm_view *view, bool value)
{
	(void)view; (void)value;
}

static void
internal_set_tiled(struct wm_view *view, uint32_t edges)
{
	(void)view; (void)edges;
}

static void
internal_close(struct wm_view *view)
{
	struct wm_internal_view *iv = internal_of(view);
	if (iv->ops && iv->ops->closed)
		iv->ops->closed(iv->data);
	wm_internal_view_destroy(view);
}

static struct wl_client *
internal_client(struct wm_view *view)
{
	(void)view;
	return NULL;
}

static pid_t
internal_pid(struct wm_view *view)
{
	(void)view;
	return 0;
}

static struct wm_view *
internal_parent(struct wm_view *view)
{
	(void)view;
	return NULL;
}

static const struct wm_view_impl internal_view_impl = {
	.surface = internal_surface,
	.content_geometry = internal_content_geometry,
	.set_size = internal_set_size,
	.moved = NULL,
	.set_activated = internal_set_bool,
	.set_maximized = internal_set_bool,
	.set_fullscreen = internal_set_bool,
	.set_tiled = internal_set_tiled,
	.close = internal_close,
	.client = internal_client,
	.pid = internal_pid,
	.parent = internal_parent,
	.size_hints = internal_size_hints,
};

struct wm_view *
wm_internal_view_create(struct wm_server *server, const char *title,
	const char *app_id, int width, int height,
	const struct wm_internal_ops *ops, void *data)
{
	struct wm_internal_view *iv = calloc(1, sizeof(*iv));
	if (!iv)
		return NULL;
	struct wm_view *view = &iv->view;
	view->type = WM_VIEW_INTERNAL;
	view->impl = &internal_view_impl;
	iv->width = width;
	iv->height = height;
	iv->ops = ops;
	iv->data = data;
	if (!wm_view_init_common(view, server)) {
		free(iv);
		return NULL;
	}
	/* Before the decoration: the content is the tree's first child */
	iv->content = wlr_scene_buffer_create(view->scene_tree, NULL);
	if (!iv->content) {
		wm_view_destroy_common(view);
		free(iv);
		return NULL;
	}
	/* The middle of the head under the pointer is where it goes */
	view->rule_placed = true;
	view->deco_hint = WM_DECOR_TINY;
	wm_view_set_title(view, title);
	wm_view_set_app_id(view, app_id);
	wm_view_map_common(view, false, false);

	struct wlr_box box = { 0, 0, 1024, 768 };
	struct wlr_output *wo = wlr_output_layout_output_at(
		server->output_layout, server->cursor->x, server->cursor->y);
	struct wm_output *o;
	wl_list_for_each(o, &server->outputs, link) {
		if (o->wlr_output == wo || !wo) {
			box = o->usable_area;
			break;
		}
	}
	int top = 0, bottom = 0, left = 0, right = 0;
	if (view->decoration)
		wm_decoration_get_extents(view->decoration,
			&top, &bottom, &left, &right);
	int fw = width + left + right, fh = height + top + bottom;
	wm_view_move(view, box.x + (box.width - fw) / 2,
		box.y + (box.height - fh) / 2);
	return view;
}

void
wm_internal_view_set_buffer(struct wm_view *view, struct wlr_buffer *buffer)
{
	struct wm_internal_view *iv = internal_of(view);
	if (iv)
		wlr_scene_buffer_set_buffer(iv->content, buffer);
}

static void
internal_free(void *data)
{
	struct wm_internal_view *iv = data;
	wm_view_destroy_common(&iv->view);
	free(iv);
}

/*
 * Gone from the screen and the focus now; freed from the event loop, as
 * what closed it may be walking the list of windows (CloseAllWindows)
 */
void
wm_internal_view_destroy(struct wm_view *view)
{
	struct wm_internal_view *iv = internal_of(view);
	if (!iv || iv->dying)
		return;
	iv->dying = true;
	iv->ops = NULL;
	if (view->mapped)
		wm_view_unmap_common(view);
	wl_event_loop_add_idle(wl_display_get_event_loop(
		view->server->wl_display), internal_free, iv);
}

bool
wm_internal_view_key(struct wm_view *view, xkb_keysym_t sym,
	const char *utf8, uint32_t modifiers)
{
	struct wm_internal_view *iv = internal_of(view);
	if (!iv || iv->dying || !iv->ops || !iv->ops->key)
		return false;
	return iv->ops->key(iv->data, sym, utf8, modifiers);
}
