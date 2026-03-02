/*
 * test_gamma_control.c - Unit tests for gamma_control.c
 *
 * Includes gamma_control.c directly with stub implementations to test
 * the gamma control protocol init and the set_gamma event handler.
 *
 * SPDX-License-Identifier: MIT
 */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

/* --- Block real headers via include guards --- */

/* Wayland */
#define WAYLAND_SERVER_CORE_H
#define WAYLAND_SERVER_H
#define WAYLAND_UTIL_H

/* wlroots types */
#define WLR_USE_UNSTABLE 1
#define WLR_TYPES_WLR_GAMMA_CONTROL_V1_H
#define WLR_TYPES_WLR_OUTPUT_H
#define WLR_TYPES_WLR_SCENE_H
#define WLR_UTIL_LOG_H

/* fluxland headers */
#define WM_SERVER_H
#define WM_GAMMA_CONTROL_H
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

#define wl_container_of(ptr, sample, member) \
	(__typeof__(sample))((char *)(ptr) - \
	offsetof(__typeof__(*sample), member))

#define wl_list_for_each(pos, head, member) \
	for (pos = wl_container_of((head)->next, pos, member); \
	     &pos->member != (head); \
	     pos = wl_container_of(pos->member.next, pos, member))

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

/* --- wlr_log stub --- */
#define wlr_log(verb, fmt, ...) ((void)0)
#define WLR_ERROR 1
#define WLR_INFO  3
#define WLR_DEBUG 7

/* --- Stub wlr types --- */

struct wlr_output {
	char *name;
};

struct wlr_output_state { int dummy; };

struct wlr_scene_output { int dummy; };

struct wlr_gamma_control_v1 { int dummy; };

struct wlr_gamma_control_manager_v1 {
	struct {
		struct wl_signal set_gamma;
	} events;
};

struct wlr_gamma_control_manager_v1_set_gamma_event {
	struct wlr_output *output;
	struct wlr_gamma_control_v1 *control;
};

/* --- Stub wm types --- */

struct wm_output {
	struct wl_list link;
	struct wlr_output *wlr_output;
	struct wlr_scene_output *scene_output;
};

struct wm_server {
	struct wl_display *wl_display;
	struct wl_list outputs;
	struct wlr_gamma_control_manager_v1 *gamma_control_mgr;
	struct wl_listener gamma_set_gamma;
};

/* --- Tracking counters --- */

static int g_create_mgr_count;
static int g_output_state_init_count;
static int g_output_state_finish_count;
static int g_scene_output_build_state_count;
static int g_gamma_apply_count;
static int g_output_commit_count;
static int g_gamma_send_failed_count;
static bool g_create_returns_null;
static bool g_build_state_returns_false;
static bool g_gamma_apply_returns_false;
static bool g_output_commit_returns_false;

/* --- Static stub instances --- */
static struct wlr_gamma_control_manager_v1 g_stub_gamma_mgr;

/* --- Stub functions --- */

static struct wlr_gamma_control_manager_v1 *
wlr_gamma_control_manager_v1_create(struct wl_display *display)
{
	(void)display;
	g_create_mgr_count++;
	if (g_create_returns_null)
		return NULL;
	wl_signal_init(&g_stub_gamma_mgr.events.set_gamma);
	return &g_stub_gamma_mgr;
}

static void
wlr_output_state_init(struct wlr_output_state *state)
{
	(void)state;
	g_output_state_init_count++;
}

static void
wlr_output_state_finish(struct wlr_output_state *state)
{
	(void)state;
	g_output_state_finish_count++;
}

static bool
wlr_scene_output_build_state(struct wlr_scene_output *scene_output,
	struct wlr_output_state *state, void *options)
{
	(void)scene_output; (void)state; (void)options;
	g_scene_output_build_state_count++;
	return !g_build_state_returns_false;
}

static bool
wlr_gamma_control_v1_apply(struct wlr_gamma_control_v1 *control,
	struct wlr_output_state *state)
{
	(void)control; (void)state;
	g_gamma_apply_count++;
	return !g_gamma_apply_returns_false;
}

static bool
wlr_output_commit_state(struct wlr_output *output,
	struct wlr_output_state *state)
{
	(void)output; (void)state;
	g_output_commit_count++;
	return !g_output_commit_returns_false;
}

static void
wlr_gamma_control_v1_send_failed_and_destroy(
	struct wlr_gamma_control_v1 *control)
{
	(void)control;
	g_gamma_send_failed_count++;
}

/* --- Forward declarations --- */
void wm_gamma_control_init(struct wm_server *server);

/* --- Include source --- */

#include "gamma_control.c"

/* --- Test helpers --- */

static void
reset_globals(void)
{
	g_create_mgr_count = 0;
	g_output_state_init_count = 0;
	g_output_state_finish_count = 0;
	g_scene_output_build_state_count = 0;
	g_gamma_apply_count = 0;
	g_output_commit_count = 0;
	g_gamma_send_failed_count = 0;
	g_create_returns_null = false;
	g_build_state_returns_false = false;
	g_gamma_apply_returns_false = false;
	g_output_commit_returns_false = false;
	memset(&g_stub_gamma_mgr, 0, sizeof(g_stub_gamma_mgr));
}

static struct wl_display g_test_display;

static void
init_server(struct wm_server *server)
{
	memset(server, 0, sizeof(*server));
	server->wl_display = &g_test_display;
	wl_list_init(&server->outputs);
}

/* ===== Tests ===== */

/* Test 1: init creates gamma control manager */
static void
test_init_creates_manager(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_gamma_control_init(&server);

	assert(g_create_mgr_count == 1);
	assert(server.gamma_control_mgr == &g_stub_gamma_mgr);
	assert(server.gamma_set_gamma.notify == handle_set_gamma);

	printf("  PASS: test_init_creates_manager\n");
}

/* Test 2: init handles creation failure */
static void
test_init_creation_failure(void)
{
	reset_globals();
	g_create_returns_null = true;

	struct wm_server server;
	init_server(&server);

	wm_gamma_control_init(&server);

	assert(g_create_mgr_count == 1);
	assert(server.gamma_control_mgr == NULL);

	printf("  PASS: test_init_creation_failure\n");
}

/* Test 3: set_gamma handler — full success path with matching output */
static void
test_set_gamma_success(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_gamma_control_init(&server);

	/* Set up a matching output */
	struct wlr_output wlr_out;
	struct wlr_scene_output scene_out;
	struct wm_output output = {
		.wlr_output = &wlr_out,
		.scene_output = &scene_out,
	};
	wl_list_insert(&server.outputs, &output.link);

	struct wlr_gamma_control_v1 control;
	struct wlr_gamma_control_manager_v1_set_gamma_event event = {
		.output = &wlr_out,
		.control = &control,
	};

	server.gamma_set_gamma.notify(&server.gamma_set_gamma, &event);

	assert(g_output_state_init_count == 1);
	assert(g_scene_output_build_state_count == 1);
	assert(g_gamma_apply_count == 1);
	assert(g_output_commit_count == 1);
	assert(g_output_state_finish_count == 1);
	assert(g_gamma_send_failed_count == 0);

	printf("  PASS: test_set_gamma_success\n");
}

/* Test 4: set_gamma handler — build state fails */
static void
test_set_gamma_build_state_fails(void)
{
	reset_globals();
	g_build_state_returns_false = true;

	struct wm_server server;
	init_server(&server);

	wm_gamma_control_init(&server);

	struct wlr_output wlr_out;
	struct wlr_scene_output scene_out;
	struct wm_output output = {
		.wlr_output = &wlr_out,
		.scene_output = &scene_out,
	};
	wl_list_insert(&server.outputs, &output.link);

	struct wlr_gamma_control_v1 control;
	struct wlr_gamma_control_manager_v1_set_gamma_event event = {
		.output = &wlr_out,
		.control = &control,
	};

	server.gamma_set_gamma.notify(&server.gamma_set_gamma, &event);

	assert(g_scene_output_build_state_count == 1);
	/* Should goto finish — no apply or commit */
	assert(g_gamma_apply_count == 0);
	assert(g_output_commit_count == 0);
	assert(g_output_state_finish_count == 1);

	printf("  PASS: test_set_gamma_build_state_fails\n");
}

/* Test 5: set_gamma handler — gamma apply fails */
static void
test_set_gamma_apply_fails(void)
{
	reset_globals();
	g_gamma_apply_returns_false = true;

	struct wm_server server;
	init_server(&server);

	wm_gamma_control_init(&server);

	struct wlr_output wlr_out;
	struct wlr_scene_output scene_out;
	struct wm_output output = {
		.wlr_output = &wlr_out,
		.scene_output = &scene_out,
	};
	wl_list_insert(&server.outputs, &output.link);

	struct wlr_gamma_control_v1 control;
	struct wlr_gamma_control_manager_v1_set_gamma_event event = {
		.output = &wlr_out,
		.control = &control,
	};

	server.gamma_set_gamma.notify(&server.gamma_set_gamma, &event);

	assert(g_gamma_apply_count == 1);
	/* Should goto finish — no commit */
	assert(g_output_commit_count == 0);
	assert(g_output_state_finish_count == 1);

	printf("  PASS: test_set_gamma_apply_fails\n");
}

/* Test 6: set_gamma handler — output commit fails, sends failed */
static void
test_set_gamma_commit_fails(void)
{
	reset_globals();
	g_output_commit_returns_false = true;

	struct wm_server server;
	init_server(&server);

	wm_gamma_control_init(&server);

	struct wlr_output wlr_out;
	struct wlr_scene_output scene_out;
	struct wm_output output = {
		.wlr_output = &wlr_out,
		.scene_output = &scene_out,
	};
	wl_list_insert(&server.outputs, &output.link);

	struct wlr_gamma_control_v1 control;
	struct wlr_gamma_control_manager_v1_set_gamma_event event = {
		.output = &wlr_out,
		.control = &control,
	};

	server.gamma_set_gamma.notify(&server.gamma_set_gamma, &event);

	assert(g_output_commit_count == 1);
	assert(g_gamma_send_failed_count == 1);
	assert(g_output_state_finish_count == 1);

	printf("  PASS: test_set_gamma_commit_fails\n");
}

/* Test 7: set_gamma with NULL control skips apply */
static void
test_set_gamma_null_control(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_gamma_control_init(&server);

	struct wlr_output wlr_out;
	struct wlr_scene_output scene_out;
	struct wm_output output = {
		.wlr_output = &wlr_out,
		.scene_output = &scene_out,
	};
	wl_list_insert(&server.outputs, &output.link);

	struct wlr_gamma_control_manager_v1_set_gamma_event event = {
		.output = &wlr_out,
		.control = NULL,
	};

	server.gamma_set_gamma.notify(&server.gamma_set_gamma, &event);

	assert(g_gamma_apply_count == 0);
	assert(g_output_commit_count == 1);
	assert(g_output_state_finish_count == 1);

	printf("  PASS: test_set_gamma_null_control\n");
}

/* Test 8: set_gamma with no matching output still commits */
static void
test_set_gamma_no_matching_output(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_gamma_control_init(&server);

	/* Add an output that doesn't match the event */
	struct wlr_output wlr_out1;
	struct wlr_output wlr_out2;
	struct wlr_scene_output scene_out;
	struct wm_output output = {
		.wlr_output = &wlr_out1,
		.scene_output = &scene_out,
	};
	wl_list_insert(&server.outputs, &output.link);

	struct wlr_gamma_control_v1 control;
	struct wlr_gamma_control_manager_v1_set_gamma_event event = {
		.output = &wlr_out2,
		.control = &control,
	};

	server.gamma_set_gamma.notify(&server.gamma_set_gamma, &event);

	/* No matching output means build_state not called, but gamma apply
	 * and commit still happen */
	assert(g_scene_output_build_state_count == 0);
	assert(g_output_state_finish_count == 1);

	printf("  PASS: test_set_gamma_no_matching_output\n");
}

int
main(void)
{
	printf("test_gamma_control:\n");

	test_init_creates_manager();
	test_init_creation_failure();
	test_set_gamma_success();
	test_set_gamma_build_state_fails();
	test_set_gamma_apply_fails();
	test_set_gamma_commit_fails();
	test_set_gamma_null_control();
	test_set_gamma_no_matching_output();

	printf("All gamma_control tests passed.\n");
	return 0;
}
