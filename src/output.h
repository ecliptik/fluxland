/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * output.h - Output (display/monitor) management
 */

#ifndef WM_OUTPUT_H
#define WM_OUTPUT_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>

struct wm_server;
struct wm_view;
struct wm_workspace;

struct wm_output {
	struct wl_list link; /* wm_server.outputs */
	struct wm_server *server;
	struct wlr_output *wlr_output;
	struct wlr_scene_output *scene_output;

	/*
	 * Layout coordinates: the whole output, and the part windows are
	 * placed and maximized in (minus layer-shell exclusive zones, the
	 * toolbar and the slit). Kept up to date by wm_output_arrange().
	 */
	struct wlr_box full_area;
	struct wlr_box usable_area;

	/* Per-output workspace state (used in per-output workspace mode) */
	struct wm_workspace *active_workspace;

	/* Rendered a blanked frame since the session was locked */
	bool lock_blanked;

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

/* The output's box in layout coordinates (empty if not in the layout) */
void wm_output_layout_box(struct wm_output *output, struct wlr_box *box);

/*
 * Recompute usable_area: the output minus layer-shell exclusive zones,
 * the toolbar and the slit. Maximized and tiled windows on it are
 * re-fitted when it changes. Call after anything that reserves space
 * changes.
 */
void wm_output_arrange(struct wm_output *output);
void wm_output_arrange_all(struct wm_server *server);

/*
 * Put an enabled output in the layout (at x,y, else auto-placed when it
 * isn't there yet); take a disabled one out.
 */
void wm_output_place(struct wm_output *output, bool has_position, int x, int y);

/* Re-apply session.output.* settings to every output (reconfigure) */
void wm_output_apply_config(struct wm_server *server);

/*
 * Fluxbox head numbering (toolbar.onhead, slit.onhead, SetHead, [Head]):
 * heads are the enabled outputs in connection order, from 1.
 * wm_output_head() is NULL for a head that doesn't exist;
 * wm_output_by_head() falls back to head 1.
 */
struct wm_output *wm_output_head(struct wm_server *server, int head);
struct wm_output *wm_output_by_head(struct wm_server *server, int head);
int wm_output_head_count(struct wm_server *server);
/* The output's head number, 0 if it is disabled */
int wm_output_head_number(struct wm_output *output);

/* Output holding the window's center, else the nearest one */
struct wm_output *wm_output_for_view(struct wm_view *view);

#endif /* WM_OUTPUT_H */
