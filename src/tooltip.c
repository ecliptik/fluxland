/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * tooltip.c - Fluxbox's TooltipWindow
 *
 * The title of an iconbar button or a tab too long for it, drawn as the
 * focused window's label (OSDWindow): its texture (the titlebar's when
 * that is ParentRelative), text and font, bevelWidth around the text,
 * in the window's border.  Centred over the pointer, 10 pixels above it
 * (below when there is no room), kept on the head.
 */

#define _GNU_SOURCE
#include "tooltip.h"

#include <cairo.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>

#include "config.h"
#include "cursor_actions.h"
#include "decoration.h"
#include "output.h"
#include "pixel_buffer.h"
#include "render.h"
#include "server.h"
#include "style.h"
#include "toolbar.h"
#include "view.h"

struct wm_tooltip {
	struct wlr_scene_buffer *buf;
	struct wl_event_source *timer;
	struct wm_view *view;	/* whose title it is, for comparing only */
	char *text;
	bool shown;
};

static void
raise_tooltip(struct wm_server *server)
{
	struct wm_tooltip *t = server->tooltip;
	struct wm_style *style = server->style;
	if (!t || !t->text || !*t->text || !style)
		return;

	const struct wm_font *font = &style->window_label_focus_font;
	int bevel = style->window_bevel_width;
	int bw = style->window_border_width;
	int tw, th;
	cairo_surface_t *ts = wm_render_text(t->text, font,
		&style->window_label_focus_text_color, 1 << 15, &tw, &th,
		WM_JUSTIFY_LEFT, 1.0f);
	if (!ts)
		return;
	int w = tw + 2 * bevel, h = wm_font_height(font, 1.0f) + 2 * bevel;
	int ow = w + 2 * bw, oh = h + 2 * bw;

	cairo_surface_t *surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, ow, oh);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surface);
		cairo_surface_destroy(ts);
		return;
	}
	cairo_t *cr = cairo_create(surface);
	const struct wm_color *bc = &style->window_border_color;
	cairo_set_source_rgb(cr, bc->r / 255.0, bc->g / 255.0, bc->b / 255.0);
	cairo_paint(cr);
	const struct wm_texture *tex = &style->window_label_focus;
	if (tex->fill == WM_TEX_PARENT_RELATIVE)
		tex = &style->window_title_focus;
	cairo_surface_t *bg = wm_render_texture(tex, w, h, 1.0f);
	if (bg) {
		cairo_set_source_surface(cr, bg, bw, bw);
		cairo_paint(cr);
		cairo_surface_destroy(bg);
	}
	cairo_set_source_surface(cr, ts, bw + bevel, bw + (h - th) / 2);
	cairo_paint(cr);
	cairo_surface_destroy(ts);
	cairo_destroy(cr);

	/* Over the pointer, on its head */
	int px = (int)server->cursor->x, py = (int)server->cursor->y;
	struct wlr_box head = { px, py, 1, 1 };
	struct wlr_output *o = wlr_output_layout_output_at(
		server->output_layout, px, py);
	struct wm_output *out = o ? wm_output_from_wlr(server, o) : NULL;
	if (out)
		wm_output_layout_box(out, &head);
	int x = px - ow / 2;
	int y = py - 10 - oh >= head.y ? py - 10 - oh : py + 10;
	if (x + ow > head.x + head.width)
		x = head.x + head.width - ow;
	if (x < head.x)
		x = head.x;

	if (!t->buf) {
		t->buf = wlr_scene_buffer_create(server->layer_overlay, NULL);
		if (!t->buf) {
			cairo_surface_destroy(surface);
			return;
		}
	}
	struct wlr_buffer *buf = wlr_buffer_from_cairo(surface);
	wlr_scene_buffer_set_buffer(t->buf, buf);
	if (buf)
		wlr_buffer_drop(buf);
	wlr_scene_node_set_position(&t->buf->node, x, y);
	wlr_scene_node_raise_to_top(&t->buf->node);
	wlr_scene_node_set_enabled(&t->buf->node, true);
	t->shown = true;
}

static int
handle_timer(void *data)
{
	raise_tooltip(data);
	return 0;
}

void
wm_tooltip_hide(struct wm_server *server)
{
	struct wm_tooltip *t = server->tooltip;
	if (!t)
		return;
	if (t->timer)
		wl_event_source_timer_update(t->timer, 0);
	if (t->buf)
		wlr_scene_node_set_enabled(&t->buf->node, false);
	free(t->text);
	t->text = NULL;
	t->view = NULL;
	t->shown = false;
}

void
wm_tooltip_update(struct wm_server *server)
{
	int delay = server->config ? server->config->tooltip_delay_ms : 500;
	if (delay < 0)
		return;

	/* The title under the pointer that doesn't fit */
	double lx = server->cursor->x, ly = server->cursor->y;
	struct wm_view *view = NULL;
	const char *text = wm_toolbar_tooltip_at(server->toolbar, lx, ly,
		&view);
	if (!text && !view) {
		double sx, sy;
		struct wlr_surface *surface = NULL;
		struct wm_view *v = view_at(server, lx, ly, &surface, &sx, &sy);
		if (v && v->decoration)
			text = wm_decoration_tooltip_at(v->decoration,
				lx - v->x, ly - v->y, &view);
	}

	struct wm_tooltip *t = server->tooltip;
	if (!text) {
		if (t && (t->text || t->shown))
			wm_tooltip_hide(server);
		return;
	}
	if (!t) {
		t = calloc(1, sizeof(*t));
		if (!t)
			return;
		server->tooltip = t;
	}
	/* Still on the same one: it changes only with its title */
	if (t->view == view && t->text && strcmp(t->text, text) == 0)
		return;
	bool update = t->shown && t->view == view;
	free(t->text);
	t->text = strdup(text);
	t->view = view;
	if (update || delay == 0) {
		raise_tooltip(server);
		return;
	}
	if (t->buf)
		wlr_scene_node_set_enabled(&t->buf->node, false);
	t->shown = false;
	if (!t->timer)
		t->timer = wl_event_loop_add_timer(server->wl_event_loop,
			handle_timer, server);
	if (t->timer)
		wl_event_source_timer_update(t->timer, delay);
}

void
wm_tooltip_view_destroyed(struct wm_server *server, struct wm_view *view)
{
	if (server->tooltip && server->tooltip->view == view)
		wm_tooltip_hide(server);
}

void
wm_tooltip_finish(struct wm_server *server)
{
	struct wm_tooltip *t = server->tooltip;
	if (!t)
		return;
	if (t->timer)
		wl_event_source_remove(t->timer);
	if (t->buf)
		wlr_scene_node_destroy(&t->buf->node);
	free(t->text);
	free(t);
	server->tooltip = NULL;
}
