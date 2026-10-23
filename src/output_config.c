/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * output_config.c - Apply session.output.* settings to an output
 */

#include <math.h>
#include <stdlib.h>
#include <wlr/types/wlr_output.h>

#include "config.h"
#include "output_config.h"

struct wlr_output_mode *
wm_output_config_pick_mode(struct wlr_output *output, int width, int height,
	int refresh_mhz)
{
	struct wlr_output_mode *best = NULL, *mode;
	wl_list_for_each(mode, &output->modes, link) {
		if (mode->width != width || mode->height != height)
			continue;
		if (!best) {
			best = mode;
		} else if (refresh_mhz > 0) {
			/* Closest to the requested rate */
			if (abs(mode->refresh - refresh_mhz) <
			    abs(best->refresh - refresh_mhz))
				best = mode;
		} else if (mode->refresh > best->refresh) {
			best = mode;
		}
	}
	return best;
}

bool
wm_output_config_build(struct wlr_output *output,
	const struct wm_output_rule *rule, struct wlr_output_state *state)
{
	if (!rule)
		return state->committed != 0;

	if (rule->enabled == 0) {
		if (output->enabled)
			wlr_output_state_set_enabled(state, false);
		/* Nothing else matters for a disabled output */
		return state->committed != 0;
	}
	if (rule->enabled == 1 && !output->enabled)
		wlr_output_state_set_enabled(state, true);

	if (rule->width > 0 && rule->height > 0) {
		struct wlr_output_mode *mode = wm_output_config_pick_mode(output,
			rule->width, rule->height, rule->refresh_mhz);
		if (mode) {
			if (mode != output->current_mode)
				wlr_output_state_set_mode(state, mode);
		} else if (wl_list_empty(&output->modes)) {
			/* Nested and headless outputs take any size */
			if (output->width != rule->width ||
			    output->height != rule->height ||
			    (rule->refresh_mhz > 0 &&
			     output->refresh != rule->refresh_mhz))
				wlr_output_state_set_custom_mode(state,
					rule->width, rule->height,
					rule->refresh_mhz);
		}
		/* else: the monitor has no such mode; keep the current one */
	}

	if (!isnan(rule->scale) && rule->scale != output->scale)
		wlr_output_state_set_scale(state, (float)rule->scale);

	if (rule->transform >= 0 &&
	    (enum wl_output_transform)rule->transform != output->transform)
		wlr_output_state_set_transform(state,
			(enum wl_output_transform)rule->transform);

	if (rule->adaptive_sync >= 0) {
		bool on = output->adaptive_sync_status ==
			WLR_OUTPUT_ADAPTIVE_SYNC_ENABLED;
		if (on != (bool)rule->adaptive_sync)
			wlr_output_state_set_adaptive_sync_enabled(state,
				rule->adaptive_sync);
	}

	return state->committed != 0;
}
