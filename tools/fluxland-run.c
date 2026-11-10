/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * fluxland-run.c - a window to type a program in (fbrun's place)
 *
 * An ordinary Wayland client: an xdg-shell window that the compositor
 * frames (xdg-decoration) like any other, with a line of text to edit.
 * Return runs the line through the shell, Escape closes the window, Tab
 * completes program and file names (and goes through them on the next
 * presses), Up and Down go through the lines run before.  Holding a key
 * repeats it, as the compositor says (wl_keyboard.repeat_info): the
 * repeating is the client's.
 */

#define _GNU_SOURCE
#include <cairo.h>
#include <errno.h>
#include <poll.h>
#include <pango/pangocairo.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include "run_dialog.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

#define WIDTH 200
#define TEXT_MAX 1024
#define DEFAULT_FONT "Sans 12"

struct app {
	struct wl_display *display;
	struct wl_compositor *compositor;
	struct wl_shm *shm;
	struct xdg_wm_base *wm_base;
	struct zxdg_decoration_manager_v1 *deco_manager;
	struct wl_seat *seat;
	struct wl_keyboard *keyboard;
	struct wl_surface *surface;
	struct xdg_surface *xdg_surface;
	struct xdg_toplevel *toplevel;
	struct zxdg_toplevel_decoration_v1 *deco;

	struct xkb_context *xkb_context;
	struct xkb_keymap *keymap;
	struct xkb_state *xkb_state;

	/* Key repeat: what the compositor says, and the key held */
	int repeat_rate, repeat_delay;	/* per second, ms */
	int timer_fd;
	xkb_keycode_t held;

	PangoFontDescription *font;
	int height;
	bool configured, closing;

	/* The line */
	char text[TEXT_MAX];
	size_t len, cursor;
	struct wm_run_history history;
	char *history_path;
	size_t history_pos;	/* in history; history.count: the new line */
	char *draft;		/* the line as it was before going up */
	char **cands;		/* what Tab is going through */
	size_t ncands, cand_next, cand_start;
	bool run;		/* Return: run text, then close */
};

/* --- Drawing --- */

static PangoLayout *
make_layout(cairo_t *cr, struct app *app, const char *text, size_t len)
{
	PangoLayout *layout = pango_cairo_create_layout(cr);
	pango_layout_set_font_description(layout, app->font);
	pango_layout_set_text(layout, text, (int)len);
	return layout;
}

static void
buffer_release(void *data, struct wl_buffer *buffer)
{
	(void)data;
	wl_buffer_destroy(buffer);
}

static const struct wl_buffer_listener buffer_listener = {
	.release = buffer_release,
};

static void
draw(struct app *app)
{
	if (!app->configured)
		return;
	int w = WIDTH, h = app->height, stride = w * 4;
	size_t size = (size_t)stride * h;
	int fd = memfd_create("fluxland-run", MFD_CLOEXEC);
	if (fd < 0 || ftruncate(fd, (off_t)size) < 0)
		return;
	void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		close(fd);
		return;
	}
	cairo_surface_t *surface = cairo_image_surface_create_for_data(data,
		CAIRO_FORMAT_ARGB32, w, h, stride);
	cairo_t *cr = cairo_create(surface);
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_paint(cr);

	/* The line, scrolled to keep the cursor in */
	PangoRectangle logical;
	PangoLayout *to_cursor = make_layout(cr, app, app->text, app->cursor);
	pango_layout_get_pixel_extents(to_cursor, NULL, &logical);
	int cursor_x = logical.width;
	g_object_unref(to_cursor);
	int scroll = cursor_x > w - 4 ? cursor_x - (w - 4) : 0;
	PangoLayout *layout = make_layout(cr, app, app->text, app->len);
	pango_layout_get_pixel_extents(layout, NULL, &logical);
	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_move_to(cr, 2 - scroll, (h - logical.height) / 2.0);
	pango_cairo_show_layout(cr, layout);
	g_object_unref(layout);
	cairo_rectangle(cr, 2 + cursor_x - scroll, 2, 1, h - 4);
	cairo_fill(cr);
	cairo_destroy(cr);
	cairo_surface_destroy(surface);

	struct wl_shm_pool *pool = wl_shm_create_pool(app->shm, fd, (int)size);
	struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, w, h,
		stride, WL_SHM_FORMAT_ARGB8888);
	wl_shm_pool_destroy(pool);
	munmap(data, size);
	close(fd);
	wl_buffer_add_listener(buffer, &buffer_listener, NULL);
	wl_surface_attach(app->surface, buffer, 0, 0);
	wl_surface_damage_buffer(app->surface, 0, 0, w, h);
	wl_surface_commit(app->surface);
}

/* --- Editing --- */

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
delete_range(struct app *a, size_t from, size_t to)
{
	memmove(a->text + from, a->text + to, a->len - to + 1);
	a->len -= to - from;
	a->cursor = from;
}

static void
forget_candidates(struct app *a)
{
	wm_run_free_list(a->cands, a->ncands);
	a->cands = NULL;
	a->ncands = a->cand_next = 0;
}

/* Replace text[from..cursor) with s */
static void
replace_word(struct app *a, size_t from, const char *s)
{
	size_t n = strlen(s), tail = a->len - a->cursor;
	if (from + n + tail >= sizeof(a->text))
		return;
	memmove(a->text + from + n, a->text + a->cursor, tail + 1);
	memcpy(a->text + from, s, n);
	a->len = from + n + tail;
	a->cursor = from + n;
}

/*
 * Tab: finish the word before the cursor as far as all the names that
 * could finish it agree, and with nothing more to add, go through them one
 * by one on the next presses
 */
static void
complete(struct app *a)
{
	if (a->ncands > 1) {
		replace_word(a, a->cand_start, a->cands[a->cand_next]);
		a->cand_next = (a->cand_next + 1) % a->ncands;
		return;
	}
	char **cands;
	size_t start;
	size_t n = wm_run_candidates(a->text, a->cursor, &cands, &start);
	if (n == 0) {
		wm_run_free_list(cands, 0);
		return;
	}
	char *common = wm_run_common_prefix(cands, n);
	if (common && strlen(common) > a->cursor - start) {
		replace_word(a, start, common);
		if (n == 1) {	/* done */
			wm_run_free_list(cands, n);
			cands = NULL;
			n = 0;
		}
	}
	free(common);
	if (n == 1) {
		wm_run_free_list(cands, n);
		cands = NULL;
		n = 0;
	}
	a->cands = cands;
	a->ncands = n;
	a->cand_start = start;
	a->cand_next = 0;
}

/* Up and Down: the lines run before */
static void
browse_history(struct app *a, int direction)
{
	size_t count = a->history.count;
	if (direction < 0) {
		if (a->history_pos == 0)
			return;
		if (a->history_pos == count) {
			free(a->draft);
			a->draft = strdup(a->text);
		}
		a->history_pos--;
	} else {
		if (a->history_pos >= count)
			return;
		a->history_pos++;
	}
	const char *line = a->history_pos < count ?
		a->history.items[a->history_pos] : (a->draft ? a->draft : "");
	snprintf(a->text, sizeof(a->text), "%s", line);
	a->len = a->cursor = strlen(a->text);
}

/* Return: remember the line and run it as a shell command, apart from us */
static void
run_line(struct app *a)
{
	char *start = a->text;
	while (*start == ' ' || *start == '\t')
		start++;
	size_t n = strlen(start);
	while (n > 0 && (start[n - 1] == ' ' || start[n - 1] == '\t'))
		start[--n] = '\0';
	if (n > 0) {
		wm_run_history_add(&a->history, start);
		wm_run_history_save(&a->history, a->history_path);
		pid_t pid = fork();
		if (pid == 0) {
			setsid();
			execl("/bin/sh", "sh", "-c", start, (char *)NULL);
			_exit(127);
		}
	}
	a->closing = true;
}

/* One key press (or repeat): true when the line changed */
static void
handle_key(struct app *a, xkb_keycode_t code)
{
	xkb_keysym_t sym = xkb_state_key_get_one_sym(a->xkb_state, code);
	char utf8[16] = "";
	xkb_state_key_get_utf8(a->xkb_state, code, utf8, sizeof(utf8));
	bool ctrl = xkb_state_mod_name_is_active(a->xkb_state,
		XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE) > 0;
	bool alt = xkb_state_mod_name_is_active(a->xkb_state,
		XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE) > 0;
	bool logo = xkb_state_mod_name_is_active(a->xkb_state,
		XKB_MOD_NAME_LOGO, XKB_STATE_MODS_EFFECTIVE) > 0;

	if (sym != XKB_KEY_Tab && a->ncands > 0)
		forget_candidates(a);
	switch (sym) {
	case XKB_KEY_Return:
	case XKB_KEY_KP_Enter:
		run_line(a);
		return;
	case XKB_KEY_Escape:
		a->closing = true;
		return;
	case XKB_KEY_Tab:
		complete(a);
		break;
	case XKB_KEY_Up:
	case XKB_KEY_KP_Up:
		browse_history(a, -1);
		break;
	case XKB_KEY_Down:
	case XKB_KEY_KP_Down:
		browse_history(a, 1);
		break;
	case XKB_KEY_BackSpace:
		if (a->cursor > 0)
			delete_range(a, utf8_prev(a->text, a->cursor),
				a->cursor);
		break;
	case XKB_KEY_Delete:
	case XKB_KEY_KP_Delete:
		if (a->cursor < a->len)
			delete_range(a, a->cursor,
				utf8_next(a->text, a->len, a->cursor));
		break;
	case XKB_KEY_Left:
	case XKB_KEY_KP_Left:
		a->cursor = utf8_prev(a->text, a->cursor);
		break;
	case XKB_KEY_Right:
	case XKB_KEY_KP_Right:
		a->cursor = utf8_next(a->text, a->len, a->cursor);
		break;
	case XKB_KEY_Home:
	case XKB_KEY_KP_Home:
		a->cursor = 0;
		break;
	case XKB_KEY_End:
	case XKB_KEY_KP_End:
		a->cursor = a->len;
		break;
	default:
		/* Ctrl+A, E, U and K as a shell has them */
		if (ctrl && (sym == XKB_KEY_a || sym == XKB_KEY_A)) {
			a->cursor = 0;
		} else if (ctrl && (sym == XKB_KEY_e || sym == XKB_KEY_E)) {
			a->cursor = a->len;
		} else if (ctrl && (sym == XKB_KEY_u || sym == XKB_KEY_U)) {
			delete_range(a, 0, a->cursor);
		} else if (ctrl && (sym == XKB_KEY_k || sym == XKB_KEY_K)) {
			delete_range(a, a->cursor, a->len);
		} else if (!ctrl && !alt && !logo &&
			   (unsigned char)utf8[0] >= 0x20 &&
			   utf8[0] != 0x7f) {
			size_t n = strlen(utf8);
			if (a->len + n < sizeof(a->text)) {
				memmove(a->text + a->cursor + n,
					a->text + a->cursor,
					a->len - a->cursor + 1);
				memcpy(a->text + a->cursor, utf8, n);
				a->len += n;
				a->cursor += n;
			}
		}
		break;
	}
	draw(a);
}

/* --- Key repeat --- */

static void
set_timer(struct app *a, int first_ms, int interval_ms)
{
	struct itimerspec spec = {
		.it_value = { first_ms / 1000, (first_ms % 1000) * 1000000L },
		.it_interval = { interval_ms / 1000,
			(interval_ms % 1000) * 1000000L },
	};
	timerfd_settime(a->timer_fd, 0, &spec, NULL);
}

static void
stop_repeat(struct app *a)
{
	a->held = XKB_KEYCODE_INVALID;
	set_timer(a, 0, 0);
}

/* --- wl_keyboard --- */

static void
keyboard_keymap(void *data, struct wl_keyboard *kb, uint32_t format,
	int32_t fd, uint32_t size)
{
	struct app *a = data;
	(void)kb;
	if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
		close(fd);
		return;
	}
	char *map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	if (map == MAP_FAILED)
		return;
	struct xkb_keymap *keymap = xkb_keymap_new_from_buffer(a->xkb_context,
		map, size - 1, XKB_KEYMAP_FORMAT_TEXT_V1,
		XKB_KEYMAP_COMPILE_NO_FLAGS);
	munmap(map, size);
	if (!keymap)
		return;
	xkb_state_unref(a->xkb_state);
	xkb_keymap_unref(a->keymap);
	a->keymap = keymap;
	a->xkb_state = xkb_state_new(keymap);
}

static void
keyboard_enter(void *data, struct wl_keyboard *kb, uint32_t serial,
	struct wl_surface *surface, struct wl_array *keys)
{
	(void)data; (void)kb; (void)serial; (void)surface; (void)keys;
}

static void
keyboard_leave(void *data, struct wl_keyboard *kb, uint32_t serial,
	struct wl_surface *surface)
{
	(void)kb; (void)serial; (void)surface;
	stop_repeat(data);
}

static void
keyboard_key(void *data, struct wl_keyboard *kb, uint32_t serial,
	uint32_t time, uint32_t key, uint32_t state)
{
	struct app *a = data;
	(void)kb; (void)serial; (void)time;
	if (!a->xkb_state)
		return;
	xkb_keycode_t code = key + 8;
	if (state == WL_KEYBOARD_KEY_STATE_RELEASED) {
		if (a->held == code)
			stop_repeat(a);
		return;
	}
	handle_key(a, code);
	if (a->repeat_rate > 0 && !a->closing &&
	    xkb_keymap_key_repeats(a->keymap, code)) {
		a->held = code;
		set_timer(a, a->repeat_delay, 1000 / a->repeat_rate);
	} else {
		stop_repeat(a);
	}
}

static void
keyboard_modifiers(void *data, struct wl_keyboard *kb, uint32_t serial,
	uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group)
{
	struct app *a = data;
	(void)kb; (void)serial;
	if (a->xkb_state)
		xkb_state_update_mask(a->xkb_state, depressed, latched, locked,
			0, 0, group);
}

static void
keyboard_repeat_info(void *data, struct wl_keyboard *kb, int32_t rate,
	int32_t delay)
{
	struct app *a = data;
	(void)kb;
	a->repeat_rate = rate;
	a->repeat_delay = delay;
}

static const struct wl_keyboard_listener keyboard_listener = {
	.keymap = keyboard_keymap,
	.enter = keyboard_enter,
	.leave = keyboard_leave,
	.key = keyboard_key,
	.modifiers = keyboard_modifiers,
	.repeat_info = keyboard_repeat_info,
};

/* --- wl_seat, registry, xdg-shell --- */

static void
seat_capabilities(void *data, struct wl_seat *seat, uint32_t caps)
{
	struct app *a = data;
	if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !a->keyboard) {
		a->keyboard = wl_seat_get_keyboard(seat);
		wl_keyboard_add_listener(a->keyboard, &keyboard_listener, a);
	}
}

static void
seat_name(void *data, struct wl_seat *seat, const char *name)
{
	(void)data; (void)seat; (void)name;
}

static const struct wl_seat_listener seat_listener = {
	.capabilities = seat_capabilities,
	.name = seat_name,
};

static void
wm_base_ping(void *data, struct xdg_wm_base *wm_base, uint32_t serial)
{
	(void)data;
	xdg_wm_base_pong(wm_base, serial);
}

static const struct xdg_wm_base_listener wm_base_listener = {
	.ping = wm_base_ping,
};

static void
registry_global(void *data, struct wl_registry *registry, uint32_t name,
	const char *interface, uint32_t version)
{
	struct app *a = data;
	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		a->compositor = wl_registry_bind(registry, name,
			&wl_compositor_interface, 4);
	} else if (strcmp(interface, wl_shm_interface.name) == 0) {
		a->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
	} else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
		a->wm_base = wl_registry_bind(registry, name,
			&xdg_wm_base_interface, 1);
		xdg_wm_base_add_listener(a->wm_base, &wm_base_listener, a);
	} else if (strcmp(interface,
			zxdg_decoration_manager_v1_interface.name) == 0) {
		a->deco_manager = wl_registry_bind(registry, name,
			&zxdg_decoration_manager_v1_interface, 1);
	} else if (strcmp(interface, wl_seat_interface.name) == 0 &&
		   !a->seat) {
		a->seat = wl_registry_bind(registry, name, &wl_seat_interface,
			version < 5 ? version : 5);
		wl_seat_add_listener(a->seat, &seat_listener, a);
	}
}

static void
registry_global_remove(void *data, struct wl_registry *registry,
	uint32_t name)
{
	(void)data; (void)registry; (void)name;
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove,
};

static void
xdg_surface_configure(void *data, struct xdg_surface *surface,
	uint32_t serial)
{
	struct app *a = data;
	xdg_surface_ack_configure(surface, serial);
	if (!a->configured) {
		a->configured = true;
		draw(a);
	}
}

static const struct xdg_surface_listener xdg_surface_listener = {
	.configure = xdg_surface_configure,
};

static void
toplevel_configure(void *data, struct xdg_toplevel *toplevel, int32_t width,
	int32_t height, struct wl_array *states)
{
	(void)data; (void)toplevel; (void)width; (void)height; (void)states;
}

static void
toplevel_close(void *data, struct xdg_toplevel *toplevel)
{
	struct app *a = data;
	(void)toplevel;
	a->closing = true;
}

static const struct xdg_toplevel_listener toplevel_listener = {
	.configure = toplevel_configure,
	.close = toplevel_close,
};

/* --- Main --- */

static void
usage(void)
{
	fprintf(stderr, "usage: fluxland-run [-f FONT]\n"
		"  a window to type a program in; FONT is a Pango font\n"
		"  description (default \"%s\")\n", DEFAULT_FONT);
}

int
main(int argc, char **argv)
{
	const char *font_name = DEFAULT_FONT;
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
			font_name = argv[++i];
		} else {
			usage();
			return strcmp(argv[i], "-h") == 0 ? 0 : 2;
		}
	}

	struct app app = { .repeat_rate = 25, .repeat_delay = 600,
		.held = XKB_KEYCODE_INVALID };
	app.display = wl_display_connect(NULL);
	if (!app.display) {
		fprintf(stderr, "fluxland-run: cannot connect to Wayland\n");
		return 1;
	}
	app.xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	app.timer_fd = timerfd_create(CLOCK_MONOTONIC,
		TFD_CLOEXEC | TFD_NONBLOCK);
	struct wl_registry *registry = wl_display_get_registry(app.display);
	wl_registry_add_listener(registry, &registry_listener, &app);
	wl_display_roundtrip(app.display);
	wl_display_roundtrip(app.display);
	if (!app.compositor || !app.shm || !app.wm_base) {
		fprintf(stderr, "fluxland-run: the compositor lacks "
			"wl_compositor, wl_shm or xdg-shell\n");
		return 1;
	}

	/* The line: a font's height and a little room */
	app.font = pango_font_description_from_string(font_name);
	{
		cairo_surface_t *s = cairo_image_surface_create(
			CAIRO_FORMAT_ARGB32, 1, 1);
		cairo_t *cr = cairo_create(s);
		PangoLayout *layout = make_layout(cr, &app, "Mg", 2);
		PangoRectangle logical;
		pango_layout_get_pixel_extents(layout, NULL, &logical);
		app.height = logical.height + 4;
		g_object_unref(layout);
		cairo_destroy(cr);
		cairo_surface_destroy(s);
	}

	app.history_path = wm_run_history_path();
	wm_run_history_load(&app.history, app.history_path);
	app.history_pos = app.history.count;

	app.surface = wl_compositor_create_surface(app.compositor);
	app.xdg_surface = xdg_wm_base_get_xdg_surface(app.wm_base,
		app.surface);
	xdg_surface_add_listener(app.xdg_surface, &xdg_surface_listener, &app);
	app.toplevel = xdg_surface_get_toplevel(app.xdg_surface);
	xdg_toplevel_add_listener(app.toplevel, &toplevel_listener, &app);
	xdg_toplevel_set_title(app.toplevel, "Run program");
	xdg_toplevel_set_app_id(app.toplevel, "fluxland-run");
	/* A window of one size */
	xdg_toplevel_set_min_size(app.toplevel, WIDTH, app.height);
	xdg_toplevel_set_max_size(app.toplevel, WIDTH, app.height);
	if (app.deco_manager) {
		app.deco = zxdg_decoration_manager_v1_get_toplevel_decoration(
			app.deco_manager, app.toplevel);
		zxdg_toplevel_decoration_v1_set_mode(app.deco,
			ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
	}
	wl_surface_commit(app.surface);

	while (!app.closing) {
		wl_display_flush(app.display);
		struct pollfd fds[2] = {
			{ wl_display_get_fd(app.display), POLLIN, 0 },
			{ app.timer_fd, POLLIN, 0 },
		};
		while (wl_display_prepare_read(app.display) != 0)
			wl_display_dispatch_pending(app.display);
		wl_display_flush(app.display);
		if (poll(fds, 2, -1) < 0 && errno != EINTR) {
			wl_display_cancel_read(app.display);
			break;
		}
		if (fds[0].revents & POLLIN) {
			if (wl_display_read_events(app.display) < 0)
				break;
		} else {
			wl_display_cancel_read(app.display);
		}
		if (wl_display_dispatch_pending(app.display) < 0)
			break;
		if (fds[1].revents & POLLIN) {
			uint64_t expirations;
			if (read(app.timer_fd, &expirations,
				 sizeof(expirations)) > 0 &&
			    app.held != XKB_KEYCODE_INVALID && !app.closing) {
				for (uint64_t i = 0; i < expirations && i < 8;
				     i++)
					handle_key(&app, app.held);
			}
		}
	}
	return 0;
}
