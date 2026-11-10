/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * output_buffer.c - A frame for outputs a configuration lights up
 */

#define _POSIX_C_SOURCE 200809L
#include <wlr/render/pass.h>
#include <wlr/render/swapchain.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_output.h>
#include <wlr/util/log.h>

#include "output_buffer.h"

/* Would wlr_output_commit_state() attach a buffer to this state? */
static bool
needs_buffer(const struct wlr_output *output,
	const struct wlr_output_state *state)
{
	/* Without a renderer the compositor attaches its own buffers */
	if ((state->committed & WLR_OUTPUT_STATE_BUFFER) || !output->renderer)
		return false;

	bool enabled = output->enabled;
	if (state->committed & WLR_OUTPUT_STATE_ENABLED)
		enabled = state->enabled;
	if (!enabled)
		return false;

	/* A configuration lists every output: only what changes matters,
	 * or the ones left as they are would flash black */
	if ((state->committed & WLR_OUTPUT_STATE_ENABLED) && state->enabled &&
	    !output->enabled)
		return true;
	if (state->committed & WLR_OUTPUT_STATE_MODE) {
		if (state->mode_type == WLR_OUTPUT_STATE_MODE_FIXED)
			return state->mode != output->current_mode;
		return state->custom_mode.width != output->width ||
			state->custom_mode.height != output->height ||
			(state->custom_mode.refresh &&
			 state->custom_mode.refresh != output->refresh);
	}
	return false;
}

static bool
add_black_frame(struct wlr_output *output, struct wlr_output_state *state)
{
	if (!wlr_output_configure_primary_swapchain(output, state,
	    &output->swapchain)) {
		wlr_log(WLR_ERROR, "output %s: cannot configure its swapchain",
			output->name);
		return false;
	}
	struct wlr_buffer *buffer = wlr_swapchain_acquire(output->swapchain);
	if (!buffer) {
		wlr_log(WLR_ERROR, "output %s: cannot allocate a buffer",
			output->name);
		return false;
	}
	struct wlr_render_pass *pass = wlr_renderer_begin_buffer_pass(
		output->renderer, buffer, NULL);
	if (!pass) {
		wlr_log(WLR_ERROR, "output %s: cannot render a buffer",
			output->name);
		wlr_buffer_unlock(buffer);
		return false;
	}
	wlr_render_pass_add_rect(pass, &(struct wlr_render_rect_options){
		.box = { .width = buffer->width, .height = buffer->height },
		.color = { 0, 0, 0, 1 },
	});
	if (!wlr_render_pass_submit(pass)) {
		wlr_log(WLR_ERROR, "output %s: cannot render a buffer",
			output->name);
		wlr_buffer_unlock(buffer);
		return false;
	}
	wlr_output_state_set_buffer(state, buffer);
	wlr_buffer_unlock(buffer);
	return true;
}

bool
wm_output_states_add_buffers(struct wlr_backend_output_state *states,
	size_t states_len)
{
	for (size_t i = 0; i < states_len; i++) {
		if (needs_buffer(states[i].output, &states[i].base) &&
		    !add_black_frame(states[i].output, &states[i].base))
			return false;
	}
	return true;
}
