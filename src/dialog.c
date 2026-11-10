/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * dialog.c - Fluxbox's text dialogs (TextDialog, CommandDialog)
 *
 * A window of the compositor's own (view_internal.c): the
 * style's frame and title, over a white line with black text in
 * dialog.font, 200 pixels wide, in the middle of the head.  Return
 * runs the line as a command (after the dialog's precommand, "SetTitle "
 * for SetTitleDialog...), Escape closes it, Tab completes a command name.
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
#include "i18n.h"
#include "render.h"
#include "server.h"
#include "style.h"
#include "util.h"
#include "view.h"
#include "view_internal.h"

#define DIALOG_MIN_WIDTH 200
#define DIALOG_TEXT_MAX 1024

struct wm_dialog {
	struct wm_server *server;
	struct wm_view *view;	/* its window */
	char *precommand;
	struct wm_view *target;
	struct wm_view *prev_focus;	/* the window it was opened over */
	char text[DIALOG_TEXT_MAX];
	size_t len, cursor;	/* bytes */
	int width, height;	/* of the line */

};

static void
render(struct wm_dialog *d)
{
	struct wm_style *style = d->server->style;
	if (!style)
		return;
	struct wm_font lf = style->dialog_font;
	int w = d->width, th = d->height;

	cairo_surface_t *surface =
		cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, th);
	if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(surface);
		return;
	}
	cairo_t *cr = cairo_create(surface);

	/* The line: white, black text, scrolled to keep the cursor in */
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_paint(cr);
	const struct wm_color black = { 0, 0, 0, 255 };
	int tw, tth;
	int cursor_x = wm_text_advance(d->text, d->cursor, &lf, 1.0f);
	int scroll = cursor_x > w - 4 ? cursor_x - (w - 4) : 0;
	if (d->len) {
		cairo_surface_t *text = wm_render_text(d->text,
			&lf, &black, 1 << 15, &tw, &tth,
			WM_JUSTIFY_LEFT, 1.0f);
		if (text) {
			cairo_set_source_surface(cr, text, 2 - scroll,
				(th - tth) / 2);
			cairo_paint(cr);
			cairo_surface_destroy(text);
		}
	}
	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_rectangle(cr, 2 + cursor_x - scroll, 1, 1, th - 2);
	cairo_fill(cr);
	cairo_destroy(cr);

	struct wlr_buffer *buf = wlr_buffer_from_cairo(surface);
	wm_internal_view_set_buffer(d->view, buf);
	if (buf)
		wlr_buffer_drop(buf);
}

static bool dialog_key(void *data, xkb_keysym_t sym, const char *utf8,
	uint32_t modifiers);
static void dialog_closed(void *data);

static const struct wm_internal_ops dialog_ops = {
	.key = dialog_key,
	.closed = dialog_closed,
};

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
	d->precommand = strdup(precommand ? precommand : "");
	d->target = target;
	d->prev_focus = server->focused_view;
	if (text) {
		snprintf(d->text, sizeof(d->text), "%s", text);
		d->len = d->cursor = strlen(d->text);
	}

	struct wm_font lf = server->style->dialog_font;
	d->width = DIALOG_MIN_WIDTH;
	d->height = wm_font_height(&lf, 1.0f) + 2;
	d->view = wm_internal_view_create(server, title, "fluxland-dialog",
		d->width, d->height, &dialog_ops, d);
	if (!d->view) {
		free(d->precommand);
		free(d);
		return;
	}
	server->dialog = d;
	render(d);
}

/* The dialog lets go of its window (which goes on its own, or is
 * destroyed by the caller) */
static struct wm_view *
detach(struct wm_server *server)
{
	struct wm_dialog *d = server->dialog;
	struct wm_view *view = d->view;
	server->dialog = NULL;
	free(d->precommand);
	free(d);
	return view;
}

void
wm_dialog_close(struct wm_server *server)
{
	if (!server || !server->dialog)
		return;
	wm_internal_view_destroy(detach(server));
}

static void
dialog_closed(void *data)
{
	struct wm_dialog *d = data;
	if (d->server->dialog == d)
		detach(d->server);
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
	struct wm_view *target = d->target ? d->target : d->prev_focus;
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

static bool
dialog_key(void *data, xkb_keysym_t sym, const char *utf8,
	uint32_t modifiers)
{
	struct wm_dialog *d = data;
	struct wm_server *server = d->server;
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

void
wm_dialog_view_destroyed(struct wm_server *server, struct wm_view *view)
{
	if (!server || !server->dialog)
		return;
	if (server->dialog->prev_focus == view)
		server->dialog->prev_focus = NULL;
	if (server->dialog->target == view)
		wm_dialog_close(server);
}
