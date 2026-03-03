/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * test_screen_cast.c - Screen cast module unit tests
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/* Force the non-PipeWire path for stub testing */
#undef WM_HAS_PIPEWIRE

struct wm_server;

#include "../src/screen_cast.c"

static void
test_init_without_pipewire(void)
{
	struct wm_screen_cast *sc = wm_screen_cast_init(NULL);
	assert(sc == NULL);
	printf("  PASS: init_without_pipewire\n");
}

static void
test_destroy_null(void)
{
	wm_screen_cast_destroy(NULL);
	printf("  PASS: destroy_null\n");
}

static void
test_start_null(void)
{
	bool ok = wm_screen_cast_start(NULL, NULL);
	assert(!ok);
	printf("  PASS: start_null\n");
}

static void
test_stop_null(void)
{
	wm_screen_cast_stop(NULL);
	printf("  PASS: stop_null\n");
}

static void
test_get_state_null(void)
{
	enum wm_screen_cast_state state = wm_screen_cast_get_state(NULL);
	assert(state == WM_SCREEN_CAST_STOPPED);
	printf("  PASS: get_state_null\n");
}

static void
test_get_node_id_null(void)
{
	uint32_t id = wm_screen_cast_get_node_id(NULL);
	assert(id == 0);
	printf("  PASS: get_node_id_null\n");
}

static void
test_state_enum_values(void)
{
	assert(WM_SCREEN_CAST_STOPPED == 0);
	assert(WM_SCREEN_CAST_STARTING == 1);
	assert(WM_SCREEN_CAST_STREAMING == 2);
	assert(WM_SCREEN_CAST_ERROR == 3);
	printf("  PASS: state_enum_values\n");
}

int
main(void)
{
	printf("test_screen_cast:\n");
	test_init_without_pipewire();
	test_destroy_null();
	test_start_null();
	test_stop_null();
	test_get_state_null();
	test_get_node_id_null();
	test_state_enum_values();
	printf("All screen cast tests passed.\n");
	return 0;
}
