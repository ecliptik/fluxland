/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * wlcs_shim.c - WLCS (Wayland Conformance Test Suite) integration module
 *
 * This file implements the WLCS integration shim that allows fluxland to be
 * tested with Canonical's Wayland Conformance Test Suite (WLCS).
 *
 * WLCS is a Google Test-based test suite that validates Wayland protocol
 * conformance. It loads this shared library (libfluxland-wlcs.so) at runtime
 * and uses the exported wlcs_server_integration symbol to create, start,
 * and interact with the compositor.
 *
 * Build:
 *   meson setup build -Dwlcs=true
 *   ninja -C build
 *
 * Run:
 *   wlcs ./build/libfluxland-wlcs.so
 *
 * Architecture:
 *   - The compositor runs in a dedicated thread via wl_display_run()
 *   - WLCS connects as a client via socketpair + wl_client_create
 *   - Synthetic pointer/touch input is injected via wlr_cursor APIs
 *   - Window positioning uses wlr_scene_node_set_position on the view tree
 *
 * Thread safety:
 *   Wayland is single-threaded by design. The compositor event loop runs in
 *   its own thread. Cross-thread calls (create_client_socket) use
 *   wl_event_loop_add_idle() to schedule work on the event loop thread,
 *   with a mutex+condvar for synchronization.
 */

#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/backend/headless.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>

#include "output.h"
#include "server.h"
#include "view.h"

/*
 * Forward-declare client-side wl_proxy_get_id(). WLCS passes client-side
 * proxy objects (wl_surface, wl_display) into position_window_absolute.
 * We need the proxy ID to look up the corresponding server-side resource.
 * Cannot include <wayland-client-core.h> as it conflicts with server headers.
 */
struct wl_proxy;
extern uint32_t wl_proxy_get_id(struct wl_proxy *proxy);

/*
 * WLCS API type definitions (self-contained — no WLCS headers required).
 * These match the structs defined in wlcs/include/wlcs/display_server.h,
 * wlcs/include/wlcs/pointer.h, and wlcs/include/wlcs/touch.h.
 */

#define WLCS_DISPLAY_SERVER_VERSION 3
#define WLCS_SERVER_INTEGRATION_VERSION 1
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

/* --- Internal types --- */

struct fluxland_wlcs_server {
	struct WlcsDisplayServer base;
	struct wm_server server;
	pthread_t thread;
	bool running;
};

struct fluxland_wlcs_pointer {
	struct WlcsPointer base;
	struct fluxland_wlcs_server *wlcs;
};

struct fluxland_wlcs_touch {
	struct WlcsTouch base;
	struct fluxland_wlcs_server *wlcs;
};

/* Cross-thread client creation context */
struct pending_client {
	struct wl_display *display;
	int fd;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	bool done;
};

/* Protocols advertised to WLCS */
static const struct WlcsExtensionDescriptor supported_extensions[] = {
	{ "wl_compositor", 6 },
	{ "wl_subcompositor", 1 },
	{ "wl_shm", 1 },
	{ "wl_seat", 9 },
	{ "wl_output", 4 },
	{ "wl_data_device_manager", 3 },
	{ "xdg_wm_base", 6 },
	{ "zxdg_decoration_manager_v1", 1 },
	{ "zwlr_layer_shell_v1", 4 },
	{ "zwp_pointer_constraints_v1", 1 },
	{ "zwp_relative_pointer_manager_v1", 1 },
	{ "wp_viewporter", 1 },
	{ "wp_fractional_scale_manager_v1", 1 },
};

static const struct WlcsIntegrationDescriptor integration_descriptor = {
	.version = 1,
	.num_extensions = sizeof(supported_extensions) /
		sizeof(supported_extensions[0]),
	.supported_extensions = supported_extensions,
};

/* --- Pointer callbacks --- */

static void
pointer_move_absolute(struct WlcsPointer *pointer, wl_fixed_t x, wl_fixed_t y)
{
	struct fluxland_wlcs_pointer *p =
		wl_container_of(pointer, p, base);
	struct wm_server *server = &p->wlcs->server;
	double dx = wl_fixed_to_double(x);
	double dy = wl_fixed_to_double(y);

	wlr_cursor_warp_closest(server->cursor, NULL, dx, dy);

	/* Notify the seat about the motion */
	double sx, sy;
	struct wlr_scene_buffer *buf = NULL;
	struct wlr_scene_node *node = wlr_scene_node_at(
		&server->scene->tree.node, dx, dy, &sx, &sy);
	if (node && node->type == WLR_SCENE_NODE_BUFFER) {
		buf = wlr_scene_buffer_from_node(node);
		struct wlr_scene_surface *scene_surface =
			wlr_scene_surface_try_from_buffer(buf);
		if (scene_surface) {
			wlr_seat_pointer_notify_enter(server->seat,
				scene_surface->surface, sx, sy);
			wlr_seat_pointer_notify_motion(server->seat,
				0, sx, sy);
		}
	}
	wlr_seat_pointer_notify_frame(server->seat);
}

static void
pointer_move_relative(struct WlcsPointer *pointer, wl_fixed_t dx, wl_fixed_t dy)
{
	struct fluxland_wlcs_pointer *p =
		wl_container_of(pointer, p, base);
	struct wm_server *server = &p->wlcs->server;
	double ddx = wl_fixed_to_double(dx);
	double ddy = wl_fixed_to_double(dy);

	wlr_cursor_move(server->cursor, NULL, ddx, ddy);

	double cx = server->cursor->x;
	double cy = server->cursor->y;
	double sx, sy;
	struct wlr_scene_node *node = wlr_scene_node_at(
		&server->scene->tree.node, cx, cy, &sx, &sy);
	if (node && node->type == WLR_SCENE_NODE_BUFFER) {
		struct wlr_scene_buffer *buf =
			wlr_scene_buffer_from_node(node);
		struct wlr_scene_surface *scene_surface =
			wlr_scene_surface_try_from_buffer(buf);
		if (scene_surface) {
			wlr_seat_pointer_notify_enter(server->seat,
				scene_surface->surface, sx, sy);
			wlr_seat_pointer_notify_motion(server->seat,
				0, sx, sy);
		}
	}
	wlr_seat_pointer_notify_frame(server->seat);
}

static void
pointer_button_down(struct WlcsPointer *pointer, int button)
{
	struct fluxland_wlcs_pointer *p =
		wl_container_of(pointer, p, base);
	struct wm_server *server = &p->wlcs->server;

	wlr_seat_pointer_notify_button(server->seat, 0, button,
		WL_POINTER_BUTTON_STATE_PRESSED);
	wlr_seat_pointer_notify_frame(server->seat);
}

static void
pointer_button_up(struct WlcsPointer *pointer, int button)
{
	struct fluxland_wlcs_pointer *p =
		wl_container_of(pointer, p, base);
	struct wm_server *server = &p->wlcs->server;

	wlr_seat_pointer_notify_button(server->seat, 0, button,
		WL_POINTER_BUTTON_STATE_RELEASED);
	wlr_seat_pointer_notify_frame(server->seat);
}

static void
pointer_destroy(struct WlcsPointer *pointer)
{
	struct fluxland_wlcs_pointer *p =
		wl_container_of(pointer, p, base);
	free(p);
}

/* --- Touch callbacks --- */

static void
touch_down(struct WlcsTouch *touch, wl_fixed_t x, wl_fixed_t y)
{
	struct fluxland_wlcs_touch *t =
		wl_container_of(touch, t, base);
	struct wm_server *server = &t->wlcs->server;
	double dx = wl_fixed_to_double(x);
	double dy = wl_fixed_to_double(y);
	double sx, sy;

	struct wlr_scene_node *node = wlr_scene_node_at(
		&server->scene->tree.node, dx, dy, &sx, &sy);
	if (node && node->type == WLR_SCENE_NODE_BUFFER) {
		struct wlr_scene_buffer *buf =
			wlr_scene_buffer_from_node(node);
		struct wlr_scene_surface *scene_surface =
			wlr_scene_surface_try_from_buffer(buf);
		if (scene_surface) {
			wlr_seat_touch_notify_down(server->seat,
				scene_surface->surface, 0, 0, sx, sy);
		}
	}
}

static void
touch_move(struct WlcsTouch *touch, wl_fixed_t x, wl_fixed_t y)
{
	struct fluxland_wlcs_touch *t =
		wl_container_of(touch, t, base);
	struct wm_server *server = &t->wlcs->server;
	double dx = wl_fixed_to_double(x);
	double dy = wl_fixed_to_double(y);
	double sx, sy;

	struct wlr_scene_node *node = wlr_scene_node_at(
		&server->scene->tree.node, dx, dy, &sx, &sy);
	if (node && node->type == WLR_SCENE_NODE_BUFFER) {
		struct wlr_scene_buffer *buf =
			wlr_scene_buffer_from_node(node);
		struct wlr_scene_surface *scene_surface =
			wlr_scene_surface_try_from_buffer(buf);
		if (scene_surface) {
			wlr_seat_touch_notify_motion(server->seat,
				0, 0, sx, sy);
		}
	}
}

static void
touch_up(struct WlcsTouch *touch)
{
	struct fluxland_wlcs_touch *t =
		wl_container_of(touch, t, base);
	struct wm_server *server = &t->wlcs->server;

	wlr_seat_touch_notify_up(server->seat, 0, 0);
}

static void
touch_destroy(struct WlcsTouch *touch)
{
	struct fluxland_wlcs_touch *t =
		wl_container_of(touch, t, base);
	free(t);
}

/* --- Display server callbacks --- */

static void *
compositor_thread(void *data)
{
	struct fluxland_wlcs_server *s = data;
	wl_display_run(s->server.wl_display);
	return NULL;
}

static void
wlcs_start(struct WlcsDisplayServer *ds)
{
	struct fluxland_wlcs_server *s =
		wl_container_of(ds, s, base);

	/* Add a virtual output so the compositor has somewhere to render */
	wlr_headless_add_output(s->server.backend, 1920, 1080);

	/* Run the compositor event loop in a dedicated thread */
	s->running = true;
	if (pthread_create(&s->thread, NULL, compositor_thread, s) != 0) {
		wlr_log(WLR_ERROR, "%s", "wlcs: failed to create compositor thread");
		s->running = false;
	}
}

static void
wlcs_stop(struct WlcsDisplayServer *ds)
{
	struct fluxland_wlcs_server *s =
		wl_container_of(ds, s, base);

	if (s->running) {
		wl_display_terminate(s->server.wl_display);
		pthread_join(s->thread, NULL);
		s->running = false;
	}
}

/*
 * Create a client connection fd for WLCS.
 *
 * Uses an idle callback to schedule wl_client_create() on the event loop
 * thread, ensuring thread safety. The calling thread blocks until the
 * client is created.
 */
static void
create_client_idle_cb(void *data)
{
	struct pending_client *pc = data;

	wl_client_create(pc->display, pc->fd);

	pthread_mutex_lock(&pc->mutex);
	pc->done = true;
	pthread_cond_signal(&pc->cond);
	pthread_mutex_unlock(&pc->mutex);
}

static int
wlcs_create_client_socket(struct WlcsDisplayServer *ds)
{
	struct fluxland_wlcs_server *s =
		wl_container_of(ds, s, base);

	int fds[2];
	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds) < 0) {
		wlr_log(WLR_ERROR, "%s", "wlcs: socketpair failed");
		return -1;
	}

	struct pending_client pc = {
		.display = s->server.wl_display,
		.fd = fds[0],
		.done = false,
	};
	pthread_mutex_init(&pc.mutex, NULL);
	pthread_cond_init(&pc.cond, NULL);

	/*
	 * Schedule wl_client_create on the event loop thread.
	 * wl_event_loop_add_idle() is safe to call from another thread
	 * as it signals the event loop internally.
	 */
	struct wl_event_loop *loop =
		wl_display_get_event_loop(s->server.wl_display);
	wl_event_loop_add_idle(loop, create_client_idle_cb, &pc);

	/* Wait for the event loop to process our request */
	pthread_mutex_lock(&pc.mutex);
	while (!pc.done) {
		pthread_cond_wait(&pc.cond, &pc.mutex);
	}
	pthread_mutex_unlock(&pc.mutex);

	pthread_mutex_destroy(&pc.mutex);
	pthread_cond_destroy(&pc.cond);

	return fds[1];
}

static void
wlcs_position_window_absolute(struct WlcsDisplayServer *ds,
	struct wl_display *client_display, struct wl_surface *surface,
	int x, int y)
{
	struct fluxland_wlcs_server *s =
		wl_container_of(ds, s, base);

	/*
	 * WLCS passes us a client-side wl_surface proxy. We need to find the
	 * corresponding server-side view. Since we can't directly map
	 * client-side to server-side objects, we iterate views and match by
	 * checking the wl_resource associated with the surface.
	 *
	 * For WLCS, the surface proxy ID on the client side should match the
	 * wl_resource ID on the server side. Use wl_proxy_get_id().
	 */
	uint32_t surface_id = wl_proxy_get_id(
		(struct wl_proxy *)surface);
	struct wl_client *client = NULL;

	/* Find the wl_client for this client_display */
	struct wl_list *client_list =
		wl_display_get_client_list(s->server.wl_display);
	struct wl_client *c;
	wl_client_for_each(c, client_list) {
		client = c;
		/* Check if this client owns the surface resource */
		struct wl_resource *res =
			wl_client_get_object(c, surface_id);
		if (res) {
			client = c;
			break;
		}
	}

	if (!client) {
		return;
	}

	/* Find the view that matches this surface resource */
	struct wl_resource *surface_res =
		wl_client_get_object(client, surface_id);
	if (!surface_res) {
		return;
	}

	struct wm_view *view;
	wl_list_for_each(view, &s->server.views, link) {
		if (!view->xdg_toplevel) {
			continue;
		}
		struct wlr_surface *wlr_surf =
			view->xdg_toplevel->base->surface;
		if (wlr_surf->resource == surface_res) {
			view->x = x;
			view->y = y;
			wlr_scene_node_set_position(
				&view->scene_tree->node, x, y);
			return;
		}
	}
}

static struct WlcsPointer *
wlcs_create_pointer(struct WlcsDisplayServer *ds)
{
	struct fluxland_wlcs_server *s =
		wl_container_of(ds, s, base);

	struct fluxland_wlcs_pointer *p = calloc(1, sizeof(*p));
	if (!p) {
		return NULL;
	}

	p->wlcs = s;
	p->base.version = WLCS_POINTER_VERSION;
	p->base.move_absolute = pointer_move_absolute;
	p->base.move_relative = pointer_move_relative;
	p->base.button_up = pointer_button_up;
	p->base.button_down = pointer_button_down;
	p->base.destroy = pointer_destroy;

	return &p->base;
}

static struct WlcsTouch *
wlcs_create_touch(struct WlcsDisplayServer *ds)
{
	struct fluxland_wlcs_server *s =
		wl_container_of(ds, s, base);

	struct fluxland_wlcs_touch *t = calloc(1, sizeof(*t));
	if (!t) {
		return NULL;
	}

	t->wlcs = s;
	t->base.version = WLCS_TOUCH_VERSION;
	t->base.touch_down = touch_down;
	t->base.touch_move = touch_move;
	t->base.touch_up = touch_up;
	t->base.destroy = touch_destroy;

	return &t->base;
}

static struct WlcsIntegrationDescriptor const *
wlcs_get_descriptor(struct WlcsDisplayServer const *ds)
{
	(void)ds;
	return &integration_descriptor;
}

/* --- Server integration callbacks --- */

static struct WlcsDisplayServer *
wlcs_create_server(int argc, char const **argv)
{
	(void)argc;
	(void)argv;

	struct fluxland_wlcs_server *s = calloc(1, sizeof(*s));
	if (!s) {
		return NULL;
	}

	/* Force headless backend with software rendering */
	setenv("WLR_BACKENDS", "headless", 1);
	setenv("WLR_RENDERER", "pixman", 1);

	wlr_log_init(WLR_INFO, NULL);

	if (!wm_server_init(&s->server)) {
		wlr_log(WLR_ERROR, "%s", "wlcs: failed to initialize server");
		free(s);
		return NULL;
	}

	/*
	 * Start the backend and add a named socket. The socket isn't used
	 * by WLCS (it connects via create_client_socket), but wm_server_start
	 * also starts the wlr_backend which is required.
	 */
	if (!wm_server_start(&s->server)) {
		wlr_log(WLR_ERROR, "%s", "wlcs: failed to start server");
		wm_server_destroy(&s->server);
		free(s);
		return NULL;
	}

	/* Wire up the WLCS vtable */
	s->base.version = WLCS_DISPLAY_SERVER_VERSION;
	s->base.start = wlcs_start;
	s->base.stop = wlcs_stop;
	s->base.create_client_socket = wlcs_create_client_socket;
	s->base.position_window_absolute = wlcs_position_window_absolute;
	s->base.create_pointer = wlcs_create_pointer;
	s->base.create_touch = wlcs_create_touch;
	s->base.get_descriptor = wlcs_get_descriptor;
	s->base.start_on_this_thread = NULL; /* not implemented */
	s->running = false;

	return &s->base;
}

static void
wlcs_destroy_server(struct WlcsDisplayServer *ds)
{
	struct fluxland_wlcs_server *s =
		wl_container_of(ds, s, base);

	/* Ensure compositor is stopped */
	if (s->running) {
		wlcs_stop(ds);
	}

	wm_server_destroy(&s->server);
	free(s);
}

/*
 * The WLCS entry point. WLCS loads this shared library and looks up
 * this symbol to discover how to create and drive the compositor.
 */
__attribute__((visibility("default")))
const struct WlcsServerIntegration wlcs_server_integration = {
	.version = WLCS_SERVER_INTEGRATION_VERSION,
	.create_server = wlcs_create_server,
	.destroy_server = wlcs_destroy_server,
};
