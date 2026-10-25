/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * test_wheel_bindings.c - The mouse wheel as Fluxbox's buttons 4 and 5
 *
 * A virtual pointer scrolls over the desktop; "OnDesktop Mouse4
 * :NextWorkspace" and "Mouse5 :PrevWorkspace" switch workspaces, which
 * ext-workspace-v1 reports. Touchpad-style scrolling (no wheel steps)
 * is not a button press. Exits 77 (meson skip) without a headless backend.
 */

#define _DEFAULT_SOURCE

#include "integration/harness.h"
#include "ext-workspace-v1-client-protocol.h"
#include "wlr-virtual-pointer-unstable-v1-client-protocol.h"

#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int tests_run, tests_passed, tests_failed;

#define TEST_START(name) \
	do { \
		tests_run++; \
		printf("  [%d] %s ... ", tests_run, (name)); \
		fflush(stdout); \
	} while (0)
#define TEST_PASS() do { tests_passed++; printf("PASS\n"); } while (0)
#define TEST_FAIL(reason) \
	do { tests_failed++; printf("FAIL: %s\n", (reason)); } while (0)

#define MAX_WS 8

struct client {
	struct wl_display *display;
	struct wl_seat *seat;
	struct zwlr_virtual_pointer_manager_v1 *vp_manager;
	struct ext_workspace_manager_v1 *ws_manager;
	struct ext_workspace_handle_v1 *ws[MAX_WS];
	uint32_t state[MAX_WS];
	int ws_count;
	int done;
};

static int
ws_index(struct client *c, struct ext_workspace_handle_v1 *h)
{
	for (int i = 0; i < c->ws_count; i++)
		if (c->ws[i] == h)
			return i;
	return -1;
}

static void ws_id(void *d, struct ext_workspace_handle_v1 *h, const char *s)
	{ (void)d; (void)h; (void)s; }
static void ws_name(void *d, struct ext_workspace_handle_v1 *h, const char *s)
	{ (void)d; (void)h; (void)s; }
static void ws_coords(void *d, struct ext_workspace_handle_v1 *h,
	struct wl_array *a) { (void)d; (void)h; (void)a; }
static void ws_caps(void *d, struct ext_workspace_handle_v1 *h, uint32_t c)
	{ (void)d; (void)h; (void)c; }
static void ws_removed(void *d, struct ext_workspace_handle_v1 *h)
	{ (void)d; (void)h; }

static void
ws_state(void *data, struct ext_workspace_handle_v1 *h, uint32_t state)
{
	struct client *c = data;
	int i = ws_index(c, h);
	if (i >= 0)
		c->state[i] = state;
}

static const struct ext_workspace_handle_v1_listener ws_listener = {
	.id = ws_id, .name = ws_name, .coordinates = ws_coords,
	.state = ws_state, .capabilities = ws_caps, .removed = ws_removed,
};

static void
mgr_group(void *d, struct ext_workspace_manager_v1 *m,
	struct ext_workspace_group_handle_v1 *g)
{
	(void)d; (void)m;
	/* Not needed; let it go */
	ext_workspace_group_handle_v1_destroy(g);
}

static void
mgr_workspace(void *data, struct ext_workspace_manager_v1 *m,
	struct ext_workspace_handle_v1 *h)
{
	struct client *c = data;
	(void)m;
	if (c->ws_count < MAX_WS) {
		c->ws[c->ws_count++] = h;
		ext_workspace_handle_v1_add_listener(h, &ws_listener, c);
	}
}

static void
mgr_done(void *data, struct ext_workspace_manager_v1 *m)
{
	struct client *c = data;
	(void)m;
	c->done++;
}

static void mgr_finished(void *d, struct ext_workspace_manager_v1 *m)
	{ (void)d; (void)m; }

static const struct ext_workspace_manager_v1_listener mgr_listener = {
	.workspace_group = mgr_group, .workspace = mgr_workspace,
	.done = mgr_done, .finished = mgr_finished,
};

static void
registry_global(void *data, struct wl_registry *registry, uint32_t name,
	const char *interface, uint32_t version)
{
	struct client *c = data;
	(void)version;
	if (strcmp(interface, wl_seat_interface.name) == 0 && !c->seat) {
		c->seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
	} else if (strcmp(interface,
	    zwlr_virtual_pointer_manager_v1_interface.name) == 0) {
		c->vp_manager = wl_registry_bind(registry, name,
			&zwlr_virtual_pointer_manager_v1_interface, 1);
	} else if (strcmp(interface,
	    ext_workspace_manager_v1_interface.name) == 0) {
		c->ws_manager = wl_registry_bind(registry, name,
			&ext_workspace_manager_v1_interface, 1);
		ext_workspace_manager_v1_add_listener(c->ws_manager,
			&mgr_listener, c);
	}
}

static void registry_remove(void *d, struct wl_registry *r, uint32_t n)
	{ (void)d; (void)r; (void)n; }

static const struct wl_registry_listener registry_listener = {
	.global = registry_global, .global_remove = registry_remove,
};

static int
active(struct client *c)
{
	for (int i = 0; i < c->ws_count; i++)
		if (c->state[i] & EXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE)
			return i;
	return -1;
}

/* Let the compositor act and report (it batches workspace events) */
static void
settle(struct client *c)
{
	for (int i = 0; i < 20; i++) {
		wl_display_roundtrip(c->display);
		usleep(10000);
	}
}

static uint32_t now_ms;

/* One wheel step: Fluxbox's button 4 (up, -1) or 5 (down, +1) */
static void
wheel(struct zwlr_virtual_pointer_v1 *vp, int steps)
{
	now_ms += 20;
	zwlr_virtual_pointer_v1_axis_source(vp,
		WL_POINTER_AXIS_SOURCE_WHEEL);
	zwlr_virtual_pointer_v1_axis_discrete(vp, now_ms,
		WL_POINTER_AXIS_VERTICAL_SCROLL,
		wl_fixed_from_int(15 * steps), steps);
	zwlr_virtual_pointer_v1_frame(vp);
}

/* Touchpad scrolling: no wheel steps */
static void
finger_scroll(struct zwlr_virtual_pointer_v1 *vp, int amount)
{
	now_ms += 20;
	zwlr_virtual_pointer_v1_axis_source(vp,
		WL_POINTER_AXIS_SOURCE_FINGER);
	zwlr_virtual_pointer_v1_axis(vp, now_ms,
		WL_POINTER_AXIS_VERTICAL_SCROLL, wl_fixed_from_int(amount));
	zwlr_virtual_pointer_v1_frame(vp);
}

static void
write_config(void)
{
	/* The harness's config directory is next to its runtime directory */
	char base[1024], path[1200];
	snprintf(base, sizeof(base), "%s", harness_get_runtime_dir());
	char *dir = dirname(base);
	snprintf(path, sizeof(path), "%s/config/fluxland/init", dir);
	FILE *f = fopen(path, "w");
	if (f) {
		fputs("session.screen0.workspaces: 3\n"
			"session.screen0.toolbar.visible: false\n", f);
		fclose(f);
	}
	snprintf(path, sizeof(path), "%s/config/fluxland/keys", dir);
	f = fopen(path, "w");
	if (f) {
		fputs("OnDesktop Mouse4 :NextWorkspace\n"
			"OnDesktop Mouse5 :PrevWorkspace\n", f);
		fclose(f);
	}
}

int
main(void)
{
	printf("wheel binding integration tests:\n");
	harness_setup();
	write_config();
	const char *socket = NULL;
	pid_t pid = harness_start_compositor(&socket);
	if (pid <= 0) {
		harness_cleanup();
		return pid == 0 ? 77 : 1;
	}

	struct client c;
	memset(&c, 0, sizeof(c));
	c.display = wl_display_connect(socket);
	if (!c.display) {
		harness_stop_compositor(pid);
		harness_cleanup();
		return 1;
	}
	struct wl_registry *registry = wl_display_get_registry(c.display);
	wl_registry_add_listener(registry, &registry_listener, &c);
	settle(&c);

	struct zwlr_virtual_pointer_v1 *vp = NULL;
	TEST_START("setup");
	if (c.vp_manager && c.ws_manager && c.ws_count == 3 &&
	    active(&c) == 0) {
		vp = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(
			c.vp_manager, c.seat);
		/* To the middle of the (empty) desktop */
		zwlr_virtual_pointer_v1_motion_absolute(vp, ++now_ms,
			500, 400, 1000, 800);
		zwlr_virtual_pointer_v1_frame(vp);
		settle(&c);
		TEST_PASS();
	} else {
		TEST_FAIL("no virtual pointer, ext-workspace or workspaces");
	}

	TEST_START("wheel_up_is_mouse4");
	if (vp) {
		wheel(vp, -1);
		settle(&c);
		if (active(&c) == 1)
			TEST_PASS();
		else
			TEST_FAIL("NextWorkspace didn't run");
	} else {
		TEST_FAIL("skipped");
	}

	TEST_START("wheel_down_is_mouse5");
	if (vp) {
		wheel(vp, 1);
		settle(&c);
		if (active(&c) == 0)
			TEST_PASS();
		else
			TEST_FAIL("PrevWorkspace didn't run");
	} else {
		TEST_FAIL("skipped");
	}

	TEST_START("two_steps_two_presses");
	if (vp) {
		wheel(vp, -2);
		settle(&c);
		if (active(&c) == 2)
			TEST_PASS();
		else
			TEST_FAIL("expected two NextWorkspace");
	} else {
		TEST_FAIL("skipped");
	}

	TEST_START("touchpad_scroll_is_not_a_button");
	if (vp) {
		finger_scroll(vp, -50);
		settle(&c);
		if (active(&c) == 2)
			TEST_PASS();
		else
			TEST_FAIL("finger scrolling switched workspaces");
	} else {
		TEST_FAIL("skipped");
	}

	if (vp)
		zwlr_virtual_pointer_v1_destroy(vp);
	for (int i = 0; i < c.ws_count; i++)
		ext_workspace_handle_v1_destroy(c.ws[i]);
	if (c.ws_manager) {
		ext_workspace_manager_v1_stop(c.ws_manager);
		wl_display_roundtrip(c.display);
		ext_workspace_manager_v1_destroy(c.ws_manager);
	}
	if (c.vp_manager)
		zwlr_virtual_pointer_manager_v1_destroy(c.vp_manager);
	if (c.seat)
		wl_seat_destroy(c.seat);
	wl_registry_destroy(registry);
	wl_display_disconnect(c.display);
	harness_stop_compositor(pid);
	harness_cleanup();

	printf("%d/%d passed\n", tests_passed, tests_run);
	return tests_failed ? 1 : 0;
}
