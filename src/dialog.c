/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * dialog.c - Fluxbox's text dialogs (TextDialog, CommandDialog)
 *
 * A title in the focused window's label style over a white line with
 * black text in window.font, in the window's border; 200 pixels wide
 * or as wide as the title, in the middle of the head.  Return runs the
 * line as a command (after the dialog's precommand, "SetTitle " for
 * SetTitleDialog...), Escape closes it, Tab completes a command name.
 */

#define _GNU_SOURCE
#include "dialog.h"

#include <cairo.h>
#include <ctype.h>
#include <pango/pangocairo.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>

#include "command.h"
#include "keybind.h"
#include "output.h"
#include "pixel_buffer.h"
#include "render.h"
#include "server.h"
#include "style.h"
#include "view.h"

#define DIALOG_MIN_WIDTH 200
#define DIALOG_TEXT_MAX 1024

struct wm_dialog {
	struct wm_server *server;
	struct wlr_scene_tree *tree;
	struct wlr_scene_buffer *buf;
	char *title;
	char *precommand;
	struct wm_view *target;
	char text[DIALOG_TEXT_MAX];
	size_t len, cursor;	/* bytes */
	int x, y, width, height, label_h, bw;
	bool dragging;
	double drag_dx, drag_dy;
};

/* The width of text as it is laid out (spaces too), for the cursor */
static int
text_advance(const struct wm_font *font, const char *text, size_t len)
{
	cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
		1, 1);
	cairo_t *cr = cairo_create(s);
	PangoLayout *layout = pango_cairo_create_layout(cr);
	PangoFontDescription *desc = pango_font_description_new();
	pango_font_description_set_family(desc,
		font->family ? font->family : "sans");
	if (font->pixel_size)
		pango_font_description_set_absolute_size(desc,
			font->size * PANGO_SCALE);
	else
		pango_font_description_set_size(desc, font->size * PANGO_SCALE);
	if (font->bold)
		pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
	if (font->italic)
		pango_font_description_set_style(desc, PANGO_STYLE_ITALIC);
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);
	pango_layout_set_text(layout, text, (int)len);
	PangoRectangle logical;
	pango_layout_get_pixel_extents(layout, NULL, &logical);
	g_object_unref(layout);
	cairo_destroy(cr);
	cairo_surface_destroy(s);
	return logical.width;
}

static void
render(struct wm_dialog *d)
{
	struct wm_style *style = d->server->style;
	if (!style)
		return;
	int bw = d->bw, w = d->width, lh = d->label_h;
	int th = wm_font_height(&style->window_font, 1.0f) + 2;
	d->height = lh + th + 2 * bw;
	int ow = w + 2 * bw;

	cairo_surface_t *surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, ow, d->height);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surface);
		return;
	}
	cairo_t *cr = cairo_create(surface);
	const struct wm_color *bc = &style->window_border_color;
	cairo_set_source_rgb(cr, bc->r / 255.0, bc->g / 255.0, bc->b / 255.0);
	cairo_paint(cr);

	/* The title: the label texture, the titlebar's when that is
	 * ParentRelative (TextDialog::render) */
	const struct wm_texture *tex = &style->window_label_focus;
	if (tex->fill == WM_TEX_PARENT_RELATIVE)
		tex = &style->window_title_focus;
	cairo_surface_t *bg = wm_render_texture(tex, w, lh, 1.0f);
	if (bg) {
		cairo_set_source_surface(cr, bg, bw, bw);
		cairo_paint(cr);
		cairo_surface_destroy(bg);
	}
	int tw, tth;
	cairo_surface_t *ts = wm_render_text(d->title,
		&style->window_label_focus_font,
		&style->window_label_focus_text_color, w - 4, &tw, &tth,
		style->window_justify, 1.0f);
	if (ts) {
		int tx = bw + 2;
		if (style->window_justify == WM_JUSTIFY_CENTER)
			tx = bw + (w - tw) / 2;
		else if (style->window_justify == WM_JUSTIFY_RIGHT)
			tx = bw + w - tw - 2;
		cairo_set_source_surface(cr, ts, tx, bw + (lh - tth) / 2);
		cairo_paint(cr);
		cairo_surface_destroy(ts);
	}

	/* The line: white, black text, scrolled to keep the cursor in */
	int by = bw + lh;
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_rectangle(cr, bw, by, w, th);
	cairo_fill(cr);
	const struct wm_color black = { 0, 0, 0, 255 };
	int cursor_x = text_advance(&style->window_font, d->text, d->cursor);
	int scroll = cursor_x > w - 4 ? cursor_x - (w - 4) : 0;
	cairo_save(cr);
	cairo_rectangle(cr, bw, by, w, th);
	cairo_clip(cr);
	if (d->len) {
		cairo_surface_t *text = wm_render_text(d->text,
			&style->window_font, &black, 1 << 15, &tw, &tth,
			WM_JUSTIFY_LEFT, 1.0f);
		if (text) {
			cairo_set_source_surface(cr, text, bw + 2 - scroll,
				by + (th - tth) / 2);
			cairo_paint(cr);
			cairo_surface_destroy(text);
		}
	}
	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_rectangle(cr, bw + 2 + cursor_x - scroll, by + 1, 1, th - 2);
	cairo_fill(cr);
	cairo_restore(cr);
	cairo_destroy(cr);

	struct wlr_buffer *buf = wlr_buffer_from_cairo(surface);
	wlr_scene_buffer_set_buffer(d->buf, buf);
	if (buf)
		wlr_buffer_drop(buf);
	wlr_scene_node_set_position(&d->tree->node, d->x, d->y);
}

void
wm_dialog_open(struct wm_server *server, const char *title,
	const char *precommand, const char *text, struct wm_view *target)
{
	if (!server || !server->style)
		return;
	wm_dialog_close(server);
	struct wm_dialog *d = calloc(1, sizeof(*d));
	if (!d)
		return;
	d->server = server;
	d->title = strdup(title ? title : "");
	d->precommand = strdup(precommand ? precommand : "");
	d->target = target;
	if (text) {
		snprintf(d->text, sizeof(d->text), "%s", text);
		d->len = d->cursor = strlen(d->text);
	}
	d->tree = wlr_scene_tree_create(server->layer_overlay);
	d->buf = d->tree ? wlr_scene_buffer_create(d->tree, NULL) : NULL;
	if (!d->buf) {
		if (d->tree)
			wlr_scene_node_destroy(&d->tree->node);
		free(d->title);
		free(d->precommand);
		free(d);
		return;
	}

	struct wm_style *style = server->style;
	d->bw = style->window_border_width;
	d->label_h = wm_font_height(&style->window_label_focus_font, 1.0f) + 2;
	int title_w = wm_measure_text_width(d->title,
		&style->window_label_focus_font, 1.0f) + 4;
	d->width = title_w > DIALOG_MIN_WIDTH ? title_w : DIALOG_MIN_WIDTH;

	/* The middle of the head under the pointer */
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
	render(d);
	d->x = box.x + (box.width - d->width - 2 * d->bw) / 2;
	d->y = box.y + (box.height - d->height) / 2;
	wlr_scene_node_set_position(&d->tree->node, d->x, d->y);
	server->dialog = d;
}

void
wm_dialog_close(struct wm_server *server)
{
	struct wm_dialog *d = server ? server->dialog : NULL;
	if (!d)
		return;
	server->dialog = NULL;
	wlr_scene_node_destroy(&d->tree->node);
	free(d->title);
	free(d->precommand);
	free(d);
}

bool
wm_dialog_is_open(struct wm_server *server)
{
	return server && server->dialog;
}

/* The start of the UTF-8 character before i, or after it */
static size_t
utf8_prev(const char *s, size_t i)
{
	while (i > 0 && ((unsigned char)s[--i] & 0xC0) == 0x80)
		;
	return i;
}

static size_t
utf8_next(const char *s, size_t len, size_t i)
{
	while (i < len && ((unsigned char)s[++i] & 0xC0) == 0x80)
		;
	return i;
}

static void
delete_range(struct wm_dialog *d, size_t from, size_t to)
{
	memmove(d->text + from, d->text + to, d->len - to + 1);
	d->len -= to - from;
	d->cursor = from;
}

/* CommandDialog::tabComplete: the command name before the cursor
 * finished as the first command that starts with it */
static void
complete(struct wm_dialog *d)
{
	size_t start = d->cursor;
	while (start > 0 && isalnum((unsigned char)d->text[start - 1]))
		start--;
	if (start == d->cursor)
		return;
	char prefix[64];
	snprintf(prefix, sizeof(prefix), "%.*s", (int)(d->cursor - start),
		d->text + start);
	const char *match = wm_action_complete(prefix);
	if (!match)
		return;
	const char *rest = match + strlen(prefix);
	size_t n = strlen(rest);
	if (d->len + n >= sizeof(d->text))
		return;
	memmove(d->text + d->cursor + n, d->text + d->cursor,
		d->len - d->cursor + 1);
	memcpy(d->text + d->cursor, rest, n);
	d->len += n;
	d->cursor += n;
}

static void
run(struct wm_server *server, struct wm_dialog *d)
{
	size_t n = strlen(d->precommand) + d->len + 1;
	char *line = malloc(n);
	if (!line)
		return;
	snprintf(line, n, "%s%s", d->precommand, d->text);
	struct wm_view *target = d->target ? d->target : server->focused_view;
	wm_dialog_close(server);
	struct wm_command *cmd = wm_command_parse(line);
	if (cmd) {
		wm_command_execute(server, cmd, target);
		wm_command_destroy(cmd);
	} else {
		wlr_log(WLR_INFO, "dialog: not a command: %s", line);
	}
	free(line);
}

bool
wm_dialog_handle_key(struct wm_server *server, xkb_keysym_t sym,
	const char *utf8, uint32_t modifiers)
{
	struct wm_dialog *d = server ? server->dialog : NULL;
	if (!d)
		return false;
	bool ctrl = modifiers & WLR_MODIFIER_CTRL;
	switch (sym) {
	case XKB_KEY_Return:
	case XKB_KEY_KP_Enter:
		run(server, d);
		return true;
	case XKB_KEY_Escape:
		wm_dialog_close(server);
		return true;
	case XKB_KEY_Tab:
		complete(d);
		break;
	case XKB_KEY_BackSpace:
		if (d->cursor > 0)
			delete_range(d, utf8_prev(d->text, d->cursor),
				d->cursor);
		break;
	case XKB_KEY_Delete:
	case XKB_KEY_KP_Delete:
		if (d->cursor < d->len)
			delete_range(d, d->cursor,
				utf8_next(d->text, d->len, d->cursor));
		break;
	case XKB_KEY_Left:
	case XKB_KEY_KP_Left:
		d->cursor = utf8_prev(d->text, d->cursor);
		break;
	case XKB_KEY_Right:
	case XKB_KEY_KP_Right:
		d->cursor = utf8_next(d->text, d->len, d->cursor);
		break;
	case XKB_KEY_Home:
	case XKB_KEY_KP_Home:
		d->cursor = 0;
		break;
	case XKB_KEY_End:
	case XKB_KEY_KP_End:
		d->cursor = d->len;
		break;
	default:
		/* FbTk::TextBox: Ctrl+A, E, U and K as a shell has them */
		if (ctrl && (sym == XKB_KEY_a || sym == XKB_KEY_A)) {
			d->cursor = 0;
		} else if (ctrl && (sym == XKB_KEY_e || sym == XKB_KEY_E)) {
			d->cursor = d->len;
		} else if (ctrl && (sym == XKB_KEY_u || sym == XKB_KEY_U)) {
			delete_range(d, 0, d->cursor);
		} else if (ctrl && (sym == XKB_KEY_k || sym == XKB_KEY_K)) {
			delete_range(d, d->cursor, d->len);
		} else if (!ctrl && !(modifiers & (WLR_MODIFIER_ALT |
			   WLR_MODIFIER_LOGO)) && utf8 && (unsigned char)
			   utf8[0] >= 0x20 && utf8[0] != 0x7f) {
			size_t n = strlen(utf8);
			if (d->len + n < sizeof(d->text)) {
				memmove(d->text + d->cursor + n,
					d->text + d->cursor,
					d->len - d->cursor + 1);
				memcpy(d->text + d->cursor, utf8, n);
				d->len += n;
				d->cursor += n;
			}
		}
		break;
	}
	render(d);
	return true;
}

bool
wm_dialog_handle_button(struct wm_server *server, double lx, double ly,
	bool pressed)
{
	struct wm_dialog *d = server ? server->dialog : NULL;
	if (!d)
		return false;
	if (!pressed) {
		bool was = d->dragging;
		d->dragging = false;
		return was;
	}
	int ow = d->width + 2 * d->bw;
	if (lx < d->x || ly < d->y || lx >= d->x + ow || ly >= d->y + d->height) {
		/* A click elsewhere: the dialog is done */
		wm_dialog_close(server);
		return false;
	}
	if (ly < d->y + d->bw + d->label_h) {
		d->dragging = true;
		d->drag_dx = lx - d->x;
		d->drag_dy = ly - d->y;
	}
	return true;
}

bool
wm_dialog_handle_motion(struct wm_server *server, double lx, double ly)
{
	struct wm_dialog *d = server ? server->dialog : NULL;
	if (!d || !d->dragging)
		return false;
	d->x = (int)(lx - d->drag_dx);
	d->y = (int)(ly - d->drag_dy);
	wlr_scene_node_set_position(&d->tree->node, d->x, d->y);
	return true;
}

void
wm_dialog_view_destroyed(struct wm_server *server, struct wm_view *view)
{
	if (server && server->dialog && server->dialog->target == view)
		wm_dialog_close(server);
}
