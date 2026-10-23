/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * test_output_config.c - session.output.* rules turned into output state
 *
 * Uses wlroots' real wlr_output_state setters on a hand-built
 * wlr_output (never committed), so the state is exactly what a commit
 * would get.
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <wlr/types/wlr_output.h>

#include "config.h"
#include "output_config.h"

static struct wlr_output_mode modes[4];

static void
fake_output(struct wlr_output *o)
{
	memset(o, 0, sizeof(*o));
	wl_list_init(&o->modes);
	const int m[4][3] = {
		{1920, 1080, 60000}, {1920, 1080, 143980},
		{1920, 1080, 120000}, {1280, 720, 60000},
	};
	for (int i = 0; i < 4; i++) {
		memset(&modes[i], 0, sizeof(modes[i]));
		modes[i].width = m[i][0];
		modes[i].height = m[i][1];
		modes[i].refresh = m[i][2];
		wl_list_insert(o->modes.prev, &modes[i].link);
	}
	o->enabled = true;
	o->current_mode = &modes[0];
	o->width = 1920;
	o->height = 1080;
	o->refresh = 60000;
	o->scale = 1;
	o->transform = WL_OUTPUT_TRANSFORM_NORMAL;
}

static struct wm_output_rule
rule(void)
{
	struct wm_output_rule r;
	memset(&r, 0, sizeof(r));
	r.enabled = -1;
	r.scale = NAN;
	r.transform = -1;
	r.adaptive_sync = -1;
	return r;
}

static void
test_pick_mode(void)
{
	struct wlr_output o;
	fake_output(&o);
	assert(wm_output_config_pick_mode(&o, 1920, 1080, 0) == &modes[1]);
	assert(wm_output_config_pick_mode(&o, 1920, 1080, 119000) ==
		&modes[2]);
	assert(wm_output_config_pick_mode(&o, 1920, 1080, 60000) == &modes[0]);
	assert(wm_output_config_pick_mode(&o, 1280, 720, 0) == &modes[3]);
	assert(wm_output_config_pick_mode(&o, 800, 600, 0) == NULL);
	printf("  PASS: pick_mode\n");
}

static void
test_build_state(void)
{
	struct wlr_output o;
	fake_output(&o);
	struct wlr_output_state st;

	/* Nothing in the rule, or a rule matching the current state */
	struct wm_output_rule r = rule();
	wlr_output_state_init(&st);
	assert(!wm_output_config_build(&o, &r, &st));
	assert(!wm_output_config_build(&o, NULL, &st));
	r.width = 1920; r.height = 1080; r.refresh_mhz = 60000;
	r.scale = 1;
	assert(!wm_output_config_build(&o, &r, &st));
	wlr_output_state_finish(&st);

	/* Mode, scale, transform and adaptive sync that differ */
	r = rule();
	r.width = 1280; r.height = 720;
	r.scale = 1.5;
	r.transform = WL_OUTPUT_TRANSFORM_90;
	r.adaptive_sync = 1;
	wlr_output_state_init(&st);
	assert(wm_output_config_build(&o, &r, &st));
	assert(st.committed & WLR_OUTPUT_STATE_MODE);
	assert(st.mode == &modes[3]);
	assert((st.committed & WLR_OUTPUT_STATE_SCALE) && st.scale == 1.5f);
	assert((st.committed & WLR_OUTPUT_STATE_TRANSFORM) &&
		st.transform == WL_OUTPUT_TRANSFORM_90);
	assert((st.committed & WLR_OUTPUT_STATE_ADAPTIVE_SYNC_ENABLED) &&
		st.adaptive_sync_enabled);
	wlr_output_state_finish(&st);

	/* A size the monitor doesn't have leaves the mode alone */
	r = rule();
	r.width = 800; r.height = 600;
	wlr_output_state_init(&st);
	assert(!wm_output_config_build(&o, &r, &st));
	wlr_output_state_finish(&st);

	/* Disabled: only that */
	r = rule();
	r.enabled = 0;
	r.scale = 2;
	wlr_output_state_init(&st);
	assert(wm_output_config_build(&o, &r, &st));
	assert(st.committed == WLR_OUTPUT_STATE_ENABLED && !st.enabled);
	wlr_output_state_finish(&st);
	printf("  PASS: build_state\n");
}

static void
test_custom_mode_without_modes(void)
{
	/* Nested and headless outputs list no modes: any size goes */
	struct wlr_output o;
	fake_output(&o);
	wl_list_init(&o.modes);
	struct wm_output_rule r = rule();
	r.width = 1024; r.height = 768; r.refresh_mhz = 75000;
	struct wlr_output_state st;
	wlr_output_state_init(&st);
	assert(wm_output_config_build(&o, &r, &st));
	assert(st.mode_type == WLR_OUTPUT_STATE_MODE_CUSTOM);
	assert(st.custom_mode.width == 1024 && st.custom_mode.height == 768);
	assert(st.custom_mode.refresh == 75000);
	wlr_output_state_finish(&st);
	printf("  PASS: custom_mode_without_modes\n");
}

int
main(void)
{
	printf("output_config tests:\n");
	test_pick_mode();
	test_build_state();
	test_custom_mode_without_modes();
	printf("All output_config tests passed.\n");
	return 0;
}
