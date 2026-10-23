/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * output_config.h - Apply session.output.* settings to an output
 */

#ifndef WM_OUTPUT_CONFIG_H
#define WM_OUTPUT_CONFIG_H

#include <stdbool.h>

struct wlr_output;
struct wlr_output_mode;
struct wlr_output_state;
struct wm_output_rule;

/*
 * The output's mode of this size: the one closest to refresh_mhz, or the
 * fastest when refresh_mhz is 0. NULL if the output has no such mode.
 */
struct wlr_output_mode *wm_output_config_pick_mode(struct wlr_output *output,
	int width, int height, int refresh_mhz);

/*
 * Add the rule's settings that differ from the output's current state to
 * `state` (a disabled output gets only enabled=false). Returns true if
 * the state has anything to commit.
 */
bool wm_output_config_build(struct wlr_output *output,
	const struct wm_output_rule *rule, struct wlr_output_state *state);

#endif /* WM_OUTPUT_CONFIG_H */
