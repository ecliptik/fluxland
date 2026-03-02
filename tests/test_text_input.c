/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * test_text_input.c - Unit tests for text input v3 / input method v2 relay
 *
 * Includes text_input.c directly with stub implementations to avoid
 * needing wlroots/wayland libraries at link time.
 *
 * CRITICAL: Tests the client-matching guard (wl_resource_get_client)
 * that prevents send_enter to text inputs owned by different clients.
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
#define WLR_TYPES_WLR_CURSOR_H
#define WLR_TYPES_WLR_OUTPUT_LAYOUT_H
#define WLR_TYPES_WLR_TEXT_INPUT_V3_H
#define WLR_TYPES_WLR_INPUT_METHOD_V2_H
#define WLR_TYPES_WLR_KEYBOARD_H
#define WLR_TYPES_WLR_SEAT_H

/* fluxland */
#define WM_SERVER_H
#define WM_OUTPUT_H

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

struct wl_display {
	int dummy;
};

/* --- Client / resource stubs for client-matching test --- */

struct wl_client {
	int id;
};

struct wl_resource {
	struct wl_client *client;
};

static struct wl_client *
wl_resource_get_client(struct wl_resource *resource)
{
	return resource->client;
}

/* --- Stub wlr types --- */

/* wlr_log no-op */
#define wlr_log(verb, fmt, ...) ((void)0)
#define WLR_ERROR 1
#define WLR_INFO 2
#define WLR_DEBUG 7

/* text input v3 feature flags */
#define WLR_TEXT_INPUT_V3_FEATURE_SURROUNDING_TEXT (1 << 0)
#define WLR_TEXT_INPUT_V3_FEATURE_CONTENT_TYPE (1 << 1)
#define WLR_TEXT_INPUT_V3_FEATURE_CURSOR_RECTANGLE (1 << 2)

struct wlr_surface {
	struct wl_resource *resource;
};

struct wlr_box {
	int x, y, width, height;
};

struct wlr_text_input_v3 {
	struct wl_resource *resource;
	struct wlr_surface *focused_surface;
	bool current_enabled;
	struct {
		uint32_t features;
		struct {
			const char *text;
			uint32_t cursor;
			uint32_t anchor;
		} surrounding;
		uint32_t text_change_cause;
		struct {
			uint32_t hint;
			uint32_t purpose;
		} content_type;
		struct wlr_box cursor_rectangle;
	} current;
	struct {
		struct wl_signal enable;
		struct wl_signal commit;
		struct wl_signal disable;
		struct wl_signal destroy;
	} events;
};

struct wlr_input_popup_surface_v2 {
	struct wl_list link;
};

struct wlr_input_method_v2 {
	struct {
		struct {
			const char *text;
			int32_t cursor_begin;
			int32_t cursor_end;
		} preedit;
		const char *commit_text;
		struct {
			uint32_t before_length;
			uint32_t after_length;
		} delete;
	} current;
	struct wl_list popup_surfaces;
	struct {
		struct wl_signal commit;
		struct wl_signal grab_keyboard;
		struct wl_signal destroy;
	} events;
};

struct wlr_keyboard {
	int dummy;
};

struct wlr_input_method_keyboard_grab_v2 {
	struct {
		struct wl_signal destroy;
	} events;
};

struct wlr_text_input_manager_v3 {
	struct {
		struct wl_signal text_input;
	} events;
};

struct wlr_input_method_manager_v2 {
	struct {
		struct wl_signal input_method;
	} events;
};

struct wlr_seat {
	struct {
		struct wlr_surface *focused_surface;
	} keyboard_state;
};

struct wlr_cursor {
	double x, y;
};

struct wlr_output_layout {
	int dummy;
};

struct wlr_output {
	int dummy;
};

/* --- Stub tracking globals --- */

static int g_ti_send_enter_count;
static struct wlr_surface *g_ti_send_enter_surface;
static int g_ti_send_leave_count;
static int g_ti_send_preedit_count;
static const char *g_ti_send_preedit_text;
static int g_ti_send_commit_string_count;
static const char *g_ti_send_commit_text;
static int g_ti_send_delete_count;
static int g_ti_send_done_count;

static int g_im_send_activate_count;
static int g_im_send_deactivate_count;
static int g_im_send_done_count;
static int g_im_send_surrounding_count;
static int g_im_send_text_change_cause_count;
static int g_im_send_content_type_count;
static int g_im_send_unavailable_count;
static int g_im_grab_set_keyboard_count;

/* --- Stub wlroots functions --- */

static struct wlr_text_input_manager_v3 g_ti_mgr;

static struct wlr_text_input_manager_v3 *
wlr_text_input_manager_v3_create(struct wl_display *display)
{
	(void)display;
	wl_signal_init(&g_ti_mgr.events.text_input);
	return &g_ti_mgr;
}

static struct wlr_input_method_manager_v2 g_im_mgr;

static struct wlr_input_method_manager_v2 *
wlr_input_method_manager_v2_create(struct wl_display *display)
{
	(void)display;
	wl_signal_init(&g_im_mgr.events.input_method);
	return &g_im_mgr;
}

static void
wlr_text_input_v3_send_enter(struct wlr_text_input_v3 *ti,
	struct wlr_surface *surface)
{
	ti->focused_surface = surface;
	g_ti_send_enter_count++;
	g_ti_send_enter_surface = surface;
}

static void
wlr_text_input_v3_send_leave(struct wlr_text_input_v3 *ti)
{
	ti->focused_surface = NULL;
	g_ti_send_leave_count++;
}

static void
wlr_text_input_v3_send_preedit_string(struct wlr_text_input_v3 *ti,
	const char *text, int32_t cursor_begin, int32_t cursor_end)
{
	(void)ti;
	(void)cursor_begin;
	(void)cursor_end;
	g_ti_send_preedit_count++;
	g_ti_send_preedit_text = text;
}

static void
wlr_text_input_v3_send_commit_string(struct wlr_text_input_v3 *ti,
	const char *text)
{
	(void)ti;
	g_ti_send_commit_string_count++;
	g_ti_send_commit_text = text;
}

static void
wlr_text_input_v3_send_delete_surrounding_text(
	struct wlr_text_input_v3 *ti,
	uint32_t before_length, uint32_t after_length)
{
	(void)ti;
	(void)before_length;
	(void)after_length;
	g_ti_send_delete_count++;
}

static void
wlr_text_input_v3_send_done(struct wlr_text_input_v3 *ti)
{
	(void)ti;
	g_ti_send_done_count++;
}

static void
wlr_input_method_v2_send_activate(struct wlr_input_method_v2 *im)
{
	(void)im;
	g_im_send_activate_count++;
}

static void
wlr_input_method_v2_send_deactivate(struct wlr_input_method_v2 *im)
{
	(void)im;
	g_im_send_deactivate_count++;
}

static void
wlr_input_method_v2_send_done(struct wlr_input_method_v2 *im)
{
	(void)im;
	g_im_send_done_count++;
}

static void
wlr_input_method_v2_send_surrounding_text(struct wlr_input_method_v2 *im,
	const char *text, uint32_t cursor, uint32_t anchor)
{
	(void)im;
	(void)text;
	(void)cursor;
	(void)anchor;
	g_im_send_surrounding_count++;
}

static void
wlr_input_method_v2_send_text_change_cause(struct wlr_input_method_v2 *im,
	uint32_t cause)
{
	(void)im;
	(void)cause;
	g_im_send_text_change_cause_count++;
}

static void
wlr_input_method_v2_send_content_type(struct wlr_input_method_v2 *im,
	uint32_t hint, uint32_t purpose)
{
	(void)im;
	(void)hint;
	(void)purpose;
	g_im_send_content_type_count++;
}

static void
wlr_input_method_v2_send_unavailable(struct wlr_input_method_v2 *im)
{
	(void)im;
	g_im_send_unavailable_count++;
}

static void
wlr_input_method_keyboard_grab_v2_set_keyboard(
	struct wlr_input_method_keyboard_grab_v2 *grab,
	struct wlr_keyboard *keyboard)
{
	(void)grab;
	(void)keyboard;
	g_im_grab_set_keyboard_count++;
}

static struct wlr_keyboard *g_seat_keyboard;

static struct wlr_keyboard *
wlr_seat_get_keyboard(struct wlr_seat *seat)
{
	(void)seat;
	return g_seat_keyboard;
}

static void
wlr_input_popup_surface_v2_send_text_input_rectangle(
	struct wlr_input_popup_surface_v2 *popup,
	struct wlr_box *box)
{
	(void)popup;
	(void)box;
}

static struct wlr_output *
wlr_output_layout_output_at(struct wlr_output_layout *layout,
	double lx, double ly)
{
	(void)layout;
	(void)lx;
	(void)ly;
	return NULL;
}

/* --- Stub fluxland functions --- */

/* Forward declaration so stubs use the same type as the definition below */
struct wm_server;

struct wm_output {
	struct wlr_box usable_area;
};

static struct wm_output *
wm_output_from_wlr(struct wm_server *server, struct wlr_output *output)
{
	(void)server;
	(void)output;
	return NULL;
}

/* --- Include text_input.h to get struct wm_text_input_relay --- */
#include "../src/text_input.h"

/* --- Minimal wm_server struct --- */

struct wm_server {
	struct wl_display *wl_display;
	struct wlr_seat *seat;
	struct wlr_cursor *cursor;
	struct wlr_output_layout *output_layout;
	struct wm_text_input_relay text_input_relay;
};

/* Forward declarations */
void wm_text_input_init(struct wm_server *server);
void wm_text_input_finish(struct wm_server *server);
void wm_text_input_focus_change(struct wm_server *server,
	struct wlr_surface *new_focus);

/* --- Include the source under test --- */
#include "../src/text_input.c"

/* --- Helper to reset all tracking globals --- */

static void
reset_globals(void)
{
	g_ti_send_enter_count = 0;
	g_ti_send_enter_surface = NULL;
	g_ti_send_leave_count = 0;
	g_ti_send_preedit_count = 0;
	g_ti_send_preedit_text = NULL;
	g_ti_send_commit_string_count = 0;
	g_ti_send_commit_text = NULL;
	g_ti_send_delete_count = 0;
	g_ti_send_done_count = 0;

	g_im_send_activate_count = 0;
	g_im_send_deactivate_count = 0;
	g_im_send_done_count = 0;
	g_im_send_surrounding_count = 0;
	g_im_send_text_change_cause_count = 0;
	g_im_send_content_type_count = 0;
	g_im_send_unavailable_count = 0;
	g_im_grab_set_keyboard_count = 0;
	g_seat_keyboard = NULL;
}

/* --- Test functions --- */

static void
test_init_finish(void)
{
	reset_globals();

	struct wl_display display = {0};
	struct wlr_seat seat = {0};
	struct wlr_cursor cursor = {0};
	struct wlr_output_layout layout = {0};

	struct wm_server server = {
		.wl_display = &display,
		.seat = &seat,
		.cursor = &cursor,
		.output_layout = &layout,
	};

	wm_text_input_init(&server);

	struct wm_text_input_relay *relay = &server.text_input_relay;
	assert(relay->server == &server);
	assert(relay->input_method == NULL);
	assert(wl_list_empty(&relay->text_inputs));
	assert(relay->text_input_mgr != NULL);
	assert(relay->input_method_mgr != NULL);

	wm_text_input_finish(&server);

	printf("PASS: test_init_finish\n");
}

static void
test_new_text_input_no_focus(void)
{
	reset_globals();

	struct wl_display display = {0};
	struct wlr_seat seat = { .keyboard_state = { .focused_surface = NULL } };
	struct wlr_cursor cursor = {0};
	struct wlr_output_layout layout = {0};

	struct wm_server server = {
		.wl_display = &display,
		.seat = &seat,
		.cursor = &cursor,
		.output_layout = &layout,
	};

	wm_text_input_init(&server);

	/* Create a text input with no focused surface */
	struct wl_client client = { .id = 1 };
	struct wl_resource resource = { .client = &client };
	struct wlr_text_input_v3 ti = {0};
	ti.resource = &resource;
	wl_signal_init(&ti.events.enable);
	wl_signal_init(&ti.events.commit);
	wl_signal_init(&ti.events.disable);
	wl_signal_init(&ti.events.destroy);

	server.text_input_relay.new_text_input.notify(
		&server.text_input_relay.new_text_input, &ti);

	/* No focused surface — should not send enter */
	assert(g_ti_send_enter_count == 0);
	assert(!wl_list_empty(&server.text_input_relay.text_inputs));

	wm_text_input_finish(&server);

	printf("PASS: test_new_text_input_no_focus\n");
}

static void
test_new_text_input_with_matching_focus(void)
{
	reset_globals();

	/* Set up client and surface owned by same client */
	struct wl_client client = { .id = 1 };
	struct wl_resource ti_resource = { .client = &client };
	struct wl_resource surface_resource = { .client = &client };
	struct wlr_surface surface = { .resource = &surface_resource };

	struct wl_display display = {0};
	struct wlr_seat seat = {
		.keyboard_state = { .focused_surface = &surface },
	};
	struct wlr_cursor cursor = {0};
	struct wlr_output_layout layout = {0};

	struct wm_server server = {
		.wl_display = &display,
		.seat = &seat,
		.cursor = &cursor,
		.output_layout = &layout,
	};

	wm_text_input_init(&server);

	struct wlr_text_input_v3 ti = {0};
	ti.resource = &ti_resource;
	wl_signal_init(&ti.events.enable);
	wl_signal_init(&ti.events.commit);
	wl_signal_init(&ti.events.disable);
	wl_signal_init(&ti.events.destroy);

	server.text_input_relay.new_text_input.notify(
		&server.text_input_relay.new_text_input, &ti);

	/* Same client owns surface — should send enter */
	assert(g_ti_send_enter_count == 1);
	assert(g_ti_send_enter_surface == &surface);

	wm_text_input_finish(&server);

	printf("PASS: test_new_text_input_with_matching_focus\n");
}

static void
test_new_text_input_client_mismatch(void)
{
	reset_globals();

	/* Text input client != surface client (the production bug) */
	struct wl_client ti_client = { .id = 1 };
	struct wl_client surface_client = { .id = 2 };
	struct wl_resource ti_resource = { .client = &ti_client };
	struct wl_resource surface_resource = { .client = &surface_client };
	struct wlr_surface surface = { .resource = &surface_resource };

	struct wl_display display = {0};
	struct wlr_seat seat = {
		.keyboard_state = { .focused_surface = &surface },
	};
	struct wlr_cursor cursor = {0};
	struct wlr_output_layout layout = {0};

	struct wm_server server = {
		.wl_display = &display,
		.seat = &seat,
		.cursor = &cursor,
		.output_layout = &layout,
	};

	wm_text_input_init(&server);

	struct wlr_text_input_v3 ti = {0};
	ti.resource = &ti_resource;
	wl_signal_init(&ti.events.enable);
	wl_signal_init(&ti.events.commit);
	wl_signal_init(&ti.events.disable);
	wl_signal_init(&ti.events.destroy);

	server.text_input_relay.new_text_input.notify(
		&server.text_input_relay.new_text_input, &ti);

	/* Different client — must NOT send enter (this was a real bug) */
	assert(g_ti_send_enter_count == 0);

	wm_text_input_finish(&server);

	printf("PASS: test_new_text_input_client_mismatch\n");
}

static void
test_single_im_per_seat(void)
{
	reset_globals();

	struct wl_display display = {0};
	struct wlr_seat seat = { .keyboard_state = { .focused_surface = NULL } };
	struct wlr_cursor cursor = {0};
	struct wlr_output_layout layout = {0};

	struct wm_server server = {
		.wl_display = &display,
		.seat = &seat,
		.cursor = &cursor,
		.output_layout = &layout,
	};

	wm_text_input_init(&server);

	/* Connect first input method */
	struct wlr_input_method_v2 im1 = {0};
	wl_signal_init(&im1.events.commit);
	wl_signal_init(&im1.events.grab_keyboard);
	wl_signal_init(&im1.events.destroy);
	wl_list_init(&im1.popup_surfaces);

	server.text_input_relay.new_input_method.notify(
		&server.text_input_relay.new_input_method, &im1);
	assert(server.text_input_relay.input_method == &im1);
	assert(g_im_send_unavailable_count == 0);

	/* Try to connect second input method — should be rejected */
	struct wlr_input_method_v2 im2 = {0};
	server.text_input_relay.new_input_method.notify(
		&server.text_input_relay.new_input_method, &im2);
	assert(g_im_send_unavailable_count == 1);
	/* First IM still active */
	assert(server.text_input_relay.input_method == &im1);

	wm_text_input_finish(&server);

	printf("PASS: test_single_im_per_seat\n");
}

static void
test_enable_activates_im(void)
{
	reset_globals();

	struct wl_display display = {0};
	struct wlr_seat seat = { .keyboard_state = { .focused_surface = NULL } };
	struct wlr_cursor cursor = {0};
	struct wlr_output_layout layout = {0};

	struct wm_server server = {
		.wl_display = &display,
		.seat = &seat,
		.cursor = &cursor,
		.output_layout = &layout,
	};

	wm_text_input_init(&server);

	/* Set up IM */
	struct wlr_input_method_v2 im = {0};
	wl_signal_init(&im.events.commit);
	wl_signal_init(&im.events.grab_keyboard);
	wl_signal_init(&im.events.destroy);
	wl_list_init(&im.popup_surfaces);

	server.text_input_relay.new_input_method.notify(
		&server.text_input_relay.new_input_method, &im);

	/* Add text input */
	struct wl_client client = { .id = 1 };
	struct wl_resource resource = { .client = &client };
	struct wlr_text_input_v3 ti = {0};
	ti.resource = &resource;
	ti.current.features = 0;
	wl_signal_init(&ti.events.enable);
	wl_signal_init(&ti.events.commit);
	wl_signal_init(&ti.events.disable);
	wl_signal_init(&ti.events.destroy);

	server.text_input_relay.new_text_input.notify(
		&server.text_input_relay.new_text_input, &ti);

	/* Fire the enable event on the text input */
	struct wl_listener *enable_listener =
		wl_container_of(ti.events.enable.listener_list.next,
			enable_listener, link);
	enable_listener->notify(enable_listener, NULL);

	assert(g_im_send_activate_count == 1);
	/* text_change_cause is always sent */
	assert(g_im_send_text_change_cause_count == 1);
	assert(g_im_send_done_count == 1);

	wm_text_input_finish(&server);

	printf("PASS: test_enable_activates_im\n");
}

static void
test_enable_no_im(void)
{
	reset_globals();

	struct wl_display display = {0};
	struct wlr_seat seat = { .keyboard_state = { .focused_surface = NULL } };
	struct wlr_cursor cursor = {0};
	struct wlr_output_layout layout = {0};

	struct wm_server server = {
		.wl_display = &display,
		.seat = &seat,
		.cursor = &cursor,
		.output_layout = &layout,
	};

	wm_text_input_init(&server);
	/* No IM connected */

	struct wl_client client = { .id = 1 };
	struct wl_resource resource = { .client = &client };
	struct wlr_text_input_v3 ti = {0};
	ti.resource = &resource;
	wl_signal_init(&ti.events.enable);
	wl_signal_init(&ti.events.commit);
	wl_signal_init(&ti.events.disable);
	wl_signal_init(&ti.events.destroy);

	server.text_input_relay.new_text_input.notify(
		&server.text_input_relay.new_text_input, &ti);

	/* Enable with no IM — should be a no-op */
	struct wl_listener *enable_listener =
		wl_container_of(ti.events.enable.listener_list.next,
			enable_listener, link);
	enable_listener->notify(enable_listener, NULL);

	assert(g_im_send_activate_count == 0);

	wm_text_input_finish(&server);

	printf("PASS: test_enable_no_im\n");
}

static void
test_disable_deactivates_im(void)
{
	reset_globals();

	struct wl_display display = {0};
	struct wlr_seat seat = { .keyboard_state = { .focused_surface = NULL } };
	struct wlr_cursor cursor = {0};
	struct wlr_output_layout layout = {0};

	struct wm_server server = {
		.wl_display = &display,
		.seat = &seat,
		.cursor = &cursor,
		.output_layout = &layout,
	};

	wm_text_input_init(&server);

	struct wlr_input_method_v2 im = {0};
	wl_signal_init(&im.events.commit);
	wl_signal_init(&im.events.grab_keyboard);
	wl_signal_init(&im.events.destroy);
	wl_list_init(&im.popup_surfaces);

	server.text_input_relay.new_input_method.notify(
		&server.text_input_relay.new_input_method, &im);

	struct wl_client client = { .id = 1 };
	struct wl_resource resource = { .client = &client };
	struct wlr_text_input_v3 ti = {0};
	ti.resource = &resource;
	wl_signal_init(&ti.events.enable);
	wl_signal_init(&ti.events.commit);
	wl_signal_init(&ti.events.disable);
	wl_signal_init(&ti.events.destroy);

	server.text_input_relay.new_text_input.notify(
		&server.text_input_relay.new_text_input, &ti);

	/* Fire disable event */
	struct wl_listener *disable_listener =
		wl_container_of(ti.events.disable.listener_list.next,
			disable_listener, link);
	disable_listener->notify(disable_listener, NULL);

	assert(g_im_send_deactivate_count == 1);
	assert(g_im_send_done_count == 1);

	wm_text_input_finish(&server);

	printf("PASS: test_disable_deactivates_im\n");
}

static void
test_im_commit_forwards_preedit(void)
{
	reset_globals();

	struct wl_client client = { .id = 1 };
	struct wl_resource ti_resource = { .client = &client };
	struct wl_resource surface_resource = { .client = &client };
	struct wlr_surface surface = { .resource = &surface_resource };

	struct wl_display display = {0};
	struct wlr_seat seat = {
		.keyboard_state = { .focused_surface = &surface },
	};
	struct wlr_cursor cursor = {0};
	struct wlr_output_layout layout = {0};

	struct wm_server server = {
		.wl_display = &display,
		.seat = &seat,
		.cursor = &cursor,
		.output_layout = &layout,
	};

	wm_text_input_init(&server);

	/* Set up IM */
	struct wlr_input_method_v2 im = {0};
	wl_signal_init(&im.events.commit);
	wl_signal_init(&im.events.grab_keyboard);
	wl_signal_init(&im.events.destroy);
	wl_list_init(&im.popup_surfaces);
	im.current.preedit.text = "hello";
	im.current.preedit.cursor_begin = 0;
	im.current.preedit.cursor_end = 5;
	im.current.commit_text = "world";
	im.current.delete.before_length = 1;
	im.current.delete.after_length = 2;

	server.text_input_relay.new_input_method.notify(
		&server.text_input_relay.new_input_method, &im);

	/* Add text input that is enabled and focused */
	struct wlr_text_input_v3 ti = {0};
	ti.resource = &ti_resource;
	ti.focused_surface = &surface;
	ti.current_enabled = true;
	wl_signal_init(&ti.events.enable);
	wl_signal_init(&ti.events.commit);
	wl_signal_init(&ti.events.disable);
	wl_signal_init(&ti.events.destroy);

	server.text_input_relay.new_text_input.notify(
		&server.text_input_relay.new_text_input, &ti);

	/* Fire IM commit */
	struct wl_listener *im_commit =
		wl_container_of(im.events.commit.listener_list.next,
			im_commit, link);
	im_commit->notify(im_commit, NULL);

	assert(g_ti_send_preedit_count == 1);
	assert(strcmp(g_ti_send_preedit_text, "hello") == 0);
	assert(g_ti_send_commit_string_count == 1);
	assert(strcmp(g_ti_send_commit_text, "world") == 0);
	assert(g_ti_send_delete_count == 1);
	assert(g_ti_send_done_count == 1);

	wm_text_input_finish(&server);

	printf("PASS: test_im_commit_forwards_preedit\n");
}

static void
test_im_commit_no_focused_ti(void)
{
	reset_globals();

	struct wl_display display = {0};
	struct wlr_seat seat = { .keyboard_state = { .focused_surface = NULL } };
	struct wlr_cursor cursor = {0};
	struct wlr_output_layout layout = {0};

	struct wm_server server = {
		.wl_display = &display,
		.seat = &seat,
		.cursor = &cursor,
		.output_layout = &layout,
	};

	wm_text_input_init(&server);

	/* Set up IM */
	struct wlr_input_method_v2 im = {0};
	wl_signal_init(&im.events.commit);
	wl_signal_init(&im.events.grab_keyboard);
	wl_signal_init(&im.events.destroy);
	wl_list_init(&im.popup_surfaces);
	im.current.preedit.text = "test";

	server.text_input_relay.new_input_method.notify(
		&server.text_input_relay.new_input_method, &im);

	/* Fire IM commit with no focused text input */
	struct wl_listener *im_commit =
		wl_container_of(im.events.commit.listener_list.next,
			im_commit, link);
	im_commit->notify(im_commit, NULL);

	/* Should not forward anything */
	assert(g_ti_send_preedit_count == 0);
	assert(g_ti_send_done_count == 0);

	wm_text_input_finish(&server);

	printf("PASS: test_im_commit_no_focused_ti\n");
}

static void
test_im_destroy_clears_preedit(void)
{
	reset_globals();

	struct wl_display display = {0};
	struct wlr_seat seat = { .keyboard_state = { .focused_surface = NULL } };
	struct wlr_cursor cursor = {0};
	struct wlr_output_layout layout = {0};

	struct wm_server server = {
		.wl_display = &display,
		.seat = &seat,
		.cursor = &cursor,
		.output_layout = &layout,
	};

	wm_text_input_init(&server);

	/* Set up IM */
	struct wlr_input_method_v2 im = {0};
	wl_signal_init(&im.events.commit);
	wl_signal_init(&im.events.grab_keyboard);
	wl_signal_init(&im.events.destroy);
	wl_list_init(&im.popup_surfaces);

	server.text_input_relay.new_input_method.notify(
		&server.text_input_relay.new_input_method, &im);

	/* Add an enabled text input */
	struct wl_client client = { .id = 1 };
	struct wl_resource resource = { .client = &client };
	struct wlr_text_input_v3 ti = {0};
	ti.resource = &resource;
	ti.current_enabled = true;
	wl_signal_init(&ti.events.enable);
	wl_signal_init(&ti.events.commit);
	wl_signal_init(&ti.events.disable);
	wl_signal_init(&ti.events.destroy);

	server.text_input_relay.new_text_input.notify(
		&server.text_input_relay.new_text_input, &ti);

	/* Destroy the IM */
	struct wl_listener *im_destroy =
		wl_container_of(im.events.destroy.listener_list.next,
			im_destroy, link);
	im_destroy->notify(im_destroy, NULL);

	assert(server.text_input_relay.input_method == NULL);
	/* Should clear preedit on enabled text inputs */
	assert(g_ti_send_preedit_count == 1);
	assert(g_ti_send_preedit_text == NULL);
	assert(g_ti_send_done_count == 1);

	/* Finish without IM cleanup since it's already destroyed */
	wl_list_remove(&server.text_input_relay.new_text_input.link);
	wl_list_remove(&server.text_input_relay.new_input_method.link);

	printf("PASS: test_im_destroy_clears_preedit\n");
}

static void
test_focus_change_enter_leave(void)
{
	reset_globals();

	struct wl_client client = { .id = 1 };
	struct wl_resource ti_resource = { .client = &client };
	struct wl_resource old_surface_resource = { .client = &client };
	struct wl_resource new_surface_resource = { .client = &client };
	struct wlr_surface old_surface = { .resource = &old_surface_resource };
	struct wlr_surface new_surface = { .resource = &new_surface_resource };

	struct wl_display display = {0};
	struct wlr_seat seat = {
		.keyboard_state = { .focused_surface = NULL },
	};
	struct wlr_cursor cursor = {0};
	struct wlr_output_layout layout = {0};

	struct wm_server server = {
		.wl_display = &display,
		.seat = &seat,
		.cursor = &cursor,
		.output_layout = &layout,
	};

	wm_text_input_init(&server);

	/* Add text input focused on old_surface */
	struct wlr_text_input_v3 ti = {0};
	ti.resource = &ti_resource;
	ti.focused_surface = &old_surface;
	ti.current_enabled = false;
	wl_signal_init(&ti.events.enable);
	wl_signal_init(&ti.events.commit);
	wl_signal_init(&ti.events.disable);
	wl_signal_init(&ti.events.destroy);

	server.text_input_relay.new_text_input.notify(
		&server.text_input_relay.new_text_input, &ti);
	/* Reset after initial enter */
	g_ti_send_enter_count = 0;

	/* Change focus to new surface (same client) */
	wm_text_input_focus_change(&server, &new_surface);

	assert(g_ti_send_leave_count == 1);
	assert(g_ti_send_enter_count == 1);
	assert(g_ti_send_enter_surface == &new_surface);

	wm_text_input_finish(&server);

	printf("PASS: test_focus_change_enter_leave\n");
}

static void
test_focus_change_client_mismatch(void)
{
	reset_globals();

	struct wl_client ti_client = { .id = 1 };
	struct wl_client surface_client = { .id = 2 };
	struct wl_resource ti_resource = { .client = &ti_client };
	struct wl_resource new_surface_resource = { .client = &surface_client };
	struct wlr_surface new_surface = { .resource = &new_surface_resource };

	struct wl_display display = {0};
	struct wlr_seat seat = {
		.keyboard_state = { .focused_surface = NULL },
	};
	struct wlr_cursor cursor = {0};
	struct wlr_output_layout layout = {0};

	struct wm_server server = {
		.wl_display = &display,
		.seat = &seat,
		.cursor = &cursor,
		.output_layout = &layout,
	};

	wm_text_input_init(&server);

	struct wlr_text_input_v3 ti = {0};
	ti.resource = &ti_resource;
	ti.focused_surface = NULL;
	wl_signal_init(&ti.events.enable);
	wl_signal_init(&ti.events.commit);
	wl_signal_init(&ti.events.disable);
	wl_signal_init(&ti.events.destroy);

	server.text_input_relay.new_text_input.notify(
		&server.text_input_relay.new_text_input, &ti);

	/* Focus change to surface owned by different client */
	wm_text_input_focus_change(&server, &new_surface);

	/* Must NOT send enter — different client (the critical guard) */
	assert(g_ti_send_enter_count == 0);

	wm_text_input_finish(&server);

	printf("PASS: test_focus_change_client_mismatch\n");
}

static void
test_focus_change_deactivates_enabled_ti(void)
{
	reset_globals();

	struct wl_client client = { .id = 1 };
	struct wl_resource ti_resource = { .client = &client };
	struct wl_resource old_surface_resource = { .client = &client };
	struct wl_resource new_surface_resource = { .client = &client };
	struct wlr_surface old_surface = { .resource = &old_surface_resource };
	struct wlr_surface new_surface = { .resource = &new_surface_resource };

	struct wl_display display = {0};
	struct wlr_seat seat = {
		.keyboard_state = { .focused_surface = NULL },
	};
	struct wlr_cursor cursor = {0};
	struct wlr_output_layout layout = {0};

	struct wm_server server = {
		.wl_display = &display,
		.seat = &seat,
		.cursor = &cursor,
		.output_layout = &layout,
	};

	wm_text_input_init(&server);

	/* Set up IM */
	struct wlr_input_method_v2 im = {0};
	wl_signal_init(&im.events.commit);
	wl_signal_init(&im.events.grab_keyboard);
	wl_signal_init(&im.events.destroy);
	wl_list_init(&im.popup_surfaces);

	server.text_input_relay.new_input_method.notify(
		&server.text_input_relay.new_input_method, &im);

	/* Add enabled text input on old surface */
	struct wlr_text_input_v3 ti = {0};
	ti.resource = &ti_resource;
	ti.focused_surface = &old_surface;
	ti.current_enabled = true;
	wl_signal_init(&ti.events.enable);
	wl_signal_init(&ti.events.commit);
	wl_signal_init(&ti.events.disable);
	wl_signal_init(&ti.events.destroy);

	server.text_input_relay.new_text_input.notify(
		&server.text_input_relay.new_text_input, &ti);
	reset_globals();

	/* Focus change — should deactivate IM for old surface */
	wm_text_input_focus_change(&server, &new_surface);

	assert(g_im_send_deactivate_count == 1);
	assert(g_im_send_done_count == 1);
	assert(g_ti_send_leave_count == 1);

	wm_text_input_finish(&server);

	printf("PASS: test_focus_change_deactivates_enabled_ti\n");
}

static void
test_ti_destroy_cleans_up(void)
{
	reset_globals();

	struct wl_display display = {0};
	struct wlr_seat seat = { .keyboard_state = { .focused_surface = NULL } };
	struct wlr_cursor cursor = {0};
	struct wlr_output_layout layout = {0};

	struct wm_server server = {
		.wl_display = &display,
		.seat = &seat,
		.cursor = &cursor,
		.output_layout = &layout,
	};

	wm_text_input_init(&server);

	struct wl_client client = { .id = 1 };
	struct wl_resource resource = { .client = &client };
	struct wlr_text_input_v3 ti = {0};
	ti.resource = &resource;
	wl_signal_init(&ti.events.enable);
	wl_signal_init(&ti.events.commit);
	wl_signal_init(&ti.events.disable);
	wl_signal_init(&ti.events.destroy);

	server.text_input_relay.new_text_input.notify(
		&server.text_input_relay.new_text_input, &ti);
	assert(!wl_list_empty(&server.text_input_relay.text_inputs));

	/* Fire destroy event */
	struct wl_listener *destroy_listener =
		wl_container_of(ti.events.destroy.listener_list.next,
			destroy_listener, link);
	destroy_listener->notify(destroy_listener, NULL);

	/* Text input should be removed from list */
	assert(wl_list_empty(&server.text_input_relay.text_inputs));

	wm_text_input_finish(&server);

	printf("PASS: test_ti_destroy_cleans_up\n");
}

static void
test_relay_send_im_state_features(void)
{
	reset_globals();

	struct wl_client client = { .id = 1 };
	struct wl_resource ti_resource = { .client = &client };
	struct wl_resource surface_resource = { .client = &client };
	struct wlr_surface surface = { .resource = &surface_resource };

	struct wl_display display = {0};
	struct wlr_seat seat = {
		.keyboard_state = { .focused_surface = &surface },
	};
	struct wlr_cursor cursor = {0};
	struct wlr_output_layout layout = {0};

	struct wm_server server = {
		.wl_display = &display,
		.seat = &seat,
		.cursor = &cursor,
		.output_layout = &layout,
	};

	wm_text_input_init(&server);

	/* Set up IM */
	struct wlr_input_method_v2 im = {0};
	wl_signal_init(&im.events.commit);
	wl_signal_init(&im.events.grab_keyboard);
	wl_signal_init(&im.events.destroy);
	wl_list_init(&im.popup_surfaces);

	server.text_input_relay.new_input_method.notify(
		&server.text_input_relay.new_input_method, &im);

	/* Add text input with surrounding text + content type features */
	struct wlr_text_input_v3 ti = {0};
	ti.resource = &ti_resource;
	ti.current.features = WLR_TEXT_INPUT_V3_FEATURE_SURROUNDING_TEXT |
		WLR_TEXT_INPUT_V3_FEATURE_CONTENT_TYPE;
	ti.current.surrounding.text = "hello";
	ti.current.surrounding.cursor = 5;
	ti.current.surrounding.anchor = 0;
	ti.current.content_type.hint = 1;
	ti.current.content_type.purpose = 2;
	wl_signal_init(&ti.events.enable);
	wl_signal_init(&ti.events.commit);
	wl_signal_init(&ti.events.disable);
	wl_signal_init(&ti.events.destroy);

	server.text_input_relay.new_text_input.notify(
		&server.text_input_relay.new_text_input, &ti);

	/* Fire enable — triggers relay_send_im_state */
	struct wl_listener *enable_listener =
		wl_container_of(ti.events.enable.listener_list.next,
			enable_listener, link);
	enable_listener->notify(enable_listener, NULL);

	assert(g_im_send_surrounding_count == 1);
	assert(g_im_send_content_type_count == 1);
	assert(g_im_send_text_change_cause_count == 1);
	assert(g_im_send_done_count == 1);

	wm_text_input_finish(&server);

	printf("PASS: test_relay_send_im_state_features\n");
}

static void
test_im_connect_activates_focused(void)
{
	reset_globals();

	struct wl_client client = { .id = 1 };
	struct wl_resource ti_resource = { .client = &client };
	struct wl_resource surface_resource = { .client = &client };
	struct wlr_surface surface = { .resource = &surface_resource };

	struct wl_display display = {0};
	struct wlr_seat seat = {
		.keyboard_state = { .focused_surface = &surface },
	};
	struct wlr_cursor cursor = {0};
	struct wlr_output_layout layout = {0};

	struct wm_server server = {
		.wl_display = &display,
		.seat = &seat,
		.cursor = &cursor,
		.output_layout = &layout,
	};

	wm_text_input_init(&server);

	/* Add an enabled, focused text input BEFORE the IM connects */
	struct wlr_text_input_v3 ti = {0};
	ti.resource = &ti_resource;
	ti.focused_surface = &surface;
	ti.current_enabled = true;
	wl_signal_init(&ti.events.enable);
	wl_signal_init(&ti.events.commit);
	wl_signal_init(&ti.events.disable);
	wl_signal_init(&ti.events.destroy);

	server.text_input_relay.new_text_input.notify(
		&server.text_input_relay.new_text_input, &ti);
	reset_globals();

	/* Now connect IM — should auto-activate for the focused TI */
	struct wlr_input_method_v2 im = {0};
	wl_signal_init(&im.events.commit);
	wl_signal_init(&im.events.grab_keyboard);
	wl_signal_init(&im.events.destroy);
	wl_list_init(&im.popup_surfaces);

	server.text_input_relay.new_input_method.notify(
		&server.text_input_relay.new_input_method, &im);

	assert(g_im_send_activate_count == 1);

	wm_text_input_finish(&server);

	printf("PASS: test_im_connect_activates_focused\n");
}

static void
test_keyboard_grab(void)
{
	reset_globals();

	struct wl_display display = {0};
	struct wlr_seat seat = { .keyboard_state = { .focused_surface = NULL } };
	struct wlr_cursor cursor = {0};
	struct wlr_output_layout layout = {0};

	struct wm_server server = {
		.wl_display = &display,
		.seat = &seat,
		.cursor = &cursor,
		.output_layout = &layout,
	};

	wm_text_input_init(&server);

	/* Set up IM */
	struct wlr_input_method_v2 im = {0};
	wl_signal_init(&im.events.commit);
	wl_signal_init(&im.events.grab_keyboard);
	wl_signal_init(&im.events.destroy);
	wl_list_init(&im.popup_surfaces);

	server.text_input_relay.new_input_method.notify(
		&server.text_input_relay.new_input_method, &im);

	/* Set up a keyboard on the seat */
	struct wlr_keyboard kb = {0};
	g_seat_keyboard = &kb;

	/* Fire grab_keyboard event */
	struct wlr_input_method_keyboard_grab_v2 grab = {0};
	wl_signal_init(&grab.events.destroy);

	struct wl_listener *grab_listener =
		wl_container_of(im.events.grab_keyboard.listener_list.next,
			grab_listener, link);
	grab_listener->notify(grab_listener, &grab);

	assert(g_im_grab_set_keyboard_count == 1);

	wm_text_input_finish(&server);

	printf("PASS: test_keyboard_grab\n");
}

/* --- main --- */

int
main(void)
{
	test_init_finish();
	test_new_text_input_no_focus();
	test_new_text_input_with_matching_focus();
	test_new_text_input_client_mismatch();
	test_single_im_per_seat();
	test_enable_activates_im();
	test_enable_no_im();
	test_disable_deactivates_im();
	test_im_commit_forwards_preedit();
	test_im_commit_no_focused_ti();
	test_im_destroy_clears_preedit();
	test_focus_change_enter_leave();
	test_focus_change_client_mismatch();
	test_focus_change_deactivates_enabled_ti();
	test_ti_destroy_cleans_up();
	test_relay_send_im_state_features();
	test_im_connect_activates_focused();
	test_keyboard_grab();

	printf("\nAll text_input tests passed! (18/18)\n");
	return 0;
}
