/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * screen_cast.h - PipeWire screen recording support
 *
 * Provides output screen capture via PipeWire streams.
 * Enabled with -Dpipewire=enabled.
 */

#ifndef WM_SCREEN_CAST_H
#define WM_SCREEN_CAST_H

#include <stdbool.h>
#include <stdint.h>

struct wm_server;
struct wm_screen_cast;

enum wm_screen_cast_state {
	WM_SCREEN_CAST_STOPPED,
	WM_SCREEN_CAST_STARTING,
	WM_SCREEN_CAST_STREAMING,
	WM_SCREEN_CAST_ERROR,
};

/* Initialize screen cast subsystem. Returns NULL on failure. */
struct wm_screen_cast *wm_screen_cast_init(struct wm_server *server);

/* Tear down and free screen cast resources. */
void wm_screen_cast_destroy(struct wm_screen_cast *sc);

/* Start screen recording on the given output name (NULL = first output). */
bool wm_screen_cast_start(struct wm_screen_cast *sc, const char *output_name);

/* Stop screen recording. */
void wm_screen_cast_stop(struct wm_screen_cast *sc);

/* Get current state. */
enum wm_screen_cast_state wm_screen_cast_get_state(
	const struct wm_screen_cast *sc);

/* Get the PipeWire node ID (0 if not streaming). */
uint32_t wm_screen_cast_get_node_id(const struct wm_screen_cast *sc);

#endif /* WM_SCREEN_CAST_H */
