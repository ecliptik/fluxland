/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * cursor_actions.h - Cursor context detection and mouse action dispatch
 */

#ifndef WM_CURSOR_ACTIONS_H
#define WM_CURSOR_ACTIONS_H

#include <stdint.h>
#include "decoration.h"
#include "keybind.h"
#include "mousebind.h"

struct wm_server;
struct wm_view;
struct wlr_surface;

/* Find the view under the given layout coordinates */
struct wm_view *view_at(struct wm_server *server, double lx, double ly,
	struct wlr_surface **surface, double *sx, double *sy);

/* Map a decoration region to a mouse context */
enum wm_mouse_context region_to_context(enum wm_decor_region region);

/* Determine the mouse context at the current cursor position */
enum wm_mouse_context get_cursor_context(struct wm_server *server,
	struct wm_view **out_view);

/* Get current keyboard modifiers from the first keyboard */
uint32_t get_keyboard_modifiers(struct wm_server *server);

/* Execute a mouse binding action */
void execute_mouse_action(struct wm_server *server,
	enum wm_action action, const char *argument, struct wm_view *view);

/* Dispatch a mouse binding (handles MacroCmd/ToggleCmd) */
void execute_mousebind(struct wm_server *server, struct wm_mousebind *bind,
	struct wm_view *view);

#endif /* WM_CURSOR_ACTIONS_H */
