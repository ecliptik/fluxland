/*
 * test_session_lock_logic.c - Unit tests for session_lock.c internal logic
 *
 * Includes session_lock.c directly with stub implementations to avoid
 * needing wlroots/wayland libraries at link time. Tests init/finish,
 * lock lifecycle, surface management, focus, and error paths.
 */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- Block real headers via their include guards --- */
#define WAYLAND_SERVER_CORE_H
#define WAYLAND_SERVER_H
#define WAYLAND_UTIL_H
#define WLR_UTIL_LOG_H
#define WLR_TYPES_WLR_OUTPUT_H
#define WLR_TYPES_WLR_SCENE_H
#define WLR_TYPES_WLR_SEAT_H
#define WLR_TYPES_WLR_SESSION_LOCK_H
#define WLR_UTIL_BOX_H
#define WLR_TYPES_WLR_COMPOSITOR_H
#define WM_SESSION_LOCK_H
#define WM_OUTPUT_H
#define WM_SERVER_H
#define WM_VIEW_H

/* --- Stub wayland types --- */

struct wl_display { int dummy; };

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

struct wl_listener {
	struct wl_list link;
	void (*notify)(struct wl_listener *listener, void *data);
};

struct wl_signal {
	struct wl_list listener_list;
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

#define wl_container_of(ptr, sample, member) \
	((void *)((char *)(ptr) - offsetof(__typeof__(*sample), member)))

#define wl_list_for_each(pos, head, member)				\
	for (pos = wl_container_of((head)->next, pos, member);		\
	     &(pos)->member != (head);					\
	     pos = wl_container_of((pos)->member.next, pos, member))

/* wlr_log no-op */
#define wlr_log(verb, fmt, ...) ((void)0)
#define WLR_ERROR 1
#define WLR_INFO 3
#define WLR_DEBUG 7

/* --- Stub wlr types --- */

struct wlr_scene_node { int x, y; };
struct wlr_scene_tree { struct wlr_scene_node node; };
struct wlr_keyboard_modifiers { uint32_t dummy; };

struct wlr_keyboard {
	uint32_t *keycodes;
	size_t num_keycodes;
	struct wlr_keyboard_modifiers modifiers;
};

struct wlr_seat { int dummy; };
struct wlr_output_layout { int dummy; };
struct wlr_box { int x, y, width, height; };

struct wlr_surface {
	bool mapped;
	struct {
		struct wl_signal map;
		struct wl_signal commit;
	} events;
};

struct wlr_output {
	char *name;
	struct {
		struct wl_signal commit;
	} events;
};

struct wlr_session_lock_surface_v1 {
	struct wlr_surface *surface;
	struct wlr_output *output;
	void *data;
	struct {
		struct wl_signal destroy;
	} events;
};

struct wlr_session_lock_v1 {
	struct {
		struct wl_signal new_surface;
		struct wl_signal unlock;
		struct wl_signal destroy;
	} events;
};

struct wlr_session_lock_manager_v1 {
	struct {
		struct wl_signal new_lock;
		struct wl_signal destroy;
	} events;
};

struct wlr_xdg_surface {
	struct wlr_surface *surface;
};

struct wlr_xdg_toplevel {
	struct wlr_xdg_surface *base;
};

/* --- Stub application types (must match session_lock.h) --- */

struct wm_lock_surface {
	struct wlr_session_lock_surface_v1 *lock_surface;
	struct wlr_scene_tree *scene_tree;
	struct wlr_scene_tree *background;

	struct wl_listener map;
	struct wl_listener destroy;
	struct wl_listener surface_commit;
	struct wl_listener output_mode;

	struct wl_list link;
};

struct wm_view {
	struct wlr_xdg_toplevel *xdg_toplevel;
};

struct wm_session_lock {
	struct wm_server *server;
	struct wlr_session_lock_manager_v1 *manager;
	struct wlr_session_lock_v1 *active_lock;
	bool locked;

	struct wl_listener new_lock;
	struct wl_listener manager_destroy;

	struct wl_listener lock_new_surface;
	struct wl_listener lock_unlock;
	struct wl_listener lock_destroy;

	struct wl_list lock_surfaces;
};

struct wm_server {
	struct wlr_seat *seat;
	struct wm_session_lock session_lock;
	struct wm_view *focused_view;
	struct wlr_scene_tree *layer_overlay;
	struct wlr_output_layout *output_layout;
	struct wl_display *wl_display;
};

/* --- Stub wlroots functions with call counters --- */

static struct wlr_session_lock_manager_v1 *stub_manager_ret;
static int mgr_create_count;

static struct wlr_session_lock_manager_v1 *
wlr_session_lock_manager_v1_create(struct wl_display *display)
{
	(void)display;
	mgr_create_count++;
	if (stub_manager_ret) {
		wl_signal_init(&stub_manager_ret->events.new_lock);
		wl_signal_init(&stub_manager_ret->events.destroy);
	}
	return stub_manager_ret;
}

static int send_locked_count;

static void
wlr_session_lock_v1_send_locked(struct wlr_session_lock_v1 *lock)
{
	(void)lock;
	send_locked_count++;
}

static int lock_v1_destroy_count;

static void
wlr_session_lock_v1_destroy(struct wlr_session_lock_v1 *lock)
{
	(void)lock;
	lock_v1_destroy_count++;
}

static int configure_count;
static uint32_t last_configure_w, last_configure_h;

static void
wlr_session_lock_surface_v1_configure(
	struct wlr_session_lock_surface_v1 *surface, uint32_t w, uint32_t h)
{
	(void)surface;
	configure_count++;
	last_configure_w = w;
	last_configure_h = h;
}

static int eff_res_w = 1920, eff_res_h = 1080;

static void
wlr_output_effective_resolution(struct wlr_output *output, int *w, int *h)
{
	(void)output;
	*w = eff_res_w;
	*h = eff_res_h;
}

static struct wlr_scene_tree *stub_scene_tree_ret;
static int scene_tree_create_count;

static struct wlr_scene_tree *
wlr_scene_subsurface_tree_create(struct wlr_scene_tree *parent,
	struct wlr_surface *surface)
{
	(void)parent;
	(void)surface;
	scene_tree_create_count++;
	return stub_scene_tree_ret;
}

static int scene_set_pos_count;

static void
wlr_scene_node_set_position(struct wlr_scene_node *node, int x, int y)
{
	node->x = x;
	node->y = y;
	scene_set_pos_count++;
}

static struct wlr_box stub_output_box;

static void
wlr_output_layout_get_box(struct wlr_output_layout *layout,
	struct wlr_output *output, struct wlr_box *box)
{
	(void)layout;
	(void)output;
	*box = stub_output_box;
}

static struct wlr_keyboard *stub_keyboard_ret;

static struct wlr_keyboard *
wlr_seat_get_keyboard(struct wlr_seat *seat)
{
	(void)seat;
	return stub_keyboard_ret;
}

static int kb_notify_enter_count;

static void
wlr_seat_keyboard_notify_enter(struct wlr_seat *seat,
	struct wlr_surface *surface, uint32_t *keycodes,
	size_t num_keycodes, struct wlr_keyboard_modifiers *modifiers)
{
	(void)seat;
	(void)surface;
	(void)keycodes;
	(void)num_keycodes;
	(void)modifiers;
	kb_notify_enter_count++;
}

static int kb_clear_focus_count;

static void
wlr_seat_keyboard_clear_focus(struct wlr_seat *seat)
{
	(void)seat;
	kb_clear_focus_count++;
}

static int ptr_clear_focus_count;

static void
wlr_seat_pointer_clear_focus(struct wlr_seat *seat)
{
	(void)seat;
	ptr_clear_focus_count++;
}

/* --- Include the source under test --- */
#include "session_lock.c"

/* --- Emit helper --- */
static void
emit_signal(struct wl_signal *signal, void *data)
{
	struct wl_list *pos, *tmp;
	for (pos = signal->listener_list.next, tmp = pos->next;
	     pos != &signal->listener_list;
	     pos = tmp, tmp = pos->next) {
		struct wl_listener *l = (struct wl_listener *)
			((char *)pos - offsetof(struct wl_listener, link));
		l->notify(l, data);
	}
}

/* --- Reset all counters --- */
static void
reset_counters(void)
{
	mgr_create_count = 0;
	send_locked_count = 0;
	lock_v1_destroy_count = 0;
	configure_count = 0;
	scene_tree_create_count = 0;
	scene_set_pos_count = 0;
	kb_notify_enter_count = 0;
	kb_clear_focus_count = 0;
	ptr_clear_focus_count = 0;
}

/* --- Server setup helper --- */
static struct wlr_seat g_seat;
static struct wlr_scene_tree g_overlay;
static struct wlr_output_layout g_layout;
static struct wl_display g_display;
static struct wlr_session_lock_manager_v1 g_manager;

static void
setup_server(struct wm_server *server)
{
	memset(server, 0, sizeof(*server));
	server->seat = &g_seat;
	server->layer_overlay = &g_overlay;
	server->output_layout = &g_layout;
	server->wl_display = &g_display;
	server->focused_view = NULL;
}

/* ------------------------------------------------------------------ */
/* Tests                                                               */
/* ------------------------------------------------------------------ */

static void
test_init_normal(void)
{
	struct wm_server server;
	setup_server(&server);
	reset_counters();

	stub_manager_ret = &g_manager;
	wm_session_lock_init(&server);

	assert(mgr_create_count == 1);
	assert(server.session_lock.manager == &g_manager);
	assert(server.session_lock.locked == false);
	assert(server.session_lock.active_lock == NULL);
	assert(server.session_lock.server == &server);

	wm_session_lock_finish(&server);
	printf("  PASS: test_init_normal\n");
}

static void
test_init_manager_fail(void)
{
	struct wm_server server;
	setup_server(&server);
	reset_counters();

	stub_manager_ret = NULL;
	wm_session_lock_init(&server);

	assert(mgr_create_count == 1);
	assert(server.session_lock.manager == NULL);
	printf("  PASS: test_init_manager_fail\n");
}

static void
test_finish(void)
{
	struct wm_server server;
	setup_server(&server);
	stub_manager_ret = &g_manager;
	wm_session_lock_init(&server);

	wm_session_lock_finish(&server);
	/* Verify listeners were cleaned up (no crash) */
	printf("  PASS: test_finish\n");
}

static void
test_is_locked(void)
{
	struct wm_server server;
	setup_server(&server);
	server.session_lock.locked = false;
	assert(wm_session_is_locked(&server) == false);

	server.session_lock.locked = true;
	assert(wm_session_is_locked(&server) == true);
	printf("  PASS: test_is_locked\n");
}

static void
test_new_lock(void)
{
	struct wm_server server;
	setup_server(&server);
	reset_counters();

	stub_manager_ret = &g_manager;
	wm_session_lock_init(&server);

	/* Create a lock object with initialized signals */
	struct wlr_session_lock_v1 lock;
	wl_signal_init(&lock.events.new_surface);
	wl_signal_init(&lock.events.unlock);
	wl_signal_init(&lock.events.destroy);

	/* Emit new_lock */
	emit_signal(&g_manager.events.new_lock, &lock);

	assert(server.session_lock.locked == true);
	assert(server.session_lock.active_lock == &lock);
	assert(send_locked_count == 1);
	assert(kb_clear_focus_count == 1);
	assert(ptr_clear_focus_count == 1);

	/* Clean up: emit unlock */
	emit_signal(&lock.events.unlock, NULL);
	wm_session_lock_finish(&server);
	printf("  PASS: test_new_lock\n");
}

static void
test_double_lock_rejected(void)
{
	struct wm_server server;
	setup_server(&server);
	reset_counters();

	stub_manager_ret = &g_manager;
	wm_session_lock_init(&server);

	struct wlr_session_lock_v1 lock1;
	wl_signal_init(&lock1.events.new_surface);
	wl_signal_init(&lock1.events.unlock);
	wl_signal_init(&lock1.events.destroy);
	emit_signal(&g_manager.events.new_lock, &lock1);

	assert(server.session_lock.active_lock == &lock1);

	/* Second lock should be rejected */
	struct wlr_session_lock_v1 lock2;
	wl_signal_init(&lock2.events.new_surface);
	wl_signal_init(&lock2.events.unlock);
	wl_signal_init(&lock2.events.destroy);
	emit_signal(&g_manager.events.new_lock, &lock2);

	assert(lock_v1_destroy_count == 1);
	assert(server.session_lock.active_lock == &lock1);

	emit_signal(&lock1.events.unlock, NULL);
	wm_session_lock_finish(&server);
	printf("  PASS: test_double_lock_rejected\n");
}

static void
test_unlock_with_focused_view_and_keyboard(void)
{
	struct wm_server server;
	setup_server(&server);
	reset_counters();

	stub_manager_ret = &g_manager;
	wm_session_lock_init(&server);

	/* Set up a focused view */
	struct wlr_surface view_surface = {0};
	struct wlr_xdg_surface xdg_surface = { .surface = &view_surface };
	struct wlr_xdg_toplevel toplevel = { .base = &xdg_surface };
	struct wm_view view = { .xdg_toplevel = &toplevel };
	server.focused_view = &view;

	/* Set up keyboard */
	struct wlr_keyboard kb = {0};
	stub_keyboard_ret = &kb;

	struct wlr_session_lock_v1 lock;
	wl_signal_init(&lock.events.new_surface);
	wl_signal_init(&lock.events.unlock);
	wl_signal_init(&lock.events.destroy);
	emit_signal(&g_manager.events.new_lock, &lock);

	reset_counters();
	emit_signal(&lock.events.unlock, NULL);

	assert(server.session_lock.locked == false);
	assert(server.session_lock.active_lock == NULL);
	assert(kb_notify_enter_count == 1); /* focus restored to view */
	assert(ptr_clear_focus_count == 1);

	wm_session_lock_finish(&server);
	printf("  PASS: test_unlock_with_focused_view_and_keyboard\n");
}

static void
test_unlock_no_focused_view(void)
{
	struct wm_server server;
	setup_server(&server);
	reset_counters();

	stub_manager_ret = &g_manager;
	wm_session_lock_init(&server);

	server.focused_view = NULL;
	stub_keyboard_ret = NULL;

	struct wlr_session_lock_v1 lock;
	wl_signal_init(&lock.events.new_surface);
	wl_signal_init(&lock.events.unlock);
	wl_signal_init(&lock.events.destroy);
	emit_signal(&g_manager.events.new_lock, &lock);

	reset_counters();
	emit_signal(&lock.events.unlock, NULL);

	assert(server.session_lock.locked == false);
	assert(kb_clear_focus_count == 1); /* no view, clear focus */
	assert(ptr_clear_focus_count == 1);

	wm_session_lock_finish(&server);
	printf("  PASS: test_unlock_no_focused_view\n");
}

static void
test_lock_destroy_while_locked(void)
{
	struct wm_server server;
	setup_server(&server);
	reset_counters();

	stub_manager_ret = &g_manager;
	wm_session_lock_init(&server);

	struct wlr_session_lock_v1 lock;
	wl_signal_init(&lock.events.new_surface);
	wl_signal_init(&lock.events.unlock);
	wl_signal_init(&lock.events.destroy);
	emit_signal(&g_manager.events.new_lock, &lock);

	assert(server.session_lock.locked == true);

	/* Destroy while still locked (client crash) */
	emit_signal(&lock.events.destroy, NULL);

	/* Session stays locked for security */
	assert(server.session_lock.locked == true);
	assert(server.session_lock.active_lock == NULL);

	wm_session_lock_finish(&server);
	printf("  PASS: test_lock_destroy_while_locked\n");
}

static void
test_lock_destroy_while_unlocked(void)
{
	struct wm_server server;
	setup_server(&server);
	reset_counters();

	stub_manager_ret = &g_manager;
	wm_session_lock_init(&server);

	struct wlr_session_lock_v1 lock;
	wl_signal_init(&lock.events.new_surface);
	wl_signal_init(&lock.events.unlock);
	wl_signal_init(&lock.events.destroy);
	emit_signal(&g_manager.events.new_lock, &lock);

	/* Unlock first */
	emit_signal(&lock.events.unlock, NULL);
	assert(server.session_lock.locked == false);

	/* Now destroy is handled by the already-cleaned-up listeners
	 * (they were removed on unlock). No crash = pass. */

	wm_session_lock_finish(&server);
	printf("  PASS: test_lock_destroy_while_unlocked\n");
}

static void
test_manager_destroy(void)
{
	struct wm_server server;
	setup_server(&server);
	reset_counters();

	stub_manager_ret = &g_manager;
	wm_session_lock_init(&server);

	assert(server.session_lock.manager == &g_manager);

	emit_signal(&g_manager.events.destroy, NULL);

	assert(server.session_lock.manager == NULL);
	printf("  PASS: test_manager_destroy\n");
}

static void
test_new_lock_surface_normal(void)
{
	struct wm_server server;
	setup_server(&server);
	reset_counters();

	stub_manager_ret = &g_manager;
	wm_session_lock_init(&server);

	struct wlr_session_lock_v1 lock;
	wl_signal_init(&lock.events.new_surface);
	wl_signal_init(&lock.events.unlock);
	wl_signal_init(&lock.events.destroy);
	emit_signal(&g_manager.events.new_lock, &lock);

	/* Create lock surface with output */
	struct wlr_surface surface = { .mapped = false };
	wl_signal_init(&surface.events.map);
	wl_signal_init(&surface.events.commit);

	struct wlr_output output = { .name = "HDMI-A-1" };
	wl_signal_init(&output.events.commit);

	struct wlr_session_lock_surface_v1 lock_surface = {
		.surface = &surface,
		.output = &output,
	};
	wl_signal_init(&lock_surface.events.destroy);

	struct wlr_scene_tree scene_tree = {0};
	stub_scene_tree_ret = &scene_tree;
	stub_output_box = (struct wlr_box){ .x = 100, .y = 200,
		.width = 1920, .height = 1080 };
	eff_res_w = 1920;
	eff_res_h = 1080;

	reset_counters();
	emit_signal(&lock.events.new_surface, &lock_surface);

	assert(scene_tree_create_count == 1);
	assert(scene_set_pos_count == 1);
	assert(scene_tree.node.x == 100);
	assert(scene_tree.node.y == 200);
	assert(configure_count == 1);
	assert(last_configure_w == 1920);
	assert(last_configure_h == 1080);
	assert(!wl_list_empty(&server.session_lock.lock_surfaces));

	/* Clean up by emitting destroy */
	emit_signal(&lock_surface.events.destroy, NULL);
	emit_signal(&lock.events.unlock, NULL);
	wm_session_lock_finish(&server);
	printf("  PASS: test_new_lock_surface_normal\n");
}

static void
test_new_lock_surface_null_output(void)
{
	struct wm_server server;
	setup_server(&server);
	reset_counters();

	stub_manager_ret = &g_manager;
	wm_session_lock_init(&server);

	struct wlr_session_lock_v1 lock;
	wl_signal_init(&lock.events.new_surface);
	wl_signal_init(&lock.events.unlock);
	wl_signal_init(&lock.events.destroy);
	emit_signal(&g_manager.events.new_lock, &lock);

	struct wlr_surface surface = { .mapped = false };
	wl_signal_init(&surface.events.map);
	wl_signal_init(&surface.events.commit);

	struct wlr_session_lock_surface_v1 lock_surface = {
		.surface = &surface,
		.output = NULL,
	};
	wl_signal_init(&lock_surface.events.destroy);

	struct wlr_scene_tree scene_tree = {0};
	stub_scene_tree_ret = &scene_tree;

	reset_counters();
	emit_signal(&lock.events.new_surface, &lock_surface);

	assert(scene_tree_create_count == 1);
	assert(scene_set_pos_count == 0); /* no output, no positioning */
	assert(configure_count == 0); /* configure_lock_surface returns early */

	emit_signal(&lock_surface.events.destroy, NULL);
	emit_signal(&lock.events.unlock, NULL);
	wm_session_lock_finish(&server);
	printf("  PASS: test_new_lock_surface_null_output\n");
}

static void
test_new_lock_surface_scene_tree_fail(void)
{
	struct wm_server server;
	setup_server(&server);
	reset_counters();

	stub_manager_ret = &g_manager;
	wm_session_lock_init(&server);

	struct wlr_session_lock_v1 lock;
	wl_signal_init(&lock.events.new_surface);
	wl_signal_init(&lock.events.unlock);
	wl_signal_init(&lock.events.destroy);
	emit_signal(&g_manager.events.new_lock, &lock);

	struct wlr_surface surface = { .mapped = false };
	wl_signal_init(&surface.events.map);
	wl_signal_init(&surface.events.commit);

	struct wlr_output output = { .name = "HDMI-A-1" };
	wl_signal_init(&output.events.commit);

	struct wlr_session_lock_surface_v1 lock_surface = {
		.surface = &surface,
		.output = &output,
	};
	wl_signal_init(&lock_surface.events.destroy);

	stub_scene_tree_ret = NULL; /* fail */

	reset_counters();
	emit_signal(&lock.events.new_surface, &lock_surface);

	assert(scene_tree_create_count == 1);
	assert(wl_list_empty(&server.session_lock.lock_surfaces));

	emit_signal(&lock.events.unlock, NULL);
	wm_session_lock_finish(&server);
	printf("  PASS: test_new_lock_surface_scene_tree_fail\n");
}

static void
test_lock_surface_map_with_keyboard(void)
{
	struct wm_server server;
	setup_server(&server);
	reset_counters();

	stub_manager_ret = &g_manager;
	wm_session_lock_init(&server);

	struct wlr_session_lock_v1 lock;
	wl_signal_init(&lock.events.new_surface);
	wl_signal_init(&lock.events.unlock);
	wl_signal_init(&lock.events.destroy);
	emit_signal(&g_manager.events.new_lock, &lock);

	struct wlr_surface surface = { .mapped = false };
	wl_signal_init(&surface.events.map);
	wl_signal_init(&surface.events.commit);

	struct wlr_output output = { .name = "HDMI-A-1" };
	wl_signal_init(&output.events.commit);

	struct wlr_session_lock_surface_v1 lock_surface = {
		.surface = &surface,
		.output = &output,
	};
	wl_signal_init(&lock_surface.events.destroy);

	struct wlr_scene_tree scene_tree = {0};
	stub_scene_tree_ret = &scene_tree;
	stub_output_box = (struct wlr_box){ .x = 0, .y = 0,
		.width = 1920, .height = 1080 };

	emit_signal(&lock.events.new_surface, &lock_surface);

	/* Now map the surface (keyboard available) */
	struct wlr_keyboard kb = {0};
	stub_keyboard_ret = &kb;
	surface.mapped = true;

	reset_counters();
	emit_signal(&surface.events.map, NULL);

	assert(kb_notify_enter_count == 1);

	emit_signal(&lock_surface.events.destroy, NULL);
	emit_signal(&lock.events.unlock, NULL);
	wm_session_lock_finish(&server);
	printf("  PASS: test_lock_surface_map_with_keyboard\n");
}

static void
test_lock_surface_map_no_keyboard(void)
{
	struct wm_server server;
	setup_server(&server);
	reset_counters();

	stub_manager_ret = &g_manager;
	wm_session_lock_init(&server);

	struct wlr_session_lock_v1 lock;
	wl_signal_init(&lock.events.new_surface);
	wl_signal_init(&lock.events.unlock);
	wl_signal_init(&lock.events.destroy);
	emit_signal(&g_manager.events.new_lock, &lock);

	struct wlr_surface surface = { .mapped = false };
	wl_signal_init(&surface.events.map);
	wl_signal_init(&surface.events.commit);

	struct wlr_output output = { .name = "HDMI-A-1" };
	wl_signal_init(&output.events.commit);

	struct wlr_session_lock_surface_v1 lock_surface = {
		.surface = &surface,
		.output = &output,
	};
	wl_signal_init(&lock_surface.events.destroy);

	struct wlr_scene_tree scene_tree = {0};
	stub_scene_tree_ret = &scene_tree;
	stub_output_box = (struct wlr_box){ .x = 0, .y = 0,
		.width = 1920, .height = 1080 };

	emit_signal(&lock.events.new_surface, &lock_surface);

	/* Map without keyboard */
	stub_keyboard_ret = NULL;
	surface.mapped = true;

	reset_counters();
	emit_signal(&surface.events.map, NULL);

	/* Still calls notify_enter with NULL keycodes */
	assert(kb_notify_enter_count == 1);

	emit_signal(&lock_surface.events.destroy, NULL);
	emit_signal(&lock.events.unlock, NULL);
	wm_session_lock_finish(&server);
	printf("  PASS: test_lock_surface_map_no_keyboard\n");
}

static void
test_focus_no_mapped_surfaces(void)
{
	struct wm_server server;
	setup_server(&server);
	reset_counters();

	stub_manager_ret = &g_manager;
	wm_session_lock_init(&server);

	struct wlr_session_lock_v1 lock;
	wl_signal_init(&lock.events.new_surface);
	wl_signal_init(&lock.events.unlock);
	wl_signal_init(&lock.events.destroy);
	emit_signal(&g_manager.events.new_lock, &lock);

	/* Create surface but don't map it */
	struct wlr_surface surface = { .mapped = false };
	wl_signal_init(&surface.events.map);
	wl_signal_init(&surface.events.commit);

	struct wlr_output output = { .name = "HDMI-A-1" };
	wl_signal_init(&output.events.commit);

	struct wlr_session_lock_surface_v1 lock_surface = {
		.surface = &surface,
		.output = &output,
	};
	wl_signal_init(&lock_surface.events.destroy);

	struct wlr_scene_tree scene_tree = {0};
	stub_scene_tree_ret = &scene_tree;
	stub_output_box = (struct wlr_box){0};

	emit_signal(&lock.events.new_surface, &lock_surface);

	/* Call focus directly on the lock with no mapped surfaces */
	reset_counters();
	focus_lock_surface(&server.session_lock);

	assert(kb_notify_enter_count == 0); /* no mapped surface */

	emit_signal(&lock_surface.events.destroy, NULL);
	emit_signal(&lock.events.unlock, NULL);
	wm_session_lock_finish(&server);
	printf("  PASS: test_focus_no_mapped_surfaces\n");
}

static void
test_lock_surface_destroy(void)
{
	struct wm_server server;
	setup_server(&server);
	reset_counters();

	stub_manager_ret = &g_manager;
	wm_session_lock_init(&server);

	struct wlr_session_lock_v1 lock;
	wl_signal_init(&lock.events.new_surface);
	wl_signal_init(&lock.events.unlock);
	wl_signal_init(&lock.events.destroy);
	emit_signal(&g_manager.events.new_lock, &lock);

	struct wlr_surface surface = { .mapped = false };
	wl_signal_init(&surface.events.map);
	wl_signal_init(&surface.events.commit);

	struct wlr_output output = { .name = "HDMI-A-1" };
	wl_signal_init(&output.events.commit);

	struct wlr_session_lock_surface_v1 lock_surface = {
		.surface = &surface,
		.output = &output,
	};
	wl_signal_init(&lock_surface.events.destroy);

	struct wlr_scene_tree scene_tree = {0};
	stub_scene_tree_ret = &scene_tree;
	stub_output_box = (struct wlr_box){0};

	emit_signal(&lock.events.new_surface, &lock_surface);
	assert(!wl_list_empty(&server.session_lock.lock_surfaces));

	/* Destroy the surface */
	emit_signal(&lock_surface.events.destroy, NULL);
	assert(wl_list_empty(&server.session_lock.lock_surfaces));

	emit_signal(&lock.events.unlock, NULL);
	wm_session_lock_finish(&server);
	printf("  PASS: test_lock_surface_destroy\n");
}

static void
test_lock_surface_commit_mapped(void)
{
	struct wm_server server;
	setup_server(&server);
	reset_counters();

	stub_manager_ret = &g_manager;
	wm_session_lock_init(&server);

	struct wlr_session_lock_v1 lock;
	wl_signal_init(&lock.events.new_surface);
	wl_signal_init(&lock.events.unlock);
	wl_signal_init(&lock.events.destroy);
	emit_signal(&g_manager.events.new_lock, &lock);

	struct wlr_surface surface = { .mapped = true };
	wl_signal_init(&surface.events.map);
	wl_signal_init(&surface.events.commit);

	struct wlr_output output = { .name = "HDMI-A-1" };
	wl_signal_init(&output.events.commit);

	struct wlr_session_lock_surface_v1 lock_surface = {
		.surface = &surface,
		.output = &output,
	};
	wl_signal_init(&lock_surface.events.destroy);

	struct wlr_scene_tree scene_tree = {0};
	stub_scene_tree_ret = &scene_tree;
	stub_output_box = (struct wlr_box){0};

	emit_signal(&lock.events.new_surface, &lock_surface);

	/* Commit on already-mapped surface: early return */
	reset_counters();
	emit_signal(&surface.events.commit, NULL);

	/* No crash = pass (handler returns early) */

	emit_signal(&lock_surface.events.destroy, NULL);
	emit_signal(&lock.events.unlock, NULL);
	wm_session_lock_finish(&server);
	printf("  PASS: test_lock_surface_commit_mapped\n");
}

static void
test_lock_surface_commit_unmapped(void)
{
	struct wm_server server;
	setup_server(&server);
	reset_counters();

	stub_manager_ret = &g_manager;
	wm_session_lock_init(&server);

	struct wlr_session_lock_v1 lock;
	wl_signal_init(&lock.events.new_surface);
	wl_signal_init(&lock.events.unlock);
	wl_signal_init(&lock.events.destroy);
	emit_signal(&g_manager.events.new_lock, &lock);

	struct wlr_surface surface = { .mapped = false };
	wl_signal_init(&surface.events.map);
	wl_signal_init(&surface.events.commit);

	struct wlr_output output = { .name = "HDMI-A-1" };
	wl_signal_init(&output.events.commit);

	struct wlr_session_lock_surface_v1 lock_surface = {
		.surface = &surface,
		.output = &output,
	};
	wl_signal_init(&lock_surface.events.destroy);

	struct wlr_scene_tree scene_tree = {0};
	stub_scene_tree_ret = &scene_tree;
	stub_output_box = (struct wlr_box){0};

	emit_signal(&lock.events.new_surface, &lock_surface);

	/* Commit on unmapped surface: falls through */
	reset_counters();
	emit_signal(&surface.events.commit, NULL);

	/* No crash = pass */

	emit_signal(&lock_surface.events.destroy, NULL);
	emit_signal(&lock.events.unlock, NULL);
	wm_session_lock_finish(&server);
	printf("  PASS: test_lock_surface_commit_unmapped\n");
}

static void
test_lock_surface_output_mode(void)
{
	struct wm_server server;
	setup_server(&server);
	reset_counters();

	stub_manager_ret = &g_manager;
	wm_session_lock_init(&server);

	struct wlr_session_lock_v1 lock;
	wl_signal_init(&lock.events.new_surface);
	wl_signal_init(&lock.events.unlock);
	wl_signal_init(&lock.events.destroy);
	emit_signal(&g_manager.events.new_lock, &lock);

	struct wlr_surface surface = { .mapped = false };
	wl_signal_init(&surface.events.map);
	wl_signal_init(&surface.events.commit);

	struct wlr_output output = { .name = "HDMI-A-1" };
	wl_signal_init(&output.events.commit);

	struct wlr_session_lock_surface_v1 lock_surface = {
		.surface = &surface,
		.output = &output,
	};
	wl_signal_init(&lock_surface.events.destroy);

	struct wlr_scene_tree scene_tree = {0};
	stub_scene_tree_ret = &scene_tree;
	stub_output_box = (struct wlr_box){0};
	eff_res_w = 1920;
	eff_res_h = 1080;

	emit_signal(&lock.events.new_surface, &lock_surface);

	/* Simulate output mode change */
	reset_counters();
	eff_res_w = 2560;
	eff_res_h = 1440;
	emit_signal(&output.events.commit, NULL);

	assert(configure_count == 1);
	assert(last_configure_w == 2560);
	assert(last_configure_h == 1440);

	emit_signal(&lock_surface.events.destroy, NULL);
	emit_signal(&lock.events.unlock, NULL);
	wm_session_lock_finish(&server);
	printf("  PASS: test_lock_surface_output_mode\n");
}

static void
test_configure_lock_surface_null_output(void)
{
	struct wm_server server;
	setup_server(&server);
	reset_counters();

	/* Directly test configure_lock_surface with NULL output */
	struct wlr_session_lock_surface_v1 lock_surface = {
		.output = NULL,
	};
	struct wm_lock_surface ls = {
		.lock_surface = &lock_surface,
	};

	configure_lock_surface(&ls);

	assert(configure_count == 0); /* early return */
	printf("  PASS: test_configure_lock_surface_null_output\n");
}

static void
test_unlock_focused_view_no_keyboard(void)
{
	struct wm_server server;
	setup_server(&server);
	reset_counters();

	stub_manager_ret = &g_manager;
	wm_session_lock_init(&server);

	/* Set up focused view but no keyboard */
	struct wlr_surface view_surface = {0};
	struct wlr_xdg_surface xdg_surface = { .surface = &view_surface };
	struct wlr_xdg_toplevel toplevel = { .base = &xdg_surface };
	struct wm_view view = { .xdg_toplevel = &toplevel };
	server.focused_view = &view;
	stub_keyboard_ret = NULL;

	struct wlr_session_lock_v1 lock;
	wl_signal_init(&lock.events.new_surface);
	wl_signal_init(&lock.events.unlock);
	wl_signal_init(&lock.events.destroy);
	emit_signal(&g_manager.events.new_lock, &lock);

	reset_counters();
	emit_signal(&lock.events.unlock, NULL);

	/* With focused view but no keyboard, notify_enter is not called */
	assert(kb_notify_enter_count == 0);
	assert(ptr_clear_focus_count == 1);

	wm_session_lock_finish(&server);
	printf("  PASS: test_unlock_focused_view_no_keyboard\n");
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

int
main(void)
{
	printf("test_session_lock_logic:\n");

	test_init_normal();
	test_init_manager_fail();
	test_finish();
	test_is_locked();
	test_new_lock();
	test_double_lock_rejected();
	test_unlock_with_focused_view_and_keyboard();
	test_unlock_no_focused_view();
	test_unlock_focused_view_no_keyboard();
	test_lock_destroy_while_locked();
	test_lock_destroy_while_unlocked();
	test_manager_destroy();
	test_new_lock_surface_normal();
	test_new_lock_surface_null_output();
	test_new_lock_surface_scene_tree_fail();
	test_lock_surface_map_with_keyboard();
	test_lock_surface_map_no_keyboard();
	test_focus_no_mapped_surfaces();
	test_lock_surface_destroy();
	test_lock_surface_commit_mapped();
	test_lock_surface_commit_unmapped();
	test_lock_surface_output_mode();
	test_configure_lock_surface_null_output();

	printf("All session_lock_logic tests passed.\n");
	return 0;
}
