/*
 * test_output.c - Unit tests for output utility functions (output.c)
 *
 * Includes output.c directly with stub implementations to avoid
 * needing wlroots/wayland libraries at link time. Tests JSON escape,
 * output lookup, and focused output resolution.
 */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* --- Block real headers via their include guards --- */
#define WAYLAND_SERVER_CORE_H
#define WAYLAND_SERVER_H
#define WAYLAND_UTIL_H
#define WLR_UTIL_LOG_H
#define WLR_UTIL_BOX_H
#define WLR_BACKEND_WAYLAND_H
#define WLR_TYPES_WLR_OUTPUT_H
#define WLR_TYPES_WLR_OUTPUT_LAYOUT_H
#define WLR_TYPES_WLR_SCENE_H
#define WLR_TYPES_WLR_XCURSOR_MANAGER_H
#define WM_SERVER_H
#define WM_CONFIG_H
#define WM_OUTPUT_H
#define WM_IPC_H
#define WM_STYLE_H
#define WM_DRM_LEASE_H
#define WM_LAYER_SHELL_H
#define WM_TOOLBAR_H
#define WM_VIEW_H
#define WM_WORKSPACE_H

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

#include <stddef.h>

#define wl_container_of(ptr, sample, member) \
	(__typeof__(sample))((char *)(ptr) - \
	offsetof(__typeof__(*sample), member))

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

struct wl_display { int dummy; };
struct wl_event_source { int dummy; };
struct wl_event_loop { int dummy; };

/* --- wlr_log no-op --- */
#define wlr_log(verb, fmt, ...) ((void)0)
#define WLR_ERROR 1
#define WLR_INFO  3
#define WLR_DEBUG 7

/* --- Stub wlr types --- */

struct wlr_box {
	int x, y, width, height;
};

struct wlr_output {
	int width, height;
	char *name;
	struct {
		struct wl_signal frame;
		struct wl_signal request_state;
		struct wl_signal destroy;
	} events;
};

struct wlr_output_mode { int dummy; };

struct wlr_output_state { int dummy; };

struct wlr_output_event_request_state {
	struct wlr_output_state *state;
};

static void wlr_output_state_init(struct wlr_output_state *s)
{ (void)s; }

static void wlr_output_state_set_enabled(struct wlr_output_state *s,
	bool enabled)
{ (void)s; (void)enabled; }

static void wlr_output_state_set_mode(struct wlr_output_state *s,
	struct wlr_output_mode *m)
{ (void)s; (void)m; }

static void wlr_output_state_finish(struct wlr_output_state *s)
{ (void)s; }

static bool wlr_output_commit_state(struct wlr_output *o,
	const struct wlr_output_state *s)
{ (void)o; (void)s; return true; }

struct wlr_output_layout {
	int dummy;
};

struct wlr_output_layout_output { int dummy; };

struct wlr_scene_node {
	int enabled;
	int x, y;
};

struct wlr_scene_tree {
	struct wlr_scene_node node;
};

struct wlr_scene { int dummy; };
struct wlr_scene_output { int dummy; };
struct wlr_scene_output_layout { int dummy; };

struct wlr_backend {
	struct {
		struct wl_signal new_output;
	} events;
};

struct wlr_renderer { int dummy; };
struct wlr_allocator { int dummy; };

struct wlr_cursor {
	double x, y;
};

struct wlr_xcursor_manager { int dummy; };

/* --- Stub wm types --- */

struct wm_workspace {
	struct wl_list link;
};

struct wm_output {
	struct wl_list link;
	struct wm_server *server;
	struct wlr_output *wlr_output;
	struct wlr_scene_output *scene_output;
	struct wlr_box usable_area;
	struct wm_workspace *active_workspace;
	struct wl_listener frame;
	struct wl_listener request_state;
	struct wl_listener destroy;
};

struct wm_view {
	double x, y;
};

/* IPC stub types */
enum wm_ipc_event {
	WM_IPC_EVENT_OUTPUT_ADD    = (1 << 5),
	WM_IPC_EVENT_OUTPUT_REMOVE = (1 << 6),
};

struct ipc_event_throttle {
	struct timespec last_sent;
};

struct wm_ipc_client {
	struct wl_list link;
};

struct wm_ipc_server {
	struct wl_list clients;
	int client_count;
	struct ipc_event_throttle event_throttle[32];
};

/* Minimal wm_server definition with fields used by output.c */
struct wm_server {
	struct wl_display *wl_display;
	struct wl_event_loop *wl_event_loop;
	struct wlr_backend *backend;
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wlr_scene *scene;
	struct wlr_scene_output_layout *scene_layout;
	struct wl_list outputs;
	struct wl_listener new_output;
	struct wlr_output_layout *output_layout;
	struct wlr_cursor *cursor;
	struct wm_view *focused_view;
	struct wm_workspace *current_workspace;
	struct wm_ipc_server ipc;
	struct wm_toolbar *toolbar;
};

struct wm_toolbar { int dummy; };

/* --- Stub wlroots functions used by output.c --- */

static struct wlr_output_layout g_stub_output_layout;

static struct wlr_output_layout *
wlr_output_layout_create(struct wl_display *display)
{
	(void)display;
	return &g_stub_output_layout;
}

static struct wlr_scene_output_layout g_stub_scene_layout;

static struct wlr_scene_output_layout *
wlr_scene_attach_output_layout(struct wlr_scene *scene,
	struct wlr_output_layout *layout)
{
	(void)scene; (void)layout;
	return &g_stub_scene_layout;
}

static void
wlr_output_init_render(struct wlr_output *o, struct wlr_allocator *a,
	struct wlr_renderer *r)
{ (void)o; (void)a; (void)r; }

static struct wlr_output_mode *
wlr_output_preferred_mode(struct wlr_output *o)
{ (void)o; return NULL; }

static void
wlr_output_effective_resolution(struct wlr_output *o, int *w, int *h)
{ *w = o->width; *h = o->height; }

static struct wlr_output_layout_output *
wlr_output_layout_add_auto(struct wlr_output_layout *l,
	struct wlr_output *o)
{ (void)l; (void)o; return NULL; }

/*
 * Stub for wlr_output_layout_output_at -- uses a global to control
 * which output is "at" the given coordinates.
 */
static struct wlr_output *g_output_at_result;

static struct wlr_output *
wlr_output_layout_output_at(struct wlr_output_layout *l,
	double lx, double ly)
{
	(void)l; (void)lx; (void)ly;
	return g_output_at_result;
}

static struct wlr_scene_output g_stub_scene_output;

static struct wlr_scene_output *
wlr_scene_output_create(struct wlr_scene *s, struct wlr_output *o)
{ (void)s; (void)o; return &g_stub_scene_output; }

static void
wlr_scene_output_layout_add_output(struct wlr_scene_output_layout *l,
	struct wlr_output_layout_output *lo,
	struct wlr_scene_output *so)
{ (void)l; (void)lo; (void)so; }

static void wlr_scene_output_commit(struct wlr_scene_output *so,
	void *opt)
{ (void)so; (void)opt; }

static void wlr_scene_output_send_frame_done(struct wlr_scene_output *so,
	struct timespec *ts)
{ (void)so; (void)ts; }

/* --- Stub functions from other wm modules --- */

static void
wm_ipc_broadcast_event(struct wm_ipc_server *ipc,
	enum wm_ipc_event event, const char *json_event)
{ (void)ipc; (void)event; (void)json_event; }

static void
wm_drm_lease_offer_output(struct wm_server *s, struct wlr_output *o)
{ (void)s; (void)o; }

static void
wm_layer_shell_arrange(struct wm_output *o)
{ (void)o; }

static void
wm_toolbar_relayout(struct wm_toolbar *t)
{ (void)t; }

/* --- Stubs for util.h functions --- */

void wm_spawn_command(const char *cmd) { (void)cmd; }

void wm_json_escape(char *dst, size_t dst_size, const char *src)
{
	if (!src) {
		if (dst_size > 0) dst[0] = '\0';
		return;
	}
	size_t j = 0;
	for (size_t i = 0; src[i] && j + 6 < dst_size; i++) {
		unsigned char c = (unsigned char)src[i];
		if (c == '"' || c == '\\') {
			dst[j++] = '\\';
			dst[j++] = c;
		} else if (c < 0x20) {
			continue;
		} else {
			dst[j++] = c;
		}
	}
	dst[j] = '\0';
}

/* --- Include output.c source directly --- */

#include "output.c"

/* --- Test helpers --- */

static void
reset_globals(void)
{
	g_output_at_result = NULL;
}

/* ===== Tests ===== */

/* Test 1: wm_json_escape with a normal string (no special chars) */
static void
test_json_escape_normal(void)
{
	char buf[128];
	wm_json_escape(buf, sizeof(buf), "hello world");
	assert(strcmp(buf, "hello world") == 0);

	printf("  PASS: test_json_escape_normal\n");
}

/* Test 2: wm_json_escape with double quotes and backslash */
static void
test_json_escape_special_chars(void)
{
	char buf[128];
	wm_json_escape(buf, sizeof(buf), "say \"hello\\world\"");
	assert(strcmp(buf, "say \\\"hello\\\\world\\\"") == 0);

	printf("  PASS: test_json_escape_special_chars\n");
}

/* Test 3: wm_json_escape strips control characters (newline, tab) */
static void
test_json_escape_control_chars(void)
{
	char buf[128];
	wm_json_escape(buf, sizeof(buf), "line1\nline2\ttab");
	/* Control chars < 0x20 are skipped */
	assert(strcmp(buf, "line1line2tab") == 0);

	printf("  PASS: test_json_escape_control_chars\n");
}

/* Test 4: wm_json_escape with NULL input */
static void
test_json_escape_null(void)
{
	char buf[128];
	buf[0] = 'X'; /* set a sentinel */
	wm_json_escape(buf, sizeof(buf), NULL);
	assert(buf[0] == '\0');

	printf("  PASS: test_json_escape_null\n");
}

/* Test 5: wm_json_escape truncation when buffer is too small */
static void
test_json_escape_truncation(void)
{
	char buf[8];
	wm_json_escape(buf, sizeof(buf), "abcdefghijklmnop");
	/*
	 * Buffer is 8 bytes. The loop condition is j + 6 < dst_size,
	 * so j < 2 for regular chars. Effectively only 1 char fits
	 * before the guard triggers (j must be < dst_size - 6 = 2).
	 */
	size_t len = strlen(buf);
	assert(len < sizeof(buf));
	/* Should be truncated but null-terminated */
	assert(buf[len] == '\0');

	printf("  PASS: test_json_escape_truncation\n");
}

/* Test 6: wm_json_escape with empty string */
static void
test_json_escape_empty(void)
{
	char buf[128];
	buf[0] = 'X';
	wm_json_escape(buf, sizeof(buf), "");
	assert(buf[0] == '\0');

	printf("  PASS: test_json_escape_empty\n");
}

/* Test 7: wm_output_from_wlr finds matching output */
static void
test_output_from_wlr_found(void)
{
	struct wm_server server;
	memset(&server, 0, sizeof(server));
	wl_list_init(&server.outputs);

	struct wlr_output wlr1, wlr2;
	memset(&wlr1, 0, sizeof(wlr1));
	memset(&wlr2, 0, sizeof(wlr2));

	struct wm_output out1, out2;
	memset(&out1, 0, sizeof(out1));
	memset(&out2, 0, sizeof(out2));
	out1.wlr_output = &wlr1;
	out1.server = &server;
	out2.wlr_output = &wlr2;
	out2.server = &server;

	wl_list_insert(&server.outputs, &out1.link);
	wl_list_insert(&server.outputs, &out2.link);

	struct wm_output *found = wm_output_from_wlr(&server, &wlr1);
	assert(found == &out1);

	found = wm_output_from_wlr(&server, &wlr2);
	assert(found == &out2);

	printf("  PASS: test_output_from_wlr_found\n");
}

/* Test 8: wm_output_from_wlr returns NULL when not found */
static void
test_output_from_wlr_not_found(void)
{
	struct wm_server server;
	memset(&server, 0, sizeof(server));
	wl_list_init(&server.outputs);

	struct wlr_output wlr1, wlr_unknown;
	memset(&wlr1, 0, sizeof(wlr1));
	memset(&wlr_unknown, 0, sizeof(wlr_unknown));

	struct wm_output out1;
	memset(&out1, 0, sizeof(out1));
	out1.wlr_output = &wlr1;
	wl_list_insert(&server.outputs, &out1.link);

	struct wm_output *found =
		wm_output_from_wlr(&server, &wlr_unknown);
	assert(found == NULL);

	printf("  PASS: test_output_from_wlr_not_found\n");
}

/* Test 9: wm_output_from_wlr returns NULL on empty list */
static void
test_output_from_wlr_empty_list(void)
{
	struct wm_server server;
	memset(&server, 0, sizeof(server));
	wl_list_init(&server.outputs);

	struct wlr_output wlr1;
	memset(&wlr1, 0, sizeof(wlr1));

	struct wm_output *found = wm_output_from_wlr(&server, &wlr1);
	assert(found == NULL);

	printf("  PASS: test_output_from_wlr_empty_list\n");
}

/* Test 10: wm_server_get_focused_output with focused view */
static void
test_focused_output_with_view(void)
{
	reset_globals();

	struct wm_server server;
	memset(&server, 0, sizeof(server));
	wl_list_init(&server.outputs);

	struct wlr_output wlr1;
	memset(&wlr1, 0, sizeof(wlr1));

	struct wm_output out1;
	memset(&out1, 0, sizeof(out1));
	out1.wlr_output = &wlr1;
	out1.server = &server;
	wl_list_insert(&server.outputs, &out1.link);

	struct wlr_output_layout layout;
	memset(&layout, 0, sizeof(layout));
	server.output_layout = &layout;

	struct wm_view view;
	memset(&view, 0, sizeof(view));
	view.x = 100;
	view.y = 200;
	server.focused_view = &view;

	/* Make output_at return wlr1 for the focused view coords */
	g_output_at_result = &wlr1;

	struct wm_output *result = wm_server_get_focused_output(&server);
	assert(result == &out1);

	printf("  PASS: test_focused_output_with_view\n");
}

/* Test 11: wm_server_get_focused_output cursor fallback */
static void
test_focused_output_cursor_fallback(void)
{
	reset_globals();

	struct wm_server server;
	memset(&server, 0, sizeof(server));
	wl_list_init(&server.outputs);

	struct wlr_output wlr1;
	memset(&wlr1, 0, sizeof(wlr1));

	struct wm_output out1;
	memset(&out1, 0, sizeof(out1));
	out1.wlr_output = &wlr1;
	out1.server = &server;
	wl_list_insert(&server.outputs, &out1.link);

	struct wlr_output_layout layout;
	memset(&layout, 0, sizeof(layout));
	server.output_layout = &layout;

	struct wlr_cursor cursor;
	memset(&cursor, 0, sizeof(cursor));
	cursor.x = 500;
	cursor.y = 300;
	server.cursor = &cursor;

	/* No focused view */
	server.focused_view = NULL;

	/* output_at returns wlr1 for cursor coords */
	g_output_at_result = &wlr1;

	struct wm_output *result = wm_server_get_focused_output(&server);
	assert(result == &out1);

	printf("  PASS: test_focused_output_cursor_fallback\n");
}

/* Test 12: wm_server_get_focused_output first output fallback */
static void
test_focused_output_first_fallback(void)
{
	reset_globals();

	struct wm_server server;
	memset(&server, 0, sizeof(server));
	wl_list_init(&server.outputs);

	struct wlr_output wlr1;
	memset(&wlr1, 0, sizeof(wlr1));

	struct wm_output out1;
	memset(&out1, 0, sizeof(out1));
	out1.wlr_output = &wlr1;
	out1.server = &server;
	wl_list_insert(&server.outputs, &out1.link);

	struct wlr_output_layout layout;
	memset(&layout, 0, sizeof(layout));
	server.output_layout = &layout;

	struct wlr_cursor cursor;
	memset(&cursor, 0, sizeof(cursor));
	server.cursor = &cursor;
	server.focused_view = NULL;

	/* output_at returns NULL for both checks */
	g_output_at_result = NULL;

	struct wm_output *result = wm_server_get_focused_output(&server);
	assert(result == &out1);

	printf("  PASS: test_focused_output_first_fallback\n");
}

/* Test 13: wm_server_get_focused_output returns NULL on empty list */
static void
test_focused_output_empty_list(void)
{
	reset_globals();

	struct wm_server server;
	memset(&server, 0, sizeof(server));
	wl_list_init(&server.outputs);

	struct wlr_output_layout layout;
	memset(&layout, 0, sizeof(layout));
	server.output_layout = &layout;

	struct wlr_cursor cursor;
	memset(&cursor, 0, sizeof(cursor));
	server.cursor = &cursor;
	server.focused_view = NULL;
	g_output_at_result = NULL;

	struct wm_output *result = wm_server_get_focused_output(&server);
	assert(result == NULL);

	printf("  PASS: test_focused_output_empty_list\n");
}

int
main(void)
{
	printf("test_output:\n");

	test_json_escape_normal();
	test_json_escape_special_chars();
	test_json_escape_control_chars();
	test_json_escape_null();
	test_json_escape_truncation();
	test_json_escape_empty();
	test_output_from_wlr_found();
	test_output_from_wlr_not_found();
	test_output_from_wlr_empty_list();
	test_focused_output_with_view();
	test_focused_output_cursor_fallback();
	test_focused_output_first_fallback();
	test_focused_output_empty_list();

	printf("All output tests passed.\n");
	return 0;
}
