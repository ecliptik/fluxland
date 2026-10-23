/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * input.h - Input device management
 */

#ifndef WM_INPUT_H
#define WM_INPUT_H

#include <wlr/types/wlr_switch.h>

#include "config.h"

struct wm_server;

/* Register input handlers (new_input, seat request_cursor, etc.) */
void wm_input_init(struct wm_server *server);

/* Remove input listeners */
void wm_input_finish(struct wm_server *server);

/* Re-apply session.input.* settings to every pointer (reconfigure) */
void wm_input_apply_config(struct wm_server *server);

/* Which session.switch.* command a switch flip runs */
enum wm_switch_event wm_switch_event_for(enum wlr_switch_type type,
	enum wlr_switch_state state);

#endif /* WM_INPUT_H */
