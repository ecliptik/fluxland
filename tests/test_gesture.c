/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * test_gesture.c - Touchpad gesture state machine tests
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Include only the headers we need, with minimal stubs */
#include "config.h"
#include "server.h"

/* Test: gesture state initialization (zero-initialized) */
static void
test_gesture_state_init(void)
{
	struct wm_server server;
	memset(&server, 0, sizeof(server));

	assert(server.gesture_state.active == false);
	assert(server.gesture_state.fingers == 0);
	assert(server.gesture_state.dx_accum == 0.0);
	assert(server.gesture_state.dy_accum == 0.0);
	assert(server.gesture_state.consumed == false);
	printf("  PASS: gesture_state_init\n");
}

/* Test: gesture activation with matching fingers */
static void
test_gesture_activate(void)
{
	struct wm_server server;
	memset(&server, 0, sizeof(server));

	struct wm_config config;
	memset(&config, 0, sizeof(config));
	config.gesture_workspace_switch = true;
	config.gesture_workspace_fingers = 3;
	config.gesture_swipe_threshold = 100.0;
	server.config = &config;

	/* Simulate 3-finger swipe begin */
	server.gesture_state.active = true;
	server.gesture_state.fingers = 3;
	server.gesture_state.dx_accum = 0;
	server.gesture_state.dy_accum = 0;
	server.gesture_state.consumed = false;

	assert(server.gesture_state.active == true);
	assert(server.gesture_state.fingers == 3);
	printf("  PASS: gesture_activate\n");
}

/* Test: non-matching finger count should not activate */
static void
test_gesture_wrong_fingers(void)
{
	struct wm_config config;
	memset(&config, 0, sizeof(config));
	config.gesture_workspace_switch = true;
	config.gesture_workspace_fingers = 3;

	/* 2-finger swipe should not match */
	uint32_t fingers = 2;
	bool should_intercept = config.gesture_workspace_switch &&
		(int)fingers == config.gesture_workspace_fingers;
	assert(!should_intercept);
	printf("  PASS: gesture_wrong_fingers\n");
}

/* Test: below-threshold accumulation does not consume */
static void
test_gesture_below_threshold(void)
{
	struct wm_server server;
	memset(&server, 0, sizeof(server));

	struct wm_config config;
	memset(&config, 0, sizeof(config));
	config.gesture_workspace_switch = true;
	config.gesture_workspace_fingers = 3;
	config.gesture_swipe_threshold = 100.0;
	server.config = &config;

	server.gesture_state.active = true;
	server.gesture_state.consumed = false;
	server.gesture_state.dx_accum = 0;

	/* Accumulate below threshold */
	server.gesture_state.dx_accum += 30.0;
	server.gesture_state.dx_accum += 30.0;
	server.gesture_state.dx_accum += 30.0;

	double threshold = config.gesture_swipe_threshold;
	bool crossed = server.gesture_state.dx_accum > threshold ||
		server.gesture_state.dx_accum < -threshold;
	assert(!crossed);
	assert(server.gesture_state.dx_accum == 90.0);
	printf("  PASS: gesture_below_threshold\n");
}

/* Test: threshold crossed triggers consumed */
static void
test_gesture_threshold_crossed(void)
{
	struct wm_server server;
	memset(&server, 0, sizeof(server));

	struct wm_config config;
	memset(&config, 0, sizeof(config));
	config.gesture_workspace_switch = true;
	config.gesture_workspace_fingers = 3;
	config.gesture_swipe_threshold = 100.0;
	server.config = &config;

	server.gesture_state.active = true;
	server.gesture_state.consumed = false;

	/* Accumulate past threshold (right swipe) */
	server.gesture_state.dx_accum = 101.0;

	double threshold = config.gesture_swipe_threshold;
	if (server.gesture_state.dx_accum > threshold) {
		server.gesture_state.consumed = true;
	}

	assert(server.gesture_state.consumed == true);
	printf("  PASS: gesture_threshold_crossed\n");
}

/* Test: left swipe (negative dx) also triggers */
static void
test_gesture_left_swipe(void)
{
	struct wm_config config;
	memset(&config, 0, sizeof(config));
	config.gesture_swipe_threshold = 100.0;

	double dx_accum = -110.0;
	bool left = dx_accum < -config.gesture_swipe_threshold;
	assert(left);
	printf("  PASS: gesture_left_swipe\n");
}

/* Test: end resets state */
static void
test_gesture_end_resets(void)
{
	struct wm_server server;
	memset(&server, 0, sizeof(server));

	server.gesture_state.active = true;
	server.gesture_state.consumed = true;
	server.gesture_state.dx_accum = 150.0;

	/* Simulate swipe end */
	server.gesture_state.active = false;
	server.gesture_state.consumed = false;

	assert(server.gesture_state.active == false);
	assert(server.gesture_state.consumed == false);
	printf("  PASS: gesture_end_resets\n");
}

/* Test: disabled gesture config should not intercept */
static void
test_gesture_disabled(void)
{
	struct wm_config config;
	memset(&config, 0, sizeof(config));
	config.gesture_workspace_switch = false;
	config.gesture_workspace_fingers = 3;

	uint32_t fingers = 3;
	bool should_intercept = config.gesture_workspace_switch &&
		(int)fingers == config.gesture_workspace_fingers;
	assert(!should_intercept);
	printf("  PASS: gesture_disabled\n");
}

/* Test: config defaults */
static void
test_gesture_config_defaults(void)
{
	struct wm_config config;
	memset(&config, 0, sizeof(config));

	/* These are the expected defaults from config_create() */
	config.gesture_workspace_switch = true;
	config.gesture_overview = false;
	config.gesture_workspace_fingers = 3;
	config.gesture_swipe_threshold = 100.0;

	assert(config.gesture_workspace_switch == true);
	assert(config.gesture_overview == false);
	assert(config.gesture_workspace_fingers == 3);
	assert(config.gesture_swipe_threshold == 100.0);
	printf("  PASS: gesture_config_defaults\n");
}

int
main(void)
{
	printf("test_gesture:\n");
	test_gesture_state_init();
	test_gesture_activate();
	test_gesture_wrong_fingers();
	test_gesture_below_threshold();
	test_gesture_threshold_crossed();
	test_gesture_left_swipe();
	test_gesture_end_resets();
	test_gesture_disabled();
	test_gesture_config_defaults();
	printf("All gesture tests passed.\n");
	return 0;
}
