/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * focus_nav.h - Keyboard-only focus navigation between UI zones
 *
 * Provides a state machine for navigating between windows, toolbar
 * elements, and menu items using only the keyboard.
 */

#ifndef WM_FOCUS_NAV_H
#define WM_FOCUS_NAV_H

#include <stdbool.h>

struct wm_server;

/* Focus zones: which UI area currently has keyboard focus */
enum wm_focus_zone {
	WM_FOCUS_ZONE_WINDOWS,  /* normal window focus (default) */
	WM_FOCUS_ZONE_TOOLBAR,  /* toolbar element focus */
};

/* Focus navigation state (stored in wm_server) */
struct wm_focus_nav {
	enum wm_focus_zone zone;
	int toolbar_index; /* which toolbar tool is focused (-1 = none) */
};

/* Initialize focus navigation state */
void wm_focus_nav_init(struct wm_focus_nav *nav);

/* Switch keyboard focus to the toolbar */
void wm_focus_nav_enter_toolbar(struct wm_server *server);

/* Return keyboard focus to windows (Escape from toolbar) */
void wm_focus_nav_return_to_windows(struct wm_server *server);

/* Cycle to next element within the current toolbar */
void wm_focus_nav_next_element(struct wm_server *server);

/* Cycle to previous element within the current toolbar */
void wm_focus_nav_prev_element(struct wm_server *server);

/* Activate (Enter/Space) the currently focused toolbar element */
void wm_focus_nav_activate(struct wm_server *server);

/* Get the currently focused toolbar tool index (-1 if none) */
int wm_focus_nav_get_toolbar_index(struct wm_server *server);

#endif /* WM_FOCUS_NAV_H */
