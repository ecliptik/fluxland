/*
 * test_output_mgmt.c - Unit tests for output management (output_management.c)
 *
 * Includes output_management.c directly with stub implementations to avoid
 * needing wlroots/wayland libraries at link time. Tests configuration
 * building, apply/test handling, update, and finish paths.
 */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- Block real headers via their include guards --- */
#define WAYLAND_SERVER_CORE_H
#define WAYLAND_SERVER_H
#define WAYLAND_UTIL_H
#define WLR_UTIL_LOG_H
#define WLR_UTIL_BOX_H
#define WLR_BACKEND_H
#define WLR_TYPES_WLR_OUTPUT_H
#define WLR_TYPES_WLR_OUTPUT_LAYOUT_H
#define WLR_TYPES_WLR_OUTPUT_MANAGEMENT_V1_H
#define WM_SERVER_H
#define WM_OUTPUT_H
#define WM_OUTPUT_MANAGEMENT_H
#define WM_LAYER_SHELL_H

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
};

struct wlr_output_state { int dummy; };

static void wlr_output_state_finish(struct wlr_output_state *s)
{ (void)s; }

static void
wlr_output_effective_resolution(struct wlr_output *o, int *w, int *h)
{ *w = o->width; *h = o->height; }

struct wlr_output_layout {
	struct {
		struct wl_signal change;
	} events;
};

struct wlr_output_layout_output {
	int x, y;
};

struct wlr_backend {
	int dummy;
};

struct wlr_backend_output_state {
	struct wlr_output_state base;
};

/* --- Output configuration v1 stubs --- */

struct wlr_output_configuration_head_v1_state {
	struct wlr_output *output;
	bool enabled;
	int x, y;
};

struct wlr_output_configuration_head_v1 {
	struct wl_list link; /* wlr_output_configuration_v1.heads */
	struct wlr_output_configuration_head_v1_state state;
};

struct wlr_output_configuration_v1 {
	struct wl_list heads;
};

struct wlr_output_manager_v1 {
	struct {
		struct wl_signal apply;
		struct wl_signal test;
		struct wl_signal destroy;
	} events;
};

/* --- Tracking counters for stub calls --- */

static int g_config_create_count;
static int g_config_destroy_count;
static int g_config_send_succeeded_count;
static int g_config_send_failed_count;
static int g_head_create_count;
static int g_backend_test_count;
static int g_backend_commit_count;
static int g_manager_set_config_count;
static int g_layout_add_count;
static int g_layer_shell_arrange_count;

/* Controls for stub behavior */
static bool g_config_create_fail;
static bool g_head_create_fail;
static bool g_backend_test_result;
static bool g_backend_commit_result;
static struct wlr_backend_output_state *g_build_state_result;
static size_t g_build_state_len;

/* --- Stub wlr output management functions --- */

static struct wlr_output_configuration_v1 g_stub_configs[4];
static int g_stub_config_idx;

static struct wlr_output_configuration_v1 *
wlr_output_configuration_v1_create(void)
{
	g_config_create_count++;
	if (g_config_create_fail)
		return NULL;
	int idx = g_stub_config_idx % 4;
	struct wlr_output_configuration_v1 *config = &g_stub_configs[idx];
	memset(config, 0, sizeof(*config));
	wl_list_init(&config->heads);
	g_stub_config_idx++;
	return config;
}

static void
wlr_output_configuration_v1_destroy(struct wlr_output_configuration_v1 *config)
{
	(void)config;
	g_config_destroy_count++;
}

static void
wlr_output_configuration_v1_send_succeeded(
	struct wlr_output_configuration_v1 *config)
{
	(void)config;
	g_config_send_succeeded_count++;
}

static void
wlr_output_configuration_v1_send_failed(
	struct wlr_output_configuration_v1 *config)
{
	(void)config;
	g_config_send_failed_count++;
}

static struct wlr_output_configuration_head_v1
	g_stub_heads[4];
static int g_stub_head_idx;

static struct wlr_output_configuration_head_v1 *
wlr_output_configuration_head_v1_create(
	struct wlr_output_configuration_v1 *config,
	struct wlr_output *output)
{
	g_head_create_count++;
	if (g_head_create_fail)
		return NULL;
	int idx = g_stub_head_idx % 4;
	struct wlr_output_configuration_head_v1 *head = &g_stub_heads[idx];
	memset(head, 0, sizeof(*head));
	head->state.output = output;
	wl_list_insert(config->heads.prev, &head->link);
	g_stub_head_idx++;
	return head;
}

static struct wlr_output_layout_output *
wlr_output_layout_get(struct wlr_output_layout *layout,
	struct wlr_output *output)
{
	(void)layout;
	(void)output;
	/* Return NULL: no layout position override */
	return NULL;
}

static void
wlr_output_layout_add(struct wlr_output_layout *layout,
	struct wlr_output *output, int x, int y)
{
	(void)layout; (void)output; (void)x; (void)y;
	g_layout_add_count++;
}

static struct wlr_backend_output_state *
wlr_output_configuration_v1_build_state(
	struct wlr_output_configuration_v1 *config, size_t *len)
{
	(void)config;
	*len = g_build_state_len;
	if (!g_build_state_result)
		return NULL;
	/* Return heap-allocated copy since caller calls free() */
	size_t sz = g_build_state_len * sizeof(struct wlr_backend_output_state);
	struct wlr_backend_output_state *ret = malloc(sz);
	if (ret)
		memcpy(ret, g_build_state_result, sz);
	return ret;
}

static bool
wlr_backend_test(struct wlr_backend *backend,
	struct wlr_backend_output_state *states, size_t len)
{
	(void)backend; (void)states; (void)len;
	g_backend_test_count++;
	return g_backend_test_result;
}

static bool
wlr_backend_commit(struct wlr_backend *backend,
	struct wlr_backend_output_state *states, size_t len)
{
	(void)backend; (void)states; (void)len;
	g_backend_commit_count++;
	return g_backend_commit_result;
}

static struct wlr_output_manager_v1 *
wlr_output_manager_v1_create(struct wl_display *display)
{
	(void)display;
	return NULL; /* Only used in init, not tested here */
}

static void
wlr_output_manager_v1_set_configuration(
	struct wlr_output_manager_v1 *manager,
	struct wlr_output_configuration_v1 *config)
{
	(void)manager; (void)config;
	g_manager_set_config_count++;
}

/* --- Stub wm types --- */

struct wm_output {
	struct wl_list link;
	struct wm_server *server;
	struct wlr_output *wlr_output;
	struct wlr_box usable_area;
};

struct wm_output_management {
	struct wlr_output_manager_v1 *manager;
	struct wm_server *server;
	struct wl_listener apply;
	struct wl_listener test;
	struct wl_listener destroy;
	struct wl_listener layout_change;
};

struct wm_server {
	struct wl_display *wl_display;
	struct wl_list outputs;
	struct wlr_output_layout *output_layout;
	struct wlr_backend *backend;
	struct wm_output_management output_mgmt;
};

/* --- Stub layer_shell --- */

static void
wm_layer_shell_arrange(struct wm_output *output)
{
	(void)output;
	g_layer_shell_arrange_count++;
}

/* --- Forward declarations for output_management.h functions --- */
void wm_output_management_init(struct wm_server *server);
void wm_output_management_finish(struct wm_server *server);
void wm_output_management_update(struct wm_server *server);

/* --- Include output_management.c source directly --- */

#include "output_management.c"

/* --- Test helpers --- */

static void
reset_globals(void)
{
	g_config_create_count = 0;
	g_config_destroy_count = 0;
	g_config_send_succeeded_count = 0;
	g_config_send_failed_count = 0;
	g_head_create_count = 0;
	g_backend_test_count = 0;
	g_backend_commit_count = 0;
	g_manager_set_config_count = 0;
	g_layout_add_count = 0;
	g_layer_shell_arrange_count = 0;
	g_config_create_fail = false;
	g_head_create_fail = false;
	g_backend_test_result = true;
	g_backend_commit_result = true;
	g_build_state_result = NULL;
	g_build_state_len = 0;
	g_stub_config_idx = 0;
	g_stub_head_idx = 0;
	memset(g_stub_configs, 0, sizeof(g_stub_configs));
	memset(g_stub_heads, 0, sizeof(g_stub_heads));
}

/* ===== Tests ===== */

/* Test 1: build_current_configuration with empty output list */
static void
test_build_config_empty_outputs(void)
{
	reset_globals();

	struct wm_server server;
	memset(&server, 0, sizeof(server));
	wl_list_init(&server.outputs);

	struct wlr_output_configuration_v1 *config =
		build_current_configuration(&server);
	assert(config != NULL);
	assert(g_config_create_count == 1);
	assert(g_head_create_count == 0);
	/* Heads list should be empty */
	assert(wl_list_empty(&config->heads));

	printf("  PASS: test_build_config_empty_outputs\n");
}

/* Test 2: build_current_configuration with a single output */
static void
test_build_config_single_output(void)
{
	reset_globals();

	struct wm_server server;
	memset(&server, 0, sizeof(server));
	wl_list_init(&server.outputs);

	struct wlr_output wlr1;
	memset(&wlr1, 0, sizeof(wlr1));
	wlr1.width = 1920;
	wlr1.height = 1080;

	struct wm_output out1;
	memset(&out1, 0, sizeof(out1));
	out1.wlr_output = &wlr1;
	out1.server = &server;
	wl_list_insert(&server.outputs, &out1.link);

	struct wlr_output_layout layout;
	memset(&layout, 0, sizeof(layout));
	server.output_layout = &layout;

	struct wlr_output_configuration_v1 *config =
		build_current_configuration(&server);
	assert(config != NULL);
	assert(g_config_create_count == 1);
	assert(g_head_create_count == 1);
	assert(!wl_list_empty(&config->heads));

	printf("  PASS: test_build_config_single_output\n");
}

/* Test 3: build_current_configuration returns NULL if create fails */
static void
test_build_config_create_fails(void)
{
	reset_globals();
	g_config_create_fail = true;

	struct wm_server server;
	memset(&server, 0, sizeof(server));
	wl_list_init(&server.outputs);

	struct wlr_output_configuration_v1 *config =
		build_current_configuration(&server);
	assert(config == NULL);

	printf("  PASS: test_build_config_create_fails\n");
}

/* Test 4: build_current_configuration returns NULL if head create fails */
static void
test_build_config_head_create_fails(void)
{
	reset_globals();
	g_head_create_fail = true;

	struct wm_server server;
	memset(&server, 0, sizeof(server));
	wl_list_init(&server.outputs);

	struct wlr_output wlr1;
	memset(&wlr1, 0, sizeof(wlr1));

	struct wm_output out1;
	memset(&out1, 0, sizeof(out1));
	out1.wlr_output = &wlr1;
	wl_list_insert(&server.outputs, &out1.link);

	struct wlr_output_layout layout;
	memset(&layout, 0, sizeof(layout));
	server.output_layout = &layout;

	struct wlr_output_configuration_v1 *config =
		build_current_configuration(&server);
	assert(config == NULL);
	/* Should have destroyed the partially-built config */
	assert(g_config_destroy_count == 1);

	printf("  PASS: test_build_config_head_create_fails\n");
}

/* Test 5: handle_output_configuration fails when build_state returns NULL */
static void
test_handle_config_build_state_fails(void)
{
	reset_globals();

	struct wm_server server;
	memset(&server, 0, sizeof(server));
	wl_list_init(&server.outputs);

	struct wlr_backend backend;
	memset(&backend, 0, sizeof(backend));
	server.backend = &backend;

	struct wlr_output_configuration_v1 *config =
		wlr_output_configuration_v1_create();
	assert(config != NULL);

	/* build_state returns NULL */
	g_build_state_result = NULL;
	g_build_state_len = 0;

	handle_output_configuration(&server, config, false);

	assert(g_config_send_failed_count == 1);
	assert(g_config_destroy_count == 1);

	printf("  PASS: test_handle_config_build_state_fails\n");
}

/* Test 6: handle_output_configuration test-only succeeds */
static void
test_handle_config_test_only_success(void)
{
	reset_globals();

	struct wm_server server;
	memset(&server, 0, sizeof(server));
	wl_list_init(&server.outputs);

	struct wlr_backend backend;
	memset(&backend, 0, sizeof(backend));
	server.backend = &backend;

	struct wlr_output_configuration_v1 *config =
		wlr_output_configuration_v1_create();
	assert(config != NULL);

	/* Provide a valid state for build_state */
	struct wlr_backend_output_state states[1];
	memset(states, 0, sizeof(states));
	g_build_state_result = states;
	g_build_state_len = 1;
	g_backend_test_result = true;

	handle_output_configuration(&server, config, true);

	assert(g_backend_test_count == 1);
	assert(g_backend_commit_count == 0); /* test only, no commit */
	assert(g_config_send_succeeded_count == 1);
	assert(g_config_destroy_count == 1);

	printf("  PASS: test_handle_config_test_only_success\n");
}

/* Test 7: handle_output_configuration test fails */
static void
test_handle_config_test_fails(void)
{
	reset_globals();

	struct wm_server server;
	memset(&server, 0, sizeof(server));
	wl_list_init(&server.outputs);

	struct wlr_backend backend;
	memset(&backend, 0, sizeof(backend));
	server.backend = &backend;

	struct wlr_output_configuration_v1 *config =
		wlr_output_configuration_v1_create();
	assert(config != NULL);

	struct wlr_backend_output_state states[1];
	memset(states, 0, sizeof(states));
	g_build_state_result = states;
	g_build_state_len = 1;
	g_backend_test_result = false;

	handle_output_configuration(&server, config, false);

	assert(g_backend_test_count == 1);
	assert(g_config_send_failed_count == 1);
	assert(g_config_destroy_count == 1);

	printf("  PASS: test_handle_config_test_fails\n");
}

/* Test 8: handle_output_configuration commit fails */
static void
test_handle_config_commit_fails(void)
{
	reset_globals();

	struct wm_server server;
	memset(&server, 0, sizeof(server));
	wl_list_init(&server.outputs);

	struct wlr_backend backend;
	memset(&backend, 0, sizeof(backend));
	server.backend = &backend;

	struct wlr_output_configuration_v1 *config =
		wlr_output_configuration_v1_create();
	assert(config != NULL);

	struct wlr_backend_output_state states[1];
	memset(states, 0, sizeof(states));
	g_build_state_result = states;
	g_build_state_len = 1;
	g_backend_test_result = true;
	g_backend_commit_result = false;

	handle_output_configuration(&server, config, false);

	assert(g_backend_test_count == 1);
	assert(g_backend_commit_count == 1);
	assert(g_config_send_failed_count == 1);
	assert(g_config_destroy_count == 1);

	printf("  PASS: test_handle_config_commit_fails\n");
}

/* Test 9: wm_output_management_update with null manager */
static void
test_update_null_manager(void)
{
	reset_globals();

	struct wm_server server;
	memset(&server, 0, sizeof(server));
	wl_list_init(&server.outputs);
	server.output_mgmt.manager = NULL;

	/* Should not crash, just return early */
	wm_output_management_update(&server);

	assert(g_config_create_count == 0);
	assert(g_manager_set_config_count == 0);

	printf("  PASS: test_update_null_manager\n");
}

/* Test 10: wm_output_management_update with valid manager */
static void
test_update_basic(void)
{
	reset_globals();

	struct wm_server server;
	memset(&server, 0, sizeof(server));
	wl_list_init(&server.outputs);

	struct wlr_output_manager_v1 manager;
	memset(&manager, 0, sizeof(manager));
	server.output_mgmt.manager = &manager;

	struct wlr_output_layout layout;
	memset(&layout, 0, sizeof(layout));
	server.output_layout = &layout;

	wm_output_management_update(&server);

	assert(g_config_create_count == 1);
	assert(g_manager_set_config_count == 1);

	printf("  PASS: test_update_basic\n");
}

/* Test 11: wm_output_management_finish with null manager */
static void
test_finish_null_manager(void)
{
	reset_globals();

	struct wm_server server;
	memset(&server, 0, sizeof(server));
	server.output_mgmt.manager = NULL;

	/* Should not crash */
	wm_output_management_finish(&server);

	printf("  PASS: test_finish_null_manager\n");
}

/* Test 12: wm_output_management_finish with valid manager */
static void
test_finish_basic(void)
{
	reset_globals();

	struct wm_server server;
	memset(&server, 0, sizeof(server));

	struct wlr_output_manager_v1 manager;
	memset(&manager, 0, sizeof(manager));
	wl_signal_init(&manager.events.apply);
	wl_signal_init(&manager.events.test);
	wl_signal_init(&manager.events.destroy);

	struct wlr_output_layout layout;
	memset(&layout, 0, sizeof(layout));
	wl_signal_init(&layout.events.change);
	server.output_layout = &layout;

	server.output_mgmt.manager = &manager;
	server.output_mgmt.server = &server;

	/* Register listeners (mimicking init) */
	server.output_mgmt.apply.notify = handle_apply;
	wl_signal_add(&manager.events.apply, &server.output_mgmt.apply);
	server.output_mgmt.test.notify = handle_test;
	wl_signal_add(&manager.events.test, &server.output_mgmt.test);
	server.output_mgmt.destroy.notify = handle_manager_destroy;
	wl_signal_add(&manager.events.destroy, &server.output_mgmt.destroy);
	server.output_mgmt.layout_change.notify = handle_layout_change;
	wl_signal_add(&layout.events.change,
		&server.output_mgmt.layout_change);

	wm_output_management_finish(&server);

	assert(server.output_mgmt.manager == NULL);

	printf("  PASS: test_finish_basic\n");
}

/* Test 13: build_current_configuration with multiple outputs */
static void
test_build_config_multiple_outputs(void)
{
	reset_globals();

	struct wm_server server;
	memset(&server, 0, sizeof(server));
	wl_list_init(&server.outputs);

	struct wlr_output wlr1, wlr2;
	memset(&wlr1, 0, sizeof(wlr1));
	memset(&wlr2, 0, sizeof(wlr2));
	wlr1.width = 1920; wlr1.height = 1080;
	wlr2.width = 2560; wlr2.height = 1440;

	struct wm_output out1, out2;
	memset(&out1, 0, sizeof(out1));
	memset(&out2, 0, sizeof(out2));
	out1.wlr_output = &wlr1;
	out2.wlr_output = &wlr2;
	wl_list_insert(&server.outputs, &out1.link);
	wl_list_insert(&server.outputs, &out2.link);

	struct wlr_output_layout layout;
	memset(&layout, 0, sizeof(layout));
	server.output_layout = &layout;

	struct wlr_output_configuration_v1 *config =
		build_current_configuration(&server);
	assert(config != NULL);
	assert(g_head_create_count == 2);

	printf("  PASS: test_build_config_multiple_outputs\n");
}

int
main(void)
{
	printf("test_output_mgmt:\n");

	test_build_config_empty_outputs();
	test_build_config_single_output();
	test_build_config_create_fails();
	test_build_config_head_create_fails();
	test_handle_config_build_state_fails();
	test_handle_config_test_only_success();
	test_handle_config_test_fails();
	test_handle_config_commit_fails();
	test_update_null_manager();
	test_update_basic();
	test_finish_null_manager();
	test_finish_basic();
	test_build_config_multiple_outputs();

	printf("All output_mgmt tests passed.\n");
	return 0;
}
