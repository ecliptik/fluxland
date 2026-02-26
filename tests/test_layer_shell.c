/*
 * test_layer_shell.c - wlr-layer-shell-unstable-v1 protocol integration tests
 *
 * Each test forks a headless compositor, connects as a Wayland client,
 * exercises layer shell functionality, and verifies expected behavior.
 * Exits 77 (meson skip) if the headless backend is unavailable or
 * the layer shell protocol global is not found.
 */

#define _GNU_SOURCE

#include "integration/harness.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

/* ------------------------------------------------------------------ */
/* Test helpers                                                        */
/* ------------------------------------------------------------------ */

static int tests_run;
static int tests_passed;
static int tests_failed;

#define TEST_START(name) \
	do { \
		tests_run++; \
		printf("  [%d] %s ... ", tests_run, (name)); \
		fflush(stdout); \
	} while (0)

#define TEST_PASS() \
	do { \
		tests_passed++; \
		printf("PASS\n"); \
	} while (0)

#define TEST_FAIL(reason) \
	do { \
		tests_failed++; \
		printf("FAIL: %s\n", (reason)); \
	} while (0)

/* ------------------------------------------------------------------ */
/* Wayland globals                                                     */
/* ------------------------------------------------------------------ */

static struct wl_display *display;
static struct wl_registry *registry;
static struct wl_compositor *compositor;
static struct wl_shm *shm;
static struct zwlr_layer_shell_v1 *layer_shell;

/* ------------------------------------------------------------------ */
/* Layer surface state                                                 */
/* ------------------------------------------------------------------ */

struct test_layer_surface {
	struct wl_surface *surface;
	struct zwlr_layer_surface_v1 *layer_surface;
	bool configured;
	uint32_t configure_serial;
	uint32_t configure_width;
	uint32_t configure_height;
	bool closed;
};

/* ------------------------------------------------------------------ */
/* Layer surface listeners                                             */
/* ------------------------------------------------------------------ */

static void
layer_surface_configure(void *data, struct zwlr_layer_surface_v1 *surface,
			uint32_t serial, uint32_t width, uint32_t height)
{
	struct test_layer_surface *ls = data;
	ls->configured = true;
	ls->configure_serial = serial;
	ls->configure_width = width;
	ls->configure_height = height;
	zwlr_layer_surface_v1_ack_configure(surface, serial);
}

static void
layer_surface_closed(void *data, struct zwlr_layer_surface_v1 *surface)
{
	struct test_layer_surface *ls = data;
	(void)surface;
	ls->closed = true;
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
	.configure = layer_surface_configure,
	.closed = layer_surface_closed,
};

/* ------------------------------------------------------------------ */
/* Registry listener                                                   */
/* ------------------------------------------------------------------ */

static void
registry_global(void *data, struct wl_registry *reg, uint32_t name,
		const char *interface, uint32_t version)
{
	(void)data;

	if (strcmp(interface, "wl_compositor") == 0) {
		compositor = wl_registry_bind(reg, name,
			&wl_compositor_interface,
			version < 6 ? version : 6);
	} else if (strcmp(interface, "wl_shm") == 0) {
		shm = wl_registry_bind(reg, name,
			&wl_shm_interface,
			version < 1 ? version : 1);
	} else if (strcmp(interface, "zwlr_layer_shell_v1") == 0) {
		layer_shell = wl_registry_bind(reg, name,
			&zwlr_layer_shell_v1_interface,
			version < 4 ? version : 4);
	}
}

static void
registry_global_remove(void *data, struct wl_registry *reg, uint32_t name)
{
	(void)data;
	(void)reg;
	(void)name;
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove,
};

/* ------------------------------------------------------------------ */
/* SHM buffer helper                                                   */
/* ------------------------------------------------------------------ */

static struct wl_buffer *
create_shm_buffer(int width, int height)
{
	int stride = width * 4;
	int size = stride * height;

	int fd = memfd_create("test-layer-buffer", MFD_CLOEXEC);
	if (fd < 0)
		return NULL;

	if (ftruncate(fd, size) < 0) {
		close(fd);
		return NULL;
	}

	struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
	close(fd);
	if (!pool)
		return NULL;

	struct wl_buffer *buffer = wl_shm_pool_create_buffer(
		pool, 0, width, height, stride, WL_SHM_FORMAT_XRGB8888);
	wl_shm_pool_destroy(pool);

	return buffer;
}

/* ------------------------------------------------------------------ */
/* Helper: create and configure a layer surface                        */
/* ------------------------------------------------------------------ */

static int
create_layer_surface(struct test_layer_surface *ls, uint32_t layer,
		     uint32_t anchor, int32_t exclusive_zone,
		     uint32_t width, uint32_t height,
		     uint32_t keyboard_interactivity)
{
	memset(ls, 0, sizeof(*ls));

	ls->surface = wl_compositor_create_surface(compositor);
	if (!ls->surface)
		return -1;

	ls->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
		layer_shell, ls->surface, NULL, layer, "test");
	if (!ls->layer_surface) {
		wl_surface_destroy(ls->surface);
		return -1;
	}

	zwlr_layer_surface_v1_add_listener(ls->layer_surface,
		&layer_surface_listener, ls);

	if (width > 0 || height > 0)
		zwlr_layer_surface_v1_set_size(ls->layer_surface,
					       width, height);
	if (anchor != 0)
		zwlr_layer_surface_v1_set_anchor(ls->layer_surface, anchor);
	if (exclusive_zone != 0)
		zwlr_layer_surface_v1_set_exclusive_zone(ls->layer_surface,
							 exclusive_zone);
	if (keyboard_interactivity != 0)
		zwlr_layer_surface_v1_set_keyboard_interactivity(
			ls->layer_surface, keyboard_interactivity);

	/* Initial commit (no buffer) to trigger configure */
	wl_surface_commit(ls->surface);
	wl_display_roundtrip(display);

	return 0;
}

static void
attach_buffer_and_commit(struct test_layer_surface *ls)
{
	if (!ls->configured)
		return;

	int w = (ls->configure_width > 0) ? (int)ls->configure_width : 100;
	int h = (ls->configure_height > 0) ? (int)ls->configure_height : 100;

	struct wl_buffer *buffer = create_shm_buffer(w, h);
	if (buffer) {
		wl_surface_attach(ls->surface, buffer, 0, 0);
		wl_surface_commit(ls->surface);
		wl_display_roundtrip(display);
	}
}

static void
destroy_layer_surface(struct test_layer_surface *ls)
{
	if (ls->layer_surface)
		zwlr_layer_surface_v1_destroy(ls->layer_surface);
	if (ls->surface)
		wl_surface_destroy(ls->surface);
	wl_display_roundtrip(display);
	memset(ls, 0, sizeof(*ls));
}

/* ------------------------------------------------------------------ */
/* Connection setup/teardown                                           */
/* ------------------------------------------------------------------ */

static void
disconnect_client(void)
{
	if (layer_shell) {
		zwlr_layer_shell_v1_destroy(layer_shell);
		layer_shell = NULL;
	}
	if (shm) {
		wl_shm_destroy(shm);
		shm = NULL;
	}
	if (compositor) {
		wl_compositor_destroy(compositor);
		compositor = NULL;
	}
	if (registry) {
		wl_registry_destroy(registry);
		registry = NULL;
	}
	if (display) {
		wl_display_disconnect(display);
		display = NULL;
	}
}

/*
 * Common pattern: start compositor, connect client.
 * Returns 0 on success, 77 if headless unavailable or protocol missing,
 * 1 on error.
 */
static int
setup(pid_t *pid, const char **socket)
{
	harness_setup();

	*pid = harness_start_compositor(socket);
	if (*pid <= 0) {
		harness_cleanup();
		return (*pid == 0) ? 77 : 1;
	}

	display = wl_display_connect(*socket);
	if (!display) {
		fprintf(stderr, "layer_shell test: connect failed: %s\n",
			strerror(errno));
		harness_stop_compositor(*pid);
		harness_cleanup();
		return 1;
	}

	registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_roundtrip(display);

	if (!compositor || !shm) {
		fprintf(stderr, "layer_shell test: missing core globals\n");
		disconnect_client();
		harness_stop_compositor(*pid);
		harness_cleanup();
		return 1;
	}

	if (!layer_shell) {
		fprintf(stderr,
			"layer_shell test: zwlr_layer_shell_v1 not found\n");
		disconnect_client();
		harness_stop_compositor(*pid);
		harness_cleanup();
		return 77;
	}

	return 0;
}

static void
teardown(pid_t pid)
{
	disconnect_client();
	harness_stop_compositor(pid);
	harness_cleanup();
}

/* ------------------------------------------------------------------ */
/* Anchor / layer / keyboard constants                                 */
/* ------------------------------------------------------------------ */

#define ANCHOR_TOP    ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP
#define ANCHOR_BOTTOM ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM
#define ANCHOR_LEFT   ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT
#define ANCHOR_RIGHT  ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT

#define LAYER_BG      ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND
#define LAYER_BOTTOM  ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM
#define LAYER_TOP     ZWLR_LAYER_SHELL_V1_LAYER_TOP
#define LAYER_OVERLAY ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY

#define KB_NONE      ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE
#define KB_EXCLUSIVE ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE
#define KB_ON_DEMAND ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND

/* ------------------------------------------------------------------ */
/* Tests                                                               */
/* ------------------------------------------------------------------ */

/*
 * Create layer surface on BACKGROUND layer, verify configure.
 */
static void
test_background_layer(void)
{
	TEST_START("layer_surface_background");

	struct test_layer_surface ls;
	if (create_layer_surface(&ls, LAYER_BG,
				 ANCHOR_TOP | ANCHOR_BOTTOM |
				 ANCHOR_LEFT | ANCHOR_RIGHT,
				 0, 0, 0, KB_NONE) < 0) {
		TEST_FAIL("failed to create layer surface");
		return;
	}

	if (!ls.configured)
		wl_display_roundtrip(display);

	if (ls.configured) {
		TEST_PASS();
	} else {
		TEST_FAIL("no configure event received");
	}

	destroy_layer_surface(&ls);
}

/*
 * Create layer surface on BOTTOM layer.
 */
static void
test_bottom_layer(void)
{
	TEST_START("layer_surface_bottom");

	struct test_layer_surface ls;
	if (create_layer_surface(&ls, LAYER_BOTTOM,
				 ANCHOR_TOP | ANCHOR_BOTTOM |
				 ANCHOR_LEFT | ANCHOR_RIGHT,
				 0, 0, 0, KB_NONE) < 0) {
		TEST_FAIL("failed to create layer surface");
		return;
	}

	if (!ls.configured)
		wl_display_roundtrip(display);

	if (ls.configured) {
		TEST_PASS();
	} else {
		TEST_FAIL("no configure event received");
	}

	destroy_layer_surface(&ls);
}

/*
 * Create layer surface on TOP layer.
 */
static void
test_top_layer(void)
{
	TEST_START("layer_surface_top");

	struct test_layer_surface ls;
	if (create_layer_surface(&ls, LAYER_TOP,
				 ANCHOR_TOP | ANCHOR_BOTTOM |
				 ANCHOR_LEFT | ANCHOR_RIGHT,
				 0, 0, 0, KB_NONE) < 0) {
		TEST_FAIL("failed to create layer surface");
		return;
	}

	if (!ls.configured)
		wl_display_roundtrip(display);

	if (ls.configured) {
		TEST_PASS();
	} else {
		TEST_FAIL("no configure event received");
	}

	destroy_layer_surface(&ls);
}

/*
 * Create layer surface on OVERLAY layer.
 */
static void
test_overlay_layer(void)
{
	TEST_START("layer_surface_overlay");

	struct test_layer_surface ls;
	if (create_layer_surface(&ls, LAYER_OVERLAY,
				 ANCHOR_TOP | ANCHOR_BOTTOM |
				 ANCHOR_LEFT | ANCHOR_RIGHT,
				 0, 0, 0, KB_NONE) < 0) {
		TEST_FAIL("failed to create layer surface");
		return;
	}

	if (!ls.configured)
		wl_display_roundtrip(display);

	if (ls.configured) {
		TEST_PASS();
	} else {
		TEST_FAIL("no configure event received");
	}

	destroy_layer_surface(&ls);
}

/*
 * Set exclusive zone (30px panel anchored to top edge).
 */
static void
test_exclusive_zone(void)
{
	TEST_START("exclusive_zone");

	struct test_layer_surface ls;
	if (create_layer_surface(&ls, LAYER_TOP,
				 ANCHOR_TOP | ANCHOR_LEFT | ANCHOR_RIGHT,
				 30, 0, 30, KB_NONE) < 0) {
		TEST_FAIL("failed to create layer surface");
		return;
	}

	if (!ls.configured)
		wl_display_roundtrip(display);

	if (ls.configured) {
		TEST_PASS();
	} else {
		TEST_FAIL("no configure after exclusive zone");
	}

	destroy_layer_surface(&ls);
}

/*
 * Anchor to top edge (top + left + right).
 */
static void
test_anchor_top(void)
{
	TEST_START("anchor_top");

	struct test_layer_surface ls;
	if (create_layer_surface(&ls, LAYER_TOP,
				 ANCHOR_TOP | ANCHOR_LEFT | ANCHOR_RIGHT,
				 0, 0, 40, KB_NONE) < 0) {
		TEST_FAIL("failed to create layer surface");
		return;
	}

	if (!ls.configured)
		wl_display_roundtrip(display);

	if (ls.configured) {
		TEST_PASS();
	} else {
		TEST_FAIL("no configure after anchor top");
	}

	destroy_layer_surface(&ls);
}

/*
 * Anchor to bottom edge (bottom + left + right).
 */
static void
test_anchor_bottom(void)
{
	TEST_START("anchor_bottom");

	struct test_layer_surface ls;
	if (create_layer_surface(&ls, LAYER_TOP,
				 ANCHOR_BOTTOM | ANCHOR_LEFT | ANCHOR_RIGHT,
				 0, 0, 40, KB_NONE) < 0) {
		TEST_FAIL("failed to create layer surface");
		return;
	}

	if (!ls.configured)
		wl_display_roundtrip(display);

	if (ls.configured) {
		TEST_PASS();
	} else {
		TEST_FAIL("no configure after anchor bottom");
	}

	destroy_layer_surface(&ls);
}

/*
 * Anchor to corner (top | left) with explicit size.
 */
static void
test_anchor_corner(void)
{
	TEST_START("anchor_corner_top_left");

	struct test_layer_surface ls;
	if (create_layer_surface(&ls, LAYER_TOP,
				 ANCHOR_TOP | ANCHOR_LEFT,
				 0, 100, 100, KB_NONE) < 0) {
		TEST_FAIL("failed to create layer surface");
		return;
	}

	if (!ls.configured)
		wl_display_roundtrip(display);

	if (ls.configured) {
		TEST_PASS();
	} else {
		TEST_FAIL("no configure after corner anchor");
	}

	destroy_layer_surface(&ls);
}

/*
 * Set explicit size and verify configure returns it.
 */
static void
test_set_size(void)
{
	TEST_START("set_size_explicit");

	struct test_layer_surface ls;
	if (create_layer_surface(&ls, LAYER_TOP,
				 ANCHOR_TOP | ANCHOR_LEFT,
				 0, 200, 150, KB_NONE) < 0) {
		TEST_FAIL("failed to create layer surface");
		return;
	}

	if (!ls.configured)
		wl_display_roundtrip(display);

	if (ls.configured &&
	    ls.configure_width == 200 && ls.configure_height == 150) {
		TEST_PASS();
	} else if (ls.configured) {
		char msg[128];
		snprintf(msg, sizeof(msg),
			 "expected 200x150, got %ux%u",
			 ls.configure_width, ls.configure_height);
		TEST_FAIL(msg);
	} else {
		TEST_FAIL("no configure event");
	}

	destroy_layer_surface(&ls);
}

/*
 * Keyboard interactivity EXCLUSIVE on top layer.
 */
static void
test_keyboard_exclusive(void)
{
	TEST_START("keyboard_exclusive");

	struct test_layer_surface ls;
	if (create_layer_surface(&ls, LAYER_TOP,
				 ANCHOR_TOP | ANCHOR_BOTTOM |
				 ANCHOR_LEFT | ANCHOR_RIGHT,
				 0, 0, 0, KB_EXCLUSIVE) < 0) {
		TEST_FAIL("failed to create layer surface");
		return;
	}

	if (!ls.configured)
		wl_display_roundtrip(display);

	if (ls.configured) {
		attach_buffer_and_commit(&ls);
		TEST_PASS();
	} else {
		TEST_FAIL("no configure after keyboard exclusive");
	}

	destroy_layer_surface(&ls);
}

/*
 * Keyboard interactivity ON_DEMAND on top layer.
 */
static void
test_keyboard_on_demand(void)
{
	TEST_START("keyboard_on_demand");

	struct test_layer_surface ls;
	if (create_layer_surface(&ls, LAYER_TOP,
				 ANCHOR_TOP | ANCHOR_BOTTOM |
				 ANCHOR_LEFT | ANCHOR_RIGHT,
				 0, 0, 0, KB_ON_DEMAND) < 0) {
		TEST_FAIL("failed to create layer surface");
		return;
	}

	if (!ls.configured)
		wl_display_roundtrip(display);

	if (ls.configured) {
		attach_buffer_and_commit(&ls);
		TEST_PASS();
	} else {
		TEST_FAIL("no configure after keyboard on_demand");
	}

	destroy_layer_surface(&ls);
}

/*
 * Set margins on a top-anchored panel.
 */
static void
test_set_margins(void)
{
	TEST_START("set_margins");

	struct test_layer_surface ls;
	memset(&ls, 0, sizeof(ls));

	ls.surface = wl_compositor_create_surface(compositor);
	if (!ls.surface) {
		TEST_FAIL("failed to create surface");
		return;
	}

	ls.layer_surface = zwlr_layer_shell_v1_get_layer_surface(
		layer_shell, ls.surface, NULL, LAYER_TOP, "test-margins");
	if (!ls.layer_surface) {
		wl_surface_destroy(ls.surface);
		TEST_FAIL("failed to create layer surface");
		return;
	}

	zwlr_layer_surface_v1_add_listener(ls.layer_surface,
		&layer_surface_listener, &ls);

	zwlr_layer_surface_v1_set_anchor(ls.layer_surface,
		ANCHOR_TOP | ANCHOR_LEFT | ANCHOR_RIGHT);
	zwlr_layer_surface_v1_set_size(ls.layer_surface, 0, 50);
	zwlr_layer_surface_v1_set_margin(ls.layer_surface, 10, 20, 0, 20);

	wl_surface_commit(ls.surface);
	wl_display_roundtrip(display);

	if (!ls.configured)
		wl_display_roundtrip(display);

	if (ls.configured) {
		TEST_PASS();
	} else {
		TEST_FAIL("no configure after set_margin");
	}

	destroy_layer_surface(&ls);
}

/*
 * Destroy layer surface cleanly, verify no protocol error.
 */
static void
test_destroy_clean(void)
{
	TEST_START("destroy_clean");

	struct test_layer_surface ls;
	if (create_layer_surface(&ls, LAYER_TOP,
				 ANCHOR_TOP | ANCHOR_BOTTOM |
				 ANCHOR_LEFT | ANCHOR_RIGHT,
				 0, 0, 0, KB_NONE) < 0) {
		TEST_FAIL("failed to create layer surface");
		return;
	}

	if (!ls.configured)
		wl_display_roundtrip(display);

	/* Attach buffer to map it */
	attach_buffer_and_commit(&ls);

	/* Destroy and verify no protocol error */
	destroy_layer_surface(&ls);

	if (wl_display_roundtrip(display) >= 0) {
		TEST_PASS();
	} else {
		TEST_FAIL("protocol error after destroy");
	}
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

int
main(void)
{
	printf("test_layer_shell:\n");

	pid_t pid;
	const char *socket;

	int rc = setup(&pid, &socket);
	if (rc == 77) {
		printf("  SKIP: headless backend or protocol unavailable\n");
		return 77;
	}
	if (rc != 0)
		return 1;

	printf("  Connected to compositor on %s\n", socket);

	/* Run all tests with the same compositor instance */
	test_background_layer();
	test_bottom_layer();
	test_top_layer();
	test_overlay_layer();
	test_exclusive_zone();
	test_anchor_top();
	test_anchor_bottom();
	test_anchor_corner();
	test_set_size();
	test_keyboard_exclusive();
	test_keyboard_on_demand();
	test_set_margins();
	test_destroy_clean();

	teardown(pid);

	printf("  Results: %d/%d passed", tests_passed, tests_run);
	if (tests_failed > 0)
		printf(", %d FAILED", tests_failed);
	printf("\n");

	return tests_failed > 0 ? 1 : 0;
}
