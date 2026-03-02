/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * test_input.c - Unit tests for input device management
 *
 * Includes input.c directly with stub implementations to avoid
 * needing wlroots/wayland libraries at link time.
 */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

/* --- Block real headers via their include guards --- */

/* wayland */
#define WAYLAND_SERVER_CORE_H
#define WAYLAND_SERVER_H
#define WAYLAND_UTIL_H

/* wlroots */
#define WLR_UTIL_LOG_H
#define WLR_UTIL_BOX_H
#define WLR_TYPES_WLR_CURSOR_H
#define WLR_TYPES_WLR_DATA_DEVICE_H
#define WLR_TYPES_WLR_INPUT_DEVICE_H
#define WLR_TYPES_WLR_SCENE_H
#define WLR_TYPES_WLR_SEAT_H
#define WLR_TYPES_WLR_XCURSOR_MANAGER_H

/* fluxland */
#define WM_SERVER_H
#define WM_KEYBOARD_H
#define WM_CURSOR_H
#define WM_TABLET_H

/* --- Stub wayland types --- */

struct wl_list {
	struct wl_list *prev;
	struct wl_list *next;
};

static inline void
wl_list_init(struct wl_list *list)
{
	list->prev = list;
	list->next = list;
}

static inline void
wl_list_insert(struct wl_list *list, struct wl_list *elm)
{
	elm->prev = list;
	elm->next = list->next;
	list->next = elm;
	elm->next->prev = elm;
}

static inline void
wl_list_remove(struct wl_list *elm)
{
	elm->prev->next = elm->next;
	elm->next->prev = elm->prev;
	elm->prev = NULL;
	elm->next = NULL;
}

static inline int
wl_list_empty(const struct wl_list *list)
{
	return list->next == list;
}

#define wl_container_of(ptr, sample, member) \
	(__typeof__(sample))((char *)(ptr) - offsetof(__typeof__(*sample), member))

#define wl_list_for_each(pos, head, member) \
	for (pos = wl_container_of((head)->next, pos, member); \
	     &pos->member != (head); \
	     pos = wl_container_of(pos->member.next, pos, member))

#define wl_list_for_each_safe(pos, tmp, head, member) \
	for (pos = wl_container_of((head)->next, pos, member), \
	     tmp = wl_container_of(pos->member.next, tmp, member); \
	     &pos->member != (head); \
	     pos = tmp, \
	     tmp = wl_container_of(pos->member.next, tmp, member))

struct wl_signal {
	struct wl_list listener_list;
};

struct wl_listener {
	struct wl_list link;
	void (*notify)(struct wl_listener *, void *);
};

static inline void
wl_signal_init(struct wl_signal *signal)
{
	wl_list_init(&signal->listener_list);
}

static inline void
wl_signal_add(struct wl_signal *signal, struct wl_listener *listener)
{
	wl_list_insert(signal->listener_list.prev, &listener->link);
}

/* --- Stub wlr types --- */

/* wlr_log no-op */
#define wlr_log(verb, fmt, ...) ((void)0)
#define WLR_ERROR 1
#define WLR_INFO 2
#define WLR_DEBUG 7

enum wlr_input_device_type {
	WLR_INPUT_DEVICE_KEYBOARD = 0,
	WLR_INPUT_DEVICE_POINTER,
	WLR_INPUT_DEVICE_TOUCH,
	WLR_INPUT_DEVICE_TABLET,
	WLR_INPUT_DEVICE_TABLET_PAD,
	WLR_INPUT_DEVICE_SWITCH,
};

struct wlr_input_device {
	enum wlr_input_device_type type;
	const char *name;
	struct {
		struct wl_signal destroy;
	} events;
};

struct wlr_scene_node {
	int x, y;
	void *data;
	void *parent;
};

struct wlr_scene_tree {
	struct wlr_scene_node node;
};

struct wlr_box {
	int x, y, width, height;
};

struct wlr_surface {
	int dummy;
};

struct wlr_cursor {
	double x, y;
};

struct wlr_xcursor_manager {
	int dummy;
};

struct wlr_seat_client {
	int id;
};

struct wlr_seat {
	struct {
		struct wlr_seat_client *focused_client;
	} pointer_state;
	uint32_t capabilities;
	struct {
		struct wl_signal request_set_cursor;
		struct wl_signal request_set_selection;
		struct wl_signal request_start_drag;
		struct wl_signal start_drag;
	} events;
};

struct wlr_seat_pointer_request_set_cursor_event {
	struct wlr_seat_client *seat_client;
	struct wlr_surface *surface;
	int32_t hotspot_x;
	int32_t hotspot_y;
};

struct wlr_data_source {
	int dummy;
};

struct wlr_seat_request_set_selection_event {
	struct wlr_data_source *source;
	uint32_t serial;
};

struct wlr_drag_icon {
	int dummy;
};

struct wlr_drag {
	struct wlr_drag_icon *icon;
	struct {
		struct wl_signal destroy;
	} events;
};

struct wlr_seat_request_start_drag_event {
	struct wlr_drag *drag;
	struct wlr_surface *origin;
	uint32_t serial;
};

struct wlr_scene_drag_icon {
	struct wlr_scene_node node;
};

struct wlr_backend {
	struct {
		struct wl_signal new_input;
	} events;
};

/* WL_SEAT_CAPABILITY flags */
#define WL_SEAT_CAPABILITY_POINTER 1
#define WL_SEAT_CAPABILITY_KEYBOARD 2
#define WL_SEAT_CAPABILITY_TOUCH 4

/* --- Stub tracking globals --- */

static int g_cursor_attach_count;
static int g_cursor_set_surface_count;
static struct wlr_surface *g_cursor_set_surface_last;
static int g_seat_set_caps_count;
static uint32_t g_seat_last_caps;
static int g_seat_set_selection_count;
static int g_seat_validate_serial_count;
static bool g_seat_validate_serial_return;
static int g_seat_start_drag_count;
static int g_scene_drag_icon_create_count;
static int g_scene_node_set_position_count;
static int g_keyboard_setup_count;
static int g_tablet_tool_setup_count;
static int g_tablet_pad_setup_count;
static int g_cursor_init_count;
static int g_cursor_finish_count;

/* --- Stub wlroots functions --- */

static void
wlr_cursor_attach_input_device(struct wlr_cursor *cursor,
	struct wlr_input_device *dev)
{
	(void)cursor;
	(void)dev;
	g_cursor_attach_count++;
}

static void
wlr_cursor_set_surface(struct wlr_cursor *cursor,
	struct wlr_surface *surface, int32_t hotspot_x, int32_t hotspot_y)
{
	(void)cursor;
	(void)hotspot_x;
	(void)hotspot_y;
	g_cursor_set_surface_count++;
	g_cursor_set_surface_last = surface;
}

static void
wlr_seat_set_capabilities(struct wlr_seat *seat, uint32_t caps)
{
	seat->capabilities = caps;
	g_seat_set_caps_count++;
	g_seat_last_caps = caps;
}

static void
wlr_seat_set_selection(struct wlr_seat *seat,
	struct wlr_data_source *source, uint32_t serial)
{
	(void)seat;
	(void)source;
	(void)serial;
	g_seat_set_selection_count++;
}

static bool
wlr_seat_validate_pointer_grab_serial(struct wlr_seat *seat,
	struct wlr_surface *origin, uint32_t serial)
{
	(void)seat;
	(void)origin;
	(void)serial;
	g_seat_validate_serial_count++;
	return g_seat_validate_serial_return;
}

static void
wlr_seat_start_pointer_drag(struct wlr_seat *seat,
	struct wlr_drag *drag, uint32_t serial)
{
	(void)seat;
	(void)drag;
	(void)serial;
	g_seat_start_drag_count++;
}

static struct wlr_scene_tree g_scene_drag_icon_tree;

static struct wlr_scene_tree *
wlr_scene_drag_icon_create(struct wlr_scene_tree *parent,
	struct wlr_drag_icon *icon)
{
	(void)parent;
	(void)icon;
	g_scene_drag_icon_create_count++;
	return &g_scene_drag_icon_tree;
}

static void
wlr_scene_node_set_position(struct wlr_scene_node *node, int x, int y)
{
	node->x = x;
	node->y = y;
	g_scene_node_set_position_count++;
}

/* --- Stub fluxland functions --- */

/* Forward declaration so stubs use the same type as the definition below */
struct wm_server;

static void
wm_keyboard_setup(struct wm_server *server, struct wlr_input_device *device)
{
	(void)server;
	(void)device;
	g_keyboard_setup_count++;
}

static void
wm_tablet_tool_setup(struct wm_server *server,
	struct wlr_input_device *device)
{
	(void)server;
	(void)device;
	g_tablet_tool_setup_count++;
}

static void
wm_tablet_pad_setup(struct wm_server *server,
	struct wlr_input_device *device)
{
	(void)server;
	(void)device;
	g_tablet_pad_setup_count++;
}

static void
wm_cursor_init(struct wm_server *server)
{
	(void)server;
	g_cursor_init_count++;
}

static void
wm_cursor_finish(struct wm_server *server)
{
	(void)server;
	g_cursor_finish_count++;
}

/* --- Minimal wm_server struct --- */

struct wm_server {
	struct wlr_backend *backend;
	struct wlr_cursor *cursor;
	struct wlr_seat *seat;
	struct wlr_xcursor_manager *cursor_mgr;
	struct wl_list keyboards;

	struct wlr_scene_tree *layer_overlay;
	struct wlr_scene_tree *drag_icon_tree;

	struct wl_listener new_input;
	struct wl_listener request_cursor;
	struct wl_listener request_set_selection;
	struct wl_listener request_start_drag;
	struct wl_listener start_drag;
};

/* Forward declarations for public API */
void wm_input_init(struct wm_server *server);
void wm_input_finish(struct wm_server *server);

/* --- Include the source under test --- */
#include "../src/input.c"

/* --- Helper to reset all tracking globals --- */

static void
reset_globals(void)
{
	g_cursor_attach_count = 0;
	g_cursor_set_surface_count = 0;
	g_cursor_set_surface_last = NULL;
	g_seat_set_caps_count = 0;
	g_seat_last_caps = 0;
	g_seat_set_selection_count = 0;
	g_seat_validate_serial_count = 0;
	g_seat_validate_serial_return = false;
	g_seat_start_drag_count = 0;
	g_scene_drag_icon_create_count = 0;
	g_scene_node_set_position_count = 0;
	g_keyboard_setup_count = 0;
	g_tablet_tool_setup_count = 0;
	g_tablet_pad_setup_count = 0;
	g_cursor_init_count = 0;
	g_cursor_finish_count = 0;
}

/* --- Test functions --- */

static void
test_new_input_keyboard(void)
{
	reset_globals();

	struct wlr_backend backend = {0};
	struct wlr_seat seat = {0};
	struct wlr_cursor cursor = {0};
	wl_signal_init(&backend.events.new_input);

	struct wm_server server = {
		.backend = &backend,
		.seat = &seat,
		.cursor = &cursor,
	};
	wl_list_init(&server.keyboards);

	/* Add a fake keyboard item to the list so caps include KEYBOARD */
	struct wl_list fake_kb;
	wl_list_insert(&server.keyboards, &fake_kb);

	struct wlr_input_device device = {
		.type = WLR_INPUT_DEVICE_KEYBOARD,
		.name = "test-keyboard",
	};

	/* Invoke the handler directly */
	server.new_input.notify = handle_new_input;
	server.new_input.notify(&server.new_input, &device);

	assert(g_keyboard_setup_count == 1);
	assert(g_seat_set_caps_count == 1);
	/* Should have both pointer (always) and keyboard caps */
	assert(g_seat_last_caps & WL_SEAT_CAPABILITY_KEYBOARD);
	assert(g_seat_last_caps & WL_SEAT_CAPABILITY_POINTER);

	printf("PASS: test_new_input_keyboard\n");
}

static void
test_new_input_pointer(void)
{
	reset_globals();

	struct wlr_backend backend = {0};
	struct wlr_seat seat = {0};
	struct wlr_cursor cursor = {0};
	wl_signal_init(&backend.events.new_input);

	struct wm_server server = {
		.backend = &backend,
		.seat = &seat,
		.cursor = &cursor,
	};
	wl_list_init(&server.keyboards);

	struct wlr_input_device device = {
		.type = WLR_INPUT_DEVICE_POINTER,
		.name = "test-pointer",
	};

	server.new_input.notify = handle_new_input;
	server.new_input.notify(&server.new_input, &device);

	assert(g_cursor_attach_count == 1);
	/* No keyboards in list, so no keyboard cap */
	assert(!(g_seat_last_caps & WL_SEAT_CAPABILITY_KEYBOARD));
	assert(g_seat_last_caps & WL_SEAT_CAPABILITY_POINTER);

	printf("PASS: test_new_input_pointer\n");
}

static void
test_new_input_touch(void)
{
	reset_globals();

	struct wlr_backend backend = {0};
	struct wlr_seat seat = {0};
	struct wlr_cursor cursor = {0};
	wl_signal_init(&backend.events.new_input);

	struct wm_server server = {
		.backend = &backend,
		.seat = &seat,
		.cursor = &cursor,
	};
	wl_list_init(&server.keyboards);

	struct wlr_input_device device = {
		.type = WLR_INPUT_DEVICE_TOUCH,
		.name = "test-touch",
	};

	server.new_input.notify = handle_new_input;
	server.new_input.notify(&server.new_input, &device);

	assert(g_cursor_attach_count == 1);
	assert(g_seat_last_caps & WL_SEAT_CAPABILITY_TOUCH);
	assert(g_seat_last_caps & WL_SEAT_CAPABILITY_POINTER);

	printf("PASS: test_new_input_touch\n");
}

static void
test_new_input_tablet(void)
{
	reset_globals();

	struct wlr_backend backend = {0};
	struct wlr_seat seat = {0};
	struct wlr_cursor cursor = {0};
	wl_signal_init(&backend.events.new_input);

	struct wm_server server = {
		.backend = &backend,
		.seat = &seat,
		.cursor = &cursor,
	};
	wl_list_init(&server.keyboards);

	struct wlr_input_device device = {
		.type = WLR_INPUT_DEVICE_TABLET,
		.name = "test-tablet",
	};

	server.new_input.notify = handle_new_input;
	server.new_input.notify(&server.new_input, &device);

	assert(g_tablet_tool_setup_count == 1);

	printf("PASS: test_new_input_tablet\n");
}

static void
test_new_input_tablet_pad(void)
{
	reset_globals();

	struct wlr_backend backend = {0};
	struct wlr_seat seat = {0};
	struct wlr_cursor cursor = {0};
	wl_signal_init(&backend.events.new_input);

	struct wm_server server = {
		.backend = &backend,
		.seat = &seat,
		.cursor = &cursor,
	};
	wl_list_init(&server.keyboards);

	struct wlr_input_device device = {
		.type = WLR_INPUT_DEVICE_TABLET_PAD,
		.name = "test-tablet-pad",
	};

	server.new_input.notify = handle_new_input;
	server.new_input.notify(&server.new_input, &device);

	assert(g_tablet_pad_setup_count == 1);

	printf("PASS: test_new_input_tablet_pad\n");
}

static void
test_caps_no_keyboard(void)
{
	reset_globals();

	struct wlr_backend backend = {0};
	struct wlr_seat seat = {0};
	struct wlr_cursor cursor = {0};
	wl_signal_init(&backend.events.new_input);

	struct wm_server server = {
		.backend = &backend,
		.seat = &seat,
		.cursor = &cursor,
	};
	wl_list_init(&server.keyboards);
	/* keyboards list is empty */

	struct wlr_input_device device = {
		.type = WLR_INPUT_DEVICE_POINTER,
		.name = "ptr",
	};

	server.new_input.notify = handle_new_input;
	server.new_input.notify(&server.new_input, &device);

	/* No keyboard cap when list empty */
	assert(!(g_seat_last_caps & WL_SEAT_CAPABILITY_KEYBOARD));
	/* Pointer always advertised */
	assert(g_seat_last_caps & WL_SEAT_CAPABILITY_POINTER);
	/* No touch */
	assert(!(g_seat_last_caps & WL_SEAT_CAPABILITY_TOUCH));

	printf("PASS: test_caps_no_keyboard\n");
}

static void
test_touch_cap_sticky(void)
{
	reset_globals();

	struct wlr_backend backend = {0};
	struct wlr_seat seat = {0};
	struct wlr_cursor cursor = {0};
	wl_signal_init(&backend.events.new_input);

	struct wm_server server = {
		.backend = &backend,
		.seat = &seat,
		.cursor = &cursor,
	};
	wl_list_init(&server.keyboards);

	/* First add a touch device */
	struct wlr_input_device touch_dev = {
		.type = WLR_INPUT_DEVICE_TOUCH,
		.name = "touch",
	};
	server.new_input.notify = handle_new_input;
	server.new_input.notify(&server.new_input, &touch_dev);
	assert(g_seat_last_caps & WL_SEAT_CAPABILITY_TOUCH);

	/* Now add a pointer — touch cap should persist */
	struct wlr_input_device ptr_dev = {
		.type = WLR_INPUT_DEVICE_POINTER,
		.name = "ptr",
	};
	server.new_input.notify(&server.new_input, &ptr_dev);
	assert(g_seat_last_caps & WL_SEAT_CAPABILITY_TOUCH);

	printf("PASS: test_touch_cap_sticky\n");
}

static void
test_request_cursor_focused_client(void)
{
	reset_globals();

	struct wlr_seat_client focused = { .id = 42 };
	struct wlr_seat seat = {
		.pointer_state = { .focused_client = &focused },
	};
	struct wlr_cursor cursor = {0};
	struct wlr_backend backend = {0};
	wl_signal_init(&seat.events.request_set_cursor);

	struct wm_server server = {
		.seat = &seat,
		.cursor = &cursor,
		.backend = &backend,
	};

	struct wlr_surface surface = {0};
	struct wlr_seat_pointer_request_set_cursor_event event = {
		.seat_client = &focused,
		.surface = &surface,
		.hotspot_x = 5,
		.hotspot_y = 10,
	};

	/* Request from focused client — should honor */
	server.request_cursor.notify = handle_request_cursor;
	server.request_cursor.notify(&server.request_cursor, &event);

	assert(g_cursor_set_surface_count == 1);
	assert(g_cursor_set_surface_last == &surface);

	printf("PASS: test_request_cursor_focused_client\n");
}

static void
test_request_cursor_unfocused_client(void)
{
	reset_globals();

	struct wlr_seat_client focused = { .id = 42 };
	struct wlr_seat_client other = { .id = 99 };
	struct wlr_seat seat = {
		.pointer_state = { .focused_client = &focused },
	};
	struct wlr_cursor cursor = {0};
	struct wlr_backend backend = {0};
	wl_signal_init(&seat.events.request_set_cursor);

	struct wm_server server = {
		.seat = &seat,
		.cursor = &cursor,
		.backend = &backend,
	};

	struct wlr_surface surface = {0};
	struct wlr_seat_pointer_request_set_cursor_event event = {
		.seat_client = &other,
		.surface = &surface,
		.hotspot_x = 5,
		.hotspot_y = 10,
	};

	/* Request from non-focused client — should ignore */
	server.request_cursor.notify = handle_request_cursor;
	server.request_cursor.notify(&server.request_cursor, &event);

	assert(g_cursor_set_surface_count == 0);

	printf("PASS: test_request_cursor_unfocused_client\n");
}

static void
test_request_set_selection(void)
{
	reset_globals();

	struct wlr_seat seat = {0};
	struct wlr_backend backend = {0};
	struct wlr_cursor cursor = {0};
	wl_signal_init(&seat.events.request_set_selection);

	struct wm_server server = {
		.seat = &seat,
		.cursor = &cursor,
		.backend = &backend,
	};

	struct wlr_data_source source = {0};
	struct wlr_seat_request_set_selection_event event = {
		.source = &source,
		.serial = 123,
	};

	server.request_set_selection.notify = handle_request_set_selection;
	server.request_set_selection.notify(
		&server.request_set_selection, &event);

	assert(g_seat_set_selection_count == 1);

	printf("PASS: test_request_set_selection\n");
}

static void
test_request_start_drag_valid_serial(void)
{
	reset_globals();
	g_seat_validate_serial_return = true;

	struct wlr_seat seat = {0};
	struct wlr_backend backend = {0};
	struct wlr_cursor cursor = {0};
	wl_signal_init(&seat.events.request_start_drag);

	struct wm_server server = {
		.seat = &seat,
		.cursor = &cursor,
		.backend = &backend,
	};

	struct wlr_drag drag = {0};
	wl_signal_init(&drag.events.destroy);
	struct wlr_surface origin = {0};

	struct wlr_seat_request_start_drag_event event = {
		.drag = &drag,
		.origin = &origin,
		.serial = 1,
	};

	server.request_start_drag.notify = handle_request_start_drag;
	server.request_start_drag.notify(
		&server.request_start_drag, &event);

	assert(g_seat_validate_serial_count == 1);
	assert(g_seat_start_drag_count == 1);

	printf("PASS: test_request_start_drag_valid_serial\n");
}

static void
test_request_start_drag_invalid_serial(void)
{
	reset_globals();
	g_seat_validate_serial_return = false;

	struct wlr_seat seat = {0};
	struct wlr_backend backend = {0};
	struct wlr_cursor cursor = {0};
	wl_signal_init(&seat.events.request_start_drag);

	struct wm_server server = {
		.seat = &seat,
		.cursor = &cursor,
		.backend = &backend,
	};

	struct wlr_drag drag = {0};
	struct wlr_surface origin = {0};

	struct wlr_seat_request_start_drag_event event = {
		.drag = &drag,
		.origin = &origin,
		.serial = 1,
	};

	server.request_start_drag.notify = handle_request_start_drag;
	server.request_start_drag.notify(
		&server.request_start_drag, &event);

	assert(g_seat_validate_serial_count == 1);
	assert(g_seat_start_drag_count == 0);

	printf("PASS: test_request_start_drag_invalid_serial\n");
}

static void
test_start_drag_with_icon(void)
{
	reset_globals();

	struct wlr_seat seat = {0};
	struct wlr_backend backend = {0};
	struct wlr_cursor cursor = { .x = 100.0, .y = 200.0 };
	struct wlr_scene_tree overlay = {0};
	wl_signal_init(&seat.events.start_drag);

	struct wm_server server = {
		.seat = &seat,
		.cursor = &cursor,
		.backend = &backend,
		.layer_overlay = &overlay,
		.drag_icon_tree = NULL,
	};

	struct wlr_drag_icon icon = {0};
	struct wlr_drag drag = { .icon = &icon };
	wl_signal_init(&drag.events.destroy);

	server.start_drag.notify = handle_start_drag;
	server.start_drag.notify(&server.start_drag, &drag);

	assert(g_scene_drag_icon_create_count == 1);
	assert(g_scene_node_set_position_count == 1);
	assert(server.drag_icon_tree != NULL);

	printf("PASS: test_start_drag_with_icon\n");
}

static void
test_start_drag_no_icon(void)
{
	reset_globals();

	struct wlr_seat seat = {0};
	struct wlr_backend backend = {0};
	struct wlr_cursor cursor = { .x = 0, .y = 0 };
	wl_signal_init(&seat.events.start_drag);

	struct wm_server server = {
		.seat = &seat,
		.cursor = &cursor,
		.backend = &backend,
		.drag_icon_tree = NULL,
	};

	struct wlr_drag drag = { .icon = NULL };
	wl_signal_init(&drag.events.destroy);

	server.start_drag.notify = handle_start_drag;
	server.start_drag.notify(&server.start_drag, &drag);

	assert(g_scene_drag_icon_create_count == 0);
	assert(g_scene_node_set_position_count == 0);

	printf("PASS: test_start_drag_no_icon\n");
}

static void
test_drag_destroy_clears_icon_tree(void)
{
	reset_globals();

	struct wlr_seat seat = {0};
	struct wlr_backend backend = {0};
	struct wlr_cursor cursor = { .x = 50, .y = 50 };
	struct wlr_scene_tree overlay = {0};
	wl_signal_init(&seat.events.start_drag);

	struct wm_server server = {
		.seat = &seat,
		.cursor = &cursor,
		.backend = &backend,
		.layer_overlay = &overlay,
		.drag_icon_tree = NULL,
	};

	struct wlr_drag_icon icon = {0};
	struct wlr_drag drag = { .icon = &icon };
	wl_signal_init(&drag.events.destroy);

	/* Start drag to register destroy listener */
	server.start_drag.notify = handle_start_drag;
	server.start_drag.notify(&server.start_drag, &drag);
	assert(server.drag_icon_tree != NULL);

	/* Find and invoke the destroy listener on the drag */
	struct wl_listener *listener =
		wl_container_of(drag.events.destroy.listener_list.next,
			listener, link);
	listener->notify(listener, NULL);

	assert(server.drag_icon_tree == NULL);

	printf("PASS: test_drag_destroy_clears_icon_tree\n");
}

static void
test_input_init(void)
{
	reset_globals();

	struct wlr_backend backend = {0};
	struct wlr_seat seat = {0};
	struct wlr_cursor cursor = {0};
	wl_signal_init(&backend.events.new_input);
	wl_signal_init(&seat.events.request_set_cursor);
	wl_signal_init(&seat.events.request_set_selection);
	wl_signal_init(&seat.events.request_start_drag);
	wl_signal_init(&seat.events.start_drag);

	struct wm_server server = {
		.backend = &backend,
		.seat = &seat,
		.cursor = &cursor,
	};

	wm_input_init(&server);

	assert(g_cursor_init_count == 1);
	assert(server.new_input.notify == handle_new_input);
	assert(server.request_cursor.notify == handle_request_cursor);
	assert(server.request_set_selection.notify ==
		handle_request_set_selection);
	assert(server.request_start_drag.notify ==
		handle_request_start_drag);
	assert(server.start_drag.notify == handle_start_drag);
	assert(wl_list_empty(&server.keyboards));

	/* Clean up */
	wm_input_finish(&server);
	assert(g_cursor_finish_count == 1);

	printf("PASS: test_input_init\n");
}

static void
test_input_finish(void)
{
	reset_globals();

	struct wlr_backend backend = {0};
	struct wlr_seat seat = {0};
	struct wlr_cursor cursor = {0};
	wl_signal_init(&backend.events.new_input);
	wl_signal_init(&seat.events.request_set_cursor);
	wl_signal_init(&seat.events.request_set_selection);
	wl_signal_init(&seat.events.request_start_drag);
	wl_signal_init(&seat.events.start_drag);

	struct wm_server server = {
		.backend = &backend,
		.seat = &seat,
		.cursor = &cursor,
	};

	wm_input_init(&server);
	wm_input_finish(&server);

	assert(g_cursor_finish_count == 1);
	/* Listeners should be removed (link pointers NULL) */
	assert(server.new_input.link.prev == NULL);
	assert(server.request_cursor.link.prev == NULL);

	printf("PASS: test_input_finish\n");
}

/* --- main --- */

int
main(void)
{
	test_new_input_keyboard();
	test_new_input_pointer();
	test_new_input_touch();
	test_new_input_tablet();
	test_new_input_tablet_pad();
	test_caps_no_keyboard();
	test_touch_cap_sticky();
	test_request_cursor_focused_client();
	test_request_cursor_unfocused_client();
	test_request_set_selection();
	test_request_start_drag_valid_serial();
	test_request_start_drag_invalid_serial();
	test_start_drag_with_icon();
	test_start_drag_no_icon();
	test_drag_destroy_clears_icon_tree();
	test_input_init();
	test_input_finish();

	printf("\nAll input tests passed! (17/17)\n");
	return 0;
}
