/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
 * text_input.c - Text input v3 and input method v2 relay for IME support
 *
 * Implements the relay between text-input-v3 (from application clients)
 * and input-method-v2 (from IME clients like fcitx5, ibus). This enables
 * CJK input, on-screen keyboards, and other input method support.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <wlr/types/wlr_text_input_v3.h>
#include <wlr/types/wlr_input_method_v2.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/log.h>

#include "text_input.h"
#include "server.h"

/*
 * Internal per-text-input state with a back-pointer to the relay.
 */
struct wm_text_input_impl {
	struct wl_list link;
	struct wlr_text_input_v3 *input;
	struct wm_text_input_relay *relay;

	struct wl_listener enable;
	struct wl_listener commit;
	struct wl_listener disable;
	struct wl_listener destroy;
};

/*
 * Find the active (enabled) text input for the currently focused surface.
 */
static struct wm_text_input_impl *
relay_find_focused(struct wm_text_input_relay *relay)
{
	struct wlr_surface *focused =
		relay->server->seat->keyboard_state.focused_surface;
	if (!focused) {
		return NULL;
	}

	struct wm_text_input_impl *ti;
	wl_list_for_each(ti, &relay->text_inputs, link) {
		if (ti->input->focused_surface == focused &&
		    ti->input->current_enabled) {
			return ti;
		}
	}
	return NULL;
}

/*
 * Forward the current text input state to the input method.
 */
static void
relay_send_im_state(struct wm_text_input_relay *relay,
	struct wlr_text_input_v3 *text_input)
{
	struct wlr_input_method_v2 *im = relay->input_method;
	if (!im) {
		return;
	}

	if (text_input->current.features &
	    WLR_TEXT_INPUT_V3_FEATURE_SURROUNDING_TEXT) {
		wlr_input_method_v2_send_surrounding_text(im,
			text_input->current.surrounding.text ?
				text_input->current.surrounding.text : "",
			text_input->current.surrounding.cursor,
			text_input->current.surrounding.anchor);
	}

	wlr_input_method_v2_send_text_change_cause(im,
		text_input->current.text_change_cause);

	if (text_input->current.features &
	    WLR_TEXT_INPUT_V3_FEATURE_CONTENT_TYPE) {
		wlr_input_method_v2_send_content_type(im,
			text_input->current.content_type.hint,
			text_input->current.content_type.purpose);
	}

	wlr_input_method_v2_send_done(im);
}

/*
 * Send cursor rectangle to all popup surfaces of the input method.
 */
static void
relay_send_im_popup_position(struct wm_text_input_relay *relay,
	struct wlr_text_input_v3 *text_input)
{
	struct wlr_input_method_v2 *im = relay->input_method;
	if (!im) {
		return;
	}

	if (!(text_input->current.features &
	      WLR_TEXT_INPUT_V3_FEATURE_CURSOR_RECTANGLE)) {
		return;
	}

	struct wlr_input_popup_surface_v2 *popup;
	wl_list_for_each(popup, &im->popup_surfaces, link) {
		wlr_input_popup_surface_v2_send_text_input_rectangle(
			popup, &text_input->current.cursor_rectangle);
	}
}

/* --- Text input event handlers --- */

static void
handle_ti_enable(struct wl_listener *listener, void *data)
{
	struct wm_text_input_impl *ti =
		wl_container_of(listener, ti, enable);
	struct wm_text_input_relay *relay = ti->relay;
	struct wlr_input_method_v2 *im = relay->input_method;

	if (!im) {
		return;
	}

	wlr_input_method_v2_send_activate(im);
	relay_send_im_state(relay, ti->input);
	relay_send_im_popup_position(relay, ti->input);
}

static void
handle_ti_commit(struct wl_listener *listener, void *data)
{
	struct wm_text_input_impl *ti =
		wl_container_of(listener, ti, commit);
	struct wm_text_input_relay *relay = ti->relay;
	struct wlr_input_method_v2 *im = relay->input_method;

	if (!im || !ti->input->current_enabled) {
		return;
	}

	relay_send_im_state(relay, ti->input);
	relay_send_im_popup_position(relay, ti->input);
}

static void
handle_ti_disable(struct wl_listener *listener, void *data)
{
	struct wm_text_input_impl *ti =
		wl_container_of(listener, ti, disable);
	struct wm_text_input_relay *relay = ti->relay;
	struct wlr_input_method_v2 *im = relay->input_method;

	if (!im) {
		return;
	}

	wlr_input_method_v2_send_deactivate(im);
	wlr_input_method_v2_send_done(im);
}

static void
handle_ti_destroy(struct wl_listener *listener, void *data)
{
	struct wm_text_input_impl *ti =
		wl_container_of(listener, ti, destroy);

	wl_list_remove(&ti->enable.link);
	wl_list_remove(&ti->commit.link);
	wl_list_remove(&ti->disable.link);
	wl_list_remove(&ti->destroy.link);
	wl_list_remove(&ti->link);
	free(ti);
}

/* --- Input method event handlers --- */

static void
handle_im_commit(struct wl_listener *listener, void *data)
{
	struct wm_text_input_relay *relay =
		wl_container_of(listener, relay, input_method_commit);
	struct wlr_input_method_v2 *im = relay->input_method;

	if (!im) {
		return;
	}

	struct wm_text_input_impl *ti = relay_find_focused(relay);
	if (!ti || !ti->input->current_enabled) {
		return;
	}

	struct wlr_text_input_v3 *text_input = ti->input;

	if (im->current.preedit.text) {
		wlr_text_input_v3_send_preedit_string(text_input,
			im->current.preedit.text,
			im->current.preedit.cursor_begin,
			im->current.preedit.cursor_end);
	} else {
		wlr_text_input_v3_send_preedit_string(text_input,
			NULL, 0, 0);
	}

	if (im->current.commit_text) {
		wlr_text_input_v3_send_commit_string(text_input,
			im->current.commit_text);
	}

	if (im->current.delete.before_length ||
	    im->current.delete.after_length) {
		wlr_text_input_v3_send_delete_surrounding_text(text_input,
			im->current.delete.before_length,
			im->current.delete.after_length);
	}

	wlr_text_input_v3_send_done(text_input);
}

static void
handle_keyboard_grab_destroy(struct wl_listener *listener, void *data)
{
	struct wm_text_input_relay *relay =
		wl_container_of(listener, relay, keyboard_grab_destroy);

	wl_list_remove(&relay->keyboard_grab_destroy.link);
	wl_list_init(&relay->keyboard_grab_destroy.link);
}

static void
handle_im_grab_keyboard(struct wl_listener *listener, void *data)
{
	struct wm_text_input_relay *relay =
		wl_container_of(listener, relay, input_method_grab_keyboard);
	struct wlr_input_method_keyboard_grab_v2 *grab = data;

	struct wlr_keyboard *keyboard =
		wlr_seat_get_keyboard(relay->server->seat);
	if (keyboard) {
		wlr_input_method_keyboard_grab_v2_set_keyboard(grab,
			keyboard);
	}

	wl_list_remove(&relay->keyboard_grab_destroy.link);
	relay->keyboard_grab_destroy.notify = handle_keyboard_grab_destroy;
	wl_signal_add(&grab->events.destroy,
		&relay->keyboard_grab_destroy);
}

static void
handle_im_destroy(struct wl_listener *listener, void *data)
{
	struct wm_text_input_relay *relay =
		wl_container_of(listener, relay, input_method_destroy);

	wl_list_remove(&relay->input_method_commit.link);
	wl_list_remove(&relay->input_method_grab_keyboard.link);
	wl_list_remove(&relay->input_method_destroy.link);
	wl_list_remove(&relay->keyboard_grab_destroy.link);
	wl_list_init(&relay->keyboard_grab_destroy.link);

	relay->input_method = NULL;

	/* Clear preedit on all active text inputs since the IME is gone */
	struct wm_text_input_impl *ti;
	wl_list_for_each(ti, &relay->text_inputs, link) {
		if (ti->input->current_enabled) {
			wlr_text_input_v3_send_preedit_string(ti->input,
				NULL, 0, 0);
			wlr_text_input_v3_send_done(ti->input);
		}
	}

	wlr_log(WLR_INFO, "%s", "input method disconnected");
}

/* --- Manager event handlers --- */

static void
handle_new_text_input(struct wl_listener *listener, void *data)
{
	struct wm_text_input_relay *relay =
		wl_container_of(listener, relay, new_text_input);
	struct wlr_text_input_v3 *text_input = data;

	struct wm_text_input_impl *ti = calloc(1, sizeof(*ti));
	if (!ti) {
		wlr_log(WLR_ERROR, "%s", "failed to allocate text input");
		return;
	}

	ti->input = text_input;
	ti->relay = relay;

	ti->enable.notify = handle_ti_enable;
	wl_signal_add(&text_input->events.enable, &ti->enable);

	ti->commit.notify = handle_ti_commit;
	wl_signal_add(&text_input->events.commit, &ti->commit);

	ti->disable.notify = handle_ti_disable;
	wl_signal_add(&text_input->events.disable, &ti->disable);

	ti->destroy.notify = handle_ti_destroy;
	wl_signal_add(&text_input->events.destroy, &ti->destroy);

	wl_list_insert(&relay->text_inputs, &ti->link);

	/* If the seat already has a focused surface, send enter */
	struct wlr_surface *focused =
		relay->server->seat->keyboard_state.focused_surface;
	if (focused) {
		wlr_text_input_v3_send_enter(text_input, focused);
	}
}

static void
handle_new_input_method(struct wl_listener *listener, void *data)
{
	struct wm_text_input_relay *relay =
		wl_container_of(listener, relay, new_input_method);
	struct wlr_input_method_v2 *im = data;

	/* Only one input method per seat */
	if (relay->input_method) {
		wlr_log(WLR_INFO,
			"%s", "rejecting second input method on seat");
		wlr_input_method_v2_send_unavailable(im);
		return;
	}

	relay->input_method = im;

	relay->input_method_commit.notify = handle_im_commit;
	wl_signal_add(&im->events.commit, &relay->input_method_commit);

	relay->input_method_grab_keyboard.notify = handle_im_grab_keyboard;
	wl_signal_add(&im->events.grab_keyboard,
		&relay->input_method_grab_keyboard);

	relay->input_method_destroy.notify = handle_im_destroy;
	wl_signal_add(&im->events.destroy, &relay->input_method_destroy);

	wlr_log(WLR_INFO, "%s", "input method connected");

	/* If there's already a focused+enabled text input, activate */
	struct wm_text_input_impl *ti = relay_find_focused(relay);
	if (ti && ti->input->current_enabled) {
		wlr_input_method_v2_send_activate(im);
		relay_send_im_state(relay, ti->input);
	}
}

/* --- Public API --- */

void
wm_text_input_focus_change(struct wm_server *server,
	struct wlr_surface *new_focus)
{
	struct wm_text_input_relay *relay = &server->text_input_relay;

	struct wm_text_input_impl *ti;
	wl_list_for_each(ti, &relay->text_inputs, link) {
		if (ti->input->focused_surface == new_focus) {
			continue;
		}

		/* Leave old surface */
		if (ti->input->focused_surface) {
			if (ti->input->current_enabled &&
			    relay->input_method) {
				wlr_input_method_v2_send_deactivate(
					relay->input_method);
				wlr_input_method_v2_send_done(
					relay->input_method);
			}
			wlr_text_input_v3_send_leave(ti->input);
		}

		/* Enter new surface */
		if (new_focus) {
			wlr_text_input_v3_send_enter(ti->input, new_focus);
		}
	}
}

void
wm_text_input_init(struct wm_server *server)
{
	struct wm_text_input_relay *relay = &server->text_input_relay;
	relay->server = server;
	relay->input_method = NULL;

	wl_list_init(&relay->text_inputs);
	wl_list_init(&relay->keyboard_grab_destroy.link);

	relay->text_input_mgr =
		wlr_text_input_manager_v3_create(server->wl_display);

	relay->new_text_input.notify = handle_new_text_input;
	wl_signal_add(&relay->text_input_mgr->events.text_input,
		&relay->new_text_input);

	relay->input_method_mgr =
		wlr_input_method_manager_v2_create(server->wl_display);

	relay->new_input_method.notify = handle_new_input_method;
	wl_signal_add(&relay->input_method_mgr->events.input_method,
		&relay->new_input_method);

	wlr_log(WLR_INFO, "%s",
		"initialized text-input-v3 and "
		"input-method-v2 protocols");
}

void
wm_text_input_finish(struct wm_server *server)
{
	struct wm_text_input_relay *relay = &server->text_input_relay;

	wl_list_remove(&relay->new_text_input.link);
	wl_list_remove(&relay->new_input_method.link);
	wl_list_remove(&relay->keyboard_grab_destroy.link);

	if (relay->input_method) {
		wl_list_remove(&relay->input_method_commit.link);
		wl_list_remove(&relay->input_method_grab_keyboard.link);
		wl_list_remove(&relay->input_method_destroy.link);
	}
}
