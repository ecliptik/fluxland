/*
 * test_small_protocols.c - Unit tests for small protocol glue modules
 *
 * Tests four thin protocol wrappers that each just create a wlroots
 * manager object:
 *   - fractional_scale.c (wp-fractional-scale-v1)
 *   - presentation.c (wp-presentation-time)
 *   - viewporter.c (viewporter + single-pixel-buffer)
 *   - drm_syncobj.c (linux-drm-syncobj-v1)
 *
 * Each module is tested via its own init function with stub create
 * functions that track calls and can simulate failures.
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
#define WLR_TYPES_WLR_FRACTIONAL_SCALE_V1_H
#define WLR_TYPES_WLR_PRESENTATION_TIME_H
#define WLR_TYPES_WLR_SINGLE_PIXEL_BUFFER_V1_H
#define WLR_TYPES_WLR_VIEWPORTER_H
#define WLR_TYPES_WLR_SCENE_H
#define WLR_TYPES_WLR_LINUX_DRM_SYNCOBJ_V1_H
#define WLR_BACKEND_H
#define WLR_RENDER_WLR_RENDERER_H
#define WLR_UTIL_LOG_H

/* fluxland headers */
#define WM_SERVER_H
#define WM_FRACTIONAL_SCALE_H
#define WM_PRESENTATION_H
#define WM_VIEWPORTER_H
#define WM_DRM_SYNCOBJ_H

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

struct wlr_backend { int dummy; };
struct wlr_renderer { int dummy; };
struct wlr_fractional_scale_manager_v1 { int dummy; };
struct wlr_presentation { int dummy; };
struct wlr_viewporter { int dummy; };
struct wlr_single_pixel_buffer_manager_v1 { int dummy; };
struct wlr_linux_drm_syncobj_manager_v1 { int dummy; };

/* --- Stub wm types --- */

struct wm_server {
	struct wl_display *wl_display;
	struct wlr_backend *backend;
	struct wlr_renderer *renderer;

	/* fractional_scale.c */
	struct wlr_fractional_scale_manager_v1 *fractional_scale_mgr;

	/* presentation.c */
	struct wlr_presentation *presentation;

	/* viewporter.c */
	struct wlr_viewporter *viewporter;
	struct wlr_single_pixel_buffer_manager_v1 *single_pixel_buffer_mgr;

	/* drm_syncobj.c */
	struct wlr_linux_drm_syncobj_manager_v1 *syncobj_mgr;
};

/* --- Tracking counters --- */

/* fractional_scale */
static int g_frac_scale_create_count;
static bool g_frac_scale_returns_null;

/* presentation */
static int g_presentation_create_count;
static bool g_presentation_returns_null;

/* viewporter */
static int g_viewporter_create_count;
static int g_single_pixel_create_count;
static bool g_viewporter_returns_null;
static bool g_single_pixel_returns_null;

/* drm_syncobj */
static int g_syncobj_create_count;
static int g_renderer_get_drm_fd_count;
static bool g_syncobj_returns_null;
static int g_drm_fd_value;

/* --- Static stub instances --- */
static struct wlr_fractional_scale_manager_v1 g_stub_frac_scale;
static struct wlr_presentation g_stub_presentation;
static struct wlr_viewporter g_stub_viewporter;
static struct wlr_single_pixel_buffer_manager_v1 g_stub_single_pixel;
static struct wlr_linux_drm_syncobj_manager_v1 g_stub_syncobj;

/* --- Stub functions for fractional_scale.c --- */

static struct wlr_fractional_scale_manager_v1 *
wlr_fractional_scale_manager_v1_create(struct wl_display *display,
	uint32_t version)
{
	(void)display; (void)version;
	g_frac_scale_create_count++;
	if (g_frac_scale_returns_null)
		return NULL;
	return &g_stub_frac_scale;
}

/* --- Stub functions for presentation.c --- */

static struct wlr_presentation *
wlr_presentation_create(struct wl_display *display,
	struct wlr_backend *backend)
{
	(void)display; (void)backend;
	g_presentation_create_count++;
	if (g_presentation_returns_null)
		return NULL;
	return &g_stub_presentation;
}

/* --- Stub functions for viewporter.c --- */

static struct wlr_viewporter *
wlr_viewporter_create(struct wl_display *display)
{
	(void)display;
	g_viewporter_create_count++;
	if (g_viewporter_returns_null)
		return NULL;
	return &g_stub_viewporter;
}

static struct wlr_single_pixel_buffer_manager_v1 *
wlr_single_pixel_buffer_manager_v1_create(struct wl_display *display)
{
	(void)display;
	g_single_pixel_create_count++;
	if (g_single_pixel_returns_null)
		return NULL;
	return &g_stub_single_pixel;
}

/* --- Stub functions for drm_syncobj.c --- */

static int
wlr_renderer_get_drm_fd(struct wlr_renderer *renderer)
{
	(void)renderer;
	g_renderer_get_drm_fd_count++;
	return g_drm_fd_value;
}

static struct wlr_linux_drm_syncobj_manager_v1 *
wlr_linux_drm_syncobj_manager_v1_create(struct wl_display *display,
	uint32_t version, int drm_fd)
{
	(void)display; (void)version; (void)drm_fd;
	g_syncobj_create_count++;
	if (g_syncobj_returns_null)
		return NULL;
	return &g_stub_syncobj;
}

/* --- Forward declarations --- */
void wm_fractional_scale_init(struct wm_server *server);
void wm_presentation_init(struct wm_server *server);
void wm_viewporter_init(struct wm_server *server);
void wm_drm_syncobj_init(struct wm_server *server);
void wm_drm_syncobj_finish(struct wm_server *server);

/* --- Include sources ---
 *
 * These modules have no overlapping static symbols, so we can include
 * all four in one translation unit.
 */

#include "fractional_scale.c"
#include "presentation.c"
#include "viewporter.c"
#include "drm_syncobj.c"

/* --- Test helpers --- */

static void
reset_globals(void)
{
	g_frac_scale_create_count = 0;
	g_frac_scale_returns_null = false;

	g_presentation_create_count = 0;
	g_presentation_returns_null = false;

	g_viewporter_create_count = 0;
	g_single_pixel_create_count = 0;
	g_viewporter_returns_null = false;
	g_single_pixel_returns_null = false;

	g_syncobj_create_count = 0;
	g_renderer_get_drm_fd_count = 0;
	g_syncobj_returns_null = false;
	g_drm_fd_value = 10; /* valid FD by default */
}

static struct wl_display g_test_display;
static struct wlr_backend g_test_backend;
static struct wlr_renderer g_test_renderer;

static void
init_server(struct wm_server *server)
{
	memset(server, 0, sizeof(*server));
	server->wl_display = &g_test_display;
	server->backend = &g_test_backend;
	server->renderer = &g_test_renderer;
}

/* ===== Fractional Scale Tests ===== */

/* Test 1: fractional scale init creates manager */
static void
test_fractional_scale_init(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_fractional_scale_init(&server);

	assert(g_frac_scale_create_count == 1);
	assert(server.fractional_scale_mgr == &g_stub_frac_scale);

	printf("  PASS: test_fractional_scale_init\n");
}

/* Test 2: fractional scale init handles failure */
static void
test_fractional_scale_init_failure(void)
{
	reset_globals();
	g_frac_scale_returns_null = true;

	struct wm_server server;
	init_server(&server);

	wm_fractional_scale_init(&server);

	assert(g_frac_scale_create_count == 1);
	assert(server.fractional_scale_mgr == NULL);

	printf("  PASS: test_fractional_scale_init_failure\n");
}

/* ===== Presentation Tests ===== */

/* Test 3: presentation init creates manager */
static void
test_presentation_init(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_presentation_init(&server);

	assert(g_presentation_create_count == 1);
	assert(server.presentation == &g_stub_presentation);

	printf("  PASS: test_presentation_init\n");
}

/* Test 4: presentation init handles failure */
static void
test_presentation_init_failure(void)
{
	reset_globals();
	g_presentation_returns_null = true;

	struct wm_server server;
	init_server(&server);

	wm_presentation_init(&server);

	assert(g_presentation_create_count == 1);
	assert(server.presentation == NULL);

	printf("  PASS: test_presentation_init_failure\n");
}

/* ===== Viewporter Tests ===== */

/* Test 5: viewporter init creates both managers */
static void
test_viewporter_init(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_viewporter_init(&server);

	assert(g_viewporter_create_count == 1);
	assert(g_single_pixel_create_count == 1);
	assert(server.viewporter == &g_stub_viewporter);
	assert(server.single_pixel_buffer_mgr == &g_stub_single_pixel);

	printf("  PASS: test_viewporter_init\n");
}

/* Test 6: viewporter creation failure prevents single-pixel-buffer */
static void
test_viewporter_init_failure(void)
{
	reset_globals();
	g_viewporter_returns_null = true;

	struct wm_server server;
	init_server(&server);

	wm_viewporter_init(&server);

	assert(g_viewporter_create_count == 1);
	assert(server.viewporter == NULL);
	/* early return means single pixel buffer not attempted */
	assert(g_single_pixel_create_count == 0);

	printf("  PASS: test_viewporter_init_failure\n");
}

/* Test 7: single-pixel-buffer creation failure */
static void
test_single_pixel_buffer_init_failure(void)
{
	reset_globals();
	g_single_pixel_returns_null = true;

	struct wm_server server;
	init_server(&server);

	wm_viewporter_init(&server);

	assert(g_viewporter_create_count == 1);
	assert(g_single_pixel_create_count == 1);
	assert(server.viewporter == &g_stub_viewporter);
	assert(server.single_pixel_buffer_mgr == NULL);

	printf("  PASS: test_single_pixel_buffer_init_failure\n");
}

/* ===== DRM Syncobj Tests ===== */

/* Test 8: syncobj init with valid DRM FD */
static void
test_drm_syncobj_init(void)
{
	reset_globals();
	g_drm_fd_value = 10;

	struct wm_server server;
	init_server(&server);

	wm_drm_syncobj_init(&server);

	assert(g_renderer_get_drm_fd_count == 1);
	assert(g_syncobj_create_count == 1);
	assert(server.syncobj_mgr == &g_stub_syncobj);

	printf("  PASS: test_drm_syncobj_init\n");
}

/* Test 9: syncobj init with no DRM FD (negative) */
static void
test_drm_syncobj_no_drm_fd(void)
{
	reset_globals();
	g_drm_fd_value = -1;

	struct wm_server server;
	init_server(&server);

	wm_drm_syncobj_init(&server);

	assert(g_renderer_get_drm_fd_count == 1);
	assert(g_syncobj_create_count == 0);
	assert(server.syncobj_mgr == NULL);

	printf("  PASS: test_drm_syncobj_no_drm_fd\n");
}

/* Test 10: syncobj create fails (kernel lacks support) */
static void
test_drm_syncobj_create_fails(void)
{
	reset_globals();
	g_drm_fd_value = 10;
	g_syncobj_returns_null = true;

	struct wm_server server;
	init_server(&server);

	wm_drm_syncobj_init(&server);

	assert(g_renderer_get_drm_fd_count == 1);
	assert(g_syncobj_create_count == 1);
	assert(server.syncobj_mgr == NULL);

	printf("  PASS: test_drm_syncobj_create_fails\n");
}

/* Test 11: syncobj finish is safe (no-op) */
static void
test_drm_syncobj_finish(void)
{
	reset_globals();

	struct wm_server server;
	init_server(&server);

	wm_drm_syncobj_init(&server);
	wm_drm_syncobj_finish(&server);

	/* finish is a no-op — just verify it doesn't crash */

	printf("  PASS: test_drm_syncobj_finish\n");
}

/* Test 12: syncobj finish with NULL manager */
static void
test_drm_syncobj_finish_null(void)
{
	reset_globals();
	g_drm_fd_value = -1;

	struct wm_server server;
	init_server(&server);

	wm_drm_syncobj_init(&server);
	assert(server.syncobj_mgr == NULL);

	wm_drm_syncobj_finish(&server);
	/* Should not crash */

	printf("  PASS: test_drm_syncobj_finish_null\n");
}

int
main(void)
{
	printf("test_small_protocols:\n");

	/* Fractional scale */
	test_fractional_scale_init();
	test_fractional_scale_init_failure();

	/* Presentation */
	test_presentation_init();
	test_presentation_init_failure();

	/* Viewporter */
	test_viewporter_init();
	test_viewporter_init_failure();
	test_single_pixel_buffer_init_failure();

	/* DRM syncobj */
	test_drm_syncobj_init();
	test_drm_syncobj_no_drm_fd();
	test_drm_syncobj_create_fails();
	test_drm_syncobj_finish();
	test_drm_syncobj_finish_null();

	printf("All small_protocols tests passed.\n");
	return 0;
}
