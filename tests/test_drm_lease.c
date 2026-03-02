/*
 * test_drm_lease.c - Unit tests for drm_lease.c
 *
 * Includes drm_lease.c directly with stub implementations to test
 * the DRM lease protocol init, finish, lease requests, and output offers.
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
#define WLR_TYPES_WLR_DRM_LEASE_V1_H
#define WLR_UTIL_LOG_H

/* fluxland headers */
#define WM_SERVER_H
#define WM_DRM_LEASE_H

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

struct wlr_backend { int dummy; };

struct wlr_output {
	char *name;
};

struct wlr_drm_lease_request_v1 {
	int dummy;
};

struct wlr_drm_lease_v1 {
	size_t n_connectors;
};

struct wlr_drm_lease_v1_manager {
	struct {
		struct wl_signal request;
	} events;
};

/* --- Stub wm types --- */

struct wm_server {
	struct wl_display *wl_display;
	struct wlr_backend *backend;
	struct wlr_drm_lease_v1_manager *drm_lease_mgr;
	struct wl_listener drm_lease_request;
};

/* --- Tracking counters --- */

static int g_create_mgr_count;
static int g_grant_count;
static int g_reject_count;
static int g_offer_output_count;
static bool g_create_returns_null;
static bool g_grant_returns_null;
static bool g_offer_returns_false;

/* --- Static stub instances --- */
static struct wlr_drm_lease_v1_manager g_stub_lease_mgr;
static struct wlr_drm_lease_v1 g_stub_lease;

/* --- Stub functions called by drm_lease.c --- */

static struct wlr_drm_lease_v1_manager *
wlr_drm_lease_v1_manager_create(struct wl_display *display,
	struct wlr_backend *backend)
{
	(void)display; (void)backend;
	g_create_mgr_count++;
	if (g_create_returns_null)
		return NULL;
	wl_signal_init(&g_stub_lease_mgr.events.request);
	return &g_stub_lease_mgr;
}

static struct wlr_drm_lease_v1 *
wlr_drm_lease_request_v1_grant(struct wlr_drm_lease_request_v1 *request)
{
	(void)request;
	g_grant_count++;
	if (g_grant_returns_null)
		return NULL;
	g_stub_lease.n_connectors = 1;
	return &g_stub_lease;
}

static void
wlr_drm_lease_request_v1_reject(struct wlr_drm_lease_request_v1 *request)
{
	(void)request;
	g_reject_count++;
}

static bool
wlr_drm_lease_v1_manager_offer_output(
	struct wlr_drm_lease_v1_manager *manager,
	struct wlr_output *output)
{
	(void)manager; (void)output;
	g_offer_output_count++;
	return !g_offer_returns_false;
}

/* --- Forward declarations --- */
void wm_drm_lease_init(struct wm_server *server);
void wm_drm_lease_finish(struct wm_server *server);
void wm_drm_lease_offer_output(struct wm_server *server,
	struct wlr_output *output);

/* --- Include source --- */

#include "drm_lease.c"

/* --- Test helpers --- */

static void
reset_globals(void)
{
	g_create_mgr_count = 0;
	g_grant_count = 0;
	g_reject_count = 0;
	g_offer_output_count = 0;
	g_create_returns_null = false;
	g_grant_returns_null = false;
	g_offer_returns_false = false;
	memset(&g_stub_lease_mgr, 0, sizeof(g_stub_lease_mgr));
	memset(&g_stub_lease, 0, sizeof(g_stub_lease));
}

static struct wl_display g_test_display;
static struct wlr_backend g_test_backend;

static void
init_server(struct wm_server *server)
{
	memset(server, 0, sizeof(*server));
	server->wl_display = &g_test_display;
	server->backend = &g_test_backend;
}

/* ===== Tests ===== */

/* Test 1: init creates lease manager and hooks signal */
static void
test_init_creates_manager(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_drm_lease_init(&server);

	assert(g_create_mgr_count == 1);
	assert(server.drm_lease_mgr == &g_stub_lease_mgr);
	assert(server.drm_lease_request.notify == handle_drm_lease_request);

	wm_drm_lease_finish(&server);

	printf("  PASS: test_init_creates_manager\n");
}

/* Test 2: init handles NULL return (no DRM backend) */
static void
test_init_no_drm_backend(void)
{
	reset_globals();
	g_create_returns_null = true;

	struct wm_server server;
	init_server(&server);

	wm_drm_lease_init(&server);

	assert(g_create_mgr_count == 1);
	assert(server.drm_lease_mgr == NULL);

	/* finish should be safe with NULL manager */
	wm_drm_lease_finish(&server);

	printf("  PASS: test_init_no_drm_backend\n");
}

/* Test 3: lease request handler grants successfully */
static void
test_lease_request_grant(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_drm_lease_init(&server);

	struct wlr_drm_lease_request_v1 request;
	server.drm_lease_request.notify(
		&server.drm_lease_request, &request);

	assert(g_grant_count == 1);
	assert(g_reject_count == 0);

	wm_drm_lease_finish(&server);

	printf("  PASS: test_lease_request_grant\n");
}

/* Test 4: lease request handler rejects on grant failure */
static void
test_lease_request_reject(void)
{
	reset_globals();
	g_grant_returns_null = true;

	struct wm_server server;
	init_server(&server);

	wm_drm_lease_init(&server);

	struct wlr_drm_lease_request_v1 request;
	server.drm_lease_request.notify(
		&server.drm_lease_request, &request);

	assert(g_grant_count == 1);
	assert(g_reject_count == 1);

	wm_drm_lease_finish(&server);

	printf("  PASS: test_lease_request_reject\n");
}

/* Test 5: finish removes listener */
static void
test_finish_removes_listener(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_drm_lease_init(&server);
	wm_drm_lease_finish(&server);

	assert(server.drm_lease_request.link.prev == NULL);

	printf("  PASS: test_finish_removes_listener\n");
}

/* Test 6: offer_output calls wlr function when manager exists */
static void
test_offer_output_success(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_drm_lease_init(&server);

	struct wlr_output output = { .name = "DP-1" };
	wm_drm_lease_offer_output(&server, &output);

	assert(g_offer_output_count == 1);

	wm_drm_lease_finish(&server);

	printf("  PASS: test_offer_output_success\n");
}

/* Test 7: offer_output is no-op when no DRM backend */
static void
test_offer_output_no_manager(void)
{
	reset_globals();
	g_create_returns_null = true;

	struct wm_server server;
	init_server(&server);

	wm_drm_lease_init(&server);

	struct wlr_output output = { .name = "DP-1" };
	wm_drm_lease_offer_output(&server, &output);

	assert(g_offer_output_count == 0);

	printf("  PASS: test_offer_output_no_manager\n");
}

/* Test 8: offer_output handles wlr rejection gracefully */
static void
test_offer_output_rejected(void)
{
	reset_globals();
	g_offer_returns_false = true;

	struct wm_server server;
	init_server(&server);

	wm_drm_lease_init(&server);

	struct wlr_output output = { .name = "HDMI-A-1" };
	wm_drm_lease_offer_output(&server, &output);

	assert(g_offer_output_count == 1);
	/* Should not crash — just logs a message */

	wm_drm_lease_finish(&server);

	printf("  PASS: test_offer_output_rejected\n");
}

int
main(void)
{
	printf("test_drm_lease:\n");

	test_init_creates_manager();
	test_init_no_drm_backend();
	test_lease_request_grant();
	test_lease_request_reject();
	test_finish_removes_listener();
	test_offer_output_success();
	test_offer_output_no_manager();
	test_offer_output_rejected();

	printf("All drm_lease tests passed.\n");
	return 0;
}
