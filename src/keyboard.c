/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
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
#include <wlr/types/wlr_input_method_v2.h>
#include <wlr/backend/session.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

#include "text_input.h"
#include "keyboard.h"
#include "keyboard_actions.h"
#include "server.h"
#include "config.h"
#include "idle.h"
#include "keybind.h"
#include "menu.h"
#include "protocols.h"
#include "xwayland_grab.h"
#include "session_lock.h"
#include "view.h"
#include "view_internal.h"

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

/* A binding in list for one of the key's candidate keysyms */
static struct wm_keybind *
find_binding(struct wm_server *server, struct wl_list *list,
	uint32_t modifiers, const xkb_keysym_t *cands, int ncands)
{
	for (int c = 0; c < ncands; c++) {
		struct wm_keybind *bind = keybind_find(list, modifiers,
			cands[c]);
		if (!bind)
			continue;
		/*
		 * A guard condition that fails passes to the next binding
		 * of the same key, so one key can have several bindings
		 * with different conditions.
		 */
		if (wm_keybind_check_condition(server, bind))
			return bind;
		bool past = false;
		struct wm_keybind *b;
		wl_list_for_each(b, list, link) {
			if (b == bind) {
				past = true;
				continue;
			}
			if (!past || b->modifiers != modifiers ||
			    b->keysym != cands[c])
				continue;
			if (wm_keybind_check_condition(server, b))
				return b;
		}
	}
	return NULL;
}

/* X's IsModifierKey */
static bool
is_modifier_key(xkb_keysym_t sym)
{
	return (sym >= XKB_KEY_Shift_L && sym <= XKB_KEY_Hyper_R) ||
		(sym >= XKB_KEY_ISO_Lock && sym <= XKB_KEY_ISO_Level5_Lock) ||
		sym == XKB_KEY_Mode_switch || sym == XKB_KEY_Num_Lock;
}

/*
 * A key press, as the keysyms it can stand for (cands): the one the
 * keyboard state gives, the key's unshifted one (bindings name keys,
 * as Fluxbox's do: "Mod1 Shift Tab"), and its keycode.
 */
static bool
handle_compositor_keybinding(struct wm_server *server,
	uint32_t modifiers, const xkb_keysym_t *cands, int ncands)
{
	/*
	 * Check hardcoded emergency keybindings first.
	 * These always work regardless of chain or keymode state.
	 */
	for (int c = 0; c < ncands; c++) {
		xkb_keysym_t sym = cands[c];
		/* Ctrl+Alt+Fn: VT switch */
		if ((modifiers & (WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT)) ==
				(WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT) &&
		    sym >= XKB_KEY_XF86Switch_VT_1 &&
		    sym <= XKB_KEY_XF86Switch_VT_12) {
			if (server->session) {
				unsigned vt = sym - XKB_KEY_XF86Switch_VT_1 + 1;
				wlr_session_change_vt(server->session, vt);
			}
			return true;
		}
		/*
		 * Hardcoded fallback: Mod4+Shift+E = exit compositor.
		 * This ensures there's always a way out even if keys file
		 * is broken.
		 */
		if ((modifiers & (WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT)) ==
				(WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT) &&
		    (sym == XKB_KEY_e || sym == XKB_KEY_E)) {
			wl_display_terminate(server->wl_display);
			return true;
		}
	}

	struct wm_chain_state *cs = &server->chain_state;

	/* Escape aborts any active chain */
	if (cs->in_chain) {
		for (int c = 0; c < ncands; c++) {
			if (cands[c] == XKB_KEY_Escape) {
				wlr_log(WLR_DEBUG, "%s",
					"chain aborted by Escape");
				wm_chain_reset(server);
				return true;
			}
		}
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

	struct wm_keybind *bind = find_binding(server, search_list,
		modifiers, cands, ncands);
	/* Nothing else here: an Arg takes the key, modifiers apart
	 * (Keys.cc's placeholder) */
	if (!bind && cs->in_chain && ncands > 0 &&
	    !is_modifier_key(cands[0])) {
		struct wm_keybind *b;
		wl_list_for_each(b, search_list, link) {
			if (b->keysym == WM_KEYSYM_ARG &&
			    wm_keybind_check_condition(server, b)) {
				bind = b;
				server->arg_key = cands[0];
				break;
			}
		}
	}
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

/* --- Repeat: a held key whose binding ran a Repeat {command} --- */

/* Wayland has no server-side repeat (a client repeats the keys it is
 * sent), so a binding that opts in with Repeat runs again here, at the
 * keyboard's repeat rate, while its key is held. */

void
wm_key_repeat_stop(struct wm_server *server)
{
	struct wm_key_repeat *kr = &server->key_repeat;
	if (kr->timer)
		wl_event_source_timer_update(kr->timer, 0);
	kr->keyboard = NULL;
	kr->keycode = 0;
}

/* Whether an xkb keycode is held down on the keyboard */
static bool
key_is_down(const struct wlr_keyboard *kb, uint32_t keycode)
{
	for (size_t i = 0; i < kb->num_keycodes; i++) {
		if (kb->keycodes[i] + 8 == keycode)
			return true;
	}
	return false;
}

static int key_repeat_tick(void *data);

static void
key_repeat_start(struct wm_keyboard *keyboard, uint32_t keycode)
{
	struct wm_server *server = keyboard->server;
	struct wlr_keyboard *kb = keyboard->wlr_keyboard;
	struct wm_key_repeat *kr = &server->key_repeat;
	if (kb->repeat_info.rate <= 0 ||
	    !xkb_keymap_key_repeats(kb->keymap, keycode))
		return;
	if (!kr->timer) {
		kr->timer = wl_event_loop_add_timer(server->wl_event_loop,
			key_repeat_tick, server);
		if (!kr->timer)
			return;
	}
	kr->keyboard = keyboard;
	kr->keycode = keycode;
	wl_event_source_timer_update(kr->timer,
		kb->repeat_info.delay > 0 ? kb->repeat_info.delay : 1);
}

static int key_candidates(struct wlr_keyboard *kb, uint32_t keycode,
	const xkb_keysym_t *syms, int nsyms, xkb_keysym_t *out, int max);

/*
 * A key press against the compositor's bindings.  *repeat is set when
 * the binding that ran said Repeat.
 */
static bool
dispatch_key(struct wm_server *server, struct wlr_keyboard *kb,
	uint32_t keycode, const xkb_keysym_t *syms, int nsyms,
	uint32_t modifiers, bool *repeat)
{
	xkb_keysym_t cands[16];
	int ncands = key_candidates(kb, keycode, syms, nsyms, cands, 16);
	server->key_repeat.requested = false;
	bool handled = handle_compositor_keybinding(server,
		modifiers & ~WM_MODIFIERS_IGNORED, cands, ncands);
	*repeat = handled && server->key_repeat.requested;
	server->key_repeat.requested = false;
	return handled;
}

static int
key_repeat_tick(void *data)
{
	struct wm_server *server = data;
	struct wm_keyboard *keyboard = server->key_repeat.keyboard;
	uint32_t keycode = server->key_repeat.keycode;
	if (!keyboard)
		return 0;
	struct wlr_keyboard *kb = keyboard->wlr_keyboard;
	if (!key_is_down(kb, keycode) || wm_session_is_locked(server) ||
	    wm_xwayland_grab_surface(server) ||
	    wm_protocols_kb_shortcuts_inhibited(server)) {
		wm_key_repeat_stop(server);
		return 0;
	}

	wm_idle_notify_activity(server);
	const xkb_keysym_t *syms;
	int nsyms = xkb_state_key_get_syms(kb->xkb_state, keycode, &syms);
	bool repeat = false;
	/* Looked up afresh: the modifiers, a condition or the bindings
	 * themselves may have changed since the press */
	dispatch_key(server, kb, keycode, syms, nsyms,
		wlr_keyboard_get_modifiers(kb), &repeat);

	/* The binding may have ended it (or moved it) itself */
	if (server->key_repeat.keyboard != keyboard ||
	    server->key_repeat.keycode != keycode)
		return 0;
	if (!repeat) {
		wm_key_repeat_stop(server);
		return 0;
	}
	int rate = kb->repeat_info.rate > 0 ? kb->repeat_info.rate : 1;
	wl_event_source_timer_update(server->key_repeat.timer,
		1000 / rate > 0 ? 1000 / rate : 1);
	return 0;
}

/* The keysyms a key press can be bound as (see above) */
static int
key_candidates(struct wlr_keyboard *kb, uint32_t keycode,
	const xkb_keysym_t *syms, int nsyms, xkb_keysym_t *out, int max)
{
	int n = 0;
	for (int i = 0; i < nsyms && n < max - 1; i++)
		out[n++] = syms[i];
	const xkb_keysym_t *raw;
	xkb_layout_index_t layout = xkb_state_key_get_layout(kb->xkb_state,
		keycode);
	int nraw = xkb_keymap_key_get_syms_by_level(kb->keymap, keycode,
		layout, 0, &raw);
	for (int i = 0; i < nraw && n < max - 1; i++) {
		bool dup = false;
		for (int j = 0; j < n; j++)
			dup |= out[j] == raw[i];
		if (!dup)
			out[n++] = raw[i];
	}
	out[n++] = WM_KEYSYM_KEYCODE(keycode);
	return n;
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

	/* A repeat ends when its key is let go, or another is pressed */
	if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED ||
	    keycode == server->key_repeat.keycode)
		wm_key_repeat_stop(server);
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
	/* An X11 program has the keyboard grabbed (xlock, a VNC viewer): the
	 * same, whatever has the focus */
	bool x_grab = wm_xwayland_grab_surface(server) != NULL;
	if (x_grab || wm_protocols_kb_shortcuts_inhibited(server)) {
		if (x_grab)
			wm_xwayland_grab_focus(server);
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

	/* A window of the compositor's own (a dialog) takes the keys typed
	 * while it has the focus; the key bindings of Alt and Mod4 stay */
	if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED && nsyms > 0 &&
	    server->focused_view &&
	    !(modifiers & (WLR_MODIFIER_ALT | WLR_MODIFIER_LOGO))) {
		char utf8[16] = "";
		xkb_state_key_get_utf8(keyboard->wlr_keyboard->xkb_state,
			keycode, utf8, sizeof(utf8));
		if (wm_internal_view_key(server->focused_view, syms[0], utf8,
				modifiers))
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
			bool repeat = false;
			handled = dispatch_key(server, keyboard->wlr_keyboard,
				keycode, syms, nsyms, modifiers, &repeat);
			if (repeat)
				key_repeat_start(keyboard, keycode);
		}
	}

	if (!handled) {
		/* Its user is typing (noFocusWhileTypingDelay) */
		if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED &&
		    server->focused_view && nsyms > 0)
			wm_view_note_key(server->focused_view, syms[0]);
		/* An input method (fcitx5...) takes the keys first; what
		 * it doesn't use comes back on its virtual keyboard */
		struct wlr_input_method_keyboard_grab_v2 *grab =
			wm_text_input_keyboard_grab(server,
				keyboard->wlr_keyboard);
		if (grab) {
			wlr_input_method_keyboard_grab_v2_set_keyboard(grab,
				keyboard->wlr_keyboard);
			wlr_input_method_keyboard_grab_v2_send_key(grab,
				event->time_msec, event->keycode,
				event->state);
			return;
		}
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

	struct wlr_input_method_keyboard_grab_v2 *grab =
		wm_text_input_keyboard_grab(server, keyboard->wlr_keyboard);
	if (grab) {
		wlr_input_method_keyboard_grab_v2_set_keyboard(grab,
			keyboard->wlr_keyboard);
		wlr_input_method_keyboard_grab_v2_send_modifiers(grab,
			&keyboard->wlr_keyboard->modifiers);
	} else {
		wlr_seat_set_keyboard(server->seat, keyboard->wlr_keyboard);
		wlr_seat_keyboard_notify_modifiers(server->seat,
			&keyboard->wlr_keyboard->modifiers);
	}

	/* A modifier let go ends the grace to undo a SnapStep minimize */
	wm_snap_step_modifiers_changed(server,
		wlr_keyboard_get_modifiers(keyboard->wlr_keyboard));

	/* Releasing the modifiers ends a NextWindow/PrevWindow cycle */
	if (server->focus_cycle.active &&
	    !(wlr_keyboard_get_modifiers(keyboard->wlr_keyboard) &
	      ~(WLR_MODIFIER_CAPS | WLR_MODIFIER_MOD2)))
		wm_focus_cycle_end(server);
}

static void
handle_keyboard_destroy(struct wl_listener *listener, void *data)
{
	struct wm_keyboard *keyboard =
		wl_container_of(listener, keyboard, destroy);

	if (keyboard->server->key_repeat.keyboard == keyboard)
		wm_key_repeat_stop(keyboard->server);

	/* Group members have no event listeners of their own */
	wl_list_remove(&keyboard->modifiers.link);
	wl_list_remove(&keyboard->key.link);
	wl_list_remove(&keyboard->destroy.link);
	wl_list_remove(&keyboard->link);
	free(keyboard);
}

/* The keymap from session.xkb.*, falling back to the default layout */
static struct xkb_keymap *
config_keymap(struct wm_server *server, bool fallback)
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
	if (!context)
		return NULL;
	struct xkb_keymap *keymap = xkb_keymap_new_from_names(
		context, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (!keymap && fallback) {
		wlr_log(WLR_ERROR, "%s",
			"failed to compile XKB keymap with "
			"configured layout, falling back to default");
		keymap = xkb_keymap_new_from_names(
			context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
	}
	xkb_context_unref(context);
	return keymap;
}

/* Key and modifier events of a keyboard come to us */
static void
listen_events(struct wm_keyboard *keyboard)
{
	keyboard->modifiers.notify = handle_modifiers;
	wl_signal_add(&keyboard->wlr_keyboard->events.modifiers,
		&keyboard->modifiers);
	keyboard->key.notify = handle_key;
	wl_signal_add(&keyboard->wlr_keyboard->events.key, &keyboard->key);
}

void
wm_keyboard_group_init(struct wm_server *server)
{
	struct wlr_keyboard_group *group = wlr_keyboard_group_create();
	struct wm_keyboard *keyboard = calloc(1, sizeof(*keyboard));
	if (!group || !keyboard) {
		if (group)
			wlr_keyboard_group_destroy(group);
		free(keyboard);
		return;
	}
	struct xkb_keymap *keymap = config_keymap(server, true);
	if (keymap) {
		wlr_keyboard_set_keymap(&group->keyboard, keymap);
		xkb_keymap_unref(keymap);
	}
	wlr_keyboard_set_repeat_info(&group->keyboard, 25, 600);

	keyboard->server = server;
	keyboard->wlr_keyboard = &group->keyboard;
	keyboard->is_group = true;
	wl_list_init(&keyboard->link);
	wl_list_init(&keyboard->destroy.link);
	listen_events(keyboard);

	server->keyboard_group = group;
	server->group_keyboard = keyboard;
}

void
wm_keyboard_group_finish(struct wm_server *server)
{
	struct wm_keyboard *keyboard = server->group_keyboard;
	if (!keyboard)
		return;
	if (server->key_repeat.keyboard == keyboard)
		wm_key_repeat_stop(server);
	wl_list_remove(&keyboard->modifiers.link);
	wl_list_remove(&keyboard->key.link);
	free(keyboard);
	server->group_keyboard = NULL;
	wlr_keyboard_group_destroy(server->keyboard_group);
	server->keyboard_group = NULL;
}

void
wm_keyboard_setup(struct wm_server *server,
	struct wlr_input_device *device, bool is_virtual)
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
	keyboard->is_virtual = is_virtual;
	wl_list_init(&keyboard->modifiers.link);
	wl_list_init(&keyboard->key.link);

	bool grouped = false;
	if (!is_virtual && server->keyboard_group) {
		/* Same keymap as the group, then its events come via it */
		wlr_keyboard_set_keymap(wlr_keyboard,
			server->keyboard_group->keyboard.keymap);
		wlr_keyboard_set_repeat_info(wlr_keyboard, 25, 600);
		grouped = wlr_keyboard_group_add_keyboard(
			server->keyboard_group, wlr_keyboard);
	}
	if (!grouped) {
		if (!is_virtual) {
			/* No group: its own copy of the configured keymap */
			struct xkb_keymap *keymap = config_keymap(server, true);
			if (keymap) {
				wlr_keyboard_set_keymap(wlr_keyboard, keymap);
				xkb_keymap_unref(keymap);
			}
			wlr_keyboard_set_repeat_info(wlr_keyboard, 25, 600);
		}
		listen_events(keyboard);
	}

	keyboard->destroy.notify = handle_keyboard_destroy;
	wl_signal_add(&device->events.destroy, &keyboard->destroy);

	wl_list_insert(&server->keyboards, &keyboard->link);

	/* Let the seat know about this keyboard */
	wlr_seat_set_keyboard(server->seat, grouped ?
		&server->keyboard_group->keyboard : wlr_keyboard);

	wlr_log(WLR_INFO, "new %skeyboard: %s%s",
		is_virtual ? "virtual " : "", device->name,
		grouped ? " (grouped)" : "");
}

/* Every keyboard whose layout we control: the group and its members */
static void
switch_layout(struct wlr_keyboard *kb, int delta)
{
	struct xkb_state *state = kb->xkb_state;
	if (!state || !kb->keymap)
		return;
	xkb_layout_index_t num = xkb_keymap_num_layouts(kb->keymap);
	if (num <= 1)
		return;
	xkb_layout_index_t current = 0;
	for (xkb_layout_index_t i = 0; i < num; i++) {
		if (xkb_state_layout_index_is_active(state, i,
				XKB_STATE_LAYOUT_EFFECTIVE)) {
			current = i;
			break;
		}
	}
	xkb_layout_index_t next = (current + num + (xkb_layout_index_t)
		(delta % (int)num + (int)num)) % num;
	struct wlr_keyboard_modifiers mods = kb->modifiers;
	wlr_keyboard_notify_modifiers(kb, mods.depressed, mods.latched,
		mods.locked, next);
	wlr_log(WLR_INFO, "keyboard layout switched to index %u", next);
}

static void
switch_layouts(struct wm_server *server, int delta)
{
	struct wm_keyboard *keyboard;
	wl_list_for_each(keyboard, &server->keyboards, link) {
		if (!keyboard->is_virtual)
			switch_layout(keyboard->wlr_keyboard, delta);
	}
	if (server->group_keyboard)
		switch_layout(server->group_keyboard->wlr_keyboard, delta);
}

void
wm_keyboard_next_layout(struct wm_server *server)
{
	switch_layouts(server, 1);
}

void
wm_keyboard_prev_layout(struct wm_server *server)
{
	switch_layouts(server, -1);
}

void
wm_keyboard_apply_config(struct wm_server *server)
{
	wm_key_repeat_stop(server);
	struct xkb_keymap *keymap = config_keymap(server, false);
	if (!keymap) {
		wlr_log(WLR_ERROR, "%s",
			"failed to compile XKB keymap on "
			"reconfigure, keeping current layout");
		return;
	}

	/* Not virtual keyboards: their clients chose their keymaps */
	struct wm_keyboard *keyboard;
	wl_list_for_each(keyboard, &server->keyboards, link) {
		if (!keyboard->is_virtual)
			wlr_keyboard_set_keymap(keyboard->wlr_keyboard, keymap);
	}
	if (server->group_keyboard)
		wlr_keyboard_set_keymap(server->group_keyboard->wlr_keyboard,
			keymap);

	xkb_keymap_unref(keymap);
	wlr_log(WLR_INFO, "%s", "XKB keymap reconfigured");
}
