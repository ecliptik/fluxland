/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
 * keyboard.c - Keyboard input handling
 */

#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <unistd.h>
#include <wlr/backend/session.h>
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
#include "keybind.h"
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

static bool
execute_keybind_action(struct wm_server *server,
	struct wm_keybind *bind)
{
	struct wm_view *view = server->focused_view;

	switch (bind->action) {
	case WM_ACTION_EXEC:
		exec_command(bind->argument);
		return true;

	case WM_ACTION_EXIT:
		wl_display_terminate(server->wl_display);
		return true;

	case WM_ACTION_CLOSE:
		if (view) {
			wlr_xdg_toplevel_send_close(view->xdg_toplevel);
		}
		return true;

	case WM_ACTION_FOCUS_NEXT:
		wm_focus_next_view(server);
		return true;

	case WM_ACTION_FOCUS_PREV:
		/* For now, same as focus_next (cycle) */
		wm_focus_next_view(server);
		return true;

	case WM_ACTION_MAXIMIZE:
		if (view) {
			/* Toggle maximize by sending the request event */
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
		if (bind->argument) {
			int ws = atoi(bind->argument) - 1;
			wm_workspace_switch(server, ws);
		}
		return true;

	case WM_ACTION_SEND_TO_WORKSPACE:
		if (bind->argument) {
			int ws = atoi(bind->argument) - 1;
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
		if (server->config) {
			config_reload(server->config);
			server->focus_policy = server->config->focus_policy;
		}
		keybind_destroy_list(&server->keybindings);
		wl_list_init(&server->keybindings);
		if (server->config && server->config->keys_file) {
			keybind_load(&server->keybindings,
				server->config->keys_file);
		}
		wlr_log(WLR_INFO, "configuration reloaded");
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

	case WM_ACTION_LOWER:
	case WM_ACTION_SHADE:
	case WM_ACTION_MAXIMIZE_VERT:
	case WM_ACTION_MAXIMIZE_HORIZ:
	case WM_ACTION_NOP:
		return true;
	}

	return false;
}

static bool
handle_compositor_keybinding(struct wm_server *server,
	uint32_t modifiers, xkb_keysym_t sym)
{
	/*
	 * Check hardcoded emergency keybindings first, then check the
	 * loaded keybinding list.  Hardcoded bindings are a safety net
	 * to ensure the compositor is always controllable.
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

	/* Check loaded keybindings */
	struct wm_keybind *bind = keybind_find(&server->keybindings,
		modifiers, sym);
	if (bind) {
		return execute_keybind_action(server, bind);
	}

	return false;
}

static void
handle_key(struct wl_listener *listener, void *data)
{
	struct wm_keyboard *keyboard =
		wl_container_of(listener, keyboard, key);
	struct wm_server *server = keyboard->server;
	struct wlr_keyboard_key_event *event = data;

	/* Translate libinput keycode -> xkbcommon */
	uint32_t keycode = event->keycode + 8;
	const xkb_keysym_t *syms;
	int nsyms = xkb_state_key_get_syms(
		keyboard->wlr_keyboard->xkb_state, keycode, &syms);

	bool handled = false;
	uint32_t modifiers = wlr_keyboard_get_modifiers(
		keyboard->wlr_keyboard);

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
		wlr_log(WLR_ERROR, "failed to allocate keyboard");
		return;
	}
	keyboard->server = server;
	keyboard->wlr_keyboard = wlr_keyboard;

	/* Set up XKB keymap (default layout) */
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	struct xkb_keymap *keymap = xkb_keymap_new_from_names(
		context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);

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
