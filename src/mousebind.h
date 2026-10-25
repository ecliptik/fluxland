/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * mousebind.h - Mouse binding types and declarations
 *
 * Parses Fluxbox-format mouse bindings from the keys file:
 *   OnTitlebar Mouse1  :MacroCmd {Raise} {Focus} {ActivateTab}
 *   OnTitlebar Double Mouse1 :Shade
 *   OnDesktop Mouse4   :NextWorkspace
 *   OnWindow Mod1 Mouse1 :MacroCmd {Raise} {Focus} {StartMoving}
 */

#ifndef WM_MOUSEBIND_H
#define WM_MOUSEBIND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <wayland-util.h>

#include "keybind.h"

/*
 * Where a mouse event happened: Fluxbox's Keys contexts, as flags. A
 * click has every context it is in (a tab is OnTab and OnTitlebar); a
 * binding lists the contexts it applies to, none meaning the desktop.
 */
enum wm_mouse_context {
	WM_MOUSE_CTX_NONE          = 0,       /* binding: no On* given */
	WM_MOUSE_CTX_GLOBAL        = 1 << 0,  /* what "none" matches */
	WM_MOUSE_CTX_DESKTOP       = 1 << 1,  /* OnDesktop */
	WM_MOUSE_CTX_TOOLBAR       = 1 << 2,  /* OnToolbar */
	WM_MOUSE_CTX_TITLEBAR      = 1 << 4,  /* OnTitlebar */
	WM_MOUSE_CTX_WINDOW        = 1 << 5,  /* OnWindow: the client */
	WM_MOUSE_CTX_WINDOW_BORDER = 1 << 6,  /* OnWindowBorder */
	WM_MOUSE_CTX_LEFT_GRIP     = 1 << 7,  /* OnLeftGrip */
	WM_MOUSE_CTX_RIGHT_GRIP    = 1 << 8,  /* OnRightGrip */
	WM_MOUSE_CTX_TAB           = 1 << 9,  /* OnTab */
	WM_MOUSE_CTX_SLIT          = 1 << 10, /* OnSlit */
	WM_MOUSE_CTX_WINBUTTON     = 1 << 11, /* OnWinButton (on release) */
	WM_MOUSE_CTX_MINBUTTON     = 1 << 12, /* OnMinButton */
	WM_MOUSE_CTX_MAXBUTTON     = 1 << 13, /* OnMaxButton */
};

/*
 * Fluxbox numbers mouse buttons as X does: 1-3 left, middle, right; 4-7
 * the wheel (up, down, left, right); 8 and 9 back and forward. The wheel
 * has no button code, so it gets these.
 */
#define WM_BUTTON_WHEEL_UP     0x7f04
#define WM_BUTTON_WHEEL_DOWN   0x7f05
#define WM_BUTTON_WHEEL_LEFT   0x7f06
#define WM_BUTTON_WHEEL_RIGHT  0x7f07

/* Mouse event type */
enum wm_mouse_event {
	WM_MOUSE_PRESS,   /* MouseN - button pressed */
	WM_MOUSE_CLICK,   /* ClickN - press + release without move */
	WM_MOUSE_DOUBLE,  /* Double MouseN (or DoubleN) - double click */
	WM_MOUSE_MOVE,    /* MoveN - button held while moving (drag) */
};

struct wm_view;

struct wm_mousebind {
	uint32_t context;        /* wm_mouse_context flags; 0 = desktop */
	enum wm_mouse_event event;
	uint32_t button;         /* linux button code (BTN_LEFT etc.) */
	uint32_t modifiers;      /* keyboard modifiers */
	enum wm_action action;   /* top-level action of `command` */
	char *argument;          /* raw argument of the top-level command */
	struct wm_command *command;

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
	bool press_consumed;        /* a binding took the press: keep the
	                               release from the client too */
	int pressed_decor_button;   /* titlebar button pressed, or -1 */
	struct wm_view *pressed_view;
	uint32_t press_context;     /* wm_mouse_context flags of the press */
	int wheel_steps[2];         /* 1/120 wheel steps not yet acted on */
};

#define WM_DOUBLE_CLICK_MS  250  /* Fluxbox's */
#define WM_CLICK_MOVE_THRESHOLD 3

/* Load mouse bindings from a keys file. Returns 0 on success.
 * button_map is a 6-element array (index 0 unused, 1-5 = button codes
 * for those numbers); NULL uses the defaults. */
int mousebind_load(struct wl_list *bindings, const char *path,
	const uint32_t *button_map);

/*
 * The first binding for this event, button and modifiers whose contexts
 * include one of context's (the flags of where the event happened).
 */
struct wm_mousebind *mousebind_find(struct wl_list *bindings,
	uint32_t context, enum wm_mouse_event event,
	uint32_t button, uint32_t modifiers);

/* Is any binding of this button a Move binding here? (Fluxbox keeps
 * such a press from the client) */
bool mousebind_has_move(struct wl_list *bindings, uint32_t context,
	uint32_t button, uint32_t modifiers);

/*
 * Would keybind_load or mousebind_load take this keys-file line? False,
 * with the reason in why, if they would drop it (for --check-config).
 */
bool wm_keys_check_line(const char *line, char *why, size_t len);

/* Destroy all mouse bindings in the list */
void mousebind_destroy_list(struct wl_list *bindings);

#endif /* WM_MOUSEBIND_H */
