/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * output.h - Output (display/monitor) management
 */

#ifndef WM_OUTPUT_H
#define WM_OUTPUT_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>

struct wm_server;
struct wm_workspace;

struct wm_output {
	struct wl_list link; /* wm_server.outputs */
	struct wm_server *server;
	struct wlr_output *wlr_output;
	struct wlr_scene_output *scene_output;

	/* Area available for window placement (full area minus exclusive zones) */
	struct wlr_box usable_area;

	/* Per-output workspace state (used in per-output workspace mode) */
	struct wm_workspace *active_workspace;

	struct wl_listener frame;
	struct wl_listener request_state;
	struct wl_listener destroy;
};

/* Initialize output layout and register for new_output events */
void wm_output_init(struct wm_server *server);

/* Clean up output resources */
void wm_output_finish(struct wm_server *server);

/*
 * Get the focused output: the output containing the focused view,
 * or falling back to the output under the cursor.
 */
struct wm_output *wm_server_get_focused_output(struct wm_server *server);

/*
 * Find the wm_output wrapper for a given wlr_output.
 */
struct wm_output *wm_output_from_wlr(struct wm_server *server,
	struct wlr_output *wlr_output);

#endif /* WM_OUTPUT_H */
