/*
 * test_atspi_bridge.c - Unit tests for AT-SPI accessibility bridge stubs
 *
 * Tests the atspi_bridge.c module. Since WM_HAS_ATSPI is not defined
 * in the test build, this verifies the no-op stub behavior:
 *   - create returns NULL
 *   - destroy accepts NULL without crashing
 *   - announce_focus accepts NULL bridge without crashing
 */

#define _GNU_SOURCE

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atspi_bridge.h"

/* --- Stub wm_server (opaque, not dereferenced in stubs) --- */
struct wm_server {
	int dummy;
};

/* Test: create returns NULL when AT-SPI is disabled */
static void
test_create_returns_null(void)
{
	struct wm_server server;
	memset(&server, 0, sizeof(server));

	struct wm_atspi_bridge *bridge = wm_atspi_bridge_create(&server);
	assert(bridge == NULL);
	printf("  PASS: test_create_returns_null\n");
}

/* Test: destroy handles NULL gracefully */
static void
test_destroy_null(void)
{
	wm_atspi_bridge_destroy(NULL);
	printf("  PASS: test_destroy_null\n");
}

/* Test: announce_focus handles NULL bridge gracefully */
static void
test_announce_null_bridge(void)
{
	wm_atspi_announce_focus(NULL, "window", "Test Window");
	printf("  PASS: test_announce_null_bridge\n");
}

/* Test: announce_focus handles NULL role and name */
static void
test_announce_null_args(void)
{
	wm_atspi_announce_focus(NULL, NULL, NULL);
	printf("  PASS: test_announce_null_args\n");
}

int
main(void)
{
	printf("test_atspi_bridge:\n");

	test_create_returns_null();
	test_destroy_null();
	test_announce_null_bridge();
	test_announce_null_args();

	printf("All atspi_bridge tests passed.\n");
	return 0;
}
