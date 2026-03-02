/*
 * test_transient_seat.c - Unit tests for transient_seat.c
 *
 * Includes transient_seat.c directly with stub implementations to test
 * the transient seat protocol init, finish, and seat creation handler.
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
#define WLR_TYPES_WLR_SEAT_H
#define WLR_TYPES_WLR_TRANSIENT_SEAT_V1_H
#define WLR_UTIL_LOG_H

/* fluxland headers */
#define WM_SERVER_H
#define WM_TRANSIENT_SEAT_H

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

struct wlr_seat { int dummy; };

struct wlr_transient_seat_v1 { int dummy; };

struct wlr_transient_seat_manager_v1 {
	struct {
		struct wl_signal create_seat;
	} events;
};

/* --- Stub wm types --- */

struct wm_server {
	struct wl_display *wl_display;
	struct wlr_transient_seat_manager_v1 *transient_seat_mgr;
	struct wl_listener transient_seat_create;
};

/* --- Tracking counters --- */

static int g_create_mgr_count;
static int g_seat_create_count;
static int g_ready_count;
static int g_deny_count;
static bool g_create_mgr_returns_null;
static bool g_seat_create_returns_null;
static char g_last_seat_name[64];

/* --- Static stub instances --- */
static struct wlr_transient_seat_manager_v1 g_stub_tseat_mgr;
static struct wlr_seat g_stub_seat;

/* --- Stub functions --- */

static struct wlr_transient_seat_manager_v1 *
wlr_transient_seat_manager_v1_create(struct wl_display *display)
{
	(void)display;
	g_create_mgr_count++;
	if (g_create_mgr_returns_null)
		return NULL;
	wl_signal_init(&g_stub_tseat_mgr.events.create_seat);
	return &g_stub_tseat_mgr;
}

static struct wlr_seat *
wlr_seat_create(struct wl_display *display, const char *name)
{
	(void)display;
	g_seat_create_count++;
	strncpy(g_last_seat_name, name, sizeof(g_last_seat_name) - 1);
	g_last_seat_name[sizeof(g_last_seat_name) - 1] = '\0';
	if (g_seat_create_returns_null)
		return NULL;
	return &g_stub_seat;
}

static void
wlr_transient_seat_v1_ready(struct wlr_transient_seat_v1 *tseat,
	struct wlr_seat *seat)
{
	(void)tseat; (void)seat;
	g_ready_count++;
}

static void
wlr_transient_seat_v1_deny(struct wlr_transient_seat_v1 *tseat)
{
	(void)tseat;
	g_deny_count++;
}

/* --- Forward declarations --- */
void wm_transient_seat_init(struct wm_server *server);
void wm_transient_seat_finish(struct wm_server *server);

/* --- Include source --- */

#include "transient_seat.c"

/* --- Test helpers --- */

static void
reset_globals(void)
{
	g_create_mgr_count = 0;
	g_seat_create_count = 0;
	g_ready_count = 0;
	g_deny_count = 0;
	g_create_mgr_returns_null = false;
	g_seat_create_returns_null = false;
	memset(g_last_seat_name, 0, sizeof(g_last_seat_name));
	memset(&g_stub_tseat_mgr, 0, sizeof(g_stub_tseat_mgr));
	/* Reset the static counter in transient_seat.c */
	transient_seat_counter = 0;
}

static struct wl_display g_test_display;

static void
init_server(struct wm_server *server)
{
	memset(server, 0, sizeof(*server));
	server->wl_display = &g_test_display;
}

/* ===== Tests ===== */

/* Test 1: init creates transient seat manager */
static void
test_init_creates_manager(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_transient_seat_init(&server);

	assert(g_create_mgr_count == 1);
	assert(server.transient_seat_mgr == &g_stub_tseat_mgr);
	assert(server.transient_seat_create.notify == handle_create_seat);

	wm_transient_seat_finish(&server);

	printf("  PASS: test_init_creates_manager\n");
}

/* Test 2: init handles creation failure */
static void
test_init_creation_failure(void)
{
	reset_globals();
	g_create_mgr_returns_null = true;

	struct wm_server server;
	init_server(&server);

	wm_transient_seat_init(&server);

	assert(g_create_mgr_count == 1);
	assert(server.transient_seat_mgr == NULL);

	/* finish should be safe with NULL manager */
	wm_transient_seat_finish(&server);

	printf("  PASS: test_init_creation_failure\n");
}

/* Test 3: create_seat handler creates and readies a seat */
static void
test_create_seat_success(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_transient_seat_init(&server);

	struct wlr_transient_seat_v1 tseat;
	server.transient_seat_create.notify(
		&server.transient_seat_create, &tseat);

	assert(g_seat_create_count == 1);
	assert(g_ready_count == 1);
	assert(g_deny_count == 0);
	assert(strcmp(g_last_seat_name, "transient-seat-0") == 0);

	wm_transient_seat_finish(&server);

	printf("  PASS: test_create_seat_success\n");
}

/* Test 4: create_seat handler denies when seat creation fails */
static void
test_create_seat_failure(void)
{
	reset_globals();
	g_seat_create_returns_null = true;

	struct wm_server server;
	init_server(&server);

	wm_transient_seat_init(&server);

	struct wlr_transient_seat_v1 tseat;
	server.transient_seat_create.notify(
		&server.transient_seat_create, &tseat);

	assert(g_seat_create_count == 1);
	assert(g_ready_count == 0);
	assert(g_deny_count == 1);

	wm_transient_seat_finish(&server);

	printf("  PASS: test_create_seat_failure\n");
}

/* Test 5: counter increments for each seat created */
static void
test_seat_counter_increments(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_transient_seat_init(&server);

	struct wlr_transient_seat_v1 tseat;

	server.transient_seat_create.notify(
		&server.transient_seat_create, &tseat);
	assert(strcmp(g_last_seat_name, "transient-seat-0") == 0);

	server.transient_seat_create.notify(
		&server.transient_seat_create, &tseat);
	assert(strcmp(g_last_seat_name, "transient-seat-1") == 0);

	server.transient_seat_create.notify(
		&server.transient_seat_create, &tseat);
	assert(strcmp(g_last_seat_name, "transient-seat-2") == 0);

	assert(g_seat_create_count == 3);
	assert(g_ready_count == 3);

	wm_transient_seat_finish(&server);

	printf("  PASS: test_seat_counter_increments\n");
}

/* Test 6: finish removes listener */
static void
test_finish_removes_listener(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_transient_seat_init(&server);
	wm_transient_seat_finish(&server);

	assert(server.transient_seat_create.link.prev == NULL);

	printf("  PASS: test_finish_removes_listener\n");
}

/* Test 7: finish with NULL manager is safe */
static void
test_finish_null_manager_safe(void)
{
	reset_globals();
	g_create_mgr_returns_null = true;

	struct wm_server server;
	init_server(&server);

	wm_transient_seat_init(&server);
	/* Should not crash */
	wm_transient_seat_finish(&server);

	printf("  PASS: test_finish_null_manager_safe\n");
}

int
main(void)
{
	printf("test_transient_seat:\n");

	test_init_creates_manager();
	test_init_creation_failure();
	test_create_seat_success();
	test_create_seat_failure();
	test_seat_counter_increments();
	test_finish_removes_listener();
	test_finish_null_manager_safe();

	printf("All transient_seat tests passed.\n");
	return 0;
}
