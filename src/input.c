/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
 * input.c - Input device management
 */

#define _POSIX_C_SOURCE 200809L
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/util/log.h>

#include "input.h"
#include "keyboard.h"
#include "cursor.h"
#include "server.h"

static void
handle_new_pointer(struct wm_server *server,
	struct wlr_input_device *device)
{
	/* Attach the pointer to the cursor */
	wlr_cursor_attach_input_device(server->cursor, device);
	wlr_log(WLR_INFO, "new pointer: %s", device->name);
}

static void
handle_new_input(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, new_input);
	struct wlr_input_device *device = data;

	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		wm_keyboard_setup(server, device);
		break;
	case WLR_INPUT_DEVICE_POINTER:
		handle_new_pointer(server, device);
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

void
wm_input_init(struct wm_server *server)
{
	wl_list_init(&server->keyboards);

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
}

void
wm_input_finish(struct wm_server *server)
{
	wl_list_remove(&server->new_input.link);
	wl_list_remove(&server->request_cursor.link);
	wl_list_remove(&server->request_set_selection.link);

	wm_cursor_finish(server);
}
