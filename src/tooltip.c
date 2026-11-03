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
#ifdef WM_HAS_SYSTRAY
#include "systray.h"
#endif
#include "view.h"

struct wm_tooltip {
	struct wlr_scene_buffer *buf;
	struct wl_event_source *timer;
	struct wm_view *view;	/* whose title it is, for comparing only */
	char *text;
	bool tray;		/* a tray icon's: not in the style's label */
	bool shown;
};

/* The tooltip of a title: the focused window's label, as Fluxbox draws it */
static cairo_surface_t *
render_label_tooltip(struct wm_server *server, const char *text)
{
	struct wm_style *style = server->style;
	if (!text || !*text || !style)
		return NULL;

	const struct wm_font *font = &style->window_label_focus_font;
	int bevel = style->window_bevel_width;
	int bw = style->window_border_width;
	/* A line each */
	enum { MAX_LINES = 16 };
	cairo_surface_t *ts[MAX_LINES] = { NULL };
	int tw[MAX_LINES] = { 0 }, th[MAX_LINES] = { 0 };
	int lines = 0, text_w = 0;
	const char *line = text;
	while (lines < MAX_LINES) {
		const char *end = strchr(line, '\n');
		size_t len = end ? (size_t)(end - line) : strlen(line);
		char *copy = strndup(line, len);
		if (!copy)
			break;
		/* A blank line has nothing to draw, but keeps its place */
		if (*copy)
			ts[lines] = wm_render_text(copy, font,
				&style->window_label_focus_text_color,
				1 << 15, &tw[lines], &th[lines],
				WM_JUSTIFY_LEFT, 1.0f);
		free(copy);
		if (tw[lines] > text_w)
			text_w = tw[lines];
		lines++;
		if (!end)
			break;
		line = end + 1;
	}
	if (lines == 0 || text_w == 0) {
		for (int i = 0; i < lines; i++)
			if (ts[i])
				cairo_surface_destroy(ts[i]);
		return NULL;
	}
	int line_h = wm_font_height(font, 1.0f);
	int w = text_w + 2 * bevel, h = lines * line_h + 2 * bevel;
	int ow = w + 2 * bw, oh = h + 2 * bw;

	cairo_surface_t *surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, ow, oh);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surface);
		for (int i = 0; i < lines; i++)
			if (ts[i])
				cairo_surface_destroy(ts[i]);
		return NULL;
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
	for (int i = 0; i < lines; i++) {
		if (!ts[i])
			continue;
		cairo_set_source_surface(cr, ts[i], bw + bevel,
			bw + bevel + i * line_h + (line_h - th[i]) / 2);
		cairo_paint(cr);
		cairo_surface_destroy(ts[i]);
	}
	cairo_destroy(cr);
	return surface;
}

/*
 * A tray icon's tooltip (systray.tooltip.*): not the style's window label.
 * By default a dark rounded box with a thin border, the application's title
 * and description a line each; the colors, font, padding, corners and
 * opacity are all settings.
 */
static cairo_surface_t *
render_tray_tooltip(struct wm_server *server, const char *text)
{
	const struct wm_style *style = server->style;
	if (!style || !text || !*text)
		return NULL;
	const struct wm_font *font = &style->systray_tooltip_font;

	enum { MAX_LINES = 16 };
	cairo_surface_t *ts[MAX_LINES] = { NULL };
	int tw[MAX_LINES] = { 0 }, th[MAX_LINES] = { 0 };
	int lines = 0, text_w = 0;
	const char *line = text;
	while (lines < MAX_LINES) {
		const char *end = strchr(line, '\n');
		size_t len = end ? (size_t)(end - line) : strlen(line);
		char *copy = strndup(line, len);
		if (!copy)
			break;
		if (*copy)
			ts[lines] = wm_render_text(copy, font,
				&style->systray_tooltip_text_color, 1 << 15,
				&tw[lines], &th[lines], WM_JUSTIFY_LEFT, 1.0f);
		free(copy);
		if (tw[lines] > text_w)
			text_w = tw[lines];
		lines++;
		if (!end)
			break;
		line = end + 1;
	}

	cairo_surface_t *surface = NULL;
	int line_h = wm_font_height(font, 1.0f);
	if (lines > 0 && text_w > 0) {
		/* the border's pixel is outside the padding, each side */
		int w = text_w + style->systray_tooltip_padding_left +
			style->systray_tooltip_padding_right + 2;
		int h = lines * line_h + style->systray_tooltip_padding_top +
			style->systray_tooltip_padding_bottom + 2;

		surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
		if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
			cairo_surface_destroy(surface);
			surface = NULL;
		}
	}
	if (surface) {
		int w = cairo_image_surface_get_width(surface);
		int h = cairo_image_surface_get_height(surface);
		double alpha = style->systray_tooltip_alpha / 255.0;
		const struct wm_color *color = &style->systray_tooltip_color;
		const struct wm_color *border =
			&style->systray_tooltip_border_color;
		uint8_t all = WM_CORNER_TOP_LEFT | WM_CORNER_TOP_RIGHT |
			WM_CORNER_BOTTOM_LEFT | WM_CORNER_BOTTOM_RIGHT;

		cairo_t *cr = cairo_create(surface);
		/* The box, filled, then its border down its edge */
		wm_render_rounded_rect_path(cr, 0.5, 0.5, w - 1, h - 1,
			style->systray_tooltip_radius, all);
		cairo_set_source_rgba(cr, color->r / 255.0, color->g / 255.0,
			color->b / 255.0, alpha);
		cairo_fill_preserve(cr);
		cairo_set_source_rgba(cr, border->r / 255.0,
			border->g / 255.0, border->b / 255.0, alpha);
		cairo_set_line_width(cr, 1.0);
		cairo_stroke(cr);
		for (int i = 0; i < lines; i++) {
			if (!ts[i])
				continue;
			cairo_set_source_surface(cr, ts[i],
				1 + style->systray_tooltip_padding_left,
				1 + style->systray_tooltip_padding_top +
				i * line_h + (line_h - th[i]) / 2);
			cairo_paint(cr);
		}
		cairo_destroy(cr);
	}
	for (int i = 0; i < lines; i++)
		if (ts[i])
			cairo_surface_destroy(ts[i]);
	return surface;
}

static void
raise_tooltip(struct wm_server *server)
{
	struct wm_tooltip *t = server->tooltip;
	if (!t || !t->text || !*t->text)
		return;

	cairo_surface_t *surface = t->tray ?
		render_tray_tooltip(server, t->text) :
		render_label_tooltip(server, t->text);
	if (!surface)
		return;
	int ow = cairo_image_surface_get_width(surface);
	int oh = cairo_image_surface_get_height(surface);

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
	t->tray = false;
	t->shown = false;
}

void
wm_tooltip_update(struct wm_server *server)
{
	int delay = server->config ? server->config->tooltip_delay_ms : 500;
	if (delay < 0)
		return;

	/* A tray icon's, else the title under the pointer that doesn't fit */
	double lx = server->cursor->x, ly = server->cursor->y;
	struct wm_view *view = NULL;
	const char *text = NULL;
	bool tray = false;
	/* Not the toolbar's when a fullscreen window is drawn over it */
	bool on_toolbar = wm_pointer_on_toolbar(server, lx, ly);
#ifdef WM_HAS_SYSTRAY
	if (server->systray && on_toolbar) {
		text = wm_systray_tooltip_at(server->systray, lx, ly);
		tray = text != NULL;
	}
#endif
	if (!text && on_toolbar)
		text = wm_toolbar_tooltip_at(server->toolbar, lx, ly, &view);
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
	if (t->tray == tray && t->view == view && t->text &&
	    strcmp(t->text, text) == 0)
		return;
	bool update = t->shown && t->view == view;
	free(t->text);
	t->text = strdup(text);
	t->view = view;
	t->tray = tray;
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
