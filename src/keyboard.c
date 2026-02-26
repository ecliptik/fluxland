/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * keyboard.c - Keyboard input handling
 *
 * Handles key events, chain state for multi-key sequences,
 * keymode switching, and XKB layout management.
 * Action dispatch is in keyboard_actions.c.
 */

#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wlr/backend/session.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

#include "keyboard.h"
#include "keyboard_actions.h"
#include "server.h"
#include "config.h"
#include "idle.h"
#include "keybind.h"
#include "menu.h"
#include "protocols.h"
#include "session_lock.h"

/*
 * Reset chain state back to root level.
 */
void
wm_chain_reset(struct wm_server *server)
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
	wm_chain_reset(server);
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
struct wl_list *
wm_get_active_bindings(struct wm_server *server)
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
		wm_chain_reset(server);
		return true;
	}

	/* Determine which binding list to search */
	struct wl_list *search_list;
	if (cs->in_chain && cs->current_level) {
		/* Mid-chain: search children of current node */
		search_list = cs->current_level;
	} else {
		/* Search the active keymode's root bindings */
		search_list = wm_get_active_bindings(server);
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
			wm_chain_reset(server);
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
	wm_chain_reset(server);
	server->focus_user_initiated = true;
	return wm_execute_keybind_action(server, bind);
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
		/*
		 * When a menu is visible, try menu key navigation first.
		 * Navigation keys (Up/Down/Enter/Escape/h/j/k/l) are
		 * consumed; unrecognized keys return false and fall
		 * through to the compositor keybinding check.
		 */
		for (int i = 0; i < nsyms; i++) {
			if (wm_menu_handle_key(server, keycode,
				syms[i], true)) {
				handled = true;
				break;
			}
		}

		if (!handled) {
			for (int i = 0; i < nsyms; i++) {
				handled = handle_compositor_keybinding(
					server, modifiers, syms[i]);
				if (handled) {
					break;
				}
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
