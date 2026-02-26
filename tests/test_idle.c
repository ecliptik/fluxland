/*
 * test_idle.c - idle-inhibit-unstable-v1 protocol integration tests
 *
 * Each test forks a headless compositor, connects as a Wayland client,
 * exercises idle inhibit functionality, and verifies expected behavior.
 * Exits 77 (meson skip) if the headless backend is unavailable.
 */

#define _POSIX_C_SOURCE 200809L

#include "integration/harness.h"
#include "idle-inhibit-unstable-v1-client-protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
/* Client state                                                        */
/* ------------------------------------------------------------------ */

struct idle_client {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct zwp_idle_inhibit_manager_v1 *inhibit_mgr;
};

/* ------------------------------------------------------------------ */
/* Registry listener                                                   */
/* ------------------------------------------------------------------ */

static void
registry_global(void *data, struct wl_registry *registry, uint32_t name,
		const char *interface, uint32_t version)
{
	struct idle_client *c = data;

	if (strcmp(interface, "wl_compositor") == 0) {
		c->compositor = wl_registry_bind(registry, name,
			&wl_compositor_interface, version < 6 ? version : 6);
	} else if (strcmp(interface, "zwp_idle_inhibit_manager_v1") == 0) {
		c->inhibit_mgr = wl_registry_bind(registry, name,
			&zwp_idle_inhibit_manager_v1_interface, 1);
	}
}

static void
registry_global_remove(void *data, struct wl_registry *registry,
		       uint32_t name)
{
	(void)data;
	(void)registry;
	(void)name;
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove,
};

/* ------------------------------------------------------------------ */
/* Setup / teardown                                                    */
/* ------------------------------------------------------------------ */

static int
setup(pid_t *pid, struct idle_client *c, const char **socket)
{
	harness_setup();

	*pid = harness_start_compositor(socket);
	if (*pid <= 0) {
		harness_cleanup();
		return (*pid == 0) ? 77 : 1;
	}

	memset(c, 0, sizeof(*c));
	c->display = wl_display_connect(*socket);
	if (!c->display) {
		harness_stop_compositor(*pid);
		harness_cleanup();
		return 1;
	}

	c->registry = wl_display_get_registry(c->display);
	wl_registry_add_listener(c->registry, &registry_listener, c);
	wl_display_roundtrip(c->display);

	if (!c->compositor || !c->inhibit_mgr) {
		fprintf(stderr, "idle: missing globals "
			"(compositor=%p inhibit_mgr=%p) - skip\n",
			(void *)c->compositor, (void *)c->inhibit_mgr);
		wl_display_disconnect(c->display);
		harness_stop_compositor(*pid);
		harness_cleanup();
		return 77;
	}

	return 0;
}

static void
teardown(pid_t pid, struct idle_client *c)
{
	if (c->inhibit_mgr)
		zwp_idle_inhibit_manager_v1_destroy(c->inhibit_mgr);
	if (c->compositor)
		wl_compositor_destroy(c->compositor);
	if (c->registry)
		wl_registry_destroy(c->registry);
	if (c->display)
		wl_display_disconnect(c->display);
	harness_stop_compositor(pid);
	harness_cleanup();
}

/* ------------------------------------------------------------------ */
/* Tests                                                               */
/* ------------------------------------------------------------------ */

/*
 * 1. Create idle inhibitor on a surface, verify roundtrip succeeds.
 */
static void
test_create_inhibitor(struct idle_client *c)
{
	TEST_START("create_inhibitor");

	struct wl_surface *surf =
		wl_compositor_create_surface(c->compositor);
	if (!surf) {
		TEST_FAIL("create surface failed");
		return;
	}

	struct zwp_idle_inhibitor_v1 *inh =
		zwp_idle_inhibit_manager_v1_create_inhibitor(
			c->inhibit_mgr, surf);
	if (!inh) {
		TEST_FAIL("create inhibitor failed");
		wl_surface_destroy(surf);
		return;
	}

	if (wl_display_roundtrip(c->display) >= 0) {
		TEST_PASS();
	} else {
		TEST_FAIL("roundtrip failed");
	}

	zwp_idle_inhibitor_v1_destroy(inh);
	wl_surface_destroy(surf);
	wl_display_roundtrip(c->display);
}

/*
 * 2. Create and destroy idle inhibitor.
 */
static void
test_destroy_inhibitor(struct idle_client *c)
{
	TEST_START("destroy_inhibitor");

	struct wl_surface *surf =
		wl_compositor_create_surface(c->compositor);
	struct zwp_idle_inhibitor_v1 *inh =
		zwp_idle_inhibit_manager_v1_create_inhibitor(
			c->inhibit_mgr, surf);

	if (!surf || !inh) {
		TEST_FAIL("create surface/inhibitor failed");
		if (inh) zwp_idle_inhibitor_v1_destroy(inh);
		if (surf) wl_surface_destroy(surf);
		return;
	}

	wl_display_roundtrip(c->display);

	zwp_idle_inhibitor_v1_destroy(inh);

	if (wl_display_roundtrip(c->display) >= 0) {
		TEST_PASS();
	} else {
		TEST_FAIL("roundtrip failed after destroy");
	}

	wl_surface_destroy(surf);
	wl_display_roundtrip(c->display);
}

/*
 * 3. Create multiple inhibitors on different surfaces.
 */
static void
test_multiple_inhibitors(struct idle_client *c)
{
	TEST_START("multiple_inhibitors");

	struct wl_surface *surf1 =
		wl_compositor_create_surface(c->compositor);
	struct wl_surface *surf2 =
		wl_compositor_create_surface(c->compositor);
	struct wl_surface *surf3 =
		wl_compositor_create_surface(c->compositor);

	if (!surf1 || !surf2 || !surf3) {
		TEST_FAIL("create surfaces failed");
		if (surf1) wl_surface_destroy(surf1);
		if (surf2) wl_surface_destroy(surf2);
		if (surf3) wl_surface_destroy(surf3);
		return;
	}

	struct zwp_idle_inhibitor_v1 *inh1 =
		zwp_idle_inhibit_manager_v1_create_inhibitor(
			c->inhibit_mgr, surf1);
	struct zwp_idle_inhibitor_v1 *inh2 =
		zwp_idle_inhibit_manager_v1_create_inhibitor(
			c->inhibit_mgr, surf2);
	struct zwp_idle_inhibitor_v1 *inh3 =
		zwp_idle_inhibit_manager_v1_create_inhibitor(
			c->inhibit_mgr, surf3);

	if (!inh1 || !inh2 || !inh3) {
		TEST_FAIL("create inhibitors failed");
	} else if (wl_display_roundtrip(c->display) >= 0) {
		TEST_PASS();
	} else {
		TEST_FAIL("roundtrip failed");
	}

	if (inh1) zwp_idle_inhibitor_v1_destroy(inh1);
	if (inh2) zwp_idle_inhibitor_v1_destroy(inh2);
	if (inh3) zwp_idle_inhibitor_v1_destroy(inh3);
	wl_surface_destroy(surf1);
	wl_surface_destroy(surf2);
	wl_surface_destroy(surf3);
	wl_display_roundtrip(c->display);
}

/*
 * 4. Destroy one inhibitor while others remain active.
 */
static void
test_destroy_one_inhibitor(struct idle_client *c)
{
	TEST_START("destroy_one_inhibitor");

	struct wl_surface *surf1 =
		wl_compositor_create_surface(c->compositor);
	struct wl_surface *surf2 =
		wl_compositor_create_surface(c->compositor);

	if (!surf1 || !surf2) {
		TEST_FAIL("create surfaces failed");
		if (surf1) wl_surface_destroy(surf1);
		if (surf2) wl_surface_destroy(surf2);
		return;
	}

	struct zwp_idle_inhibitor_v1 *inh1 =
		zwp_idle_inhibit_manager_v1_create_inhibitor(
			c->inhibit_mgr, surf1);
	struct zwp_idle_inhibitor_v1 *inh2 =
		zwp_idle_inhibit_manager_v1_create_inhibitor(
			c->inhibit_mgr, surf2);

	if (!inh1 || !inh2) {
		TEST_FAIL("create inhibitors failed");
		if (inh1) zwp_idle_inhibitor_v1_destroy(inh1);
		if (inh2) zwp_idle_inhibitor_v1_destroy(inh2);
		wl_surface_destroy(surf1);
		wl_surface_destroy(surf2);
		return;
	}

	wl_display_roundtrip(c->display);

	/* Destroy one inhibitor, keep the other */
	zwp_idle_inhibitor_v1_destroy(inh1);

	if (wl_display_roundtrip(c->display) >= 0) {
		TEST_PASS();
	} else {
		TEST_FAIL("roundtrip failed after partial destroy");
	}

	zwp_idle_inhibitor_v1_destroy(inh2);
	wl_surface_destroy(surf1);
	wl_surface_destroy(surf2);
	wl_display_roundtrip(c->display);
}

/*
 * 5. Destroy surface with active inhibitor (cleanup).
 */
static void
test_destroy_surface_with_inhibitor(struct idle_client *c)
{
	TEST_START("destroy_surface_with_inhibitor");

	struct wl_surface *surf =
		wl_compositor_create_surface(c->compositor);
	if (!surf) {
		TEST_FAIL("create surface failed");
		return;
	}

	struct zwp_idle_inhibitor_v1 *inh =
		zwp_idle_inhibit_manager_v1_create_inhibitor(
			c->inhibit_mgr, surf);
	if (!inh) {
		TEST_FAIL("create inhibitor failed");
		wl_surface_destroy(surf);
		return;
	}

	wl_display_roundtrip(c->display);

	/*
	 * Destroy the surface while inhibitor is active.
	 * The compositor should clean up the inhibitor automatically.
	 */
	wl_surface_destroy(surf);

	if (wl_display_roundtrip(c->display) >= 0) {
		TEST_PASS();
	} else {
		TEST_FAIL("roundtrip failed after surface destroy");
	}

	/* Destroy the inhibitor proxy (surface is already gone) */
	zwp_idle_inhibitor_v1_destroy(inh);
	wl_display_roundtrip(c->display);
}

/*
 * 6. Create inhibitor, destroy, create again (reuse).
 */
static void
test_create_destroy_create(struct idle_client *c)
{
	TEST_START("create_destroy_create");

	struct wl_surface *surf =
		wl_compositor_create_surface(c->compositor);
	if (!surf) {
		TEST_FAIL("create surface failed");
		return;
	}

	/* First inhibitor */
	struct zwp_idle_inhibitor_v1 *inh1 =
		zwp_idle_inhibit_manager_v1_create_inhibitor(
			c->inhibit_mgr, surf);
	if (!inh1) {
		TEST_FAIL("create first inhibitor failed");
		wl_surface_destroy(surf);
		return;
	}

	wl_display_roundtrip(c->display);
	zwp_idle_inhibitor_v1_destroy(inh1);
	wl_display_roundtrip(c->display);

	/* Second inhibitor on same surface */
	struct zwp_idle_inhibitor_v1 *inh2 =
		zwp_idle_inhibit_manager_v1_create_inhibitor(
			c->inhibit_mgr, surf);
	if (!inh2) {
		TEST_FAIL("create second inhibitor failed");
		wl_surface_destroy(surf);
		return;
	}

	if (wl_display_roundtrip(c->display) >= 0) {
		TEST_PASS();
	} else {
		TEST_FAIL("roundtrip failed after re-create");
	}

	zwp_idle_inhibitor_v1_destroy(inh2);
	wl_surface_destroy(surf);
	wl_display_roundtrip(c->display);
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

int
main(void)
{
	printf("test_idle:\n");

	pid_t pid;
	struct idle_client client;
	const char *socket;

	int rc = setup(&pid, &client, &socket);
	if (rc == 77) {
		printf("  SKIP: headless backend or protocol unavailable\n");
		return 77;
	}
	if (rc != 0)
		return 1;

	printf("  Connected to compositor on %s\n", socket);

	test_create_inhibitor(&client);
	test_destroy_inhibitor(&client);
	test_multiple_inhibitors(&client);
	test_destroy_one_inhibitor(&client);
	test_destroy_surface_with_inhibitor(&client);
	test_create_destroy_create(&client);

	teardown(pid, &client);

	printf("  Results: %d/%d passed", tests_passed, tests_run);
	if (tests_failed > 0)
		printf(", %d FAILED", tests_failed);
	printf("\n");

	return tests_failed > 0 ? 1 : 0;
}
