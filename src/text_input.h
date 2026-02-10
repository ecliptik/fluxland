/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * text_input.h - Text input v3 and input method v2 relay for IME support
 */

#ifndef WM_TEXT_INPUT_H
#define WM_TEXT_INPUT_H

#include <wayland-server-core.h>

struct wm_server;
struct wlr_surface;
struct wlr_text_input_manager_v3;
struct wlr_input_method_manager_v2;
struct wlr_input_method_v2;

/*
 * Central relay that connects text-input-v3 clients to an
 * input-method-v2 IME. One per seat.
 */
struct wm_text_input_relay {
	struct wm_server *server;

	struct wl_list text_inputs; /* wm_text_input_impl.link */

	struct wlr_text_input_manager_v3 *text_input_mgr;
	struct wlr_input_method_manager_v2 *input_method_mgr;

	struct wlr_input_method_v2 *input_method;

	struct wl_listener new_text_input;
	struct wl_listener new_input_method;

	/* Input method event listeners */
	struct wl_listener input_method_commit;
	struct wl_listener input_method_grab_keyboard;
	struct wl_listener input_method_destroy;

	/* Keyboard grab listeners */
	struct wl_listener keyboard_grab_destroy;
};

/* Initialize text input and input method protocols */
void wm_text_input_init(struct wm_server *server);

/* Clean up text input relay */
void wm_text_input_finish(struct wm_server *server);

/*
 * Notify the relay that keyboard focus changed to a new surface.
 * Sends enter/leave to relevant text inputs.
 */
void wm_text_input_focus_change(struct wm_server *server,
	struct wlr_surface *new_focus);

#endif /* WM_TEXT_INPUT_H */
