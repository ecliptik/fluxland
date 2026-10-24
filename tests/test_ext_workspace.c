/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * test_ext_workspace.c - ext-workspace-v1 integration test
 *
 * What a panel's workspace module sees and does: the workspaces with
 * their names and which one is active; switching to another; adding one.
 * Exits 77 (meson skip) if the headless backend is unavailable.
 */

#define _DEFAULT_SOURCE

#include "integration/harness.h"
#include "ext-workspace-v1-client-protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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

#define MAX_WS 16

struct ws {
	struct ext_workspace_handle_v1 *handle;
	char name[64];
	uint32_t state;
	uint32_t caps;
	bool removed;
};

struct ws_client {
	struct wl_display *display;
	struct wl_registry *registry;
	struct ext_workspace_manager_v1 *manager;
	struct ext_workspace_group_handle_v1 *group;
	uint32_t group_caps;
	struct ws ws[MAX_WS];
	int count;
	int done_count;
};

static struct ws *
ws_of(struct ws_client *c, struct ext_workspace_handle_v1 *h)
{
	for (int i = 0; i < c->count; i++) {
		if (c->ws[i].handle == h)
			return &c->ws[i];
	}
	return NULL;
}

/* --- workspace handle events --- */

static void
ws_id(void *data, struct ext_workspace_handle_v1 *h, const char *id)
{
	(void)data; (void)h; (void)id;
}

static void
ws_name(void *data, struct ext_workspace_handle_v1 *h, const char *name)
{
	struct ws *w = ws_of(data, h);
	if (w)
		snprintf(w->name, sizeof(w->name), "%s", name);
}

static void
ws_coordinates(void *data, struct ext_workspace_handle_v1 *h,
	struct wl_array *coords)
{
	(void)data; (void)h; (void)coords;
}

static void
ws_state(void *data, struct ext_workspace_handle_v1 *h, uint32_t state)
{
	struct ws *w = ws_of(data, h);
	if (w)
		w->state = state;
}

static void
ws_capabilities(void *data, struct ext_workspace_handle_v1 *h, uint32_t caps)
{
	struct ws *w = ws_of(data, h);
	if (w)
		w->caps = caps;
}

static void
ws_removed(void *data, struct ext_workspace_handle_v1 *h)
{
	struct ws *w = ws_of(data, h);
	if (w)
		w->removed = true;
}

static const struct ext_workspace_handle_v1_listener ws_listener = {
	.id = ws_id,
	.name = ws_name,
	.coordinates = ws_coordinates,
	.state = ws_state,
	.capabilities = ws_capabilities,
	.removed = ws_removed,
};

/* --- group events --- */

static void
group_capabilities(void *data, struct ext_workspace_group_handle_v1 *g,
	uint32_t caps)
{
	struct ws_client *c = data;
	(void)g;
	c->group_caps = caps;
}

static void
group_output_enter(void *data, struct ext_workspace_group_handle_v1 *g,
	struct wl_output *o)
{
	(void)data; (void)g; (void)o;
}

static void
group_output_leave(void *data, struct ext_workspace_group_handle_v1 *g,
	struct wl_output *o)
{
	(void)data; (void)g; (void)o;
}

static void
group_workspace_enter(void *data, struct ext_workspace_group_handle_v1 *g,
	struct ext_workspace_handle_v1 *w)
{
	(void)data; (void)g; (void)w;
}

static void
group_workspace_leave(void *data, struct ext_workspace_group_handle_v1 *g,
	struct ext_workspace_handle_v1 *w)
{
	(void)data; (void)g; (void)w;
}

static void
group_removed(void *data, struct ext_workspace_group_handle_v1 *g)
{
	(void)data; (void)g;
}

static const struct ext_workspace_group_handle_v1_listener group_listener = {
	.capabilities = group_capabilities,
	.output_enter = group_output_enter,
	.output_leave = group_output_leave,
	.workspace_enter = group_workspace_enter,
	.workspace_leave = group_workspace_leave,
	.removed = group_removed,
};

/* --- manager events --- */

static void
mgr_workspace_group(void *data, struct ext_workspace_manager_v1 *m,
	struct ext_workspace_group_handle_v1 *g)
{
	struct ws_client *c = data;
	(void)m;
	c->group = g;
	ext_workspace_group_handle_v1_add_listener(g, &group_listener, c);
}

static void
mgr_workspace(void *data, struct ext_workspace_manager_v1 *m,
	struct ext_workspace_handle_v1 *h)
{
	struct ws_client *c = data;
	(void)m;
	if (c->count >= MAX_WS)
		return;
	c->ws[c->count++].handle = h;
	ext_workspace_handle_v1_add_listener(h, &ws_listener, c);
}

static void
mgr_done(void *data, struct ext_workspace_manager_v1 *m)
{
	struct ws_client *c = data;
	(void)m;
	c->done_count++;
}

static void
mgr_finished(void *data, struct ext_workspace_manager_v1 *m)
{
	(void)data; (void)m;
}

static const struct ext_workspace_manager_v1_listener mgr_listener = {
	.workspace_group = mgr_workspace_group,
	.workspace = mgr_workspace,
	.done = mgr_done,
	.finished = mgr_finished,
};

static void
registry_global(void *data, struct wl_registry *registry, uint32_t name,
	const char *interface, uint32_t version)
{
	struct ws_client *c = data;
	(void)version;
	if (strcmp(interface, ext_workspace_manager_v1_interface.name) == 0) {
		c->manager = wl_registry_bind(registry, name,
			&ext_workspace_manager_v1_interface, 1);
		ext_workspace_manager_v1_add_listener(c->manager,
			&mgr_listener, c);
	}
}

static void
registry_global_remove(void *data, struct wl_registry *r, uint32_t name)
{
	(void)data; (void)r; (void)name;
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove,
};

static int
live_count(struct ws_client *c)
{
	int n = 0;
	for (int i = 0; i < c->count; i++)
		n += !c->ws[i].removed;
	return n;
}

static int
active_index(struct ws_client *c)
{
	for (int i = 0; i < c->count; i++) {
		if (!c->ws[i].removed &&
		    (c->ws[i].state & EXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE))
			return i;
	}
	return -1;
}

/* Roundtrip until a new done event (the compositor batches in idle) */
static void
wait_done(struct ws_client *c)
{
	int before = c->done_count;
	for (int i = 0; i < 50 && c->done_count == before; i++) {
		wl_display_roundtrip(c->display);
		usleep(10000);
	}
}

int
main(void)
{
	printf("ext-workspace integration tests:\n");
	harness_setup();
	const char *socket = NULL;
	pid_t pid = harness_start_compositor(&socket);
	if (pid <= 0) {
		harness_cleanup();
		return pid == 0 ? 77 : 1;
	}

	struct ws_client c;
	memset(&c, 0, sizeof(c));
	c.display = wl_display_connect(socket);
	if (!c.display) {
		harness_stop_compositor(pid);
		harness_cleanup();
		return 1;
	}
	c.registry = wl_display_get_registry(c.display);
	wl_registry_add_listener(c.registry, &registry_listener, &c);
	wl_display_roundtrip(c.display);
	wl_display_roundtrip(c.display);

	TEST_START("workspace_listed");
	if (c.manager && c.group && live_count(&c) == 1 &&
	    c.ws[0].name[0] && active_index(&c) == 0 &&
	    (c.group_caps &
	     EXT_WORKSPACE_GROUP_HANDLE_V1_GROUP_CAPABILITIES_CREATE_WORKSPACE))
		TEST_PASS();
	else
		TEST_FAIL("no group, workspace or active state");

	TEST_START("create_workspaces");
	if (c.group) {
		ext_workspace_group_handle_v1_create_workspace(c.group, "Two");
		ext_workspace_group_handle_v1_create_workspace(c.group, "Three");
		ext_workspace_manager_v1_commit(c.manager);
		wait_done(&c);
		if (live_count(&c) == 3 && strcmp(c.ws[1].name, "Two") == 0 &&
		    strcmp(c.ws[2].name, "Three") == 0)
			TEST_PASS();
		else
			TEST_FAIL("workspaces not created with their names");
	} else {
		TEST_FAIL("skipped");
	}

	TEST_START("capabilities");
	if (c.count == 3 &&
	    (c.ws[1].caps &
	     EXT_WORKSPACE_HANDLE_V1_WORKSPACE_CAPABILITIES_REMOVE) &&
	    (c.ws[1].caps &
	     EXT_WORKSPACE_HANDLE_V1_WORKSPACE_CAPABILITIES_ACTIVATE))
		TEST_PASS();
	else
		TEST_FAIL("wrong capabilities");

	TEST_START("activate_switches_workspace");
	if (c.count == 3) {
		ext_workspace_handle_v1_activate(c.ws[1].handle);
		ext_workspace_manager_v1_commit(c.manager);
		wait_done(&c);
		if (active_index(&c) == 1)
			TEST_PASS();
		else
			TEST_FAIL("workspace Two not active");
	} else {
		TEST_FAIL("skipped");
	}

	TEST_START("remove_middle_workspace");
	if (c.count == 3) {
		/* "Two" (shown) goes: "Three" moves into its place, shown */
		ext_workspace_handle_v1_remove(c.ws[1].handle);
		ext_workspace_manager_v1_commit(c.manager);
		wait_done(&c);
		if (live_count(&c) == 2 && c.ws[2].removed &&
		    strcmp(c.ws[1].name, "Three") == 0 && active_index(&c) == 0)
			TEST_PASS();
		else
			TEST_FAIL("wrong workspace removed");
	} else {
		TEST_FAIL("skipped");
	}

	TEST_START("last_workspace_stays");
	if (c.count == 3) {
		ext_workspace_handle_v1_remove(c.ws[1].handle);
		ext_workspace_handle_v1_remove(c.ws[0].handle);
		ext_workspace_manager_v1_commit(c.manager);
		wait_done(&c);
		if (live_count(&c) == 1 && !c.ws[0].removed)
			TEST_PASS();
		else
			TEST_FAIL("the only workspace was removed");
	} else {
		TEST_FAIL("skipped");
	}

	for (int i = 0; i < c.count; i++)
		ext_workspace_handle_v1_destroy(c.ws[i].handle);
	if (c.group)
		ext_workspace_group_handle_v1_destroy(c.group);
	if (c.manager)
		ext_workspace_manager_v1_stop(c.manager);
	wl_display_roundtrip(c.display);
	if (c.manager)
		ext_workspace_manager_v1_destroy(c.manager);
	wl_registry_destroy(c.registry);
	wl_display_disconnect(c.display);
	harness_stop_compositor(pid);
	harness_cleanup();

	printf("%d/%d passed\n", tests_passed, tests_run);
	return tests_failed ? 1 : 0;
}
