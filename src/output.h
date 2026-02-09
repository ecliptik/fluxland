/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
 * output.h - Output (display/monitor) management
 */

#ifndef WM_OUTPUT_H
#define WM_OUTPUT_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>

struct wm_server;

struct wm_output {
	struct wl_list link; /* wm_server.outputs */
	struct wm_server *server;
	struct wlr_output *wlr_output;
	struct wlr_scene_output *scene_output;

	struct wl_listener frame;
	struct wl_listener request_state;
	struct wl_listener destroy;
};

/* Initialize output layout and register for new_output events */
void wm_output_init(struct wm_server *server);

/* Clean up output resources */
void wm_output_finish(struct wm_server *server);

#endif /* WM_OUTPUT_H */
