/*
 * test_foreign_toplevel.c - Unit tests for foreign toplevel management
 *
 * Includes foreign_toplevel.c directly with stub implementations to avoid
 * needing the full wlroots/wayland libraries at link time.
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
#define WLR_TYPES_WLR_SCENE_H
#define WLR_TYPES_WLR_XDG_SHELL_H
#define WLR_TYPES_WLR_OUTPUT_LAYOUT_H
#define WLR_TYPES_WLR_SEAT_H
#define WLR_TYPES_WLR_FOREIGN_TOPLEVEL_MANAGEMENT_V1_H
#define WM_VIEW_H
#define WM_SERVER_H
#define WM_FOREIGN_TOPLEVEL_H

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

static inline void
wl_signal_emit_mutable(struct wl_signal *signal, void *data)
{
	/* Walk listener list and call each notify */
	struct wl_list *pos, *tmp;
	for (pos = signal->listener_list.next, tmp = pos->next;
	     pos != &signal->listener_list;
	     pos = tmp, tmp = pos->next) {
		struct wl_listener *l = (struct wl_listener *)
			((char *)pos - offsetof(struct wl_listener, link));
		l->notify(l, data);
	}
}

#include <stddef.h>
#define wl_container_of(ptr, sample, member) \
	((void *)((char *)(ptr) - offsetof(__typeof__(*sample), member)))

#define wl_list_for_each(pos, head, member)				\
	for (pos = wl_container_of((head)->next, pos, member);		\
	     &(pos)->member != (head);					\
	     pos = wl_container_of((pos)->member.next, pos, member))

/* wlr_log no-op */
#define wlr_log(verb, fmt, ...) ((void)0)
#define WLR_ERROR 1
#define WLR_DEBUG 7

/* --- Stub wlr types --- */

struct wlr_scene_node {
	int enabled;
	int x, y;
};

struct wlr_scene_tree {
	struct wlr_scene_node node;
};

struct wlr_box {
	int x, y, width, height;
};

struct wlr_surface { int dummy; };

struct wlr_output {
	int dummy;
};

struct wlr_output_layout { int dummy; };
struct wlr_seat { int dummy; };

struct wlr_xdg_surface {
	struct wlr_surface *surface;
};

struct wlr_xdg_toplevel {
	struct wlr_xdg_surface *base;
	int width, height;
	struct {
		struct wl_signal request_maximize;
		struct wl_signal request_fullscreen;
	} events;
};

/* --- Stub foreign toplevel wlr types --- */

struct wlr_foreign_toplevel_manager_v1 {
	int dummy;
};

struct wlr_foreign_toplevel_handle_v1 {
	char *title;
	char *app_id;
	bool maximized;
	bool minimized;
	bool activated;
	bool fullscreen;
	struct wlr_output *output_entered;
	struct {
		struct wl_signal request_activate;
		struct wl_signal request_maximize;
		struct wl_signal request_minimize;
		struct wl_signal request_fullscreen;
		struct wl_signal request_close;
	} events;
};

struct wlr_foreign_toplevel_handle_v1_maximized_event {
	struct wlr_foreign_toplevel_handle_v1 *toplevel;
	bool maximized;
};

struct wlr_foreign_toplevel_handle_v1_minimized_event {
	struct wlr_foreign_toplevel_handle_v1 *toplevel;
	bool minimized;
};

struct wlr_foreign_toplevel_handle_v1_activated_event {
	struct wlr_foreign_toplevel_handle_v1 *toplevel;
	struct wlr_seat *seat;
};

struct wlr_foreign_toplevel_handle_v1_fullscreen_event {
	struct wlr_foreign_toplevel_handle_v1 *toplevel;
	bool fullscreen;
	struct wlr_output *output;
};

/* --- Stub wm types --- */

struct wm_style { int dummy; };
struct wm_decoration { int dummy; };
struct wm_tab_group;
struct wm_animation;
struct wm_workspace;

struct wm_server {
	struct wl_display *wl_display;
	struct wlr_output_layout *output_layout;
	struct wlr_seat *seat;
	struct wlr_foreign_toplevel_manager_v1 *foreign_toplevel_manager;
	struct wm_view *focused_view;
	struct wl_list views;
};

struct wm_view {
	struct wl_list link;
	struct wm_server *server;
	struct wlr_xdg_toplevel *xdg_toplevel;
	struct wlr_scene_tree *scene_tree;
	uint32_t id;
	int x, y;
	struct wlr_box saved_geometry;
	bool maximized, fullscreen;
	bool maximized_vert, maximized_horiz;
	bool lhalf, rhalf;
	bool show_decoration;
	char *title;
	char *app_id;
	struct wm_workspace *workspace;
	bool sticky;
	int layer;
	struct wm_tab_group *tab_group;
	struct wl_list tab_link;
	int focus_alpha;
	int unfocus_alpha;
	struct wm_animation *animation;
	int focus_protection;
	bool ignore_size_hints;
	struct wlr_foreign_toplevel_handle_v1 *foreign_toplevel_handle;
	struct wl_listener foreign_toplevel_request_activate;
	struct wl_listener foreign_toplevel_request_maximize;
	struct wl_listener foreign_toplevel_request_minimize;
	struct wl_listener foreign_toplevel_request_fullscreen;
	struct wl_listener foreign_toplevel_request_close;
	struct wm_decoration *decoration;
};

/* --- Tracking globals --- */

static int g_focus_view_count;
static struct wm_view *g_last_focused_view;
static int g_manager_create_count;
static int g_handle_create_count;
static int g_handle_destroy_count;
static int g_set_title_count;
static char g_last_title[256];
static int g_set_app_id_count;
static char g_last_app_id[256];
static int g_set_maximized_count;
static bool g_last_maximized;
static int g_set_minimized_count;
static bool g_last_minimized;
static int g_set_activated_count;
static bool g_last_activated;
static int g_set_fullscreen_count;
static bool g_last_fullscreen;
static int g_output_enter_count;
static int g_send_close_count;
static int g_scene_set_enabled_count;
static bool g_last_scene_enabled;
static int g_seat_clear_focus_count;

/* Counts for xdg_toplevel signal emissions */
static int g_maximize_signal_count;
static int g_fullscreen_signal_count;

/* Stub handles pool */
static struct wlr_foreign_toplevel_handle_v1 g_handles[8];
static int g_handle_idx;
static struct wlr_foreign_toplevel_manager_v1 g_manager;

static void
reset_counters(void)
{
	g_focus_view_count = 0;
	g_last_focused_view = NULL;
	g_manager_create_count = 0;
	g_handle_create_count = 0;
	g_handle_destroy_count = 0;
	g_set_title_count = 0;
	g_last_title[0] = '\0';
	g_set_app_id_count = 0;
	g_last_app_id[0] = '\0';
	g_set_maximized_count = 0;
	g_last_maximized = false;
	g_set_minimized_count = 0;
	g_last_minimized = false;
	g_set_activated_count = 0;
	g_last_activated = false;
	g_set_fullscreen_count = 0;
	g_last_fullscreen = false;
	g_output_enter_count = 0;
	g_send_close_count = 0;
	g_scene_set_enabled_count = 0;
	g_last_scene_enabled = true;
	g_seat_clear_focus_count = 0;
	g_maximize_signal_count = 0;
	g_fullscreen_signal_count = 0;
}

/* Full reset including handle pool — call only at start of each test */
static void
reset_globals(void)
{
	reset_counters();
	g_handle_idx = 0;
	memset(g_handles, 0, sizeof(g_handles));
}

/* --- Stub wlroots functions used by foreign_toplevel.c --- */

static struct wlr_foreign_toplevel_manager_v1 *
wlr_foreign_toplevel_manager_v1_create(struct wl_display *display)
{
	(void)display;
	g_manager_create_count++;
	return &g_manager;
}

static struct wlr_foreign_toplevel_handle_v1 *
wlr_foreign_toplevel_handle_v1_create(
	struct wlr_foreign_toplevel_manager_v1 *manager)
{
	(void)manager;
	assert(g_handle_idx < 8);
	struct wlr_foreign_toplevel_handle_v1 *h = &g_handles[g_handle_idx++];
	wl_signal_init(&h->events.request_activate);
	wl_signal_init(&h->events.request_maximize);
	wl_signal_init(&h->events.request_minimize);
	wl_signal_init(&h->events.request_fullscreen);
	wl_signal_init(&h->events.request_close);
	g_handle_create_count++;
	return h;
}

static void
wlr_foreign_toplevel_handle_v1_destroy(
	struct wlr_foreign_toplevel_handle_v1 *toplevel)
{
	(void)toplevel;
	g_handle_destroy_count++;
}

static void
wlr_foreign_toplevel_handle_v1_set_title(
	struct wlr_foreign_toplevel_handle_v1 *toplevel, const char *title)
{
	(void)toplevel;
	g_set_title_count++;
	if (title) {
		snprintf(g_last_title, sizeof(g_last_title), "%s", title);
	}
}

static void
wlr_foreign_toplevel_handle_v1_set_app_id(
	struct wlr_foreign_toplevel_handle_v1 *toplevel, const char *app_id)
{
	(void)toplevel;
	g_set_app_id_count++;
	if (app_id) {
		snprintf(g_last_app_id, sizeof(g_last_app_id), "%s", app_id);
	}
}

static void
wlr_foreign_toplevel_handle_v1_set_maximized(
	struct wlr_foreign_toplevel_handle_v1 *toplevel, bool maximized)
{
	(void)toplevel;
	g_set_maximized_count++;
	g_last_maximized = maximized;
}

static void
wlr_foreign_toplevel_handle_v1_set_minimized(
	struct wlr_foreign_toplevel_handle_v1 *toplevel, bool minimized)
{
	(void)toplevel;
	g_set_minimized_count++;
	g_last_minimized = minimized;
}

static void
wlr_foreign_toplevel_handle_v1_set_activated(
	struct wlr_foreign_toplevel_handle_v1 *toplevel, bool activated)
{
	(void)toplevel;
	g_set_activated_count++;
	g_last_activated = activated;
}

static void
wlr_foreign_toplevel_handle_v1_set_fullscreen(
	struct wlr_foreign_toplevel_handle_v1 *toplevel, bool fullscreen)
{
	(void)toplevel;
	g_set_fullscreen_count++;
	g_last_fullscreen = fullscreen;
}

static void
wlr_foreign_toplevel_handle_v1_output_enter(
	struct wlr_foreign_toplevel_handle_v1 *toplevel,
	struct wlr_output *output)
{
	(void)toplevel;
	(void)output;
	g_output_enter_count++;
}

static struct wlr_output *
wlr_output_layout_output_at(struct wlr_output_layout *layout,
	double lx, double ly)
{
	(void)layout;
	(void)lx;
	(void)ly;
	/* Return a fake output */
	static struct wlr_output fake_output;
	return &fake_output;
}

static void
wlr_xdg_toplevel_send_close(struct wlr_xdg_toplevel *toplevel)
{
	(void)toplevel;
	g_send_close_count++;
}

static void
wlr_scene_node_set_enabled(struct wlr_scene_node *node, bool enabled)
{
	if (node) {
		node->enabled = enabled ? 1 : 0;
	}
	g_scene_set_enabled_count++;
	g_last_scene_enabled = enabled;
}

static void
wlr_seat_keyboard_notify_clear_focus(struct wlr_seat *seat)
{
	(void)seat;
	g_seat_clear_focus_count++;
}

/* --- Stub compositor functions --- */

static void
wm_focus_view(struct wm_view *view, struct wlr_surface *surface)
{
	(void)surface;
	g_last_focused_view = view;
	g_focus_view_count++;
}

/* --- Maximize/fullscreen signal listeners for tracking --- */

static void
maximize_signal_handler(struct wl_listener *listener, void *data)
{
	(void)listener;
	(void)data;
	g_maximize_signal_count++;
}

static void
fullscreen_signal_handler(struct wl_listener *listener, void *data)
{
	(void)listener;
	(void)data;
	g_fullscreen_signal_count++;
}

/* --- Include foreign_toplevel source directly --- */

#include "foreign_toplevel.c"

/* --- Test helpers --- */

static struct wm_server test_server;
static struct wl_display test_display;
static struct wlr_output_layout test_output_layout;
static struct wlr_seat test_seat;
static struct wlr_scene_tree test_trees[8];
static struct wlr_xdg_toplevel test_toplevels[8];
static struct wlr_xdg_surface test_xdg_surfaces[8];
static struct wlr_surface test_surfaces[8];

static void
init_server(void)
{
	memset(&test_server, 0, sizeof(test_server));
	test_server.wl_display = &test_display;
	test_server.output_layout = &test_output_layout;
	test_server.seat = &test_seat;
	test_server.focused_view = NULL;
	test_server.foreign_toplevel_manager = NULL;
	wl_list_init(&test_server.views);
}

static struct wm_view
make_view(int idx, const char *title, const char *app_id)
{
	test_surfaces[idx].dummy = 0;
	test_xdg_surfaces[idx].surface = &test_surfaces[idx];
	test_toplevels[idx].base = &test_xdg_surfaces[idx];
	test_toplevels[idx].width = 800;
	test_toplevels[idx].height = 600;
	wl_signal_init(&test_toplevels[idx].events.request_maximize);
	wl_signal_init(&test_toplevels[idx].events.request_fullscreen);
	test_trees[idx].node.enabled = 1;
	test_trees[idx].node.x = 100;
	test_trees[idx].node.y = 100;

	struct wm_view view;
	memset(&view, 0, sizeof(view));
	view.server = &test_server;
	view.xdg_toplevel = &test_toplevels[idx];
	view.scene_tree = &test_trees[idx];
	view.id = idx + 1;
	view.x = 100;
	view.y = 100;
	view.title = title ? strdup(title) : NULL;
	view.app_id = app_id ? strdup(app_id) : NULL;
	view.foreign_toplevel_handle = NULL;
	wl_list_init(&view.link);
	return view;
}

static void
free_view(struct wm_view *view)
{
	free(view->title);
	free(view->app_id);
}

/* ===== Tests ===== */

static void
test_init_creates_manager(void)
{
	reset_globals();
	init_server();

	wm_foreign_toplevel_init(&test_server);

	assert(g_manager_create_count == 1);
	assert(test_server.foreign_toplevel_manager == &g_manager);
	printf("  PASS: init_creates_manager\n");
}

static void
test_handle_create_basic(void)
{
	reset_globals();
	init_server();
	test_server.foreign_toplevel_manager = &g_manager;

	struct wm_view v = make_view(0, "Terminal", "org.terminal");
	wm_foreign_toplevel_handle_create(&v);

	assert(g_handle_create_count == 1);
	assert(v.foreign_toplevel_handle != NULL);
	assert(g_set_title_count == 1);
	assert(strcmp(g_last_title, "Terminal") == 0);
	assert(g_set_app_id_count == 1);
	assert(strcmp(g_last_app_id, "org.terminal") == 0);
	assert(g_output_enter_count == 1);

	wm_foreign_toplevel_handle_destroy(&v);
	free_view(&v);
	printf("  PASS: handle_create_basic\n");
}

static void
test_handle_create_no_manager(void)
{
	reset_globals();
	init_server();
	/* manager is NULL */

	struct wm_view v = make_view(0, "Terminal", "org.terminal");
	wm_foreign_toplevel_handle_create(&v);

	/* Should bail out without creating handle */
	assert(g_handle_create_count == 0);
	assert(v.foreign_toplevel_handle == NULL);

	free_view(&v);
	printf("  PASS: handle_create_no_manager\n");
}

static void
test_handle_create_null_title_appid(void)
{
	reset_globals();
	init_server();
	test_server.foreign_toplevel_manager = &g_manager;

	struct wm_view v = make_view(0, NULL, NULL);
	wm_foreign_toplevel_handle_create(&v);

	assert(g_handle_create_count == 1);
	assert(v.foreign_toplevel_handle != NULL);
	/* Title and app_id should NOT be set when NULL */
	assert(g_set_title_count == 0);
	assert(g_set_app_id_count == 0);

	wm_foreign_toplevel_handle_destroy(&v);
	free_view(&v);
	printf("  PASS: handle_create_null_title_appid\n");
}

static void
test_handle_create_initial_state(void)
{
	reset_globals();
	init_server();
	test_server.foreign_toplevel_manager = &g_manager;

	struct wm_view v = make_view(0, "Vim", "nvim");
	v.maximized = true;
	v.fullscreen = false;
	test_server.focused_view = &v; /* view is focused */

	wm_foreign_toplevel_handle_create(&v);

	/* Should set maximized=true, fullscreen=false, activated=true */
	assert(g_set_maximized_count == 1);
	assert(g_last_maximized == true);
	assert(g_set_fullscreen_count == 1);
	assert(g_last_fullscreen == false);
	assert(g_set_activated_count == 1);
	assert(g_last_activated == true);

	wm_foreign_toplevel_handle_destroy(&v);
	free_view(&v);
	printf("  PASS: handle_create_initial_state\n");
}

static void
test_handle_destroy_basic(void)
{
	reset_globals();
	init_server();
	test_server.foreign_toplevel_manager = &g_manager;

	struct wm_view v = make_view(0, "Firefox", "firefox");
	wm_foreign_toplevel_handle_create(&v);
	assert(v.foreign_toplevel_handle != NULL);

	wm_foreign_toplevel_handle_destroy(&v);

	assert(g_handle_destroy_count == 1);
	assert(v.foreign_toplevel_handle == NULL);

	free_view(&v);
	printf("  PASS: handle_destroy_basic\n");
}

static void
test_handle_destroy_null(void)
{
	reset_globals();
	init_server();

	struct wm_view v = make_view(0, "test", "test");
	v.foreign_toplevel_handle = NULL;

	/* Should be a no-op */
	wm_foreign_toplevel_handle_destroy(&v);
	assert(g_handle_destroy_count == 0);

	free_view(&v);
	printf("  PASS: handle_destroy_null\n");
}

static void
test_update_title(void)
{
	reset_globals();
	init_server();
	test_server.foreign_toplevel_manager = &g_manager;

	struct wm_view v = make_view(0, "Old Title", "app");
	wm_foreign_toplevel_handle_create(&v);
	reset_counters();

	free(v.title);
	v.title = strdup("New Title");
	wm_foreign_toplevel_update_title(&v);

	assert(g_set_title_count == 1);
	assert(strcmp(g_last_title, "New Title") == 0);

	wm_foreign_toplevel_handle_destroy(&v);
	free_view(&v);
	printf("  PASS: update_title\n");
}

static void
test_update_title_null_handle(void)
{
	reset_globals();
	init_server();

	struct wm_view v = make_view(0, "Title", "app");
	v.foreign_toplevel_handle = NULL;

	wm_foreign_toplevel_update_title(&v);
	assert(g_set_title_count == 0);

	free_view(&v);
	printf("  PASS: update_title_null_handle\n");
}

static void
test_update_app_id(void)
{
	reset_globals();
	init_server();
	test_server.foreign_toplevel_manager = &g_manager;

	struct wm_view v = make_view(0, "App", "old.app");
	wm_foreign_toplevel_handle_create(&v);
	reset_counters();

	free(v.app_id);
	v.app_id = strdup("new.app");
	wm_foreign_toplevel_update_app_id(&v);

	assert(g_set_app_id_count == 1);
	assert(strcmp(g_last_app_id, "new.app") == 0);

	wm_foreign_toplevel_handle_destroy(&v);
	free_view(&v);
	printf("  PASS: update_app_id\n");
}

static void
test_set_activated(void)
{
	reset_globals();
	init_server();
	test_server.foreign_toplevel_manager = &g_manager;

	struct wm_view v = make_view(0, "App", "app");
	wm_foreign_toplevel_handle_create(&v);
	reset_counters();

	wm_foreign_toplevel_set_activated(&v, true);
	assert(g_set_activated_count == 1);
	assert(g_last_activated == true);

	wm_foreign_toplevel_set_activated(&v, false);
	assert(g_set_activated_count == 2);
	assert(g_last_activated == false);

	wm_foreign_toplevel_handle_destroy(&v);
	free_view(&v);
	printf("  PASS: set_activated\n");
}

static void
test_set_maximized(void)
{
	reset_globals();
	init_server();
	test_server.foreign_toplevel_manager = &g_manager;

	struct wm_view v = make_view(0, "App", "app");
	wm_foreign_toplevel_handle_create(&v);
	reset_counters();

	wm_foreign_toplevel_set_maximized(&v, true);
	assert(g_set_maximized_count == 1);
	assert(g_last_maximized == true);

	wm_foreign_toplevel_set_maximized(&v, false);
	assert(g_set_maximized_count == 2);
	assert(g_last_maximized == false);

	wm_foreign_toplevel_handle_destroy(&v);
	free_view(&v);
	printf("  PASS: set_maximized\n");
}

static void
test_set_fullscreen(void)
{
	reset_globals();
	init_server();
	test_server.foreign_toplevel_manager = &g_manager;

	struct wm_view v = make_view(0, "App", "app");
	wm_foreign_toplevel_handle_create(&v);
	reset_counters();

	wm_foreign_toplevel_set_fullscreen(&v, true);
	assert(g_set_fullscreen_count == 1);
	assert(g_last_fullscreen == true);

	wm_foreign_toplevel_set_fullscreen(&v, false);
	assert(g_set_fullscreen_count == 2);
	assert(g_last_fullscreen == false);

	wm_foreign_toplevel_handle_destroy(&v);
	free_view(&v);
	printf("  PASS: set_fullscreen\n");
}

static void
test_set_minimized(void)
{
	reset_globals();
	init_server();
	test_server.foreign_toplevel_manager = &g_manager;

	struct wm_view v = make_view(0, "App", "app");
	wm_foreign_toplevel_handle_create(&v);
	reset_counters();

	wm_foreign_toplevel_set_minimized(&v, true);
	assert(g_set_minimized_count == 1);
	assert(g_last_minimized == true);

	wm_foreign_toplevel_set_minimized(&v, false);
	assert(g_set_minimized_count == 2);
	assert(g_last_minimized == false);

	wm_foreign_toplevel_handle_destroy(&v);
	free_view(&v);
	printf("  PASS: set_minimized\n");
}

static void
test_set_state_null_handle(void)
{
	reset_globals();
	init_server();

	struct wm_view v = make_view(0, "App", "app");
	v.foreign_toplevel_handle = NULL;

	/* All should be no-ops */
	wm_foreign_toplevel_set_activated(&v, true);
	wm_foreign_toplevel_set_maximized(&v, true);
	wm_foreign_toplevel_set_fullscreen(&v, true);
	wm_foreign_toplevel_set_minimized(&v, true);

	assert(g_set_activated_count == 0);
	assert(g_set_maximized_count == 0);
	assert(g_set_fullscreen_count == 0);
	assert(g_set_minimized_count == 0);

	free_view(&v);
	printf("  PASS: set_state_null_handle\n");
}

static void
test_request_activate(void)
{
	reset_globals();
	init_server();
	test_server.foreign_toplevel_manager = &g_manager;

	struct wm_view v = make_view(0, "App", "app");
	wm_foreign_toplevel_handle_create(&v);
	reset_counters();

	/* Fire the activate request signal on the handle */
	wl_signal_emit_mutable(
		&v.foreign_toplevel_handle->events.request_activate, NULL);

	assert(g_focus_view_count == 1);
	assert(g_last_focused_view == &v);

	wm_foreign_toplevel_handle_destroy(&v);
	free_view(&v);
	printf("  PASS: request_activate\n");
}

static void
test_request_maximize_on(void)
{
	reset_globals();
	init_server();
	test_server.foreign_toplevel_manager = &g_manager;

	struct wm_view v = make_view(0, "App", "app");
	v.maximized = false;

	/* Add a listener to track the maximize signal emission */
	struct wl_listener max_listener = {
		.notify = maximize_signal_handler
	};
	wl_signal_add(&test_toplevels[0].events.request_maximize,
		&max_listener);

	wm_foreign_toplevel_handle_create(&v);
	reset_counters();

	struct wlr_foreign_toplevel_handle_v1_maximized_event event = {
		.toplevel = v.foreign_toplevel_handle,
		.maximized = true,
	};
	wl_signal_emit_mutable(
		&v.foreign_toplevel_handle->events.request_maximize, &event);

	/* Should emit the xdg maximize signal */
	assert(g_maximize_signal_count == 1);

	wl_list_remove(&max_listener.link);
	wm_foreign_toplevel_handle_destroy(&v);
	free_view(&v);
	printf("  PASS: request_maximize_on\n");
}

static void
test_request_maximize_noop_same_state(void)
{
	reset_globals();
	init_server();
	test_server.foreign_toplevel_manager = &g_manager;

	struct wm_view v = make_view(0, "App", "app");
	v.maximized = true; /* already maximized */

	struct wl_listener max_listener = {
		.notify = maximize_signal_handler
	};
	wl_signal_add(&test_toplevels[0].events.request_maximize,
		&max_listener);

	wm_foreign_toplevel_handle_create(&v);
	reset_counters();

	struct wlr_foreign_toplevel_handle_v1_maximized_event event = {
		.toplevel = v.foreign_toplevel_handle,
		.maximized = true, /* same as current */
	};
	wl_signal_emit_mutable(
		&v.foreign_toplevel_handle->events.request_maximize, &event);

	/* Should be a no-op */
	assert(g_maximize_signal_count == 0);

	wl_list_remove(&max_listener.link);
	wm_foreign_toplevel_handle_destroy(&v);
	free_view(&v);
	printf("  PASS: request_maximize_noop_same_state\n");
}

static void
test_request_minimize(void)
{
	reset_globals();
	init_server();
	test_server.foreign_toplevel_manager = &g_manager;

	struct wm_view v = make_view(0, "App", "app");
	wl_list_insert(&test_server.views, &v.link);
	test_server.focused_view = &v;

	wm_foreign_toplevel_handle_create(&v);
	reset_counters();

	struct wlr_foreign_toplevel_handle_v1_minimized_event event = {
		.toplevel = v.foreign_toplevel_handle,
		.minimized = true,
	};
	wl_signal_emit_mutable(
		&v.foreign_toplevel_handle->events.request_minimize, &event);

	/* View should be hidden */
	assert(g_scene_set_enabled_count >= 1);
	assert(v.scene_tree->node.enabled == 0);
	/* Minimized state should be set on handle */
	assert(g_set_minimized_count == 1);
	assert(g_last_minimized == true);

	wl_list_remove(&v.link);
	wm_foreign_toplevel_handle_destroy(&v);
	free_view(&v);
	printf("  PASS: request_minimize\n");
}

static void
test_request_unminimize(void)
{
	reset_globals();
	init_server();
	test_server.foreign_toplevel_manager = &g_manager;

	struct wm_view v = make_view(0, "App", "app");
	v.scene_tree->node.enabled = 0; /* start minimized */

	wm_foreign_toplevel_handle_create(&v);
	reset_counters();

	struct wlr_foreign_toplevel_handle_v1_minimized_event event = {
		.toplevel = v.foreign_toplevel_handle,
		.minimized = false,
	};
	wl_signal_emit_mutable(
		&v.foreign_toplevel_handle->events.request_minimize, &event);

	/* View should be shown and focused */
	assert(v.scene_tree->node.enabled == 1);
	assert(g_set_minimized_count == 1);
	assert(g_last_minimized == false);
	assert(g_focus_view_count == 1);
	assert(g_last_focused_view == &v);

	wm_foreign_toplevel_handle_destroy(&v);
	free_view(&v);
	printf("  PASS: request_unminimize\n");
}

static void
test_request_fullscreen_on(void)
{
	reset_globals();
	init_server();
	test_server.foreign_toplevel_manager = &g_manager;

	struct wm_view v = make_view(0, "App", "app");
	v.fullscreen = false;

	struct wl_listener fs_listener = {
		.notify = fullscreen_signal_handler
	};
	wl_signal_add(&test_toplevels[0].events.request_fullscreen,
		&fs_listener);

	wm_foreign_toplevel_handle_create(&v);
	reset_counters();

	struct wlr_foreign_toplevel_handle_v1_fullscreen_event event = {
		.toplevel = v.foreign_toplevel_handle,
		.fullscreen = true,
	};
	wl_signal_emit_mutable(
		&v.foreign_toplevel_handle->events.request_fullscreen, &event);

	assert(g_fullscreen_signal_count == 1);

	wl_list_remove(&fs_listener.link);
	wm_foreign_toplevel_handle_destroy(&v);
	free_view(&v);
	printf("  PASS: request_fullscreen_on\n");
}

static void
test_request_fullscreen_noop_same_state(void)
{
	reset_globals();
	init_server();
	test_server.foreign_toplevel_manager = &g_manager;

	struct wm_view v = make_view(0, "App", "app");
	v.fullscreen = true;

	struct wl_listener fs_listener = {
		.notify = fullscreen_signal_handler
	};
	wl_signal_add(&test_toplevels[0].events.request_fullscreen,
		&fs_listener);

	wm_foreign_toplevel_handle_create(&v);
	reset_counters();

	struct wlr_foreign_toplevel_handle_v1_fullscreen_event event = {
		.toplevel = v.foreign_toplevel_handle,
		.fullscreen = true,
	};
	wl_signal_emit_mutable(
		&v.foreign_toplevel_handle->events.request_fullscreen, &event);

	assert(g_fullscreen_signal_count == 0);

	wl_list_remove(&fs_listener.link);
	wm_foreign_toplevel_handle_destroy(&v);
	free_view(&v);
	printf("  PASS: request_fullscreen_noop_same_state\n");
}

static void
test_request_close(void)
{
	reset_globals();
	init_server();
	test_server.foreign_toplevel_manager = &g_manager;

	struct wm_view v = make_view(0, "App", "app");
	wm_foreign_toplevel_handle_create(&v);
	reset_counters();

	wl_signal_emit_mutable(
		&v.foreign_toplevel_handle->events.request_close, NULL);

	assert(g_send_close_count == 1);

	wm_foreign_toplevel_handle_destroy(&v);
	free_view(&v);
	printf("  PASS: request_close\n");
}

static void
test_multiple_toplevels(void)
{
	reset_globals();
	init_server();
	test_server.foreign_toplevel_manager = &g_manager;

	struct wm_view v1 = make_view(0, "Firefox", "firefox");
	struct wm_view v2 = make_view(1, "Terminal", "alacritty");
	struct wm_view v3 = make_view(2, "Editor", "code");

	wm_foreign_toplevel_handle_create(&v1);
	wm_foreign_toplevel_handle_create(&v2);
	wm_foreign_toplevel_handle_create(&v3);

	assert(g_handle_create_count == 3);
	assert(v1.foreign_toplevel_handle != NULL);
	assert(v2.foreign_toplevel_handle != NULL);
	assert(v3.foreign_toplevel_handle != NULL);
	/* Each handle should be unique */
	assert(v1.foreign_toplevel_handle != v2.foreign_toplevel_handle);
	assert(v2.foreign_toplevel_handle != v3.foreign_toplevel_handle);

	/* Destroy middle one */
	wm_foreign_toplevel_handle_destroy(&v2);
	assert(v2.foreign_toplevel_handle == NULL);
	assert(v1.foreign_toplevel_handle != NULL);
	assert(v3.foreign_toplevel_handle != NULL);

	wm_foreign_toplevel_handle_destroy(&v1);
	wm_foreign_toplevel_handle_destroy(&v3);
	free_view(&v1);
	free_view(&v2);
	free_view(&v3);
	printf("  PASS: multiple_toplevels\n");
}

static void
test_minimize_focuses_next(void)
{
	reset_globals();
	init_server();
	test_server.foreign_toplevel_manager = &g_manager;

	struct wm_view v1 = make_view(0, "App1", "app1");
	struct wm_view v2 = make_view(1, "App2", "app2");
	wl_list_insert(&test_server.views, &v2.link);
	wl_list_insert(&test_server.views, &v1.link);
	test_server.focused_view = &v1;

	wm_foreign_toplevel_handle_create(&v1);
	wm_foreign_toplevel_handle_create(&v2);
	reset_counters();

	/* Minimize v1 (the focused view) */
	struct wlr_foreign_toplevel_handle_v1_minimized_event event = {
		.toplevel = v1.foreign_toplevel_handle,
		.minimized = true,
	};
	wl_signal_emit_mutable(
		&v1.foreign_toplevel_handle->events.request_minimize, &event);

	/* Should focus next view (v2) */
	assert(g_focus_view_count == 1);
	assert(g_last_focused_view == &v2);

	wl_list_remove(&v1.link);
	wl_list_remove(&v2.link);
	wm_foreign_toplevel_handle_destroy(&v1);
	wm_foreign_toplevel_handle_destroy(&v2);
	free_view(&v1);
	free_view(&v2);
	printf("  PASS: minimize_focuses_next\n");
}

static void
test_minimize_clears_focus_no_next(void)
{
	reset_globals();
	init_server();
	test_server.foreign_toplevel_manager = &g_manager;

	struct wm_view v = make_view(0, "App", "app");
	wl_list_insert(&test_server.views, &v.link);
	test_server.focused_view = &v;

	wm_foreign_toplevel_handle_create(&v);
	reset_counters();

	/* Minimize the only view */
	struct wlr_foreign_toplevel_handle_v1_minimized_event event = {
		.toplevel = v.foreign_toplevel_handle,
		.minimized = true,
	};
	wl_signal_emit_mutable(
		&v.foreign_toplevel_handle->events.request_minimize, &event);

	/* No next view to focus, should clear keyboard focus */
	assert(g_focus_view_count == 0);
	assert(g_seat_clear_focus_count == 1);
	assert(test_server.focused_view == NULL);

	wl_list_remove(&v.link);
	wm_foreign_toplevel_handle_destroy(&v);
	free_view(&v);
	printf("  PASS: minimize_clears_focus_no_next\n");
}

static void
test_update_title_null_title(void)
{
	reset_globals();
	init_server();
	test_server.foreign_toplevel_manager = &g_manager;

	struct wm_view v = make_view(0, "Title", "app");
	wm_foreign_toplevel_handle_create(&v);
	reset_counters();

	free(v.title);
	v.title = NULL;
	wm_foreign_toplevel_update_title(&v);

	assert(g_set_title_count == 1);
	assert(strcmp(g_last_title, "") == 0);

	wm_foreign_toplevel_handle_destroy(&v);
	free_view(&v);
	printf("  PASS: update_title_null_title\n");
}

static void
test_full_lifecycle(void)
{
	reset_globals();
	init_server();
	test_server.foreign_toplevel_manager = &g_manager;

	/* Create a view and its toplevel handle */
	struct wm_view v = make_view(0, "Firefox", "firefox");
	wl_list_insert(&test_server.views, &v.link);
	test_server.focused_view = &v;

	wm_foreign_toplevel_handle_create(&v);
	assert(v.foreign_toplevel_handle != NULL);

	/* Update title */
	free(v.title);
	v.title = strdup("Firefox - Wikipedia");
	wm_foreign_toplevel_update_title(&v);

	/* Set activated */
	wm_foreign_toplevel_set_activated(&v, true);

	/* Set maximized */
	wm_foreign_toplevel_set_maximized(&v, true);

	/* Set fullscreen */
	wm_foreign_toplevel_set_fullscreen(&v, true);

	/* Set minimized */
	wm_foreign_toplevel_set_minimized(&v, true);

	/* Destroy handle (simulating window close) */
	wm_foreign_toplevel_handle_destroy(&v);
	assert(v.foreign_toplevel_handle == NULL);

	wl_list_remove(&v.link);
	free_view(&v);
	printf("  PASS: full_lifecycle\n");
}

int
main(void)
{
	printf("test_foreign_toplevel:\n");

	test_init_creates_manager();
	test_handle_create_basic();
	test_handle_create_no_manager();
	test_handle_create_null_title_appid();
	test_handle_create_initial_state();
	test_handle_destroy_basic();
	test_handle_destroy_null();
	test_update_title();
	test_update_title_null_handle();
	test_update_app_id();
	test_set_activated();
	test_set_maximized();
	test_set_fullscreen();
	test_set_minimized();
	test_set_state_null_handle();
	test_request_activate();
	test_request_maximize_on();
	test_request_maximize_noop_same_state();
	test_request_minimize();
	test_request_unminimize();
	test_request_fullscreen_on();
	test_request_fullscreen_noop_same_state();
	test_request_close();
	test_multiple_toplevels();
	test_minimize_focuses_next();
	test_minimize_clears_focus_no_next();
	test_update_title_null_title();
	test_full_lifecycle();

	printf("All foreign_toplevel tests passed.\n");
	return 0;
}
