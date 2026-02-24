/*
 * fluxland - A Fluxbox-inspired Wayland compositor
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
};

/*
 * A single sub-command within a MacroCmd or ToggleCmd.
 * Stored as a singly-linked list.
 */
struct wm_subcmd {
	enum wm_action action;
	char *argument;
	struct wm_subcmd *next;
};

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
	char *argument;

	/* For MacroCmd / ToggleCmd */
	struct wm_subcmd *subcmds;
	int subcmd_count;
	int toggle_index;          /* current index for ToggleCmd */

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
};

/*
 * Chain state maintained per-server for multi-key sequences.
 */
struct wm_chain_state {
	struct wl_list *current_level;     /* current position in binding tree */
	bool in_chain;                     /* waiting for next key in sequence */
	struct wl_event_source *timeout;   /* auto-abort timer */
};

#define WM_CHAIN_TIMEOUT_MS 3000

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

#endif /* WM_KEYBIND_H */
