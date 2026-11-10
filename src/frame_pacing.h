/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * frame_pacing.h - fifo-v1 and commit-timing-v1
 */

#ifndef WM_FRAME_PACING_H
#define WM_FRAME_PACING_H

struct wm_server;
struct wlr_output;

/* Advertise wp_fifo_v1 and wp_commit_timing_v1 */
void wm_frame_pacing_init(struct wm_server *server);

/*
 * An output is about to render a frame: let go the content updates whose
 * fifo barrier or commit timestamp the frame satisfies. Called by the
 * output's frame handler before the scene is committed.
 */
void wm_frame_pacing_output_frame(struct wm_server *server,
	struct wlr_output *output);

#endif /* WM_FRAME_PACING_H */
