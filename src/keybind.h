/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * keybind.h - Keybinding types and declarations
 *
 * Parses Fluxbox-format keys files with support for:
 *   - Simple bindings: Mod4 Return :Exec foot
 *   - Chained bindings: Mod4 x Mod4 t :Exec xterm
 *   - Keymodes: moving: h :MoveLeft 10
 *   - Meta-commands: MacroCmd, ToggleCmd
 */

#ifndef WM_KEYBIND_H
#define WM_KEYBIND_H

#include <stdbool.h>
#include <stdint.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <xkbcommon/xkbcommon.h>

enum wm_action {
	WM_ACTION_NOP,
	WM_ACTION_EXEC,
	WM_ACTION_CLOSE,
	WM_ACTION_KILL,
	WM_ACTION_MAXIMIZE,
	WM_ACTION_MAXIMIZE_VERT,
	WM_ACTION_MAXIMIZE_HORIZ,
	WM_ACTION_FULLSCREEN,
	WM_ACTION_MINIMIZE,
	WM_ACTION_SHADE,
	WM_ACTION_SHADE_ON,
	WM_ACTION_SHADE_OFF,
	WM_ACTION_STICK,
	WM_ACTION_MOVE,
	WM_ACTION_RESIZE,
	WM_ACTION_RAISE,
	WM_ACTION_LOWER,
	WM_ACTION_RAISE_LAYER,
	WM_ACTION_LOWER_LAYER,
	WM_ACTION_SET_LAYER,
	WM_ACTION_FOCUS_NEXT,
	WM_ACTION_FOCUS_PREV,
	WM_ACTION_WORKSPACE,
	WM_ACTION_SEND_TO_WORKSPACE,
	WM_ACTION_NEXT_WORKSPACE,
	WM_ACTION_PREV_WORKSPACE,
	WM_ACTION_SHOW_DESKTOP,
	WM_ACTION_MOVE_LEFT,
	WM_ACTION_MOVE_RIGHT,
	WM_ACTION_MOVE_UP,
	WM_ACTION_MOVE_DOWN,
	WM_ACTION_MOVE_TO,
	WM_ACTION_RESIZE_TO,
	WM_ACTION_TOGGLE_DECOR,
	WM_ACTION_SET_DECOR,
	WM_ACTION_MACRO_CMD,
	WM_ACTION_TOGGLE_CMD,
	WM_ACTION_KEY_MODE,
	WM_ACTION_NEXT_TAB,
	WM_ACTION_PREV_TAB,
	WM_ACTION_DETACH_CLIENT,
	WM_ACTION_MOVE_TAB_LEFT,
	WM_ACTION_MOVE_TAB_RIGHT,
	WM_ACTION_START_TABBING,
	WM_ACTION_START_MOVING,
	WM_ACTION_START_RESIZING,
	WM_ACTION_ACTIVATE_TAB,
	WM_ACTION_TAB,             /* Tab N */
	WM_ACTION_ROOT_MENU,
	WM_ACTION_WINDOW_MENU,
	WM_ACTION_WINDOW_LIST,
	WM_ACTION_HIDE_MENUS,
	WM_ACTION_FOCUS,
	WM_ACTION_RECONFIGURE,
	WM_ACTION_EXIT,
	WM_ACTION_NEXT_LAYOUT,
	WM_ACTION_PREV_LAYOUT,
	WM_ACTION_DEICONIFY,
	WM_ACTION_NEXT_WINDOW,
	WM_ACTION_PREV_WINDOW,
	WM_ACTION_ARRANGE_WINDOWS,
	WM_ACTION_ARRANGE_VERT,
	WM_ACTION_ARRANGE_HORIZ,
	WM_ACTION_CASCADE_WINDOWS,
	WM_ACTION_LHALF,
	WM_ACTION_RHALF,
	WM_ACTION_RESIZE_HORIZ,
	WM_ACTION_RESIZE_VERT,
	WM_ACTION_FOCUS_LEFT,
	WM_ACTION_FOCUS_RIGHT,
	WM_ACTION_FOCUS_UP,
	WM_ACTION_FOCUS_DOWN,
	WM_ACTION_SET_HEAD,
	WM_ACTION_SEND_TO_NEXT_HEAD,
	WM_ACTION_SEND_TO_PREV_HEAD,
	WM_ACTION_ARRANGE_STACK_LEFT,
	WM_ACTION_ARRANGE_STACK_RIGHT,
	WM_ACTION_ARRANGE_STACK_TOP,
	WM_ACTION_ARRANGE_STACK_BOTTOM,
	WM_ACTION_CLOSE_ALL_WINDOWS,
	WM_ACTION_WORKSPACE_MENU,
	WM_ACTION_CLIENT_MENU,
	WM_ACTION_CUSTOM_MENU,
	WM_ACTION_SET_STYLE,
	WM_ACTION_RELOAD_STYLE,
	WM_ACTION_TAKE_TO_WORKSPACE,
	WM_ACTION_SEND_TO_NEXT_WORKSPACE,
	WM_ACTION_SEND_TO_PREV_WORKSPACE,
	WM_ACTION_TAKE_TO_NEXT_WORKSPACE,
	WM_ACTION_TAKE_TO_PREV_WORKSPACE,
	WM_ACTION_ADD_WORKSPACE,
	WM_ACTION_REMOVE_LAST_WORKSPACE,
	WM_ACTION_RIGHT_WORKSPACE,
	WM_ACTION_LEFT_WORKSPACE,
	WM_ACTION_SET_WORKSPACE_NAME,
	WM_ACTION_REMEMBER,
	WM_ACTION_TOGGLE_SLIT_ABOVE,
	WM_ACTION_TOGGLE_SLIT_HIDDEN,
	WM_ACTION_TOGGLE_TOOLBAR_ABOVE,
	WM_ACTION_TOGGLE_TOOLBAR_VISIBLE,
	WM_ACTION_SET_ALPHA,
	WM_ACTION_SET_ENV,
	WM_ACTION_BIND_KEY,
	WM_ACTION_GOTO_WINDOW,
	WM_ACTION_NEXT_GROUP,
	WM_ACTION_PREV_GROUP,
	WM_ACTION_UNCLUTTER,
	WM_ACTION_TOGGLE_SHOW_POSITION,
	WM_ACTION_IF,
	WM_ACTION_FOREACH,
	WM_ACTION_MAP,
	WM_ACTION_DELAY,
	WM_ACTION_FOCUS_TOOLBAR,
	WM_ACTION_FOCUS_NEXT_ELEMENT,
	WM_ACTION_FOCUS_PREV_ELEMENT,
	WM_ACTION_FOCUS_ACTIVATE,
	WM_ACTION_FOCUS_SLIT,
};

struct wm_command;
struct wm_condition;

/*
 * Keybinding node. Forms a tree for chained bindings:
 *   - Leaf nodes have action != NOP and are executed.
 *   - Interior nodes have action == NOP and children for the next key.
 *   - All bindings at the root level are siblings in a wl_list.
 */
struct wm_keybind {
	uint32_t modifiers;
	xkb_keysym_t keysym;
	enum wm_action action;     /* NOP for intermediate chain nodes */
	char *argument;            /* raw argument of the top-level command */

	/* Parsed command (NULL for intermediate chain nodes) */
	struct wm_command *command;

	/* Optional guard: {condition} in the key spec (fluxland extension) */
	struct wm_condition *guard;

	struct wl_list children;   /* child bindings (next key in chain) */
	struct wl_list link;       /* sibling list */
};

/*
 * A named keymode: a set of bindings active when that mode is selected.
 * "default" is the initial mode.
 */
struct wm_keymode {
	char *name;
	struct wl_list bindings;   /* wm_keybind.link */
	struct wl_list link;       /* wm_server keymodes list */
	bool end_bound;            /* KeyMode bound its end keys here */
};

/*
 * Chain state maintained per-server for multi-key sequences.
 */
struct wm_chain_state {
	struct wl_list *current_level;     /* current position in binding tree */
	bool in_chain;                     /* waiting for next key in sequence */
	struct wl_event_source *timeout;   /* auto-abort timer */
};

#define WM_CHAIN_TIMEOUT_MS 5000  /* Fluxbox's */

/*
 * A key given as a number in the keys file is a keycode (X numbering,
 * evdev + 8), stored in place of a keysym above the keysym range.
 */
#define WM_KEYSYM_KEYCODE(kc)  (0x80000000u | (uint32_t)(kc))

/* Lock modifiers (CapsLock, NumLock) never take part in a binding */
#define WM_MODIFIERS_IGNORED   (WLR_MODIFIER_CAPS | WLR_MODIFIER_MOD2)

/*
 * A modifier name of the keys file: Shift, Control (Ctrl), Mod1 (Alt),
 * Mod2, Mod3, Mod4, Mod5, Lock, or None (no bit). False if it is not one.
 */
bool wm_modifier_from_name(const char *name, uint32_t *mods);

/* Load keybindings from a keys file. Populates the default mode's bindings
 * and any additional keymodes. Returns 0 on success, -1 on error. */
int keybind_load(struct wl_list *keymodes, const char *path);

/* Find a keybinding matching the given modifier+keysym in a binding list.
 * Returns the binding, or NULL if no match. */
struct wm_keybind *keybind_find(struct wl_list *bindings, uint32_t modifiers,
				xkb_keysym_t keysym);

/* Find or create a keymode by name. Returns NULL on allocation failure. */
struct wm_keymode *keybind_get_mode(struct wl_list *keymodes,
				    const char *name);

/* Destroy all keymodes and their bindings */
void keybind_destroy_all(struct wl_list *keymodes);

/* Legacy: destroy a flat binding list (used during transition) */
void keybind_destroy_list(struct wl_list *bindings);

/* Parse a keybinding line (e.g. "Mod4 t :Exec xterm") and add to bindings.
 * Returns true on success. Used by BindKey action. */
bool keybind_add_from_string(struct wl_list *bindings, const char *line);

/* List all available action names to stdout (for --list-commands) */
void wm_keybind_list_actions(void);

/* Look up an action enum by name (case-insensitive).
 * Returns WM_ACTION_NOP if the name is not recognized. */
enum wm_action wm_action_from_name(const char *name);

#endif /* WM_KEYBIND_H */
