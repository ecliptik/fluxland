/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * keyboard_actions.h - Keyboard action dispatch interface
 *
 * Declares the interface between keyboard event handling (keyboard.c)
 * and action execution (keyboard_actions.c).
 */

#ifndef WM_KEYBOARD_ACTIONS_H
#define WM_KEYBOARD_ACTIONS_H

#include <stdbool.h>
#include <wayland-server-core.h>

#include "keybind.h"

struct wm_server;

/* Execute a single action by enum value */
bool wm_execute_action(struct wm_server *server,
	enum wm_action action, const char *argument);

/* Execute a keybind, including MacroCmd/ToggleCmd/If/ForEach/Map/Delay */
bool wm_execute_keybind_action(struct wm_server *server,
	struct wm_keybind *bind);

/* Reset chain state back to root level (shared: called from both sides) */
void wm_chain_reset(struct wm_server *server);

/* Get the currently active keymode's binding list (shared) */
struct wl_list *wm_get_active_bindings(struct wm_server *server);

#endif /* WM_KEYBOARD_ACTIONS_H */
