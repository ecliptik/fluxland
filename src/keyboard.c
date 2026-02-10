/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * keyboard.c - Keyboard input handling
 *
 * Handles key events, chain state for multi-key sequences,
 * keymode switching, and action dispatch including MacroCmd/ToggleCmd.
 */

#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <unistd.h>
#include <wlr/backend/session.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/edges.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

#include "keyboard.h"
#include "server.h"
#include "config.h"
#include "decoration.h"
#include "idle.h"
#include "keybind.h"
#include "menu.h"
#include "placement.h"
#include "protocols.h"
#include "session_lock.h"
#include "tabgroup.h"
#include "view.h"
#include "workspace.h"

static void
exec_command(const char *cmd)
{
	if (!cmd || !*cmd) {
		return;
	}
	pid_t pid = fork();
	if (pid == 0) {
		setsid();
		execl("/bin/sh", "/bin/sh", "-c", cmd, (char *)NULL);
		_exit(1);
	} else if (pid < 0) {
		wlr_log(WLR_ERROR, "fork failed for: %s", cmd);
	}
}

/*
 * Reset chain state back to root level.
 */
static void
chain_reset(struct wm_server *server)
{
	struct wm_chain_state *cs = &server->chain_state;
	cs->in_chain = false;
	cs->current_level = NULL;
	if (cs->timeout) {
		wl_event_source_remove(cs->timeout);
		cs->timeout = NULL;
	}
}

/*
 * Timer callback: abort chain on timeout.
 */
static int
chain_timeout_cb(void *data)
{
	struct wm_server *server = data;
	wlr_log(WLR_DEBUG, "%s", "key chain timed out");
	chain_reset(server);
	return 0;
}

/*
 * Start or restart the chain timeout timer.
 */
static void
chain_start_timeout(struct wm_server *server)
{
	struct wm_chain_state *cs = &server->chain_state;
	if (cs->timeout) {
		wl_event_source_timer_update(cs->timeout,
			WM_CHAIN_TIMEOUT_MS);
		return;
	}
	cs->timeout = wl_event_loop_add_timer(server->wl_event_loop,
		chain_timeout_cb, server);
	if (cs->timeout) {
		wl_event_source_timer_update(cs->timeout,
			WM_CHAIN_TIMEOUT_MS);
	}
}

/*
 * Get the currently active keymode's binding list.
 */
static struct wl_list *
get_active_bindings(struct wm_server *server)
{
	const char *mode_name = server->current_keymode;
	if (!mode_name)
		mode_name = "default";

	struct wm_keymode *mode;
	wl_list_for_each(mode, &server->keymodes, link) {
		if (strcasecmp(mode->name, mode_name) == 0)
			return &mode->bindings;
	}
	return NULL;
}

/*
 * Forward declaration for recursive use by MacroCmd/ToggleCmd.
 */
static bool execute_action(struct wm_server *server,
	enum wm_action action, const char *argument);

/*
 * Execute a keybind, including MacroCmd/ToggleCmd dispatch.
 */
static bool
execute_keybind_action(struct wm_server *server,
	struct wm_keybind *bind)
{
	if (bind->action == WM_ACTION_MACRO_CMD) {
		struct wm_subcmd *cmd = bind->subcmds;
		while (cmd) {
			execute_action(server, cmd->action, cmd->argument);
			cmd = cmd->next;
		}
		return true;
	}

	if (bind->action == WM_ACTION_TOGGLE_CMD) {
		if (bind->subcmd_count > 0) {
			struct wm_subcmd *cmd = bind->subcmds;
			int idx = bind->toggle_index % bind->subcmd_count;
			for (int i = 0; i < idx && cmd; i++)
				cmd = cmd->next;
			if (cmd) {
				execute_action(server, cmd->action,
					cmd->argument);
			}
			bind->toggle_index =
				(bind->toggle_index + 1) % bind->subcmd_count;
		}
		return true;
	}

	return execute_action(server, bind->action, bind->argument);
}

static bool
execute_action(struct wm_server *server,
	enum wm_action action, const char *argument)
{
	struct wm_view *view = server->focused_view;

	switch (action) {
	case WM_ACTION_EXEC:
		exec_command(argument);
		return true;

	case WM_ACTION_EXIT:
		wl_display_terminate(server->wl_display);
		return true;

	case WM_ACTION_CLOSE:
		if (view) {
			wlr_xdg_toplevel_send_close(view->xdg_toplevel);
		}
		return true;

	case WM_ACTION_KILL:
		if (view) {
			struct wl_resource *resource =
				view->xdg_toplevel->base->resource;
			struct wl_client *client =
				wl_resource_get_client(resource);
			if (client) {
				pid_t pid;
				wl_client_get_credentials(client,
					&pid, NULL, NULL);
				if (pid > 0)
					kill(pid, SIGKILL);
			}
		}
		return true;

	case WM_ACTION_FOCUS_NEXT:
		wm_focus_next_view(server);
		return true;

	case WM_ACTION_FOCUS_PREV:
		/* For now, same as focus_next (cycle) */
		wm_focus_next_view(server);
		return true;

	case WM_ACTION_NEXT_WINDOW:
		wm_view_cycle_next(server);
		return true;

	case WM_ACTION_PREV_WINDOW:
		wm_view_cycle_prev(server);
		return true;

	case WM_ACTION_DEICONIFY:
		wm_view_deiconify_last(server);
		return true;

	case WM_ACTION_MAXIMIZE:
		if (view) {
			struct wl_listener *listener = &view->request_maximize;
			listener->notify(listener, NULL);
		}
		return true;

	case WM_ACTION_FULLSCREEN:
		if (view) {
			struct wl_listener *listener =
				&view->request_fullscreen;
			listener->notify(listener, NULL);
		}
		return true;

	case WM_ACTION_MINIMIZE:
		if (view) {
			struct wl_listener *listener =
				&view->request_minimize;
			listener->notify(listener, NULL);
		}
		return true;

	case WM_ACTION_RAISE:
		if (view) {
			wm_view_raise(view);
		}
		return true;

	case WM_ACTION_WORKSPACE:
		if (argument) {
			int ws = atoi(argument) - 1;
			wm_workspace_switch(server, ws);
		}
		return true;

	case WM_ACTION_SEND_TO_WORKSPACE:
		if (argument) {
			int ws = atoi(argument) - 1;
			wm_view_send_to_workspace(server, ws);
		}
		return true;

	case WM_ACTION_NEXT_WORKSPACE:
		wm_workspace_switch_next(server);
		return true;

	case WM_ACTION_PREV_WORKSPACE:
		wm_workspace_switch_prev(server);
		return true;

	case WM_ACTION_STICK:
		if (view) {
			wm_view_set_sticky(view, !view->sticky);
		}
		return true;

	case WM_ACTION_RECONFIGURE:
		wm_server_reconfigure(server);
		return true;

	case WM_ACTION_MOVE:
		if (view) {
			wm_view_begin_interactive(view, WM_CURSOR_MOVE, 0);
		}
		return true;

	case WM_ACTION_RESIZE:
		if (view) {
			wm_view_begin_interactive(view, WM_CURSOR_RESIZE,
				WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT);
		}
		return true;

	case WM_ACTION_MOVE_LEFT:
		if (view && argument) {
			int px = atoi(argument);
			view->x -= px;
			wlr_scene_node_set_position(
				&view->scene_tree->node, view->x, view->y);
		}
		return true;

	case WM_ACTION_MOVE_RIGHT:
		if (view && argument) {
			int px = atoi(argument);
			view->x += px;
			wlr_scene_node_set_position(
				&view->scene_tree->node, view->x, view->y);
		}
		return true;

	case WM_ACTION_MOVE_UP:
		if (view && argument) {
			int px = atoi(argument);
			view->y -= px;
			wlr_scene_node_set_position(
				&view->scene_tree->node, view->x, view->y);
		}
		return true;

	case WM_ACTION_MOVE_DOWN:
		if (view && argument) {
			int px = atoi(argument);
			view->y += px;
			wlr_scene_node_set_position(
				&view->scene_tree->node, view->x, view->y);
		}
		return true;

	case WM_ACTION_MOVE_TO:
		if (view && argument) {
			int x = 0, y = 0;
			sscanf(argument, "%d %d", &x, &y);
			view->x = x;
			view->y = y;
			wlr_scene_node_set_position(
				&view->scene_tree->node, x, y);
		}
		return true;

	case WM_ACTION_RESIZE_TO:
		if (view && argument) {
			int w = 0, h = 0;
			sscanf(argument, "%d %d", &w, &h);
			if (w > 0 && h > 0)
				wlr_xdg_toplevel_set_size(
					view->xdg_toplevel, w, h);
		}
		return true;

	case WM_ACTION_SHOW_DESKTOP: {
		struct wm_view *v;
		wl_list_for_each(v, &server->views, link) {
			if (v->workspace == server->current_workspace) {
				struct wl_listener *listener =
					&v->request_minimize;
				listener->notify(listener, NULL);
			}
		}
		return true;
	}

	case WM_ACTION_KEY_MODE:
		if (argument) {
			char mode_name[256];
			const char *sp = argument;
			while (*sp && !isspace((unsigned char)*sp))
				sp++;
			size_t len = sp - argument;
			if (len >= sizeof(mode_name))
				len = sizeof(mode_name) - 1;
			memcpy(mode_name, argument, len);
			mode_name[len] = '\0';

			free(server->current_keymode);
			server->current_keymode = strdup(mode_name);
			if (!server->current_keymode)
				server->current_keymode = strdup("default");
			chain_reset(server);
			wlr_log(WLR_INFO, "keymode switched to: %s",
				mode_name);
		}
		return true;

	/* Tab group actions (from tabbing engineer) */
	case WM_ACTION_NEXT_TAB:
		if (view && view->tab_group) {
			wm_tab_group_next(view->tab_group);
		}
		return true;

	case WM_ACTION_PREV_TAB:
		if (view && view->tab_group) {
			wm_tab_group_prev(view->tab_group);
		}
		return true;

	case WM_ACTION_DETACH_CLIENT:
		if (view && view->tab_group) {
			wm_tab_group_remove(view);
		}
		return true;

	case WM_ACTION_MOVE_TAB_LEFT:
		if (view && view->tab_group) {
			wm_tab_group_move_left(view->tab_group, view);
		}
		return true;

	case WM_ACTION_MOVE_TAB_RIGHT:
		if (view && view->tab_group) {
			wm_tab_group_move_right(view->tab_group, view);
		}
		return true;

	case WM_ACTION_START_TABBING:
		/* Tabbing is initiated by mouse action, not keyboard */
		return true;

	case WM_ACTION_FOCUS:
		/* Focus is implicit in mouse binding dispatch */
		if (view) {
			wm_focus_view(view, NULL);
		}
		return true;

	case WM_ACTION_START_MOVING:
		if (view) {
			wm_view_begin_interactive(view, WM_CURSOR_MOVE, 0);
		}
		return true;

	case WM_ACTION_START_RESIZING:
		if (view) {
			wm_view_begin_interactive(view, WM_CURSOR_RESIZE,
				WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT);
		}
		return true;

	case WM_ACTION_ROOT_MENU:
		wm_menu_show_root(server,
			(int)server->cursor->x, (int)server->cursor->y);
		return true;

	case WM_ACTION_WINDOW_MENU:
		wm_menu_show_window(server,
			(int)server->cursor->x, (int)server->cursor->y);
		return true;

	case WM_ACTION_WINDOW_LIST:
		wm_menu_show_window_list(server,
			(int)server->cursor->x, (int)server->cursor->y);
		return true;

	case WM_ACTION_HIDE_MENUS:
		wm_menu_hide_all(server);
		return true;

	case WM_ACTION_SHADE:
		if (view && view->decoration && server->style)
			wm_decoration_set_shaded(view->decoration,
				!view->decoration->shaded, server->style);
		return true;

	case WM_ACTION_SHADE_ON:
		if (view && view->decoration && server->style)
			wm_decoration_set_shaded(view->decoration,
				true, server->style);
		return true;

	case WM_ACTION_SHADE_OFF:
		if (view && view->decoration && server->style)
			wm_decoration_set_shaded(view->decoration,
				false, server->style);
		return true;

	case WM_ACTION_NEXT_LAYOUT:
		wm_keyboard_next_layout(server);
		return true;

	case WM_ACTION_PREV_LAYOUT:
		wm_keyboard_prev_layout(server);
		return true;

	case WM_ACTION_MAXIMIZE_VERT:
		if (view) {
			wm_view_maximize_vert(view);
		}
		return true;

	case WM_ACTION_MAXIMIZE_HORIZ:
		if (view) {
			wm_view_maximize_horiz(view);
		}
		return true;

	case WM_ACTION_TOGGLE_DECOR:
		if (view) {
			wm_view_toggle_decoration(view);
		}
		return true;

	case WM_ACTION_ACTIVATE_TAB:
		if (view && argument) {
			int idx = atoi(argument) - 1;
			wm_view_activate_tab(view, idx);
		}
		return true;

	case WM_ACTION_LOWER:
		if (view) {
			wm_view_lower(view);
		}
		return true;

	case WM_ACTION_RAISE_LAYER:
		if (view) {
			wm_view_raise_layer(view);
		}
		return true;

	case WM_ACTION_LOWER_LAYER:
		if (view) {
			wm_view_lower_layer(view);
		}
		return true;

	case WM_ACTION_SET_LAYER:
		if (view && argument) {
			enum wm_view_layer layer =
				wm_view_layer_from_name(argument);
			wm_view_set_layer(view, layer);
		}
		return true;

	case WM_ACTION_SET_DECOR:
		if (view && view->decoration && server->style && argument) {
			enum wm_decor_preset preset = WM_DECOR_NORMAL;
			if (strcasecmp(argument, "NONE") == 0 ||
			    strcmp(argument, "0") == 0)
				preset = WM_DECOR_NONE;
			else if (strcasecmp(argument, "BORDER") == 0)
				preset = WM_DECOR_BORDER;
			else if (strcasecmp(argument, "TAB") == 0)
				preset = WM_DECOR_TAB;
			else if (strcasecmp(argument, "TINY") == 0)
				preset = WM_DECOR_TINY;
			else if (strcasecmp(argument, "TOOL") == 0)
				preset = WM_DECOR_TOOL;
			wm_decoration_set_preset(view->decoration,
				preset, server->style);
		}
		return true;

	case WM_ACTION_ARRANGE_WINDOWS:
		wm_arrange_windows_grid(server);
		return true;

	case WM_ACTION_ARRANGE_VERT:
		wm_arrange_windows_vert(server);
		return true;

	case WM_ACTION_ARRANGE_HORIZ:
		wm_arrange_windows_horiz(server);
		return true;

	case WM_ACTION_CASCADE_WINDOWS:
		wm_arrange_windows_cascade(server);
		return true;

	case WM_ACTION_NOP:
		return true;

	/*
	 * MacroCmd/ToggleCmd are dispatched by execute_keybind_action()
	 * which has access to the keybind's subcmd list and toggle state.
	 * If reached here via a direct execute_action() call (e.g. from a
	 * subcmd), these are invalid — log and ignore.
	 */
	case WM_ACTION_MACRO_CMD:
	case WM_ACTION_TOGGLE_CMD:
		wlr_log(WLR_ERROR,
			"%s", "MacroCmd/ToggleCmd cannot be nested in subcmds");
		return false;
	}

	return false;
}

static bool
handle_compositor_keybinding(struct wm_server *server,
	uint32_t modifiers, xkb_keysym_t sym)
{
	/*
	 * Check hardcoded emergency keybindings first.
	 * These always work regardless of chain or keymode state.
	 */

	/* Ctrl+Alt+Fn: VT switch */
	if ((modifiers & (WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT)) ==
			(WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT)) {
		if (sym >= XKB_KEY_XF86Switch_VT_1 &&
				sym <= XKB_KEY_XF86Switch_VT_12) {
			if (server->session) {
				struct wlr_session *session = server->session;
				unsigned vt = sym - XKB_KEY_XF86Switch_VT_1 + 1;
				wlr_session_change_vt(session, vt);
			}
			return true;
		}
	}

	/*
	 * Hardcoded fallback: Mod4+Shift+E = exit compositor.
	 * This ensures there's always a way out even if keys file
	 * is broken.
	 */
	if ((modifiers & (WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT)) ==
			(WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT) &&
			sym == XKB_KEY_e) {
		wl_display_terminate(server->wl_display);
		return true;
	}

	struct wm_chain_state *cs = &server->chain_state;

	/* Escape aborts any active chain */
	if (sym == XKB_KEY_Escape && cs->in_chain) {
		wlr_log(WLR_DEBUG, "%s", "chain aborted by Escape");
		chain_reset(server);
		return true;
	}

	/* Determine which binding list to search */
	struct wl_list *search_list;
	if (cs->in_chain && cs->current_level) {
		/* Mid-chain: search children of current node */
		search_list = cs->current_level;
	} else {
		/* Search the active keymode's root bindings */
		search_list = get_active_bindings(server);
		if (!search_list) {
			/* Fallback to legacy flat list */
			search_list = &server->keybindings;
		}
	}

	struct wm_keybind *bind = keybind_find(search_list, modifiers, sym);
	if (!bind) {
		/* No match — if we were in a chain, abort it */
		if (cs->in_chain) {
			wlr_log(WLR_DEBUG, "%s", "chain broken by unmatched key");
			chain_reset(server);
		}
		return false;
	}

	/* Check if this is an intermediate chain node */
	if (bind->action == WM_ACTION_NOP &&
	    !wl_list_empty(&bind->children)) {
		cs->in_chain = true;
		cs->current_level = &bind->children;
		chain_start_timeout(server);
		wlr_log(WLR_DEBUG, "%s", "entered key chain");
		return true;
	}

	/* Leaf node — execute the action and reset chain */
	chain_reset(server);
	return execute_keybind_action(server, bind);
}

static void
handle_key(struct wl_listener *listener, void *data)
{
	struct wm_keyboard *keyboard =
		wl_container_of(listener, keyboard, key);
	struct wm_server *server = keyboard->server;
	struct wlr_keyboard_key_event *event = data;

	/* Notify idle system of user activity */
	wm_idle_notify_activity(server);

	/* Translate libinput keycode -> xkbcommon */
	uint32_t keycode = event->keycode + 8;
	const xkb_keysym_t *syms;
	int nsyms = xkb_state_key_get_syms(
		keyboard->wlr_keyboard->xkb_state, keycode, &syms);

	bool handled = false;
	uint32_t modifiers = wlr_keyboard_get_modifiers(
		keyboard->wlr_keyboard);

	/*
	 * When the session is locked, only allow VT switch keybindings.
	 * All other keys are forwarded to the lock surface.
	 */
	if (wm_session_is_locked(server)) {
		if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
			for (int i = 0; i < nsyms; i++) {
				if ((modifiers & (WLR_MODIFIER_CTRL |
				    WLR_MODIFIER_ALT)) ==
				    (WLR_MODIFIER_CTRL |
				    WLR_MODIFIER_ALT) &&
				    syms[i] >= XKB_KEY_XF86Switch_VT_1 &&
				    syms[i] <= XKB_KEY_XF86Switch_VT_12) {
					if (server->session) {
						unsigned vt = syms[i] -
							XKB_KEY_XF86Switch_VT_1 + 1;
						wlr_session_change_vt(
							server->session, vt);
					}
					handled = true;
					break;
				}
			}
		}
		if (!handled) {
			wlr_seat_set_keyboard(server->seat,
				keyboard->wlr_keyboard);
			wlr_seat_keyboard_notify_key(server->seat,
				event->time_msec, event->keycode,
				event->state);
		}
		return;
	}

	/*
	 * When keyboard shortcuts are inhibited (e.g. games, remote
	 * desktop), forward all keys to the client except emergency
	 * combos: VT switch (Ctrl+Alt+Fn) and exit (Mod4+Shift+E).
	 */
	if (wm_protocols_kb_shortcuts_inhibited(server)) {
		if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
			for (int i = 0; i < nsyms; i++) {
				/* VT switch: always allow */
				if ((modifiers & (WLR_MODIFIER_CTRL |
				    WLR_MODIFIER_ALT)) ==
				    (WLR_MODIFIER_CTRL |
				    WLR_MODIFIER_ALT) &&
				    syms[i] >= XKB_KEY_XF86Switch_VT_1 &&
				    syms[i] <= XKB_KEY_XF86Switch_VT_12) {
					if (server->session) {
						unsigned vt = syms[i] -
							XKB_KEY_XF86Switch_VT_1 + 1;
						wlr_session_change_vt(
							server->session, vt);
					}
					handled = true;
					break;
				}
				/* Emergency exit: always allow */
				if ((modifiers & (WLR_MODIFIER_LOGO |
				    WLR_MODIFIER_SHIFT)) ==
				    (WLR_MODIFIER_LOGO |
				    WLR_MODIFIER_SHIFT) &&
				    syms[i] == XKB_KEY_e) {
					wl_display_terminate(
						server->wl_display);
					handled = true;
					break;
				}
			}
		}
		if (!handled) {
			wlr_seat_set_keyboard(server->seat,
				keyboard->wlr_keyboard);
			wlr_seat_keyboard_notify_key(server->seat,
				event->time_msec, event->keycode,
				event->state);
		}
		return;
	}

	if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		for (int i = 0; i < nsyms; i++) {
			handled = handle_compositor_keybinding(
				server, modifiers, syms[i]);
			if (handled) {
				break;
			}
		}
	}

	if (!handled) {
		/* Forward the key event to the focused client */
		wlr_seat_set_keyboard(server->seat,
			keyboard->wlr_keyboard);
		wlr_seat_keyboard_notify_key(server->seat,
			event->time_msec, event->keycode, event->state);
	}
}

static void
handle_modifiers(struct wl_listener *listener, void *data)
{
	struct wm_keyboard *keyboard =
		wl_container_of(listener, keyboard, modifiers);
	struct wm_server *server = keyboard->server;

	wlr_seat_set_keyboard(server->seat, keyboard->wlr_keyboard);
	wlr_seat_keyboard_notify_modifiers(server->seat,
		&keyboard->wlr_keyboard->modifiers);
}

static void
handle_keyboard_destroy(struct wl_listener *listener, void *data)
{
	struct wm_keyboard *keyboard =
		wl_container_of(listener, keyboard, destroy);

	wl_list_remove(&keyboard->modifiers.link);
	wl_list_remove(&keyboard->key.link);
	wl_list_remove(&keyboard->destroy.link);
	wl_list_remove(&keyboard->link);
	free(keyboard);
}

void
wm_keyboard_setup(struct wm_server *server,
	struct wlr_input_device *device)
{
	struct wlr_keyboard *wlr_keyboard =
		wlr_keyboard_from_input_device(device);

	struct wm_keyboard *keyboard = calloc(1, sizeof(*keyboard));
	if (!keyboard) {
		wlr_log(WLR_ERROR, "%s", "failed to allocate keyboard");
		return;
	}
	keyboard->server = server;
	keyboard->wlr_keyboard = wlr_keyboard;

	/* Build XKB rule names from config (NULL fields use system defaults) */
	struct xkb_rule_names rules = { 0 };
	if (server->config) {
		rules.rules = server->config->xkb_rules;
		rules.model = server->config->xkb_model;
		rules.layout = server->config->xkb_layout;
		rules.variant = server->config->xkb_variant;
		rules.options = server->config->xkb_options;
	}

	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	struct xkb_keymap *keymap = xkb_keymap_new_from_names(
		context, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (!keymap) {
		/* Fallback to default layout if configured one fails */
		wlr_log(WLR_ERROR, "%s",
			"failed to compile XKB keymap with "
			"configured layout, falling back to default");
		keymap = xkb_keymap_new_from_names(
			context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
	}

	wlr_keyboard_set_keymap(wlr_keyboard, keymap);
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);

	wlr_keyboard_set_repeat_info(wlr_keyboard, 25, 600);

	/* Connect listeners */
	keyboard->modifiers.notify = handle_modifiers;
	wl_signal_add(&wlr_keyboard->events.modifiers,
		&keyboard->modifiers);

	keyboard->key.notify = handle_key;
	wl_signal_add(&wlr_keyboard->events.key, &keyboard->key);

	keyboard->destroy.notify = handle_keyboard_destroy;
	wl_signal_add(&device->events.destroy, &keyboard->destroy);

	wl_list_insert(&server->keyboards, &keyboard->link);

	/* Let the seat know about this keyboard */
	wlr_seat_set_keyboard(server->seat, wlr_keyboard);

	wlr_log(WLR_INFO, "new keyboard: %s", device->name);
}

void
wm_keyboard_next_layout(struct wm_server *server)
{
	struct wm_keyboard *keyboard;
	wl_list_for_each(keyboard, &server->keyboards, link) {
		struct xkb_state *state = keyboard->wlr_keyboard->xkb_state;
		if (!state)
			continue;
		xkb_layout_index_t num_layouts =
			xkb_keymap_num_layouts(
				keyboard->wlr_keyboard->keymap);
		if (num_layouts <= 1)
			continue;
		xkb_layout_index_t current = 0;
		for (xkb_layout_index_t i = 0; i < num_layouts; i++) {
			if (xkb_state_layout_index_is_active(state, i,
					XKB_STATE_LAYOUT_EFFECTIVE)) {
				current = i;
				break;
			}
		}
		xkb_layout_index_t next = (current + 1) % num_layouts;
		/* Update layout via wlr_keyboard API */
		struct wlr_keyboard_modifiers mods =
			keyboard->wlr_keyboard->modifiers;
		mods.group = next;
		wlr_keyboard_notify_modifiers(keyboard->wlr_keyboard,
			mods.depressed, mods.latched, mods.locked,
			mods.group);
		wlr_log(WLR_INFO, "keyboard layout switched to index %u",
			next);
	}
}

void
wm_keyboard_prev_layout(struct wm_server *server)
{
	struct wm_keyboard *keyboard;
	wl_list_for_each(keyboard, &server->keyboards, link) {
		struct xkb_state *state = keyboard->wlr_keyboard->xkb_state;
		if (!state)
			continue;
		xkb_layout_index_t num_layouts =
			xkb_keymap_num_layouts(
				keyboard->wlr_keyboard->keymap);
		if (num_layouts <= 1)
			continue;
		xkb_layout_index_t current = 0;
		for (xkb_layout_index_t i = 0; i < num_layouts; i++) {
			if (xkb_state_layout_index_is_active(state, i,
					XKB_STATE_LAYOUT_EFFECTIVE)) {
				current = i;
				break;
			}
		}
		xkb_layout_index_t prev = (current == 0)
			? num_layouts - 1 : current - 1;
		struct wlr_keyboard_modifiers mods =
			keyboard->wlr_keyboard->modifiers;
		mods.group = prev;
		wlr_keyboard_notify_modifiers(keyboard->wlr_keyboard,
			mods.depressed, mods.latched, mods.locked,
			mods.group);
		wlr_log(WLR_INFO, "keyboard layout switched to index %u",
			prev);
	}
}

const char *
wm_keyboard_get_layout_name(struct wm_server *server)
{
	struct wm_keyboard *keyboard;
	wl_list_for_each(keyboard, &server->keyboards, link) {
		struct xkb_state *state = keyboard->wlr_keyboard->xkb_state;
		if (!state)
			continue;
		xkb_layout_index_t num_layouts =
			xkb_keymap_num_layouts(
				keyboard->wlr_keyboard->keymap);
		for (xkb_layout_index_t i = 0; i < num_layouts; i++) {
			if (xkb_state_layout_index_is_active(state, i,
					XKB_STATE_LAYOUT_EFFECTIVE)) {
				return xkb_keymap_layout_get_name(
					keyboard->wlr_keyboard->keymap, i);
			}
		}
		break; /* Only check first keyboard */
	}
	return "default";
}

void
wm_keyboard_apply_config(struct wm_server *server)
{
	struct xkb_rule_names rules = { 0 };
	if (server->config) {
		rules.rules = server->config->xkb_rules;
		rules.model = server->config->xkb_model;
		rules.layout = server->config->xkb_layout;
		rules.variant = server->config->xkb_variant;
		rules.options = server->config->xkb_options;
	}

	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	struct xkb_keymap *keymap = xkb_keymap_new_from_names(
		context, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (!keymap) {
		wlr_log(WLR_ERROR, "%s",
			"failed to compile XKB keymap on "
			"reconfigure, keeping current layout");
		xkb_context_unref(context);
		return;
	}

	struct wm_keyboard *keyboard;
	wl_list_for_each(keyboard, &server->keyboards, link) {
		wlr_keyboard_set_keymap(keyboard->wlr_keyboard, keymap);
	}

	xkb_keymap_unref(keymap);
	xkb_context_unref(context);
	wlr_log(WLR_INFO, "%s", "XKB keymap reconfigured");
}
