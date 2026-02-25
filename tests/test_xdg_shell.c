/*
 * test_xdg_shell.c - XDG shell protocol integration tests
 *
 * Each test forks a headless compositor, connects as a Wayland client,
 * exercises xdg_shell functionality, and verifies expected behavior.
 * Exits 77 (meson skip) if the headless backend is unavailable.
 */

#define _POSIX_C_SOURCE 200809L

#include "integration/harness.h"

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

/*
 * Common pattern: start compositor, connect client.
 * Returns 0 on success, 77 if headless unavailable, 1 on error.
 */
static int
setup(pid_t *pid, struct test_client *client, const char **socket)
{
	harness_setup();

	*pid = harness_start_compositor(socket);
	if (*pid <= 0) {
		harness_cleanup();
		return (*pid == 0) ? 77 : 1;
	}

	if (harness_connect(client, *socket) < 0) {
		harness_stop_compositor(*pid);
		harness_cleanup();
		return 1;
	}

	return 0;
}

static void
teardown(pid_t pid, struct test_client *client)
{
	harness_disconnect(client);
	harness_stop_compositor(pid);
	harness_cleanup();
}

/* ------------------------------------------------------------------ */
/* Tests                                                               */
/* ------------------------------------------------------------------ */

/*
 * test_xdg_shell_create_window:
 * Create a toplevel and verify that a configure event is received.
 */
static int
test_create_window(pid_t pid, struct test_client *client)
{
	TEST_START("xdg_shell_create_window");

	struct test_toplevel tl;
	if (harness_create_toplevel(client, &tl, 640, 480) < 0) {
		TEST_FAIL("failed to create toplevel");
		return -1;
	}

	/* The harness does a roundtrip, so we should have a configure */
	if (!tl.configured) {
		/* Try one more roundtrip */
		harness_roundtrip(client);
	}

	if (tl.configured) {
		TEST_PASS();
	} else {
		TEST_FAIL("no configure event received");
	}

	harness_destroy_toplevel(client, &tl);
	return 0;
}

/*
 * test_xdg_shell_set_title:
 * Set a title on the toplevel and verify no protocol errors.
 */
static int
test_set_title(pid_t pid, struct test_client *client)
{
	TEST_START("xdg_shell_set_title");

	struct test_toplevel tl;
	if (harness_create_toplevel(client, &tl, 640, 480) < 0) {
		TEST_FAIL("failed to create toplevel");
		return -1;
	}

	xdg_toplevel_set_title(tl.xdg_toplevel, "Test Window Title");
	if (harness_roundtrip(client) < 0) {
		TEST_FAIL("roundtrip failed after set_title");
		harness_destroy_toplevel(client, &tl);
		return -1;
	}

	/* Also test with an empty title and a long title */
	xdg_toplevel_set_title(tl.xdg_toplevel, "");
	harness_roundtrip(client);

	char long_title[512];
	memset(long_title, 'A', sizeof(long_title) - 1);
	long_title[sizeof(long_title) - 1] = '\0';
	xdg_toplevel_set_title(tl.xdg_toplevel, long_title);

	if (harness_roundtrip(client) < 0) {
		TEST_FAIL("roundtrip failed after long title");
		harness_destroy_toplevel(client, &tl);
		return -1;
	}

	TEST_PASS();
	harness_destroy_toplevel(client, &tl);
	return 0;
}

/*
 * test_xdg_shell_maximize:
 * Request maximize, verify configure event includes maximized state.
 */
static int
test_maximize(pid_t pid, struct test_client *client)
{
	TEST_START("xdg_shell_maximize");

	struct test_toplevel tl;
	if (harness_create_toplevel(client, &tl, 640, 480) < 0) {
		TEST_FAIL("failed to create toplevel");
		return -1;
	}

	/* Request maximize */
	xdg_toplevel_set_maximized(tl.xdg_toplevel);
	wl_surface_commit(tl.surface);

	/* Reset configure state and wait for new configure */
	tl.configured = false;
	harness_roundtrip(client);

	/* May need another roundtrip for the compositor to process */
	if (!tl.configured)
		harness_roundtrip(client);

	if (tl.configured && tl.maximized) {
		TEST_PASS();
	} else if (tl.configured && !tl.maximized) {
		/* Some compositors may not honor maximize immediately;
		 * for our compositor it should work. */
		TEST_FAIL("configured but not maximized");
	} else {
		TEST_FAIL("no configure event after maximize request");
	}

	harness_destroy_toplevel(client, &tl);
	return 0;
}

/*
 * test_xdg_shell_fullscreen:
 * Request fullscreen, verify configure event includes fullscreen state.
 */
static int
test_fullscreen(pid_t pid, struct test_client *client)
{
	TEST_START("xdg_shell_fullscreen");

	struct test_toplevel tl;
	if (harness_create_toplevel(client, &tl, 640, 480) < 0) {
		TEST_FAIL("failed to create toplevel");
		return -1;
	}

	/* Request fullscreen */
	xdg_toplevel_set_fullscreen(tl.xdg_toplevel, NULL);
	wl_surface_commit(tl.surface);

	tl.configured = false;
	harness_roundtrip(client);

	if (!tl.configured)
		harness_roundtrip(client);

	if (tl.configured && tl.fullscreen) {
		TEST_PASS();
	} else if (tl.configured && !tl.fullscreen) {
		TEST_FAIL("configured but not fullscreen");
	} else {
		TEST_FAIL("no configure event after fullscreen request");
	}

	harness_destroy_toplevel(client, &tl);
	return 0;
}

/*
 * test_multiple_windows:
 * Create two toplevels and verify both receive configure events.
 */
static int
test_multiple_windows(pid_t pid, struct test_client *client)
{
	TEST_START("multiple_windows");

	struct test_toplevel tl1, tl2;

	if (harness_create_toplevel(client, &tl1, 640, 480) < 0) {
		TEST_FAIL("failed to create first toplevel");
		return -1;
	}

	if (harness_create_toplevel(client, &tl2, 800, 600) < 0) {
		TEST_FAIL("failed to create second toplevel");
		harness_destroy_toplevel(client, &tl1);
		return -1;
	}

	/* Roundtrip to ensure both are configured */
	harness_roundtrip(client);

	if (tl1.configured && tl2.configured) {
		TEST_PASS();
	} else {
		char msg[128];
		snprintf(msg, sizeof(msg),
			 "tl1.configured=%d tl2.configured=%d",
			 tl1.configured, tl2.configured);
		TEST_FAIL(msg);
	}

	harness_destroy_toplevel(client, &tl2);
	harness_destroy_toplevel(client, &tl1);
	return 0;
}

/*
 * test_xdg_shell_set_app_id:
 * Set an app_id on the toplevel and verify no protocol errors.
 */
static int
test_set_app_id(pid_t pid, struct test_client *client)
{
	TEST_START("xdg_shell_set_app_id");

	struct test_toplevel tl;
	if (harness_create_toplevel(client, &tl, 640, 480) < 0) {
		TEST_FAIL("failed to create toplevel");
		return -1;
	}

	xdg_toplevel_set_app_id(tl.xdg_toplevel, "org.fluxland.test");
	if (harness_roundtrip(client) < 0) {
		TEST_FAIL("roundtrip failed after set_app_id");
		harness_destroy_toplevel(client, &tl);
		return -1;
	}

	TEST_PASS();
	harness_destroy_toplevel(client, &tl);
	return 0;
}

/*
 * test_xdg_shell_unmaximize:
 * Maximize then unmaximize, verify the maximized state is cleared.
 */
static int
test_unmaximize(pid_t pid, struct test_client *client)
{
	TEST_START("xdg_shell_unmaximize");

	struct test_toplevel tl;
	if (harness_create_toplevel(client, &tl, 640, 480) < 0) {
		TEST_FAIL("failed to create toplevel");
		return -1;
	}

	/* Maximize first */
	xdg_toplevel_set_maximized(tl.xdg_toplevel);
	wl_surface_commit(tl.surface);
	tl.configured = false;
	harness_roundtrip(client);
	if (!tl.configured)
		harness_roundtrip(client);

	/* Unmaximize */
	xdg_toplevel_unset_maximized(tl.xdg_toplevel);
	wl_surface_commit(tl.surface);
	tl.configured = false;
	harness_roundtrip(client);
	if (!tl.configured)
		harness_roundtrip(client);

	if (tl.configured && !tl.maximized) {
		TEST_PASS();
	} else if (tl.configured && tl.maximized) {
		TEST_FAIL("still maximized after unset_maximized");
	} else {
		TEST_FAIL("no configure after unset_maximized");
	}

	harness_destroy_toplevel(client, &tl);
	return 0;
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

int
main(void)
{
	printf("test_xdg_shell:\n");

	pid_t pid;
	struct test_client client;
	const char *socket;

	int rc = setup(&pid, &client, &socket);
	if (rc == 77) {
		printf("  SKIP: headless backend unavailable\n");
		return 77;
	}
	if (rc != 0)
		return 1;

	printf("  Connected to compositor on %s\n", socket);

	/* Run all tests with the same compositor instance */
	test_create_window(pid, &client);
	test_set_title(pid, &client);
	test_set_app_id(pid, &client);
	test_maximize(pid, &client);
	test_fullscreen(pid, &client);
	test_unmaximize(pid, &client);
	test_multiple_windows(pid, &client);

	teardown(pid, &client);

	printf("  Results: %d/%d passed", tests_passed, tests_run);
	if (tests_failed > 0)
		printf(", %d FAILED", tests_failed);
	printf("\n");

	return tests_failed > 0 ? 1 : 0;
}
