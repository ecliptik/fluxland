/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * eis.c - Input emulation for X11 programs (an EIS server for XWayland)
 */

#define _GNU_SOURCE

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <wlr/util/log.h>

#include "eis.h"
#include "server.h"

#ifndef WM_HAS_EIS

void
wm_eis_init(struct wm_server *server)
{
	(void)server;
}

void
wm_eis_finish(struct wm_server *server)
{
	(void)server;
}

void
wm_eis_session_lock_changed(struct wm_server *server)
{
	(void)server;
}

void
wm_eis_layout_changed(struct wm_server *server)
{
	(void)server;
}

#else /* WM_HAS_EIS */

#include <libeis.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/interfaces/wlr_pointer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>

#include "config.h"
#include "idle.h"
#include "input.h"
#include "keyboard.h"
#include "menu.h"
#include "menu_parse.h"
#include "session_lock.h"

#define EIS_SEAT_NAME "fluxland"
#define EIS_HELD_MAX 16
/* libei's logical scroll step; a Wayland wheel step is 15 */
#define EIS_SCROLL_STEP 120.0
#define WAYLAND_SCROLL_STEP 15.0

/* A program's answer, kept for as long as fluxland runs */
struct wm_eis_decision {
	struct wl_list link;
	char *name;
	bool allowed;
};

struct wm_eis_client {
	struct wl_list link;
	struct wm_eis *eis;
	struct eis_client *client;
	char *name;
	struct eis_seat *seat;
	struct eis_device *pointer;
	struct eis_device *absolute;
	struct eis_device *keyboard;
	bool waiting;		/* for the user's answer */
	bool connected;
	bool paused;		/* the session is locked */
	/* Down, to be released when it stops or goes: a program that exits
	 * with a key held must not leave it so */
	uint32_t buttons[EIS_HELD_MAX];
	int button_count;
	uint32_t keys[EIS_HELD_MAX];
	int key_count;
};

struct wm_eis {
	struct wm_server *server;
	struct eis *eis;
	struct wl_event_source *source;
	char socket_name[64];

	/* The devices everything arrives through */
	struct wlr_pointer pointer;
	struct wlr_keyboard keyboard;
	bool devices_ready;

	struct wl_list clients;		/* wm_eis_client.link */
	struct wl_list decisions;	/* wm_eis_decision.link */

	/* The question that is asked (one at a time) */
	struct wm_menu *prompt_menu;
	struct wm_eis_client *prompt_client;
	bool prompt_answered;
	struct wl_event_source *prompt_idle;
};

static const struct wlr_pointer_impl pointer_impl = {
	.name = "fluxland-eis-pointer",
};

static const struct wlr_keyboard_impl keyboard_impl = {
	.name = "fluxland-eis-keyboard",
};

/* --- Names and decisions --- */

/* Is name in list (names separated by spaces, tabs or commas)? */
static bool
name_in_list(const char *list, const char *name)
{
	if (!list || !name || !*name)
		return false;
	size_t len = strlen(name);
	const char *p = list;
	while (*p) {
		while (*p == ' ' || *p == '\t' || *p == ',')
			p++;
		const char *start = p;
		while (*p && *p != ' ' && *p != '\t' && *p != ',')
			p++;
		if ((size_t)(p - start) == len && !strncmp(start, name, len))
			return true;
	}
	return false;
}

static struct wm_eis_decision *
decision_find(struct wm_eis *eis, const char *name)
{
	struct wm_eis_decision *d;
	wl_list_for_each(d, &eis->decisions, link) {
		if (!strcmp(d->name, name))
			return d;
	}
	return NULL;
}

static void
decision_set(struct wm_eis *eis, const char *name, bool allowed)
{
	struct wm_eis_decision *d = decision_find(eis, name);
	if (!d) {
		d = calloc(1, sizeof(*d));
		if (!d)
			return;
		d->name = strdup(name);
		if (!d->name) {
			free(d);
			return;
		}
		wl_list_insert(&eis->decisions, &d->link);
	}
	d->allowed = allowed;
}

/* --- Emulated input --- */

/* When it happened; the events that only start or stop have no time */
static uint32_t
time_msec(struct eis_event *event)
{
	switch (eis_event_get_type(event)) {
	case EIS_EVENT_DEVICE_START_EMULATING:
	case EIS_EVENT_DEVICE_STOP_EMULATING:
		return (uint32_t)(eis_now(eis_client_get_context(
			eis_event_get_client(event))) / 1000);
	default:
		return (uint32_t)(eis_event_get_time(event) / 1000);
	}
}

static void
emit_motion(struct wm_eis *eis, uint32_t time, double dx, double dy)
{
	struct wlr_pointer_motion_event event = {
		.pointer = &eis->pointer,
		.time_msec = time,
		.delta_x = dx, .delta_y = dy,
		.unaccel_dx = dx, .unaccel_dy = dy,
	};
	wl_signal_emit_mutable(&eis->pointer.events.motion, &event);
}

/* (x, y) in layout coordinates: the cursor wants them as 0..1 of the layout */
static void
emit_motion_absolute(struct wm_eis *eis, uint32_t time, double x, double y)
{
	struct wlr_box box;
	wlr_output_layout_get_box(eis->server->output_layout, NULL, &box);
	if (box.width <= 0 || box.height <= 0)
		return;
	double nx = (x - box.x) / box.width;
	double ny = (y - box.y) / box.height;
	struct wlr_pointer_motion_absolute_event event = {
		.pointer = &eis->pointer,
		.time_msec = time,
		.x = nx < 0 ? 0 : nx > 1 ? 1 : nx,
		.y = ny < 0 ? 0 : ny > 1 ? 1 : ny,
	};
	wl_signal_emit_mutable(&eis->pointer.events.motion_absolute, &event);
}

static void
emit_button(struct wm_eis *eis, uint32_t time, uint32_t button, bool pressed)
{
	struct wlr_pointer_button_event event = {
		.pointer = &eis->pointer,
		.time_msec = time,
		.button = button,
		.state = pressed ? WL_POINTER_BUTTON_STATE_PRESSED :
			WL_POINTER_BUTTON_STATE_RELEASED,
	};
	wlr_pointer_notify_button(&eis->pointer, &event);
}

static void
emit_axis(struct wm_eis *eis, uint32_t time, enum wl_pointer_axis axis,
	enum wl_pointer_axis_source source, double delta, int32_t discrete)
{
	struct wlr_pointer_axis_event event = {
		.pointer = &eis->pointer,
		.time_msec = time,
		.source = source,
		.orientation = axis,
		.relative_direction = WL_POINTER_AXIS_RELATIVE_DIRECTION_IDENTICAL,
		.delta = delta,
		.delta_discrete = discrete,
	};
	wl_signal_emit_mutable(&eis->pointer.events.axis, &event);
}

static void
emit_frame(struct wm_eis *eis)
{
	wl_signal_emit_mutable(&eis->pointer.events.frame, &eis->pointer);
}

static void
emit_key(struct wm_eis *eis, uint32_t time, uint32_t key, bool pressed)
{
	struct wlr_keyboard_key_event event = {
		.time_msec = time,
		.keycode = key,
		.update_state = true,
		.state = pressed ? WL_KEYBOARD_KEY_STATE_PRESSED :
			WL_KEYBOARD_KEY_STATE_RELEASED,
	};
	wlr_keyboard_notify_key(&eis->keyboard, &event);
}

/* --- What is held down --- */

static void
held_update(uint32_t *list, int *count, uint32_t code, bool pressed)
{
	for (int i = 0; i < *count; i++) {
		if (list[i] == code) {
			if (!pressed)
				list[i] = list[--*count];
			return;
		}
	}
	if (pressed && *count < EIS_HELD_MAX)
		list[(*count)++] = code;
}

static void
release_held(struct wm_eis_client *c, uint32_t time)
{
	struct wm_eis *eis = c->eis;
	bool buttons = c->button_count > 0;
	while (c->button_count > 0)
		emit_button(eis, time, c->buttons[--c->button_count], false);
	if (buttons)
		emit_frame(eis);
	while (c->key_count > 0)
		emit_key(eis, time, c->keys[--c->key_count], false);
}

/* --- Devices of a client --- */

static void
device_remove(struct eis_device **slot)
{
	if (!*slot)
		return;
	eis_device_remove(*slot);
	eis_device_unref(*slot);
	*slot = NULL;
}

/* One region for each output: where absolute motion may land */
static void
add_regions(struct wm_eis *eis, struct eis_device *device)
{
	struct wlr_output_layout *layout = eis->server->output_layout;
	struct wlr_output_layout_output *lo;
	wl_list_for_each(lo, &layout->outputs, link) {
		if (!lo->output->enabled)
			continue;
		struct wlr_box box;
		wlr_output_layout_get_box(layout, lo->output, &box);
		if (box.width <= 0 || box.height <= 0)
			continue;
		struct eis_region *region = eis_device_new_region(device);
		if (!region)
			continue;
		/* X coordinates start at the layout's origin */
		eis_region_set_offset(region, box.x < 0 ? 0 : (uint32_t)box.x,
			box.y < 0 ? 0 : (uint32_t)box.y);
		eis_region_set_size(region, (uint32_t)box.width,
			(uint32_t)box.height);
		eis_region_add(region);
		eis_region_unref(region);
	}
}

static struct eis_device *
device_add(struct wm_eis_client *c, const char *name,
	const enum eis_device_capability *caps, int cap_count, bool regions)
{
	struct eis_device *device = eis_seat_new_device(c->seat);
	if (!device)
		return NULL;
	eis_device_configure_type(device, EIS_DEVICE_TYPE_VIRTUAL);
	eis_device_configure_name(device, name);
	for (int i = 0; i < cap_count; i++)
		eis_device_configure_capability(device, caps[i]);
	if (regions)
		add_regions(c->eis, device);
	eis_device_add(device);
	if (!c->paused)
		eis_device_resume(device);
	return device;
}

static void
absolute_device_add(struct wm_eis_client *c)
{
	static const enum eis_device_capability caps[] = {
		EIS_DEVICE_CAP_POINTER_ABSOLUTE, EIS_DEVICE_CAP_BUTTON,
		EIS_DEVICE_CAP_SCROLL,
	};
	/* With no output there is nowhere to point */
	struct wlr_output_layout *layout = c->eis->server->output_layout;
	if (wl_list_empty(&layout->outputs))
		return;
	c->absolute = device_add(c, "fluxland absolute pointer", caps, 3,
		true);
}

/* The devices a client's seat binding asks for */
static void
devices_sync(struct wm_eis_client *c, struct eis_event *event)
{
	static const enum eis_device_capability pointer_caps[] = {
		EIS_DEVICE_CAP_POINTER, EIS_DEVICE_CAP_BUTTON,
		EIS_DEVICE_CAP_SCROLL,
	};
	static const enum eis_device_capability keyboard_caps[] = {
		EIS_DEVICE_CAP_KEYBOARD,
	};
	bool buttons = eis_event_seat_has_capability(event,
		EIS_DEVICE_CAP_BUTTON) &&
		eis_event_seat_has_capability(event, EIS_DEVICE_CAP_SCROLL);
	bool pointer = buttons && eis_event_seat_has_capability(event,
		EIS_DEVICE_CAP_POINTER);
	bool absolute = buttons && eis_event_seat_has_capability(event,
		EIS_DEVICE_CAP_POINTER_ABSOLUTE);
	bool keyboard = eis_event_seat_has_capability(event,
		EIS_DEVICE_CAP_KEYBOARD);

	if (pointer && !c->pointer)
		c->pointer = device_add(c, "fluxland pointer", pointer_caps, 3,
			false);
	else if (!pointer)
		device_remove(&c->pointer);

	if (absolute && !c->absolute)
		absolute_device_add(c);
	else if (!absolute)
		device_remove(&c->absolute);

	if (keyboard && !c->keyboard)
		c->keyboard = device_add(c, "fluxland keyboard", keyboard_caps,
			1, false);
	else if (!keyboard)
		device_remove(&c->keyboard);
}

static void
client_set_paused(struct wm_eis_client *c, bool paused)
{
	if (c->paused == paused)
		return;
	c->paused = paused;
	struct eis_device *devices[] = { c->pointer, c->absolute,
		c->keyboard };
	for (size_t i = 0; i < sizeof(devices) / sizeof(devices[0]); i++) {
		if (!devices[i])
			continue;
		if (paused)
			eis_device_pause(devices[i]);
		else
			eis_device_resume(devices[i]);
	}
}

/* --- Clients --- */

static void prompt_next(struct wm_eis *eis);

static void
client_free(struct wm_eis_client *c)
{
	struct wm_eis *eis = c->eis;
	release_held(c, 0);
	if (eis->prompt_client == c) {
		/* Nobody is left to answer for */
		eis->prompt_client = NULL;
		if (eis->prompt_menu && eis->server->custom_menu ==
		    eis->prompt_menu)
			wm_menu_hide_all(eis->server);
	}
	wl_list_remove(&c->link);
	eis_client_set_user_data(c->client, NULL);
	device_remove(&c->pointer);
	device_remove(&c->absolute);
	device_remove(&c->keyboard);
	if (c->seat)
		eis_seat_unref(c->seat);
	eis_client_unref(c->client);
	free(c->name);
	free(c);
}

/* Let it talk to the compositor: one seat with everything on it */
static void
client_accept(struct wm_eis_client *c)
{
	eis_client_connect(c->client);
	c->seat = eis_client_new_seat(c->client, EIS_SEAT_NAME);
	if (!c->seat)
		return;
	eis_seat_configure_capability(c->seat, EIS_DEVICE_CAP_POINTER);
	eis_seat_configure_capability(c->seat,
		EIS_DEVICE_CAP_POINTER_ABSOLUTE);
	eis_seat_configure_capability(c->seat, EIS_DEVICE_CAP_BUTTON);
	eis_seat_configure_capability(c->seat, EIS_DEVICE_CAP_SCROLL);
	eis_seat_configure_capability(c->seat, EIS_DEVICE_CAP_KEYBOARD);
	eis_seat_add(c->seat);
	c->connected = true;
	c->paused = wm_session_is_locked(c->eis->server);
}

static void
client_refuse(struct wm_eis_client *c)
{
	eis_client_disconnect(c->client);
	client_free(c);
}

/* --- The question --- */

static void
answer(struct wm_eis *eis, bool allowed, bool always)
{
	struct wm_eis_client *c = eis->prompt_client;
	if (!c)
		return;
	eis->prompt_answered = true;
	eis->prompt_client = NULL;
	c->waiting = false;
	decision_set(eis, c->name, allowed);
	wlr_log(WLR_INFO, "eis: %s may%s emulate input", c->name,
		allowed ? "" : " not");
	if (allowed && always) {
		struct wm_config *config = eis->server->config;
		const char *old = config && config->eis_allow ?
			config->eis_allow : "";
		size_t len = strlen(old) + strlen(c->name) + 2;
		char *list = malloc(len);
		if (list) {
			snprintf(list, len, "%s%s%s", old, *old ? " " : "",
				c->name);
			if (config_set_resource(config,
			    "session.xwayland.inputEmulation.allow", list) < 0)
				wlr_log(WLR_ERROR, "eis: can't save %s to the "
					"init file", c->name);
			free(list);
		}
	}
	if (allowed)
		client_accept(c);
	else
		client_refuse(c);
}

static void
prompt_allow(struct wm_server *server, void *data)
{
	(void)server;
	answer(data, true, false);
}

static void
prompt_always(struct wm_server *server, void *data)
{
	(void)server;
	answer(data, true, true);
}

static void
prompt_deny(struct wm_server *server, void *data)
{
	(void)server;
	answer(data, false, false);
}

/* After the menu is gone, and whatever item was chosen has run */
static void
prompt_idle_cb(void *data)
{
	struct wm_eis *eis = data;
	eis->prompt_idle = NULL;
	if (!eis->prompt_answered && eis->prompt_client)
		answer(eis, false, false);
	eis->prompt_answered = false;
	prompt_next(eis);
}

/*
 * The menu went away: if it wasn't by an item it is a no.  A menu hides
 * before its item runs, so that is for later to tell, not here (nor is
 * this the place to start the next question, in the middle of the menus
 * being taken down).
 */
static void
prompt_closed(struct wm_menu *menu, void *data)
{
	struct wm_eis *eis = data;
	if (eis->prompt_menu != menu)
		return;
	eis->prompt_menu = NULL;
	if (!eis->prompt_idle && eis->server->wl_event_loop)
		eis->prompt_idle = wl_event_loop_add_idle(
			eis->server->wl_event_loop, prompt_idle_cb, eis);
}

static void
prompt_add_item(struct wm_eis *eis, struct wm_menu *menu, const char *label,
	void (*callback)(struct wm_server *, void *))
{
	struct wm_menu_item *item = menu_item_create(WM_MENU_CALLBACK, label);
	if (!item)
		return;
	item->callback = callback;
	item->callback_data = eis;
	wl_list_insert(menu->items.prev, &item->link);
}

static void
prompt_next(struct wm_eis *eis)
{
	struct wm_server *server = eis->server;
	if (eis->prompt_menu || eis->prompt_idle ||
	    wm_session_is_locked(server))
		return;
	struct wm_eis_client *c = NULL, *it;
	wl_list_for_each(it, &eis->clients, link) {
		if (it->waiting) {
			c = it;
			break;
		}
	}
	if (!c)
		return;

	char title[160];
	snprintf(title, sizeof(title), "Input control: %.100s", c->name);
	struct wm_menu *menu = menu_create(server, title);
	if (!menu) {
		client_refuse(c);
		return;
	}
	prompt_add_item(eis, menu, "Allow", prompt_allow);
	prompt_add_item(eis, menu, "Always allow", prompt_always);
	prompt_add_item(eis, menu, "Deny", prompt_deny);
	menu->on_close = prompt_closed;
	menu->on_close_data = eis;

	/* In the middle of the head the pointer is on */
	struct wlr_box box = { 0 };
	struct wlr_output *output = wlr_output_layout_output_at(
		server->output_layout, server->cursor->x, server->cursor->y);
	if (output)
		wlr_output_layout_get_box(server->output_layout, output, &box);
	else
		wlr_output_layout_get_box(server->output_layout, NULL, &box);

	wm_menu_hide_all(server);
	server->custom_menu = menu;
	eis->prompt_menu = menu;
	eis->prompt_client = c;
	eis->prompt_answered = false;
	wlr_log(WLR_INFO, "eis: asking about %s", c->name);
	wm_menu_show(menu, box.x + box.width / 2 - 100,
		box.y + box.height / 3);
}

/* --- Events --- */

/*
 * Is a process command line (NUL separated, as /proc has it) the one of
 * the Xwayland serving this display?  wlroots starts it through a process
 * that goes at once, and a socket pair's credentials are the compositor's,
 * so what it was started as is all there is to tell it by.
 */
static bool
cmdline_is_xwayland(const char *cmdline, size_t len, const char *display)
{
	if (!display || !*display)
		return false;
	const char *end = cmdline + len;
	size_t first = strnlen(cmdline, len);
	if (first >= len)
		return false;
	const char *base = memrchr(cmdline, '/', first);
	base = base ? base + 1 : cmdline;
	if (strcmp(base, "Xwayland") != 0)
		return false;
	const char *arg = cmdline + first + 1;
	return arg < end && strnlen(arg, (size_t)(end - arg)) ==
		strlen(display) && !strncmp(arg, display, strlen(display));
}

static bool
pid_is_xwayland(pid_t pid, const char *display)
{
	char path[64], buf[512];
	snprintf(path, sizeof(path), "/proc/%d/cmdline", (int)pid);
	FILE *f = fopen(path, "re");
	if (!f)
		return false;
	size_t len = fread(buf, 1, sizeof(buf), f);
	fclose(f);
	return cmdline_is_xwayland(buf, len, display);
}

static void
client_connect_requested(struct wm_eis *eis, struct eis_client *client)
{
	struct wm_server *server = eis->server;

	/* Xwayland speaks for the X11 programs: nothing else is one */
	pid_t pid = eis_backend_socket_get_client_pid(client);
	if (!server->xwayland || pid <= 0 ||
	    !pid_is_xwayland(pid, server->xwayland->display_name)) {
		wlr_log(WLR_INFO, "eis: refusing %s (pid %d is not XWayland)",
			eis_client_get_name(client), (int)pid);
		eis_client_disconnect(client);
		return;
	}

	struct wm_eis_client *c = calloc(1, sizeof(*c));
	const char *name = eis_client_get_name(client);
	if (!c) {
		eis_client_disconnect(client);
		return;
	}
	c->eis = eis;
	c->client = eis_client_ref(client);
	c->name = strdup(name ? name : "");
	eis_client_set_user_data(client, c);
	wl_list_insert(&eis->clients, &c->link);

	struct wm_config *config = server->config;
	struct wm_eis_decision *known = decision_find(eis, c->name);
	if (name_in_list(config ? config->eis_deny : NULL, c->name) ||
	    (known && !known->allowed) ||
	    (config && config->eis_policy == WM_EIS_DENY &&
	     !name_in_list(config->eis_allow, c->name) &&
	     !(known && known->allowed))) {
		wlr_log(WLR_INFO, "eis: %s may not emulate input", c->name);
		client_refuse(c);
	} else if (name_in_list(config ? config->eis_allow : NULL, c->name) ||
		   (known && known->allowed) ||
		   (config && config->eis_policy == WM_EIS_ALLOW)) {
		wlr_log(WLR_INFO, "eis: %s may emulate input", c->name);
		client_accept(c);
	} else {
		c->waiting = true;
		prompt_next(eis);
	}
}

static void
handle_input_event(struct wm_eis *eis, struct wm_eis_client *c,
	struct eis_event *event)
{
	uint32_t time = time_msec(event);
	if (!c->connected || c->paused)
		return;
	wm_idle_notify_activity(eis->server);

	switch (eis_event_get_type(event)) {
	case EIS_EVENT_POINTER_MOTION:
		emit_motion(eis, time, eis_event_pointer_get_dx(event),
			eis_event_pointer_get_dy(event));
		break;
	case EIS_EVENT_POINTER_MOTION_ABSOLUTE:
		emit_motion_absolute(eis, time,
			eis_event_pointer_get_absolute_x(event),
			eis_event_pointer_get_absolute_y(event));
		break;
	case EIS_EVENT_BUTTON_BUTTON: {
		uint32_t button = eis_event_button_get_button(event);
		bool pressed = eis_event_button_get_is_press(event);
		held_update(c->buttons, &c->button_count, button, pressed);
		emit_button(eis, time, button, pressed);
		break;
	}
	case EIS_EVENT_SCROLL_DISCRETE: {
		int32_t dx = eis_event_scroll_get_discrete_dx(event);
		int32_t dy = eis_event_scroll_get_discrete_dy(event);
		if (dx)
			emit_axis(eis, time, WL_POINTER_AXIS_HORIZONTAL_SCROLL,
				WL_POINTER_AXIS_SOURCE_WHEEL,
				dx / EIS_SCROLL_STEP * WAYLAND_SCROLL_STEP, dx);
		if (dy)
			emit_axis(eis, time, WL_POINTER_AXIS_VERTICAL_SCROLL,
				WL_POINTER_AXIS_SOURCE_WHEEL,
				dy / EIS_SCROLL_STEP * WAYLAND_SCROLL_STEP, dy);
		break;
	}
	case EIS_EVENT_SCROLL_DELTA: {
		double dx = eis_event_scroll_get_dx(event);
		double dy = eis_event_scroll_get_dy(event);
		if (dx != 0)
			emit_axis(eis, time, WL_POINTER_AXIS_HORIZONTAL_SCROLL,
				WL_POINTER_AXIS_SOURCE_FINGER, dx, 0);
		if (dy != 0)
			emit_axis(eis, time, WL_POINTER_AXIS_VERTICAL_SCROLL,
				WL_POINTER_AXIS_SOURCE_FINGER, dy, 0);
		break;
	}
	case EIS_EVENT_KEYBOARD_KEY: {
		uint32_t key = eis_event_keyboard_get_key(event);
		bool pressed = eis_event_keyboard_get_key_is_press(event);
		held_update(c->keys, &c->key_count, key, pressed);
		emit_key(eis, time, key, pressed);
		break;
	}
	case EIS_EVENT_FRAME:
		emit_frame(eis);
		break;
	case EIS_EVENT_DEVICE_STOP_EMULATING:
		release_held(c, time);
		break;
	default:
		break;
	}
}

static void
handle_event(struct wm_eis *eis, struct eis_event *event)
{
	struct eis_client *client = eis_event_get_client(event);
	struct wm_eis_client *c = client ? eis_client_get_user_data(client) :
		NULL;

	switch (eis_event_get_type(event)) {
	case EIS_EVENT_CLIENT_CONNECT:
		client_connect_requested(eis, client);
		break;
	case EIS_EVENT_CLIENT_DISCONNECT:
		if (c)
			client_free(c);
		break;
	case EIS_EVENT_SEAT_BIND:
		if (c && c->connected)
			devices_sync(c, event);
		break;
	case EIS_EVENT_DEVICE_CLOSED: {
		struct eis_device *device = eis_event_get_device(event);
		if (c && device == c->pointer)
			device_remove(&c->pointer);
		else if (c && device == c->absolute)
			device_remove(&c->absolute);
		else if (c && device == c->keyboard)
			device_remove(&c->keyboard);
		break;
	}
	default:
		if (c)
			handle_input_event(eis, c, event);
		break;
	}
}

static int
handle_eis_readable(int fd, uint32_t mask, void *data)
{
	(void)fd;
	(void)mask;
	struct wm_eis *eis = data;
	eis_dispatch(eis->eis);
	struct eis_event *event;
	while ((event = eis_get_event(eis->eis))) {
		handle_event(eis, event);
		eis_event_unref(event);
	}
	return 0;
}

/* --- The whole --- */

void
wm_eis_init(struct wm_server *server)
{
	if (server->eis || !getenv("XDG_RUNTIME_DIR"))
		return;
	struct wm_eis *eis = calloc(1, sizeof(*eis));
	if (!eis)
		return;
	eis->server = server;
	wl_list_init(&eis->clients);
	wl_list_init(&eis->decisions);
	snprintf(eis->socket_name, sizeof(eis->socket_name),
		"fluxland-eis.%d", (int)getpid());

	eis->eis = eis_new(NULL);
	if (!eis->eis || eis_setup_backend_socket(eis->eis,
	    eis->socket_name) < 0) {
		wlr_log(WLR_ERROR, "%s", "eis: can't listen; XTest input "
			"will not reach the compositor");
		goto fail;
	}
	eis->source = wl_event_loop_add_fd(server->wl_event_loop,
		eis_get_fd(eis->eis), WL_EVENT_READABLE, handle_eis_readable,
		eis);
	if (!eis->source)
		goto fail;

	/* The devices its input comes from: a pointer like a virtual one,
	 * and a keyboard of the group, with the configured keymap */
	wlr_pointer_init(&eis->pointer, &pointer_impl,
		"fluxland emulated pointer");
	wlr_cursor_attach_input_device(server->cursor, &eis->pointer.base);
	wlr_keyboard_init(&eis->keyboard, &keyboard_impl,
		"fluxland emulated keyboard");
	wm_keyboard_setup(server, &eis->keyboard.base, false);
	wm_input_update_capabilities(server, false);
	eis->devices_ready = true;

	/* Where Xwayland, started later, looks for it */
	setenv("LIBEI_SOCKET", eis->socket_name, true);
	server->eis = eis;
	wlr_log(WLR_INFO, "eis: listening for XWayland on %s",
		eis->socket_name);
	return;

fail:
	if (eis->eis)
		eis_unref(eis->eis);
	free(eis);
}

void
wm_eis_finish(struct wm_server *server)
{
	struct wm_eis *eis = server->eis;
	if (!eis)
		return;
	server->eis = NULL;
	struct wm_eis_client *c, *tmp;
	wl_list_for_each_safe(c, tmp, &eis->clients, link)
		client_refuse(c);
	if (eis->prompt_idle)
		wl_event_source_remove(eis->prompt_idle);
	if (eis->source)
		wl_event_source_remove(eis->source);
	if (eis->prompt_menu)
		eis->prompt_menu->on_close = NULL;
	if (eis->devices_ready) {
		wlr_keyboard_finish(&eis->keyboard);
		wlr_pointer_finish(&eis->pointer);
	}
	eis_unref(eis->eis);
	unsetenv("LIBEI_SOCKET");
	struct wm_eis_decision *d, *dtmp;
	wl_list_for_each_safe(d, dtmp, &eis->decisions, link) {
		wl_list_remove(&d->link);
		free(d->name);
		free(d);
	}
	free(eis);
}

void
wm_eis_session_lock_changed(struct wm_server *server)
{
	struct wm_eis *eis = server->eis;
	if (!eis)
		return;
	bool locked = wm_session_is_locked(server);
	struct wm_eis_client *c;
	wl_list_for_each(c, &eis->clients, link) {
		if (locked)
			release_held(c, 0);
		if (c->connected)
			client_set_paused(c, locked);
	}
	if (!locked)
		prompt_next(eis);
}

void
wm_eis_layout_changed(struct wm_server *server)
{
	struct wm_eis *eis = server->eis;
	if (!eis)
		return;
	struct wm_eis_client *c;
	wl_list_for_each(c, &eis->clients, link) {
		if (!c->absolute)
			continue;
		device_remove(&c->absolute);
		absolute_device_add(c);
	}
}

#endif /* WM_HAS_EIS */
