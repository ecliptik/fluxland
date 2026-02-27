/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * input.h - Input device management
 */

#ifndef WM_INPUT_H
#define WM_INPUT_H

struct wm_server;

/* Register input handlers (new_input, seat request_cursor, etc.) */
void wm_input_init(struct wm_server *server);

/* Remove input listeners */
void wm_input_finish(struct wm_server *server);

#endif /* WM_INPUT_H */
