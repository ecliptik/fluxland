/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * input.c - Input device management
 */

#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <wlr/backend/libinput.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_switch.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/util/log.h>

#include "command.h"
#include "config.h"
#include "input.h"
#include "input_config.h"
#include "keyboard.h"
#include "cursor.h"
#include "server.h"
#include "tablet.h"

/* A pointer device, kept so settings can be re-applied on reconfigure */
struct wm_pointer {
	struct wm_server *server;
	struct wlr_input_device *device;
	struct wl_listener destroy;
	struct wl_list link; /* wm_server.pointers */
};

static void
configure_pointer(struct wm_server *server, struct wlr_input_device *device)
{
	if (!server->config || !wlr_input_device_is_libinput(device))
		return;
	wm_input_config_apply(wlr_libinput_get_device_handle(device),
		device->name, server->config->input_rules,
		server->config->input_rule_count);
}

static void
handle_pointer_destroy(struct wl_listener *listener, void *data)
{
	(void)data;
	struct wm_pointer *pointer =
		wl_container_of(listener, pointer, destroy);
	wl_list_remove(&pointer->destroy.link);
	wl_list_remove(&pointer->link);
	free(pointer);
}

static void
handle_new_pointer(struct wm_server *server,
	struct wlr_input_device *device)
{
	/* Attach the pointer to the cursor */
	wlr_cursor_attach_input_device(server->cursor, device);
	wlr_log(WLR_INFO, "new pointer: %s", device->name);

	struct wm_pointer *pointer = calloc(1, sizeof(*pointer));
	if (pointer) {
		pointer->server = server;
		pointer->device = device;
		pointer->destroy.notify = handle_pointer_destroy;
		wl_signal_add(&device->events.destroy, &pointer->destroy);
		wl_list_insert(&server->pointers, &pointer->link);
	}
	configure_pointer(server, device);
}

void
wm_input_apply_config(struct wm_server *server)
{
	struct wm_pointer *pointer;
	wl_list_for_each(pointer, &server->pointers, link)
		configure_pointer(server, pointer->device);
}

/* --- Switches (laptop lid, tablet mode) --- */

struct wm_switch {
	struct wm_server *server;
	struct wl_listener toggle;
	struct wl_listener destroy;
};

enum wm_switch_event
wm_switch_event_for(enum wlr_switch_type type, enum wlr_switch_state state)
{
	bool on = state == WLR_SWITCH_STATE_ON;
	if (type == WLR_SWITCH_TYPE_LID)
		return on ? WM_SWITCH_LID_CLOSE : WM_SWITCH_LID_OPEN;
	return on ? WM_SWITCH_TABLET_MODE_ON : WM_SWITCH_TABLET_MODE_OFF;
}

static void
handle_switch_toggle(struct wl_listener *listener, void *data)
{
	struct wm_switch *sw = wl_container_of(listener, sw, toggle);
	struct wlr_switch_toggle_event *event = data;
	struct wm_server *server = sw->server;

	enum wm_switch_event ev =
		wm_switch_event_for(event->switch_type, event->switch_state);
	const char *cmd = server->config ?
		server->config->switch_commands[ev] : NULL;
	wlr_log(WLR_INFO, "switch event %d%s%s", ev, cmd ? ": " : "",
		cmd ? cmd : "");
	if (cmd)
		wm_command_run(server, cmd, NULL);
}

static void
handle_switch_destroy(struct wl_listener *listener, void *data)
{
	(void)data;
	struct wm_switch *sw = wl_container_of(listener, sw, destroy);
	wl_list_remove(&sw->toggle.link);
	wl_list_remove(&sw->destroy.link);
	free(sw);
}

static void
handle_new_switch(struct wm_server *server, struct wlr_input_device *device)
{
	struct wm_switch *sw = calloc(1, sizeof(*sw));
	if (!sw)
		return;
	sw->server = server;
	sw->toggle.notify = handle_switch_toggle;
	wl_signal_add(&wlr_switch_from_input_device(device)->events.toggle,
		&sw->toggle);
	sw->destroy.notify = handle_switch_destroy;
	wl_signal_add(&device->events.destroy, &sw->destroy);
	wlr_log(WLR_INFO, "new switch: %s", device->name);
}

static void
handle_new_input(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, new_input);
	struct wlr_input_device *device = data;

	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		wm_keyboard_setup(server, device, false);
		break;
	case WLR_INPUT_DEVICE_POINTER:
		handle_new_pointer(server, device);
		break;
	case WLR_INPUT_DEVICE_TOUCH:
		wlr_cursor_attach_input_device(server->cursor, device);
		wlr_log(WLR_INFO, "new touch device: %s", device->name);
		break;
	case WLR_INPUT_DEVICE_TABLET:
		wm_tablet_tool_setup(server, device);
		break;
	case WLR_INPUT_DEVICE_TABLET_PAD:
		wm_tablet_pad_setup(server, device);
		break;
	case WLR_INPUT_DEVICE_SWITCH:
		handle_new_switch(server, device);
		break;
	default:
		break;
	}

	/*
	 * Update seat capabilities based on available devices.
	 * For simplicity, always advertise both pointer and keyboard
	 * capabilities once we have at least one of each type.
	 */
	uint32_t caps = 0;
	if (!wl_list_empty(&server->keyboards)) {
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	}
	caps |= WL_SEAT_CAPABILITY_POINTER;
	/*
	 * Once a touch device has been seen, keep advertising touch
	 * capability (even if other device types are added later).
	 */
	if (device->type == WLR_INPUT_DEVICE_TOUCH ||
	    (server->seat->capabilities & WL_SEAT_CAPABILITY_TOUCH)) {
		caps |= WL_SEAT_CAPABILITY_TOUCH;
	}
	wlr_seat_set_capabilities(server->seat, caps);
}

static void
handle_request_cursor(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, request_cursor);
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	struct wlr_seat_client *focused_client =
		server->seat->pointer_state.focused_client;

	/*
	 * Only honor cursor image requests from the client that
	 * currently has pointer focus.
	 */
	if (focused_client == event->seat_client) {
		wlr_cursor_set_surface(server->cursor, event->surface,
			event->hotspot_x, event->hotspot_y);
	}
}

static void
handle_request_set_selection(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, request_set_selection);
	struct wlr_seat_request_set_selection_event *event = data;

	wlr_seat_set_selection(server->seat, event->source, event->serial);
}

static void
handle_request_start_drag(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, request_start_drag);
	struct wlr_seat_request_start_drag_event *event = data;

	if (wlr_seat_validate_pointer_grab_serial(
			server->seat, event->origin, event->serial)) {
		wlr_seat_start_pointer_drag(server->seat, event->drag,
			event->serial);
		return;
	}

	wlr_log(WLR_DEBUG, "%s",
		"ignoring request_start_drag: failed serial validation");
}

struct wm_drag_state {
	struct wm_server *server;
	struct wl_listener destroy;
};

static void
handle_drag_destroy(struct wl_listener *listener, void *data)
{
	(void)data;
	struct wm_drag_state *state =
		wl_container_of(listener, state, destroy);
	state->server->drag_icon_tree = NULL;
	wl_list_remove(&state->destroy.link);
	free(state);
}

static void
handle_start_drag(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, start_drag);
	struct wlr_drag *drag = data;

	wlr_log(WLR_INFO, "drag started (icon=%s)",
		drag->icon ? "yes" : "no");

	if (drag->icon) {
		server->drag_icon_tree = wlr_scene_drag_icon_create(
			server->layer_overlay, drag->icon);
		if (server->drag_icon_tree) {
			wlr_scene_node_set_position(
				&server->drag_icon_tree->node,
				(int)server->cursor->x,
				(int)server->cursor->y);
		}
	}

	/* Listen for drag destroy to clear drag_icon_tree */
	struct wm_drag_state *state = calloc(1, sizeof(*state));
	if (state) {
		state->server = server;
		state->destroy.notify = handle_drag_destroy;
		wl_signal_add(&drag->events.destroy, &state->destroy);
	}
}

void
wm_input_init(struct wm_server *server)
{
	wl_list_init(&server->keyboards);
	wl_list_init(&server->pointers);
	wm_keyboard_group_init(server);

	wm_cursor_init(server);

	server->new_input.notify = handle_new_input;
	wl_signal_add(&server->backend->events.new_input,
		&server->new_input);

	server->request_cursor.notify = handle_request_cursor;
	wl_signal_add(&server->seat->events.request_set_cursor,
		&server->request_cursor);

	server->request_set_selection.notify = handle_request_set_selection;
	wl_signal_add(&server->seat->events.request_set_selection,
		&server->request_set_selection);

	server->request_start_drag.notify = handle_request_start_drag;
	wl_signal_add(&server->seat->events.request_start_drag,
		&server->request_start_drag);

	server->start_drag.notify = handle_start_drag;
	wl_signal_add(&server->seat->events.start_drag,
		&server->start_drag);
}

void
wm_input_finish(struct wm_server *server)
{
	wl_list_remove(&server->new_input.link);
	wl_list_remove(&server->request_cursor.link);
	wl_list_remove(&server->request_set_selection.link);
	wl_list_remove(&server->request_start_drag.link);
	wl_list_remove(&server->start_drag.link);

	wm_keyboard_group_finish(server);
	wm_cursor_finish(server);
}
