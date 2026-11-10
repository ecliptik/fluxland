/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * output_buffer.h - A frame for outputs a configuration lights up
 *
 * wlr_output_commit_state() attaches an empty buffer when it turns an
 * output on or changes its mode, as the DRM backend must have something to
 * scan out.  wlr_backend_test() and wlr_backend_commit(), which a
 * configuration of several outputs goes through, do not: the compositor
 * has to (sway renders a black frame).  Without one, the DRM backend
 * refuses ("No primary frame buffer available for this connector") and
 * turning an output back on with wlr-randr fails.
 */

#ifndef WM_OUTPUT_BUFFER_H
#define WM_OUTPUT_BUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <wlr/backend.h>

/*
 * Give a black frame to each output of states (from
 * wlr_output_configuration_v1_build_state()) that is being turned on or
 * changing mode and has no buffer; the others are left as they are.
 * False when a frame could not be made.
 */
bool wm_output_states_add_buffers(struct wlr_backend_output_state *states,
	size_t states_len);

#endif /* WM_OUTPUT_BUFFER_H */
