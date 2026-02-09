/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
 *
 * mousebind.h - Mouse binding types and declarations
 *
 * Parses Fluxbox-format mouse bindings from the keys file:
 *   OnTitlebar Mouse1  :MacroCmd {Raise} {Focus} {ActivateTab}
 *   OnTitlebar Move1   :StartMoving
 *   OnDesktop Mouse3   :RootMenu
 *   Mod1 Mouse1        :MacroCmd {Raise} {Focus} {StartMoving}
 */

#ifndef WM_MOUSEBIND_H
#define WM_MOUSEBIND_H

#include <stdint.h>
#include <wayland-util.h>

#include "keybind.h"

/* Click context — where the mouse event happened */
enum wm_mouse_context {
	WM_MOUSE_CTX_NONE = 0,      /* global (no On* prefix) */
	WM_MOUSE_CTX_DESKTOP,       /* OnDesktop - root/background */
	WM_MOUSE_CTX_TOOLBAR,       /* OnToolbar */
	WM_MOUSE_CTX_TITLEBAR,      /* OnTitlebar */
	WM_MOUSE_CTX_TAB,           /* OnTab */
	WM_MOUSE_CTX_WINDOW,        /* OnWindow - anywhere on frame */
	WM_MOUSE_CTX_WINDOW_BORDER, /* OnWindowBorder */
	WM_MOUSE_CTX_LEFT_GRIP,     /* OnLeftGrip */
	WM_MOUSE_CTX_RIGHT_GRIP,    /* OnRightGrip */
};

/* Mouse event type */
enum wm_mouse_event {
	WM_MOUSE_PRESS,   /* MouseN - button pressed */
	WM_MOUSE_CLICK,   /* ClickN - press + release without move */
	WM_MOUSE_DOUBLE,  /* DoubleN - double click */
	WM_MOUSE_MOVE,    /* MoveN - button held while moving (drag) */
};

struct wm_mousebind {
	enum wm_mouse_context context;
	enum wm_mouse_event event;
	uint32_t button;         /* linux button code (BTN_LEFT etc.) */
	uint32_t modifiers;      /* keyboard modifiers */
	enum wm_action action;
	char *argument;

	/* For MacroCmd / ToggleCmd */
	struct wm_subcmd *subcmds;
	int subcmd_count;
	int toggle_index;

	struct wl_list link;
};

/*
 * Mouse click tracking state (for double-click and click-vs-drag detection).
 * Maintained per-server in cursor.c.
 */
struct wm_mouse_state {
	uint32_t last_button;
	uint32_t last_time_msec;
	double press_x, press_y;    /* cursor position at button press */
	bool button_pressed;        /* a button is currently held */
	uint32_t pressed_button;    /* which button is held */
};

#define WM_DOUBLE_CLICK_MS  300
#define WM_CLICK_MOVE_THRESHOLD 3

/* Load mouse bindings from a keys file. Returns 0 on success. */
int mousebind_load(struct wl_list *bindings, const char *path);

/* Find a matching mouse binding. context=NONE matches global bindings. */
struct wm_mousebind *mousebind_find(struct wl_list *bindings,
	enum wm_mouse_context context, enum wm_mouse_event event,
	uint32_t button, uint32_t modifiers);

/* Destroy all mouse bindings in the list */
void mousebind_destroy_list(struct wl_list *bindings);

#endif /* WM_MOUSEBIND_H */
