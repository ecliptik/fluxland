/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * wlcs_shim.c - WLCS (Wayland Conformance Test Suite) integration module
 *
 * WLCS (Canonical's Google Test-based conformance suite) loads
 * libfluxland-wlcs.so and drives the compositor through the exported
 * wlcs_server_integration symbol:
 *
 *   meson setup build-wlcs -Dwlcs=true && ninja -C build-wlcs
 *   tools/wlcs.sh build-wlcs
 *
 * The compositor runs headless (pixman) on the thread WLCS gives it
 * (start_on_this_thread): WLCS hands every call -- clients, pointer,
 * touch, window positions -- to its dispatcher loop, which our event
 * loop drives, so all of it runs on the compositor's thread. Input goes
 * through real wlr_pointer/wlr_touch devices, so fluxland's own cursor
 * handling (focus on click, bindings) is what gets tested; a keyboard and
 * a touchscreen are plugged in from the start, as on a laptop, so clients
 * have their wl_keyboard, wl_pointer and wl_touch before a test's first
 * event (bound later, a capability comes too late for the event after
 * it). Each server reads fluxland's defaults, less the toolbar.
 */

#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/interfaces/wlr_pointer.h>
#include <wlr/interfaces/wlr_touch.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>

#include "server.h"
#include "view.h"

/*
 * Client-side libwayland calls on the objects WLCS passes in. Its
 * header can't be included next to the server's; both only take
 * pointers, and the opaque client struct wl_display is never touched.
 */
struct wl_proxy;
extern uint32_t wl_proxy_get_id(struct wl_proxy *proxy);
extern int wl_display_get_fd(struct wl_display *display);

/* The WLCS API (wlcs/display_server.h, pointer.h, touch.h) */

#define WLCS_DISPLAY_SERVER_VERSION 3
#define WLCS_SERVER_INTEGRATION_VERSION 1
#define WLCS_INTEGRATION_DESCRIPTOR_VERSION 1
#define WLCS_POINTER_VERSION 1
#define WLCS_TOUCH_VERSION 1

struct WlcsExtensionDescriptor {
	char const *name;
	uint32_t version;
};

struct WlcsIntegrationDescriptor {
	uint32_t version;
	size_t num_extensions;
	struct WlcsExtensionDescriptor const *supported_extensions;
};

struct WlcsPointer {
	uint32_t version;
	void (*move_absolute)(struct WlcsPointer *pointer,
		wl_fixed_t x, wl_fixed_t y);
	void (*move_relative)(struct WlcsPointer *pointer,
		wl_fixed_t dx, wl_fixed_t dy);
	void (*button_up)(struct WlcsPointer *pointer, int button);
	void (*button_down)(struct WlcsPointer *pointer, int button);
	void (*destroy)(struct WlcsPointer *pointer);
};

struct WlcsTouch {
	uint32_t version;
	void (*touch_down)(struct WlcsTouch *touch,
		wl_fixed_t x, wl_fixed_t y);
	void (*touch_move)(struct WlcsTouch *touch,
		wl_fixed_t x, wl_fixed_t y);
	void (*touch_up)(struct WlcsTouch *touch);
	void (*destroy)(struct WlcsTouch *touch);
};

struct WlcsDisplayServer {
	uint32_t version;
	void (*start)(struct WlcsDisplayServer *server);
	void (*stop)(struct WlcsDisplayServer *server);
	int (*create_client_socket)(struct WlcsDisplayServer *server);
	void (*position_window_absolute)(struct WlcsDisplayServer *server,
		struct wl_display *client_display,
		struct wl_surface *surface,
		int x, int y);
	struct WlcsPointer *(*create_pointer)(
		struct WlcsDisplayServer *server);
	struct WlcsTouch *(*create_touch)(
		struct WlcsDisplayServer *server);
	struct WlcsIntegrationDescriptor const *(*get_descriptor)(
		struct WlcsDisplayServer const *server);
	void (*start_on_this_thread)(struct WlcsDisplayServer *server,
		struct wl_event_loop *loop);
};

struct WlcsServerIntegration {
	uint32_t version;
	struct WlcsDisplayServer *(*create_server)(int argc,
		char const **argv);
	void (*destroy_server)(struct WlcsDisplayServer *server);
};

/* --- Our side --- */

/* A client WLCS connected, known by the fd we gave it */
struct wlcs_client {
	struct wl_list link;
	int client_fd;
	struct wl_client *client;
	struct wl_listener destroy;
};

struct fluxland_wlcs_server {
	struct WlcsDisplayServer base;
	struct wm_server server;
	bool started;
	struct wl_list clients;		/* wlcs_client.link */
	struct wlr_keyboard keyboard;
	struct wlr_touch touchscreen;	/* never touched */
};

struct fluxland_wlcs_pointer {
	struct WlcsPointer base;
	struct fluxland_wlcs_server *wlcs;
	struct wlr_pointer pointer;
};

struct fluxland_wlcs_touch {
	struct WlcsTouch base;
	struct fluxland_wlcs_server *wlcs;
	struct wlr_touch touch;
	int32_t id;
	bool down;
};

/* What fluxland advertises (wayland-info on a headless session) */
static const struct WlcsExtensionDescriptor supported_extensions[] = {
	{ "ext_data_control_manager_v1", 1 },
	{ "ext_foreign_toplevel_image_capture_source_manager_v1", 1 },
	{ "ext_foreign_toplevel_list_v1", 1 },
	{ "ext_idle_notifier_v1", 2 },
	{ "ext_image_copy_capture_manager_v1", 1 },
	{ "ext_output_image_capture_source_manager_v1", 1 },
	{ "ext_session_lock_manager_v1", 1 },
	{ "ext_transient_seat_manager_v1", 1 },
	{ "ext_workspace_manager_v1", 1 },
	{ "org_kde_kwin_server_decoration_manager", 1 },
	{ "wl_compositor", 6 },
	{ "wl_data_device_manager", 3 },
	{ "wl_fixes", 1 },
	{ "wl_output", 4 },
	{ "wl_seat", 9 },
	{ "wl_shm", 2 },
	{ "wl_subcompositor", 1 },
	{ "wp_alpha_modifier_v1", 1 },
	{ "wp_color_representation_manager_v1", 1 },
	{ "wp_content_type_manager_v1", 1 },
	{ "wp_cursor_shape_manager_v1", 2 },
	{ "wp_fractional_scale_manager_v1", 1 },
	{ "wp_presentation", 2 },
	{ "wp_security_context_manager_v1", 1 },
	{ "wp_single_pixel_buffer_manager_v1", 1 },
	{ "wp_tearing_control_manager_v1", 1 },
	{ "wp_viewporter", 1 },
	{ "xdg_activation_v1", 1 },
	{ "xdg_system_bell_v1", 1 },
	{ "xdg_toplevel_icon_manager_v1", 1 },
	{ "xdg_toplevel_tag_manager_v1", 1 },
	{ "xdg_wm_base", 6 },
	{ "xdg_wm_dialog_v1", 1 },
	{ "zwlr_data_control_manager_v1", 2 },
	{ "zwlr_export_dmabuf_manager_v1", 1 },
	{ "zwlr_foreign_toplevel_manager_v1", 3 },
	{ "zwlr_gamma_control_manager_v1", 1 },
	{ "zwlr_layer_shell_v1", 4 },
	{ "zwlr_output_manager_v1", 4 },
	{ "zwlr_output_power_manager_v1", 1 },
	{ "zwlr_screencopy_manager_v1", 3 },
	{ "zwlr_virtual_pointer_manager_v1", 2 },
	{ "zwp_idle_inhibit_manager_v1", 1 },
	{ "zwp_input_method_manager_v2", 1 },
	{ "zwp_keyboard_shortcuts_inhibit_manager_v1", 1 },
	{ "zwp_pointer_constraints_v1", 1 },
	{ "zwp_pointer_gestures_v1", 3 },
	{ "zwp_primary_selection_device_manager_v1", 1 },
	{ "zwp_relative_pointer_manager_v1", 1 },
	{ "zwp_tablet_manager_v2", 1 },
	{ "zwp_text_input_manager_v3", 1 },
	{ "zwp_virtual_keyboard_manager_v1", 1 },
	{ "zxdg_decoration_manager_v1", 1 },
	{ "zxdg_exporter_v1", 1 },
	{ "zxdg_exporter_v2", 1 },
	{ "zxdg_importer_v1", 1 },
	{ "zxdg_importer_v2", 1 },
	{ "zxdg_output_manager_v1", 3 },
};

static const struct WlcsIntegrationDescriptor integration_descriptor = {
	.version = WLCS_INTEGRATION_DESCRIPTOR_VERSION,
	.num_extensions = sizeof(supported_extensions) /
		sizeof(supported_extensions[0]),
	.supported_extensions = supported_extensions,
};

static uint32_t
now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/* The fraction of [start, start + size) that wlroots turns back into
 * v exactly: 30 / 720 multiplied back is 29.999..., a pixel short of an
 * edge, so step up to the next double until it isn't */
static double
fraction_of(double v, int start, int size)
{
	if (size <= 0)
		return 0;
	double f = (v - start) / size;
	for (int i = 0; i < 4 && start + f * size < v; i++)
		f = nextafter(f, 2.0);
	return f;
}

/* A compositor-space point as wlroots' absolute (0..1 of the layout) */
static void
to_absolute(struct wm_server *server, double x, double y, double *ax,
	double *ay)
{
	struct wlr_box box;
	wlr_output_layout_get_box(server->output_layout, NULL, &box);
	*ax = fraction_of(x, box.x, box.width);
	*ay = fraction_of(y, box.y, box.height);
}

/* --- Pointer: a wlr_pointer, its events as a real mouse's --- */

static const struct wlr_pointer_impl wlcs_pointer_impl = {
	.name = "wlcs-pointer",
};

static void
pointer_move_absolute(struct WlcsPointer *pointer, wl_fixed_t x, wl_fixed_t y)
{
	struct fluxland_wlcs_pointer *p = wl_container_of(pointer, p, base);
	struct wlr_pointer_motion_absolute_event ev = {
		.pointer = &p->pointer,
		.time_msec = now_ms(),
	};
	to_absolute(&p->wlcs->server, wl_fixed_to_double(x),
		wl_fixed_to_double(y), &ev.x, &ev.y);
	wl_signal_emit_mutable(&p->pointer.events.motion_absolute, &ev);
	wl_signal_emit_mutable(&p->pointer.events.frame, &p->pointer);
}

static void
pointer_move_relative(struct WlcsPointer *pointer, wl_fixed_t dx,
	wl_fixed_t dy)
{
	struct fluxland_wlcs_pointer *p = wl_container_of(pointer, p, base);
	struct wlr_pointer_motion_event ev = {
		.pointer = &p->pointer,
		.time_msec = now_ms(),
		.delta_x = wl_fixed_to_double(dx),
		.delta_y = wl_fixed_to_double(dy),
		.unaccel_dx = wl_fixed_to_double(dx),
		.unaccel_dy = wl_fixed_to_double(dy),
	};
	wl_signal_emit_mutable(&p->pointer.events.motion, &ev);
	wl_signal_emit_mutable(&p->pointer.events.frame, &p->pointer);
}

static void
pointer_button(struct fluxland_wlcs_pointer *p, int button,
	enum wl_pointer_button_state state)
{
	struct wlr_pointer_button_event ev = {
		.pointer = &p->pointer,
		.time_msec = now_ms(),
		.button = (uint32_t)button,
		.state = state,
	};
	wl_signal_emit_mutable(&p->pointer.events.button, &ev);
	wl_signal_emit_mutable(&p->pointer.events.frame, &p->pointer);
}

static void
pointer_button_down(struct WlcsPointer *pointer, int button)
{
	struct fluxland_wlcs_pointer *p = wl_container_of(pointer, p, base);
	pointer_button(p, button, WL_POINTER_BUTTON_STATE_PRESSED);
}

static void
pointer_button_up(struct WlcsPointer *pointer, int button)
{
	struct fluxland_wlcs_pointer *p = wl_container_of(pointer, p, base);
	pointer_button(p, button, WL_POINTER_BUTTON_STATE_RELEASED);
}

static void
pointer_destroy(struct WlcsPointer *pointer)
{
	struct fluxland_wlcs_pointer *p = wl_container_of(pointer, p, base);
	wlr_pointer_finish(&p->pointer);
	free(p);
}

static struct WlcsPointer *
wlcs_create_pointer(struct WlcsDisplayServer *ds)
{
	struct fluxland_wlcs_server *s = wl_container_of(ds, s, base);
	struct fluxland_wlcs_pointer *p = calloc(1, sizeof(*p));
	if (!p)
		return NULL;
	p->wlcs = s;
	p->base.version = WLCS_POINTER_VERSION;
	p->base.move_absolute = pointer_move_absolute;
	p->base.move_relative = pointer_move_relative;
	p->base.button_up = pointer_button_up;
	p->base.button_down = pointer_button_down;
	p->base.destroy = pointer_destroy;
	wlr_pointer_init(&p->pointer, &wlcs_pointer_impl, "wlcs-pointer");
	/* Plugged in as the backend announces a device */
	wl_signal_emit_mutable(&s->server.backend->events.new_input,
		&p->pointer.base);
	return &p->base;
}

/* --- Touch: a wlr_touch with one touch point --- */

static const struct wlr_touch_impl wlcs_touch_impl = {
	.name = "wlcs-touch",
};

static void
touch_frame(struct fluxland_wlcs_touch *t)
{
	wl_signal_emit_mutable(&t->touch.events.frame, NULL);
}

static void
touch_down(struct WlcsTouch *touch, wl_fixed_t x, wl_fixed_t y)
{
	struct fluxland_wlcs_touch *t = wl_container_of(touch, t, base);
	struct wlr_touch_down_event ev = {
		.touch = &t->touch,
		.time_msec = now_ms(),
		.touch_id = ++t->id,
	};
	/* WLCS passes whole pixels here, not the wl_fixed_t its
	 * header says (in_process_server.cpp's Touch) */
	to_absolute(&t->wlcs->server, (double)x, (double)y, &ev.x, &ev.y);
	t->down = true;
	wl_signal_emit_mutable(&t->touch.events.down, &ev);
	touch_frame(t);
}

static void
touch_move(struct WlcsTouch *touch, wl_fixed_t x, wl_fixed_t y)
{
	struct fluxland_wlcs_touch *t = wl_container_of(touch, t, base);
	struct wlr_touch_motion_event ev = {
		.touch = &t->touch,
		.time_msec = now_ms(),
		.touch_id = t->id,
	};
	/* WLCS passes whole pixels here, not the wl_fixed_t its
	 * header says (in_process_server.cpp's Touch) */
	to_absolute(&t->wlcs->server, (double)x, (double)y, &ev.x, &ev.y);
	wl_signal_emit_mutable(&t->touch.events.motion, &ev);
	touch_frame(t);
}

static void
touch_up(struct WlcsTouch *touch)
{
	struct fluxland_wlcs_touch *t = wl_container_of(touch, t, base);
	struct wlr_touch_up_event ev = {
		.touch = &t->touch,
		.time_msec = now_ms(),
		.touch_id = t->id,
	};
	t->down = false;
	wl_signal_emit_mutable(&t->touch.events.up, &ev);
	touch_frame(t);
}

static void
touch_destroy(struct WlcsTouch *touch)
{
	struct fluxland_wlcs_touch *t = wl_container_of(touch, t, base);
	if (t->down)
		touch_up(touch);
	wlr_touch_finish(&t->touch);
	free(t);
}

static struct WlcsTouch *
wlcs_create_touch(struct WlcsDisplayServer *ds)
{
	struct fluxland_wlcs_server *s = wl_container_of(ds, s, base);
	struct fluxland_wlcs_touch *t = calloc(1, sizeof(*t));
	if (!t)
		return NULL;
	t->wlcs = s;
	t->base.version = WLCS_TOUCH_VERSION;
	t->base.touch_down = touch_down;
	t->base.touch_move = touch_move;
	t->base.touch_up = touch_up;
	t->base.destroy = touch_destroy;
	wlr_touch_init(&t->touch, &wlcs_touch_impl, "wlcs-touch");
	wl_signal_emit_mutable(&s->server.backend->events.new_input,
		&t->touch.base);
	return &t->base;
}

/* --- Keyboard: there from the start, never typed on --- */

static const struct wlr_keyboard_impl wlcs_keyboard_impl = {
	.name = "wlcs-keyboard",
};

/* --- Clients --- */

static void
handle_client_destroy(struct wl_listener *listener, void *data)
{
	(void)data;
	struct wlcs_client *c = wl_container_of(listener, c, destroy);
	wl_list_remove(&c->destroy.link);
	wl_list_remove(&c->link);
	free(c);
}

static int
wlcs_create_client_socket(struct WlcsDisplayServer *ds)
{
	struct fluxland_wlcs_server *s = wl_container_of(ds, s, base);
	int fds[2];
	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds) < 0) {
		wlr_log_errno(WLR_ERROR, "%s", "wlcs: socketpair");
		return -1;
	}
	struct wlcs_client *c = calloc(1, sizeof(*c));
	struct wl_client *client = c ?
		wl_client_create(s->server.wl_display, fds[0]) : NULL;
	if (!client) {
		free(c);
		close(fds[0]);
		close(fds[1]);
		return -1;
	}
	c->client = client;
	c->client_fd = fds[1];
	c->destroy.notify = handle_client_destroy;
	wl_client_add_destroy_listener(client, &c->destroy);
	wl_list_insert(&s->clients, &c->link);
	return fds[1];
}

/* Where in its window's scene the surface's own buffer sits */
struct find_surface {
	struct wlr_surface *surface;
	int sx, sy;
	bool found;
};

static void
find_surface_buffer(struct wlr_scene_buffer *buffer, int sx, int sy,
	void *data)
{
	struct find_surface *f = data;
	struct wlr_scene_surface *ss =
		wlr_scene_surface_try_from_buffer(buffer);
	if (!f->found && ss && ss->surface == f->surface) {
		f->sx = sx;
		f->sy = sy;
		f->found = true;
	}
}

/* The window moved so that its window geometry's top left (the
 * surface's, less any shadow it draws) is at x, y */
static void
wlcs_position_window_absolute(struct WlcsDisplayServer *ds,
	struct wl_display *client_display, struct wl_surface *surface,
	int x, int y)
{
	struct fluxland_wlcs_server *s = wl_container_of(ds, s, base);
	int fd = wl_display_get_fd(client_display);
	struct wl_client *client = NULL;
	struct wlcs_client *c;
	wl_list_for_each(c, &s->clients, link) {
		if (c->client_fd == fd) {
			client = c->client;
			break;
		}
	}
	if (!client)
		return;
	struct wl_resource *res = wl_client_get_object(client,
		wl_proxy_get_id((struct wl_proxy *)surface));
	if (!res)
		return;
	struct wlr_surface *wlr_surface = wlr_surface_from_resource(res);

	struct wm_view *view;
	wl_list_for_each(view, &s->server.views, link) {
		if (wm_view_surface(view) != wlr_surface || !view->scene_tree)
			continue;
		struct find_surface f = { .surface = wlr_surface };
		wlr_scene_node_for_each_buffer(&view->scene_tree->node,
			find_surface_buffer, &f);
		if (!f.found)
			return;
		/* f is from the window's parent (the walk counts the
		 * window's own position): the surface's offset in it, and
		 * where its parent is */
		struct wlr_scene_node *node = &view->scene_tree->node;
		int off_x = f.sx - node->x, off_y = f.sy - node->y;
		int lx, ly;
		wlr_scene_node_coords(node, &lx, &ly);
		int parent_x = lx - node->x, parent_y = ly - node->y;
		struct wlr_box geo;
		wm_view_content_geometry(view, &geo);
		wm_view_move(view, x - geo.x - off_x - parent_x,
			y - geo.y - off_y - parent_y);
		return;
	}
}

/* --- The server --- */

static int
dispatch_wlcs(int fd, uint32_t mask, void *data)
{
	(void)fd;
	(void)mask;
	wl_event_loop_dispatch(data, 0);
	return 0;
}

static void
wlcs_start_on_this_thread(struct WlcsDisplayServer *ds,
	struct wl_event_loop *wlcs_loop)
{
	struct fluxland_wlcs_server *s = wl_container_of(ds, s, base);
	if (!wm_server_start(&s->server)) {
		wlr_log(WLR_ERROR, "%s", "wlcs: failed to start server");
		return;
	}
	s->started = true;
	/* The pointer starts in the far corner, where no window opens
	 * (fluxland places the first one top left) */
	struct wlr_box box;
	wlr_output_layout_get_box(s->server.output_layout, NULL, &box);
	wlr_cursor_warp(s->server.cursor, NULL, box.x + box.width - 1,
		box.y + box.height - 1);
	wlr_keyboard_init(&s->keyboard, &wlcs_keyboard_impl, "wlcs-keyboard");
	wl_signal_emit_mutable(&s->server.backend->events.new_input,
		&s->keyboard.base);
	wlr_touch_init(&s->touchscreen, &wlcs_touch_impl, "wlcs-touchscreen");
	wl_signal_emit_mutable(&s->server.backend->events.new_input,
		&s->touchscreen.base);
	struct wl_event_source *source = wl_event_loop_add_fd(
		wl_display_get_event_loop(s->server.wl_display),
		wl_event_loop_get_fd(wlcs_loop), WL_EVENT_READABLE,
		dispatch_wlcs, wlcs_loop);
	/* Anything WLCS queued before we listened */
	wl_event_loop_dispatch(wlcs_loop, 0);
	wl_display_run(s->server.wl_display);
	wl_event_source_remove(source);
}

static void
wlcs_stop(struct WlcsDisplayServer *ds)
{
	struct fluxland_wlcs_server *s = wl_container_of(ds, s, base);
	wl_display_terminate(s->server.wl_display);
}

static struct WlcsIntegrationDescriptor const *
wlcs_get_descriptor(struct WlcsDisplayServer const *ds)
{
	(void)ds;
	return &integration_descriptor;
}

/*
 * A configuration directory of fluxland's defaults, not the user's (one
 * for the whole run), with no toolbar: WLCS lays its panels and windows
 * out on a bare output.
 */
static const char *
test_config_dir(void)
{
	static char dir[] = "/tmp/fluxland-wlcs-XXXXXX";
	static bool made;
	if (made || !mkdtemp(dir))
		return made ? dir : NULL;
	char path[sizeof(dir) + 8];
	snprintf(path, sizeof(path), "%s/init", dir);
	FILE *f = fopen(path, "w");
	if (f) {
		fputs("session.screen0.toolbar.visible: false\n", f);
		fclose(f);
	}
	made = true;
	return dir;
}

static struct WlcsDisplayServer *
wlcs_create_server(int argc, char const **argv)
{
	(void)argc;
	(void)argv;
	struct fluxland_wlcs_server *s = calloc(1, sizeof(*s));
	if (!s)
		return NULL;
	wl_list_init(&s->clients);

	setenv("WLR_BACKENDS", "headless", 1);
	setenv("WLR_RENDERER", "pixman", 1);
	setenv("WLR_HEADLESS_OUTPUTS", "1", 1);
	const char *cfg = test_config_dir();
	if (cfg)
		setenv("FLUXLAND_CONFIG_DIR", cfg, 1);
	wlr_log_init(getenv("FLUXLAND_WLCS_DEBUG") ? WLR_DEBUG : WLR_ERROR,
		NULL);

	if (!wm_server_init(&s->server)) {
		wlr_log(WLR_ERROR, "%s", "wlcs: failed to initialize server");
		free(s);
		return NULL;
	}

	s->base.version = WLCS_DISPLAY_SERVER_VERSION;
	s->base.start = NULL;	/* start_on_this_thread instead */
	s->base.stop = wlcs_stop;
	s->base.create_client_socket = wlcs_create_client_socket;
	s->base.position_window_absolute = wlcs_position_window_absolute;
	s->base.create_pointer = wlcs_create_pointer;
	s->base.create_touch = wlcs_create_touch;
	s->base.get_descriptor = wlcs_get_descriptor;
	s->base.start_on_this_thread = wlcs_start_on_this_thread;
	return &s->base;
}

static void
wlcs_destroy_server(struct WlcsDisplayServer *ds)
{
	struct fluxland_wlcs_server *s = wl_container_of(ds, s, base);
	if (s->started) {
		wlr_touch_finish(&s->touchscreen);
		wlr_keyboard_finish(&s->keyboard);
	}
	wm_server_destroy(&s->server);
	free(s);
}

/* The entry point WLCS looks up */
__attribute__((visibility("default")))
const struct WlcsServerIntegration wlcs_server_integration = {
	.version = WLCS_SERVER_INTEGRATION_VERSION,
	.create_server = wlcs_create_server,
	.destroy_server = wlcs_destroy_server,
};
