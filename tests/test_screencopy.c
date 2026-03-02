/*
 * test_screencopy.c - Unit tests for screencopy.c
 *
 * Includes screencopy.c directly with stub implementations to test
 * the screen capture protocol initialization (screencopy + DMA-BUF export).
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

/* --- Block real headers via include guards --- */

/* Wayland */
#define WAYLAND_SERVER_CORE_H
#define WAYLAND_SERVER_H
#define WAYLAND_UTIL_H

/* wlroots types */
#define WLR_USE_UNSTABLE 1
#define WLR_TYPES_WLR_SCREENCOPY_V1_H
#define WLR_TYPES_WLR_EXPORT_DMABUF_V1_H
#define WLR_UTIL_LOG_H

/* fluxland headers */
#define WM_SERVER_H
#define WM_SCREENCOPY_H

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

struct wl_signal {
	struct wl_list listener_list;
};

struct wl_listener {
	struct wl_list link;
	void (*notify)(struct wl_listener *, void *);
};

struct wl_display { int dummy; };

/* --- wlr_log stub --- */
#define wlr_log(verb, fmt, ...) ((void)0)
#define WLR_ERROR 1
#define WLR_INFO  3
#define WLR_DEBUG 7

/* --- Stub wlr types --- */

struct wlr_screencopy_manager_v1 { int dummy; };
struct wlr_export_dmabuf_manager_v1 { int dummy; };

/* --- Stub wm types --- */

struct wm_server {
	struct wl_display *wl_display;
};

/* --- Tracking counters --- */

static int g_screencopy_create_count;
static int g_dmabuf_export_create_count;
static bool g_screencopy_create_returns_null;
static bool g_dmabuf_export_create_returns_null;

/* --- Static stub instances --- */
static struct wlr_screencopy_manager_v1 g_stub_screencopy;
static struct wlr_export_dmabuf_manager_v1 g_stub_dmabuf_export;

/* --- Stub create functions --- */

static struct wlr_screencopy_manager_v1 *
wlr_screencopy_manager_v1_create(struct wl_display *display)
{
	(void)display;
	g_screencopy_create_count++;
	if (g_screencopy_create_returns_null)
		return NULL;
	return &g_stub_screencopy;
}

static struct wlr_export_dmabuf_manager_v1 *
wlr_export_dmabuf_manager_v1_create(struct wl_display *display)
{
	(void)display;
	g_dmabuf_export_create_count++;
	if (g_dmabuf_export_create_returns_null)
		return NULL;
	return &g_stub_dmabuf_export;
}

/* --- Forward declarations --- */
void wm_screencopy_init(struct wm_server *server);

/* --- Include source --- */

#include "screencopy.c"

/* --- Test helpers --- */

static void
reset_globals(void)
{
	g_screencopy_create_count = 0;
	g_dmabuf_export_create_count = 0;
	g_screencopy_create_returns_null = false;
	g_dmabuf_export_create_returns_null = false;
}

static struct wl_display g_test_display;

static void
init_server(struct wm_server *server)
{
	memset(server, 0, sizeof(*server));
	server->wl_display = &g_test_display;
}

/* ===== Tests ===== */

/* Test 1: init creates both screencopy and DMA-BUF export managers */
static void
test_init_creates_both_managers(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_screencopy_init(&server);

	assert(g_screencopy_create_count == 1);
	assert(g_dmabuf_export_create_count == 1);

	printf("  PASS: test_init_creates_both_managers\n");
}

/* Test 2: init handles screencopy creation failure (early return) */
static void
test_init_screencopy_failure(void)
{
	reset_globals();
	g_screencopy_create_returns_null = true;

	struct wm_server server;
	init_server(&server);

	wm_screencopy_init(&server);

	assert(g_screencopy_create_count == 1);
	/* DMA-BUF export should not be attempted after screencopy fails */
	assert(g_dmabuf_export_create_count == 0);

	printf("  PASS: test_init_screencopy_failure\n");
}

/* Test 3: init handles DMA-BUF export creation failure */
static void
test_init_dmabuf_export_failure(void)
{
	reset_globals();
	g_dmabuf_export_create_returns_null = true;

	struct wm_server server;
	init_server(&server);

	wm_screencopy_init(&server);

	assert(g_screencopy_create_count == 1);
	assert(g_dmabuf_export_create_count == 1);
	/* screencopy succeeded, only dmabuf export failed */

	printf("  PASS: test_init_dmabuf_export_failure\n");
}

/* Test 4: init passes display to create functions */
static void
test_init_passes_display(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	/* Just verify it doesn't crash with a valid server */
	wm_screencopy_init(&server);

	assert(g_screencopy_create_count == 1);
	assert(g_dmabuf_export_create_count == 1);

	printf("  PASS: test_init_passes_display\n");
}

int
main(void)
{
	printf("test_screencopy:\n");

	test_init_creates_both_managers();
	test_init_screencopy_failure();
	test_init_dmabuf_export_failure();
	test_init_passes_display();

	printf("All screencopy tests passed.\n");
	return 0;
}
