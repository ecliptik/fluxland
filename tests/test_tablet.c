/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * test_tablet.c - Unit tests for tablet/stylus input via tablet-v2 protocol
 *
 * Includes tablet.c directly with stub implementations to avoid
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
#define WLR_TYPES_WLR_INPUT_DEVICE_H
#define WLR_TYPES_WLR_OUTPUT_LAYOUT_H
#define WLR_TYPES_WLR_SCENE_H
#define WLR_TYPES_WLR_SEAT_H
#define WLR_TYPES_WLR_TABLET_PAD_H
#define WLR_TYPES_TABLET_TOOL_H
#define WLR_TYPES_WLR_TABLET_V2_H

/* fluxland */
#define WM_SERVER_H
#define WM_IDLE_H
#define WM_VIEW_H

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
};

struct wlr_input_device {
	enum wlr_input_device_type type;
	const char *name;
	struct {
		struct wl_signal destroy;
	} events;
};

struct wlr_box {
	int x, y, width, height;
};

enum wlr_scene_node_type {
	WLR_SCENE_NODE_TREE = 0,
	WLR_SCENE_NODE_BUFFER,
};

struct wlr_scene_tree;

struct wlr_scene_node {
	enum wlr_scene_node_type type;
	void *data;
	struct wlr_scene_tree *parent;
	int x, y;
};

struct wlr_scene_tree {
	struct wlr_scene_node node;
};

struct wlr_scene {
	struct wlr_scene_tree tree;
};

struct wlr_scene_buffer {
	struct wlr_scene_node node;
};

struct wlr_surface {
	int dummy;
};

struct wlr_scene_surface {
	struct wlr_surface *surface;
};

struct wlr_cursor {
	double x, y;
};

struct wlr_output_layout {
	int dummy;
};

struct wlr_seat {
	int dummy;
};

struct wlr_tablet_tool {
	void *data;
};

struct wlr_tablet {
	struct wlr_input_device base;
	struct {
		struct wl_signal axis;
		struct wl_signal proximity;
		struct wl_signal tip;
		struct wl_signal button;
	} events;
};

struct wlr_tablet_pad {
	struct wlr_input_device base;
	struct {
		struct wl_signal button;
		struct wl_signal ring;
		struct wl_signal strip;
		struct wl_signal attach_tablet;
	} events;
	size_t button_count;
	size_t ring_count;
	size_t strip_count;
};

struct wlr_tablet_v2_tablet {
	int dummy;
};

struct wlr_tablet_v2_tablet_tool {
	struct wlr_surface *focused_surface;
};

struct wlr_tablet_v2_tablet_pad {
	int dummy;
};

struct wlr_tablet_manager_v2 {
	int dummy;
};

/* Tablet tool axis flags */
#define WLR_TABLET_TOOL_AXIS_X        (1 << 0)
#define WLR_TABLET_TOOL_AXIS_Y        (1 << 1)
#define WLR_TABLET_TOOL_AXIS_PRESSURE (1 << 2)
#define WLR_TABLET_TOOL_AXIS_DISTANCE (1 << 3)
#define WLR_TABLET_TOOL_AXIS_TILT_X   (1 << 4)
#define WLR_TABLET_TOOL_AXIS_TILT_Y   (1 << 5)
#define WLR_TABLET_TOOL_AXIS_ROTATION (1 << 6)
#define WLR_TABLET_TOOL_AXIS_SLIDER   (1 << 7)
#define WLR_TABLET_TOOL_AXIS_WHEEL    (1 << 8)

enum wlr_tablet_tool_proximity_state {
	WLR_TABLET_TOOL_PROXIMITY_OUT = 0,
	WLR_TABLET_TOOL_PROXIMITY_IN,
};

enum wlr_tablet_tool_tip_state {
	WLR_TABLET_TOOL_TIP_UP = 0,
	WLR_TABLET_TOOL_TIP_DOWN,
};

enum wlr_button_state {
	WLR_BUTTON_RELEASED = 0,
	WLR_BUTTON_PRESSED,
};

/* Tablet pad button state (from zwp_tablet_pad_v2) */
enum zwp_tablet_pad_v2_button_state {
	ZWP_TABLET_PAD_V2_BUTTON_STATE_RELEASED = 0,
	ZWP_TABLET_PAD_V2_BUTTON_STATE_PRESSED,
};

enum wlr_tablet_pad_ring_source {
	WLR_TABLET_PAD_RING_SOURCE_UNKNOWN = 0,
	WLR_TABLET_PAD_RING_SOURCE_FINGER,
};

enum wlr_tablet_pad_strip_source {
	WLR_TABLET_PAD_STRIP_SOURCE_UNKNOWN = 0,
	WLR_TABLET_PAD_STRIP_SOURCE_FINGER,
};

struct wlr_tablet_tool_axis_event {
	struct wlr_tablet *tablet;
	struct wlr_tablet_tool *tool;
	uint32_t time_msec;
	uint32_t updated_axes;
	double x, y;
	double pressure;
	double distance;
	double tilt_x, tilt_y;
	double rotation;
	double slider;
	double wheel_delta;
};

struct wlr_tablet_tool_proximity_event {
	struct wlr_tablet *tablet;
	struct wlr_tablet_tool *tool;
	uint32_t time_msec;
	double x, y;
	enum wlr_tablet_tool_proximity_state state;
};

struct wlr_tablet_tool_tip_event {
	struct wlr_tablet *tablet;
	struct wlr_tablet_tool *tool;
	uint32_t time_msec;
	double x, y;
	enum wlr_tablet_tool_tip_state state;
};

struct wlr_tablet_tool_button_event {
	struct wlr_tablet *tablet;
	struct wlr_tablet_tool *tool;
	uint32_t time_msec;
	uint32_t button;
	enum wlr_button_state state;
};

struct wlr_tablet_pad_button_event {
	uint32_t time_msec;
	uint32_t button;
	enum wlr_button_state state;
};

struct wlr_tablet_pad_ring_event {
	uint32_t time_msec;
	uint32_t ring;
	double position;
	enum wlr_tablet_pad_ring_source source;
};

struct wlr_tablet_pad_strip_event {
	uint32_t time_msec;
	uint32_t strip;
	double position;
	enum wlr_tablet_pad_strip_source source;
};

/* --- Stub fluxland types --- */

struct wm_view {
	int id;
};

struct wm_idle {
	int dummy;
};

/* --- Stub wm_server struct --- */

struct wm_server {
	struct wl_display *wl_display;
	struct wlr_scene *scene;
	struct wlr_cursor *cursor;
	struct wlr_seat *seat;
	struct wlr_output_layout *output_layout;
	struct wlr_tablet_manager_v2 *tablet_manager;
	struct wl_list tablets;
	struct wl_list tablet_pads;
	struct wm_idle idle;
};

/* Forward declarations */
void wm_tablet_init(struct wm_server *server);
void wm_tablet_finish(struct wm_server *server);
void wm_tablet_tool_setup(struct wm_server *server,
	struct wlr_input_device *device);
void wm_tablet_pad_setup(struct wm_server *server,
	struct wlr_input_device *device);

/* --- Stub tracking globals --- */

static int g_idle_notify_count;
static int g_cursor_warp_count;
static int g_tool_v2_create_count;
static int g_tablet_v2_create_count;
static int g_pad_v2_create_count;
static int g_proximity_in_count;
static int g_proximity_out_count;
static int g_tool_motion_count;
static int g_tool_down_count;
static int g_tool_up_count;
static int g_tool_button_count;
static int g_tool_pressure_count;
static int g_tool_distance_count;
static int g_tool_tilt_count;
static int g_tool_rotation_count;
static int g_tool_slider_count;
static int g_tool_wheel_count;
static int g_pad_button_count;
static int g_pad_ring_count;
static int g_pad_strip_count;
static int g_seat_pointer_enter_count;
static int g_seat_pointer_motion_count;
static int g_seat_pointer_clear_count;
static int g_focus_view_count;
static struct wm_view *g_focus_view_last;

/* Scene node stubbing */
static struct wlr_scene_node *g_scene_node_at_result;
static double g_scene_node_at_sx;
static double g_scene_node_at_sy;
static struct wlr_scene_surface *g_scene_surface_result;
static bool g_surface_accepts_tablet;

/* --- Stub wlroots functions --- */

static void
wm_idle_notify_activity(struct wm_server *server)
{
	(void)server;
	g_idle_notify_count++;
}

static void
wlr_cursor_warp_absolute(struct wlr_cursor *cursor,
	struct wlr_input_device *dev, double x, double y)
{
	(void)dev;
	cursor->x = x * 1920;
	cursor->y = y * 1080;
	g_cursor_warp_count++;
}

static struct wlr_scene_node *
wlr_scene_node_at(struct wlr_scene_node *root,
	double lx, double ly, double *sx, double *sy)
{
	(void)root;
	(void)lx;
	(void)ly;
	if (sx) *sx = g_scene_node_at_sx;
	if (sy) *sy = g_scene_node_at_sy;
	return g_scene_node_at_result;
}

static struct wlr_scene_buffer *
wlr_scene_buffer_from_node(struct wlr_scene_node *node)
{
	return (struct wlr_scene_buffer *)node;
}

static struct wlr_scene_surface *
wlr_scene_surface_try_from_buffer(struct wlr_scene_buffer *buffer)
{
	(void)buffer;
	return g_scene_surface_result;
}

static bool
wlr_surface_accepts_tablet_v2(struct wlr_tablet_v2_tablet *tablet,
	struct wlr_surface *surface)
{
	(void)tablet;
	(void)surface;
	return g_surface_accepts_tablet;
}

static struct wlr_tablet_v2_tablet g_tablet_v2;

static struct wlr_tablet_v2_tablet *
wlr_tablet_create(struct wlr_tablet_manager_v2 *manager,
	struct wlr_seat *seat, struct wlr_input_device *device)
{
	(void)manager;
	(void)seat;
	(void)device;
	g_tablet_v2_create_count++;
	return &g_tablet_v2;
}

static struct wlr_tablet_v2_tablet_tool g_tool_v2;

static struct wlr_tablet_v2_tablet_tool *
wlr_tablet_tool_create(struct wlr_tablet_manager_v2 *manager,
	struct wlr_seat *seat, struct wlr_tablet_tool *tool)
{
	(void)manager;
	(void)seat;
	(void)tool;
	g_tool_v2_create_count++;
	return &g_tool_v2;
}

static struct wlr_tablet *
wlr_tablet_from_input_device(struct wlr_input_device *device)
{
	return (struct wlr_tablet *)device;
}

static struct wlr_tablet_v2_tablet_pad g_pad_v2;

static struct wlr_tablet_v2_tablet_pad *
wlr_tablet_pad_create(struct wlr_tablet_manager_v2 *manager,
	struct wlr_seat *seat, struct wlr_input_device *device)
{
	(void)manager;
	(void)seat;
	(void)device;
	g_pad_v2_create_count++;
	return &g_pad_v2;
}

static struct wlr_tablet_pad *
wlr_tablet_pad_from_input_device(struct wlr_input_device *device)
{
	return (struct wlr_tablet_pad *)device;
}

static struct wlr_tablet_manager_v2 g_tablet_mgr;

static struct wlr_tablet_manager_v2 *
wlr_tablet_v2_create(struct wl_display *display)
{
	(void)display;
	return &g_tablet_mgr;
}

/* Tablet v2 protocol send stubs */
static void
wlr_send_tablet_v2_tablet_tool_proximity_in(
	struct wlr_tablet_v2_tablet_tool *tool,
	struct wlr_tablet_v2_tablet *tablet,
	struct wlr_surface *surface)
{
	(void)tablet;
	tool->focused_surface = surface;
	g_proximity_in_count++;
}

static void
wlr_send_tablet_v2_tablet_tool_proximity_out(
	struct wlr_tablet_v2_tablet_tool *tool)
{
	tool->focused_surface = NULL;
	g_proximity_out_count++;
}

static void
wlr_send_tablet_v2_tablet_tool_motion(
	struct wlr_tablet_v2_tablet_tool *tool,
	double sx, double sy)
{
	(void)tool;
	(void)sx;
	(void)sy;
	g_tool_motion_count++;
}

static void
wlr_send_tablet_v2_tablet_tool_down(
	struct wlr_tablet_v2_tablet_tool *tool)
{
	(void)tool;
	g_tool_down_count++;
}

static void
wlr_send_tablet_v2_tablet_tool_up(
	struct wlr_tablet_v2_tablet_tool *tool)
{
	(void)tool;
	g_tool_up_count++;
}

static void
wlr_send_tablet_v2_tablet_tool_button(
	struct wlr_tablet_v2_tablet_tool *tool,
	uint32_t button, enum zwp_tablet_pad_v2_button_state state)
{
	(void)tool;
	(void)button;
	(void)state;
	g_tool_button_count++;
}

static void
wlr_send_tablet_v2_tablet_tool_pressure(
	struct wlr_tablet_v2_tablet_tool *tool, double pressure)
{
	(void)tool;
	(void)pressure;
	g_tool_pressure_count++;
}

static void
wlr_send_tablet_v2_tablet_tool_distance(
	struct wlr_tablet_v2_tablet_tool *tool, double distance)
{
	(void)tool;
	(void)distance;
	g_tool_distance_count++;
}

static void
wlr_send_tablet_v2_tablet_tool_tilt(
	struct wlr_tablet_v2_tablet_tool *tool,
	double tilt_x, double tilt_y)
{
	(void)tool;
	(void)tilt_x;
	(void)tilt_y;
	g_tool_tilt_count++;
}

static void
wlr_send_tablet_v2_tablet_tool_rotation(
	struct wlr_tablet_v2_tablet_tool *tool, double rotation)
{
	(void)tool;
	(void)rotation;
	g_tool_rotation_count++;
}

static void
wlr_send_tablet_v2_tablet_tool_slider(
	struct wlr_tablet_v2_tablet_tool *tool, double slider)
{
	(void)tool;
	(void)slider;
	g_tool_slider_count++;
}

static void
wlr_send_tablet_v2_tablet_tool_wheel(
	struct wlr_tablet_v2_tablet_tool *tool,
	double degrees, int32_t clicks)
{
	(void)tool;
	(void)degrees;
	(void)clicks;
	g_tool_wheel_count++;
}

static void
wlr_send_tablet_v2_tablet_pad_button(
	struct wlr_tablet_v2_tablet_pad *pad,
	uint32_t button, uint32_t time_msec,
	enum zwp_tablet_pad_v2_button_state state)
{
	(void)pad;
	(void)button;
	(void)time_msec;
	(void)state;
	g_pad_button_count++;
}

static void
wlr_send_tablet_v2_tablet_pad_ring(
	struct wlr_tablet_v2_tablet_pad *pad,
	uint32_t ring, double position, bool finger,
	uint32_t time_msec)
{
	(void)pad;
	(void)ring;
	(void)position;
	(void)finger;
	(void)time_msec;
	g_pad_ring_count++;
}

static void
wlr_send_tablet_v2_tablet_pad_strip(
	struct wlr_tablet_v2_tablet_pad *pad,
	uint32_t strip, double position, bool finger,
	uint32_t time_msec)
{
	(void)pad;
	(void)strip;
	(void)position;
	(void)finger;
	(void)time_msec;
	g_pad_strip_count++;
}

static void
wlr_seat_pointer_notify_enter(struct wlr_seat *seat,
	struct wlr_surface *surface, double sx, double sy)
{
	(void)seat;
	(void)surface;
	(void)sx;
	(void)sy;
	g_seat_pointer_enter_count++;
}

static void
wlr_seat_pointer_notify_motion(struct wlr_seat *seat,
	uint32_t time_msec, double sx, double sy)
{
	(void)seat;
	(void)time_msec;
	(void)sx;
	(void)sy;
	g_seat_pointer_motion_count++;
}

static void
wlr_seat_pointer_clear_focus(struct wlr_seat *seat)
{
	(void)seat;
	g_seat_pointer_clear_count++;
}

static void
wm_focus_view(struct wm_view *view, struct wlr_surface *surface)
{
	(void)surface;
	g_focus_view_count++;
	g_focus_view_last = view;
}

/* --- Include the source under test --- */
#include "../src/tablet.c"

/* --- Helper to reset all tracking globals --- */

static void
reset_globals(void)
{
	g_idle_notify_count = 0;
	g_cursor_warp_count = 0;
	g_tool_v2_create_count = 0;
	g_tablet_v2_create_count = 0;
	g_pad_v2_create_count = 0;
	g_proximity_in_count = 0;
	g_proximity_out_count = 0;
	g_tool_motion_count = 0;
	g_tool_down_count = 0;
	g_tool_up_count = 0;
	g_tool_button_count = 0;
	g_tool_pressure_count = 0;
	g_tool_distance_count = 0;
	g_tool_tilt_count = 0;
	g_tool_rotation_count = 0;
	g_tool_slider_count = 0;
	g_tool_wheel_count = 0;
	g_pad_button_count = 0;
	g_pad_ring_count = 0;
	g_pad_strip_count = 0;
	g_seat_pointer_enter_count = 0;
	g_seat_pointer_motion_count = 0;
	g_seat_pointer_clear_count = 0;
	g_focus_view_count = 0;
	g_focus_view_last = NULL;
	g_scene_node_at_result = NULL;
	g_scene_node_at_sx = 0;
	g_scene_node_at_sy = 0;
	g_scene_surface_result = NULL;
	g_surface_accepts_tablet = false;
	memset(&g_tool_v2, 0, sizeof(g_tool_v2));
}

/* --- Test functions --- */

static void
test_tablet_init(void)
{
	reset_globals();

	struct wl_display display = {0};
	struct wm_server server = { .wl_display = &display };

	wm_tablet_init(&server);

	assert(server.tablet_manager != NULL);
	assert(wl_list_empty(&server.tablets));
	assert(wl_list_empty(&server.tablet_pads));

	wm_tablet_finish(&server);

	printf("PASS: test_tablet_init\n");
}

static void
test_tablet_tool_setup(void)
{
	reset_globals();

	struct wl_display display = {0};
	struct wlr_seat seat = {0};
	struct wlr_tablet_manager_v2 mgr = {0};
	struct wm_server server = {
		.wl_display = &display,
		.seat = &seat,
		.tablet_manager = &mgr,
	};
	wl_list_init(&server.tablets);
	wl_list_init(&server.tablet_pads);

	/* Create a tablet input device.
	 * wlr_tablet_from_input_device casts device -> wlr_tablet,
	 * so we allocate a wlr_tablet and pass its base.
	 */
	struct wlr_tablet wlr_tab = {0};
	wlr_tab.base.type = WLR_INPUT_DEVICE_TABLET;
	wlr_tab.base.name = "test-stylus";
	wl_signal_init(&wlr_tab.events.axis);
	wl_signal_init(&wlr_tab.events.proximity);
	wl_signal_init(&wlr_tab.events.tip);
	wl_signal_init(&wlr_tab.events.button);
	wl_signal_init(&wlr_tab.base.events.destroy);

	wm_tablet_tool_setup(&server, &wlr_tab.base);

	assert(g_tablet_v2_create_count == 1);
	assert(!wl_list_empty(&server.tablets));

	/* Clean up */
	wm_tablet_finish(&server);
	assert(wl_list_empty(&server.tablets));

	printf("PASS: test_tablet_tool_setup\n");
}

static void
test_tablet_pad_setup(void)
{
	reset_globals();

	struct wl_display display = {0};
	struct wlr_seat seat = {0};
	struct wlr_tablet_manager_v2 mgr = {0};
	struct wm_server server = {
		.wl_display = &display,
		.seat = &seat,
		.tablet_manager = &mgr,
	};
	wl_list_init(&server.tablets);
	wl_list_init(&server.tablet_pads);

	struct wlr_tablet_pad pad = {0};
	pad.button_count = 8;
	pad.ring_count = 2;
	pad.strip_count = 1;
	wl_signal_init(&pad.events.button);
	wl_signal_init(&pad.events.ring);
	wl_signal_init(&pad.events.strip);
	wl_signal_init(&pad.events.attach_tablet);

	/* wlr_tablet_pad_from_input_device casts to wlr_tablet_pad */
	struct wlr_input_device *dev = (struct wlr_input_device *)&pad;
	dev->type = WLR_INPUT_DEVICE_TABLET_PAD;
	dev->name = "test-pad";
	wl_signal_init(&dev->events.destroy);

	wm_tablet_pad_setup(&server, dev);

	assert(g_pad_v2_create_count == 1);
	assert(!wl_list_empty(&server.tablet_pads));

	wm_tablet_finish(&server);
	assert(wl_list_empty(&server.tablet_pads));

	printf("PASS: test_tablet_pad_setup\n");
}

static void
test_get_or_create_tool_v2_lazy(void)
{
	reset_globals();

	struct wlr_seat seat = {0};
	struct wlr_tablet_manager_v2 mgr = {0};
	struct wm_server server = {
		.seat = &seat,
		.tablet_manager = &mgr,
	};

	struct wlr_tablet_tool tool = { .data = NULL };

	/* First call should create */
	struct wlr_tablet_v2_tablet_tool *v2 =
		get_or_create_tool_v2(&server, &tool);
	assert(v2 != NULL);
	assert(g_tool_v2_create_count == 1);
	assert(tool.data == v2);

	/* Second call should return cached */
	struct wlr_tablet_v2_tablet_tool *v2_again =
		get_or_create_tool_v2(&server, &tool);
	assert(v2_again == v2);
	assert(g_tool_v2_create_count == 1); /* no new creation */

	printf("PASS: test_get_or_create_tool_v2_lazy\n");
}

static void
test_tool_axis_warps_cursor(void)
{
	reset_globals();

	struct wlr_cursor cursor = { .x = 0, .y = 0 };
	struct wlr_scene scene = {0};
	struct wlr_seat seat = {0};
	struct wlr_tablet_manager_v2 mgr = {0};
	struct wm_server server = {
		.cursor = &cursor,
		.scene = &scene,
		.seat = &seat,
		.tablet_manager = &mgr,
	};
	wl_list_init(&server.tablets);
	wl_list_init(&server.tablet_pads);

	/* Set up tablet */
	struct wlr_tablet wlr_tab = {0};
	wlr_tab.base.type = WLR_INPUT_DEVICE_TABLET;
	wlr_tab.base.name = "stylus";
	wl_signal_init(&wlr_tab.events.axis);
	wl_signal_init(&wlr_tab.events.proximity);
	wl_signal_init(&wlr_tab.events.tip);
	wl_signal_init(&wlr_tab.events.button);
	wl_signal_init(&wlr_tab.base.events.destroy);

	wm_tablet_tool_setup(&server, &wlr_tab.base);

	/* Fire axis event with XY update */
	struct wlr_tablet_tool tool = { .data = NULL };
	struct wlr_tablet_tool_axis_event event = {
		.tablet = &wlr_tab,
		.tool = &tool,
		.time_msec = 100,
		.updated_axes = WLR_TABLET_TOOL_AXIS_X | WLR_TABLET_TOOL_AXIS_Y,
		.x = 0.5,
		.y = 0.5,
	};

	/* No surface under cursor */
	g_scene_node_at_result = NULL;

	struct wl_listener *axis_listener =
		wl_container_of(wlr_tab.events.axis.listener_list.next,
			axis_listener, link);
	axis_listener->notify(axis_listener, &event);

	assert(g_idle_notify_count == 1);
	assert(g_cursor_warp_count == 1);
	/* No surface -> clear focus */
	assert(g_seat_pointer_clear_count == 1);

	wm_tablet_finish(&server);

	printf("PASS: test_tool_axis_warps_cursor\n");
}

static void
test_tool_axis_pressure(void)
{
	reset_globals();

	struct wlr_cursor cursor = { .x = 500, .y = 300 };
	struct wlr_scene scene = {0};
	struct wlr_seat seat = {0};
	struct wlr_tablet_manager_v2 mgr = {0};
	struct wm_server server = {
		.cursor = &cursor,
		.scene = &scene,
		.seat = &seat,
		.tablet_manager = &mgr,
	};
	wl_list_init(&server.tablets);
	wl_list_init(&server.tablet_pads);

	struct wlr_tablet wlr_tab = {0};
	wlr_tab.base.type = WLR_INPUT_DEVICE_TABLET;
	wlr_tab.base.name = "stylus";
	wl_signal_init(&wlr_tab.events.axis);
	wl_signal_init(&wlr_tab.events.proximity);
	wl_signal_init(&wlr_tab.events.tip);
	wl_signal_init(&wlr_tab.events.button);
	wl_signal_init(&wlr_tab.base.events.destroy);

	wm_tablet_tool_setup(&server, &wlr_tab.base);

	struct wlr_tablet_tool tool = { .data = NULL };
	struct wlr_tablet_tool_axis_event event = {
		.tablet = &wlr_tab,
		.tool = &tool,
		.updated_axes = WLR_TABLET_TOOL_AXIS_PRESSURE |
			WLR_TABLET_TOOL_AXIS_TILT_X |
			WLR_TABLET_TOOL_AXIS_TILT_Y,
		.pressure = 0.75,
		.tilt_x = 10.0,
		.tilt_y = -5.0,
	};

	g_scene_node_at_result = NULL;

	struct wl_listener *axis_listener =
		wl_container_of(wlr_tab.events.axis.listener_list.next,
			axis_listener, link);
	axis_listener->notify(axis_listener, &event);

	assert(g_tool_pressure_count == 1);
	assert(g_tool_tilt_count == 1);

	wm_tablet_finish(&server);

	printf("PASS: test_tool_axis_pressure\n");
}

static void
test_tool_proximity_in(void)
{
	reset_globals();

	struct wlr_surface surface = {0};
	struct wlr_scene_surface scene_surface = { .surface = &surface };
	struct wlr_cursor cursor = { .x = 0, .y = 0 };
	struct wlr_scene scene = {0};
	struct wlr_seat seat = {0};
	struct wlr_tablet_manager_v2 mgr = {0};
	struct wm_server server = {
		.cursor = &cursor,
		.scene = &scene,
		.seat = &seat,
		.tablet_manager = &mgr,
	};
	wl_list_init(&server.tablets);
	wl_list_init(&server.tablet_pads);

	struct wlr_tablet wlr_tab = {0};
	wlr_tab.base.type = WLR_INPUT_DEVICE_TABLET;
	wlr_tab.base.name = "stylus";
	wl_signal_init(&wlr_tab.events.axis);
	wl_signal_init(&wlr_tab.events.proximity);
	wl_signal_init(&wlr_tab.events.tip);
	wl_signal_init(&wlr_tab.events.button);
	wl_signal_init(&wlr_tab.base.events.destroy);

	wm_tablet_tool_setup(&server, &wlr_tab.base);

	struct wlr_tablet_tool tool = { .data = NULL };

	/* Set up scene to return a surface */
	struct wlr_scene_buffer scene_buffer = {
		.node = { .type = WLR_SCENE_NODE_BUFFER },
	};
	g_scene_node_at_result = &scene_buffer.node;
	g_scene_surface_result = &scene_surface;
	g_surface_accepts_tablet = true;

	struct wlr_tablet_tool_proximity_event event = {
		.tablet = &wlr_tab,
		.tool = &tool,
		.state = WLR_TABLET_TOOL_PROXIMITY_IN,
		.x = 0.5,
		.y = 0.5,
	};

	struct wl_listener *prox_listener =
		wl_container_of(wlr_tab.events.proximity.listener_list.next,
			prox_listener, link);
	prox_listener->notify(prox_listener, &event);

	assert(g_idle_notify_count == 1);
	assert(g_cursor_warp_count == 1);
	assert(g_proximity_in_count == 1);

	wm_tablet_finish(&server);

	printf("PASS: test_tool_proximity_in\n");
}

static void
test_tool_proximity_out(void)
{
	reset_globals();

	struct wlr_cursor cursor = { .x = 0, .y = 0 };
	struct wlr_scene scene = {0};
	struct wlr_seat seat = {0};
	struct wlr_tablet_manager_v2 mgr = {0};
	struct wm_server server = {
		.cursor = &cursor,
		.scene = &scene,
		.seat = &seat,
		.tablet_manager = &mgr,
	};
	wl_list_init(&server.tablets);
	wl_list_init(&server.tablet_pads);

	struct wlr_tablet wlr_tab = {0};
	wlr_tab.base.type = WLR_INPUT_DEVICE_TABLET;
	wlr_tab.base.name = "stylus";
	wl_signal_init(&wlr_tab.events.axis);
	wl_signal_init(&wlr_tab.events.proximity);
	wl_signal_init(&wlr_tab.events.tip);
	wl_signal_init(&wlr_tab.events.button);
	wl_signal_init(&wlr_tab.base.events.destroy);

	wm_tablet_tool_setup(&server, &wlr_tab.base);

	struct wlr_tablet_tool tool = { .data = NULL };

	struct wlr_tablet_tool_proximity_event event = {
		.tablet = &wlr_tab,
		.tool = &tool,
		.state = WLR_TABLET_TOOL_PROXIMITY_OUT,
	};

	struct wl_listener *prox_listener =
		wl_container_of(wlr_tab.events.proximity.listener_list.next,
			prox_listener, link);
	prox_listener->notify(prox_listener, &event);

	assert(g_idle_notify_count == 1);
	assert(g_proximity_out_count == 1);
	/* Proximity out should not warp cursor */
	assert(g_cursor_warp_count == 0);

	wm_tablet_finish(&server);

	printf("PASS: test_tool_proximity_out\n");
}

static void
test_tool_tip_down_focuses_view(void)
{
	reset_globals();

	struct wlr_surface surface = {0};
	struct wlr_scene_surface scene_surface = { .surface = &surface };
	struct wm_view view = { .id = 42 };
	struct wlr_cursor cursor = { .x = 100, .y = 200 };
	struct wlr_scene scene = {0};
	struct wlr_seat seat = {0};
	struct wlr_tablet_manager_v2 mgr = {0};
	struct wm_server server = {
		.cursor = &cursor,
		.scene = &scene,
		.seat = &seat,
		.tablet_manager = &mgr,
	};
	wl_list_init(&server.tablets);
	wl_list_init(&server.tablet_pads);

	struct wlr_tablet wlr_tab = {0};
	wlr_tab.base.type = WLR_INPUT_DEVICE_TABLET;
	wlr_tab.base.name = "stylus";
	wl_signal_init(&wlr_tab.events.axis);
	wl_signal_init(&wlr_tab.events.proximity);
	wl_signal_init(&wlr_tab.events.tip);
	wl_signal_init(&wlr_tab.events.button);
	wl_signal_init(&wlr_tab.base.events.destroy);

	wm_tablet_tool_setup(&server, &wlr_tab.base);

	/* Set up scene so tablet_view_at returns our view */
	struct wlr_scene_tree view_tree = {
		.node = { .type = WLR_SCENE_NODE_TREE, .data = &view },
	};
	struct wlr_scene_buffer scene_buffer = {
		.node = {
			.type = WLR_SCENE_NODE_BUFFER,
			.parent = &view_tree,
		},
	};
	g_scene_node_at_result = &scene_buffer.node;
	g_scene_surface_result = &scene_surface;

	struct wlr_tablet_tool tool = { .data = NULL };
	struct wlr_tablet_tool_tip_event event = {
		.tablet = &wlr_tab,
		.tool = &tool,
		.state = WLR_TABLET_TOOL_TIP_DOWN,
	};

	struct wl_listener *tip_listener =
		wl_container_of(wlr_tab.events.tip.listener_list.next,
			tip_listener, link);
	tip_listener->notify(tip_listener, &event);

	assert(g_tool_down_count == 1);
	assert(g_focus_view_count == 1);
	assert(g_focus_view_last == &view);

	wm_tablet_finish(&server);

	printf("PASS: test_tool_tip_down_focuses_view\n");
}

static void
test_tool_tip_up(void)
{
	reset_globals();

	struct wlr_cursor cursor = { .x = 100, .y = 200 };
	struct wlr_scene scene = {0};
	struct wlr_seat seat = {0};
	struct wlr_tablet_manager_v2 mgr = {0};
	struct wm_server server = {
		.cursor = &cursor,
		.scene = &scene,
		.seat = &seat,
		.tablet_manager = &mgr,
	};
	wl_list_init(&server.tablets);
	wl_list_init(&server.tablet_pads);

	struct wlr_tablet wlr_tab = {0};
	wlr_tab.base.type = WLR_INPUT_DEVICE_TABLET;
	wlr_tab.base.name = "stylus";
	wl_signal_init(&wlr_tab.events.axis);
	wl_signal_init(&wlr_tab.events.proximity);
	wl_signal_init(&wlr_tab.events.tip);
	wl_signal_init(&wlr_tab.events.button);
	wl_signal_init(&wlr_tab.base.events.destroy);

	wm_tablet_tool_setup(&server, &wlr_tab.base);

	struct wlr_tablet_tool tool = { .data = NULL };
	struct wlr_tablet_tool_tip_event event = {
		.tablet = &wlr_tab,
		.tool = &tool,
		.state = WLR_TABLET_TOOL_TIP_UP,
	};

	struct wl_listener *tip_listener =
		wl_container_of(wlr_tab.events.tip.listener_list.next,
			tip_listener, link);
	tip_listener->notify(tip_listener, &event);

	assert(g_tool_up_count == 1);
	assert(g_focus_view_count == 0); /* no focus on tip-up */

	wm_tablet_finish(&server);

	printf("PASS: test_tool_tip_up\n");
}

static void
test_tool_button_pressed(void)
{
	reset_globals();

	struct wlr_cursor cursor = {0};
	struct wlr_scene scene = {0};
	struct wlr_seat seat = {0};
	struct wlr_tablet_manager_v2 mgr = {0};
	struct wm_server server = {
		.cursor = &cursor,
		.scene = &scene,
		.seat = &seat,
		.tablet_manager = &mgr,
	};
	wl_list_init(&server.tablets);
	wl_list_init(&server.tablet_pads);

	struct wlr_tablet wlr_tab = {0};
	wlr_tab.base.type = WLR_INPUT_DEVICE_TABLET;
	wlr_tab.base.name = "stylus";
	wl_signal_init(&wlr_tab.events.axis);
	wl_signal_init(&wlr_tab.events.proximity);
	wl_signal_init(&wlr_tab.events.tip);
	wl_signal_init(&wlr_tab.events.button);
	wl_signal_init(&wlr_tab.base.events.destroy);

	wm_tablet_tool_setup(&server, &wlr_tab.base);

	struct wlr_tablet_tool tool = { .data = NULL };
	struct wlr_tablet_tool_button_event event = {
		.tablet = &wlr_tab,
		.tool = &tool,
		.button = 0x14b, /* BTN_STYLUS */
		.state = WLR_BUTTON_PRESSED,
	};

	struct wl_listener *btn_listener =
		wl_container_of(wlr_tab.events.button.listener_list.next,
			btn_listener, link);
	btn_listener->notify(btn_listener, &event);

	assert(g_idle_notify_count == 1);
	assert(g_tool_button_count == 1);

	wm_tablet_finish(&server);

	printf("PASS: test_tool_button_pressed\n");
}

static void
test_pad_button(void)
{
	reset_globals();

	struct wlr_seat seat = {0};
	struct wlr_tablet_manager_v2 mgr = {0};
	struct wm_server server = {
		.seat = &seat,
		.tablet_manager = &mgr,
	};
	wl_list_init(&server.tablets);
	wl_list_init(&server.tablet_pads);

	struct wlr_tablet_pad pad = {0};
	pad.button_count = 4;
	pad.ring_count = 0;
	pad.strip_count = 0;
	wl_signal_init(&pad.events.button);
	wl_signal_init(&pad.events.ring);
	wl_signal_init(&pad.events.strip);
	wl_signal_init(&pad.events.attach_tablet);

	struct wlr_input_device *dev = (struct wlr_input_device *)&pad;
	dev->name = "pad";
	wl_signal_init(&dev->events.destroy);

	wm_tablet_pad_setup(&server, dev);

	struct wlr_tablet_pad_button_event event = {
		.time_msec = 100,
		.button = 0,
		.state = WLR_BUTTON_PRESSED,
	};

	struct wl_listener *btn_listener =
		wl_container_of(pad.events.button.listener_list.next,
			btn_listener, link);
	btn_listener->notify(btn_listener, &event);

	assert(g_idle_notify_count == 1);
	assert(g_pad_button_count == 1);

	wm_tablet_finish(&server);

	printf("PASS: test_pad_button\n");
}

static void
test_pad_ring(void)
{
	reset_globals();

	struct wlr_seat seat = {0};
	struct wlr_tablet_manager_v2 mgr = {0};
	struct wm_server server = {
		.seat = &seat,
		.tablet_manager = &mgr,
	};
	wl_list_init(&server.tablets);
	wl_list_init(&server.tablet_pads);

	struct wlr_tablet_pad pad = {0};
	pad.button_count = 0;
	pad.ring_count = 1;
	pad.strip_count = 0;
	wl_signal_init(&pad.events.button);
	wl_signal_init(&pad.events.ring);
	wl_signal_init(&pad.events.strip);
	wl_signal_init(&pad.events.attach_tablet);

	struct wlr_input_device *dev = (struct wlr_input_device *)&pad;
	dev->name = "pad";
	wl_signal_init(&dev->events.destroy);

	wm_tablet_pad_setup(&server, dev);

	struct wlr_tablet_pad_ring_event event = {
		.time_msec = 200,
		.ring = 0,
		.position = 0.5,
		.source = WLR_TABLET_PAD_RING_SOURCE_FINGER,
	};

	struct wl_listener *ring_listener =
		wl_container_of(pad.events.ring.listener_list.next,
			ring_listener, link);
	ring_listener->notify(ring_listener, &event);

	assert(g_idle_notify_count == 1);
	assert(g_pad_ring_count == 1);

	wm_tablet_finish(&server);

	printf("PASS: test_pad_ring\n");
}

static void
test_pad_strip(void)
{
	reset_globals();

	struct wlr_seat seat = {0};
	struct wlr_tablet_manager_v2 mgr = {0};
	struct wm_server server = {
		.seat = &seat,
		.tablet_manager = &mgr,
	};
	wl_list_init(&server.tablets);
	wl_list_init(&server.tablet_pads);

	struct wlr_tablet_pad pad = {0};
	pad.button_count = 0;
	pad.ring_count = 0;
	pad.strip_count = 1;
	wl_signal_init(&pad.events.button);
	wl_signal_init(&pad.events.ring);
	wl_signal_init(&pad.events.strip);
	wl_signal_init(&pad.events.attach_tablet);

	struct wlr_input_device *dev = (struct wlr_input_device *)&pad;
	dev->name = "pad";
	wl_signal_init(&dev->events.destroy);

	wm_tablet_pad_setup(&server, dev);

	struct wlr_tablet_pad_strip_event event = {
		.time_msec = 300,
		.strip = 0,
		.position = 0.75,
		.source = WLR_TABLET_PAD_STRIP_SOURCE_FINGER,
	};

	struct wl_listener *strip_listener =
		wl_container_of(pad.events.strip.listener_list.next,
			strip_listener, link);
	strip_listener->notify(strip_listener, &event);

	assert(g_idle_notify_count == 1);
	assert(g_pad_strip_count == 1);

	wm_tablet_finish(&server);

	printf("PASS: test_pad_strip\n");
}

static void
test_tablet_destroy_cleanup(void)
{
	reset_globals();

	struct wlr_seat seat = {0};
	struct wlr_tablet_manager_v2 mgr = {0};
	struct wm_server server = {
		.seat = &seat,
		.tablet_manager = &mgr,
	};
	wl_list_init(&server.tablets);
	wl_list_init(&server.tablet_pads);

	struct wlr_tablet wlr_tab = {0};
	wlr_tab.base.type = WLR_INPUT_DEVICE_TABLET;
	wlr_tab.base.name = "stylus";
	wl_signal_init(&wlr_tab.events.axis);
	wl_signal_init(&wlr_tab.events.proximity);
	wl_signal_init(&wlr_tab.events.tip);
	wl_signal_init(&wlr_tab.events.button);
	wl_signal_init(&wlr_tab.base.events.destroy);

	wm_tablet_tool_setup(&server, &wlr_tab.base);
	assert(!wl_list_empty(&server.tablets));

	/* Fire the destroy event */
	struct wl_listener *destroy_listener =
		wl_container_of(wlr_tab.base.events.destroy.listener_list.next,
			destroy_listener, link);
	destroy_listener->notify(destroy_listener, NULL);

	assert(wl_list_empty(&server.tablets));

	printf("PASS: test_tablet_destroy_cleanup\n");
}

static void
test_pad_destroy_cleanup(void)
{
	reset_globals();

	struct wlr_seat seat = {0};
	struct wlr_tablet_manager_v2 mgr = {0};
	struct wm_server server = {
		.seat = &seat,
		.tablet_manager = &mgr,
	};
	wl_list_init(&server.tablets);
	wl_list_init(&server.tablet_pads);

	struct wlr_tablet_pad pad = {0};
	pad.button_count = 0;
	pad.ring_count = 0;
	pad.strip_count = 0;
	wl_signal_init(&pad.events.button);
	wl_signal_init(&pad.events.ring);
	wl_signal_init(&pad.events.strip);
	wl_signal_init(&pad.events.attach_tablet);

	struct wlr_input_device *dev = (struct wlr_input_device *)&pad;
	dev->name = "pad";
	wl_signal_init(&dev->events.destroy);

	wm_tablet_pad_setup(&server, dev);
	assert(!wl_list_empty(&server.tablet_pads));

	/* Fire destroy */
	struct wl_listener *destroy_listener =
		wl_container_of(dev->events.destroy.listener_list.next,
			destroy_listener, link);
	destroy_listener->notify(destroy_listener, NULL);

	assert(wl_list_empty(&server.tablet_pads));

	printf("PASS: test_pad_destroy_cleanup\n");
}

static void
test_tablet_finish_cleans_all(void)
{
	reset_globals();

	struct wl_display display = {0};
	struct wlr_seat seat = {0};
	struct wlr_tablet_manager_v2 mgr = {0};
	struct wm_server server = {
		.wl_display = &display,
		.seat = &seat,
		.tablet_manager = &mgr,
	};
	wl_list_init(&server.tablets);
	wl_list_init(&server.tablet_pads);

	/* Add two tablets */
	struct wlr_tablet tab1 = {0};
	tab1.base.type = WLR_INPUT_DEVICE_TABLET;
	tab1.base.name = "tab1";
	wl_signal_init(&tab1.events.axis);
	wl_signal_init(&tab1.events.proximity);
	wl_signal_init(&tab1.events.tip);
	wl_signal_init(&tab1.events.button);
	wl_signal_init(&tab1.base.events.destroy);
	wm_tablet_tool_setup(&server, &tab1.base);

	struct wlr_tablet tab2 = {0};
	tab2.base.type = WLR_INPUT_DEVICE_TABLET;
	tab2.base.name = "tab2";
	wl_signal_init(&tab2.events.axis);
	wl_signal_init(&tab2.events.proximity);
	wl_signal_init(&tab2.events.tip);
	wl_signal_init(&tab2.events.button);
	wl_signal_init(&tab2.base.events.destroy);
	wm_tablet_tool_setup(&server, &tab2.base);

	assert(!wl_list_empty(&server.tablets));

	wm_tablet_finish(&server);

	assert(wl_list_empty(&server.tablets));
	assert(wl_list_empty(&server.tablet_pads));

	printf("PASS: test_tablet_finish_cleans_all\n");
}

/* --- main --- */

int
main(void)
{
	test_tablet_init();
	test_tablet_tool_setup();
	test_tablet_pad_setup();
	test_get_or_create_tool_v2_lazy();
	test_tool_axis_warps_cursor();
	test_tool_axis_pressure();
	test_tool_proximity_in();
	test_tool_proximity_out();
	test_tool_tip_down_focuses_view();
	test_tool_tip_up();
	test_tool_button_pressed();
	test_pad_button();
	test_pad_ring();
	test_pad_strip();
	test_tablet_destroy_cleanup();
	test_pad_destroy_cleanup();
	test_tablet_finish_cleans_all();

	printf("\nAll tablet tests passed! (17/17)\n");
	return 0;
}
