/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * output.c - Output (display/monitor) management
 */

#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wlr/backend/wayland.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/util/log.h>

#include "config.h"
#include "drm_lease.h"
#include "foreign_toplevel.h"
#include "ipc.h"
#include "layer_shell.h"
#include "output.h"
#include "output_config.h"
#include "slit.h"
#include "session_lock.h"
#include "ext_workspace.h"
#include "server.h"
#include "toolbar.h"
#include "util.h"
#include "view.h"
#include "view_geometry.h"
#include "workspace.h"
#include "xwayland.h"

static void
handle_output_frame(struct wl_listener *listener, void *data)
{
	struct wm_output *output = wl_container_of(listener, output, frame);
	struct wlr_scene_output *scene_output = output->scene_output;

	wm_xwayland_sync_positions(output->server);
	wm_foreign_toplevel_sync_outputs(output->server);
	wlr_scene_output_commit(scene_output, NULL);
	wm_session_lock_output_frame(output->server, output);

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(scene_output, &now);
}

static void
handle_output_request_state(struct wl_listener *listener, void *data)
{
	/* Handle state requests from nested backends (resize, etc.) */
	struct wm_output *output =
		wl_container_of(listener, output, request_state);
	const struct wlr_output_event_request_state *event = data;
	wlr_output_commit_state(output->wlr_output, event->state);
}

static void
handle_output_destroy(struct wl_listener *listener, void *data)
{
	struct wm_output *output = wl_container_of(listener, output, destroy);

	/* Broadcast output remove event via IPC */
	{
		char esc_name[128], buf[256];
		wm_json_escape(esc_name, sizeof(esc_name),
			output->wlr_output->name);
		snprintf(buf, sizeof(buf),
			"{\"event\":\"output_remove\","
			"\"name\":\"%s\"}",
			esc_name);
		wm_ipc_broadcast_event(&output->server->ipc,
			WM_IPC_EVENT_OUTPUT_REMOVE, buf);
	}

	struct wm_server *server = output->server;
	struct wlr_output *wlr_output = output->wlr_output;
	wl_list_remove(&output->link);
	wl_list_remove(&output->frame.link);
	wl_list_remove(&output->request_state.link);
	wl_list_remove(&output->destroy.link);
	free(output);

	/* Panels and backgrounds on it go away with it */
	wm_layer_shell_output_destroyed(server, wlr_output);
	wm_foreign_toplevel_output_destroyed(server, wlr_output);
	wm_ext_workspace_output_leave(server, wlr_output);

	/*
	 * Leaving the layout moves windows, the toolbar and the slit to
	 * the remaining outputs (handle_layout_change).
	 */
	wlr_output_layout_remove(server->output_layout, wlr_output);

	wm_session_lock_output_changed(server);
}

/*
 * Outputs were added, removed, moved, rescaled or rotated: move the
 * toolbar and slit, recompute every usable area (re-fitting maximized
 * windows) and bring back windows left off-screen.
 */
static void
handle_layout_change(struct wl_listener *listener, void *data)
{
	(void)data;
	struct wm_server *server =
		wl_container_of(listener, server, output_layout_change);

	if (server->toolbar)
		wm_toolbar_relayout(server->toolbar);
	if (server->slit)
		wm_slit_reconfigure(server->slit);
	wm_output_arrange_all(server);

	struct wm_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (!view->mapped || view->docked)
			continue;
		wm_view_keep_on_screen(view);
		wm_view_refit(view);
	}
}

void
wm_output_layout_box(struct wm_output *output, struct wlr_box *box)
{
	wlr_output_layout_get_box(output->server->output_layout,
		output->wlr_output, box);
}

void
wm_output_place(struct wm_output *output, bool has_position, int x, int y)
{
	struct wm_server *server = output->server;
	struct wlr_output *wlr_output = output->wlr_output;
	bool present = wlr_output_layout_get(server->output_layout,
		wlr_output) != NULL;

	if (!wlr_output->enabled) {
		/* Disabled outputs have no place in the layout */
		if (present)
			wlr_output_layout_remove(server->output_layout,
				wlr_output);
		return;
	}

	struct wlr_output_layout_output *lo;
	if (has_position)
		lo = wlr_output_layout_add(server->output_layout, wlr_output,
			x, y);
	else if (!present)
		lo = wlr_output_layout_add_auto(server->output_layout,
			wlr_output);
	else
		return;

	/* The scene output follows a new layout entry */
	if (lo && !present && output->scene_output)
		wlr_scene_output_layout_add_output(server->scene_layout, lo,
			output->scene_output);
}

void
wm_output_arrange(struct wm_output *output)
{
	struct wm_server *server = output->server;
	struct wlr_box full = {0};
	wm_output_layout_box(output, &full);

	struct wlr_box usable = full;
	if (!wlr_box_empty(&full)) {
		wm_layer_shell_configure(output, &full, &usable);
		wm_toolbar_reserve(server->toolbar, output, &usable);
		wm_slit_reserve(server->slit, output, &usable);
	}

	bool changed = !wlr_box_equal(&usable, &output->usable_area) ||
		!wlr_box_equal(&full, &output->full_area);
	output->usable_area = usable;
	output->full_area = full;
	if (!changed)
		return;

	/* Maximized and tiled windows on this output follow */
	struct wm_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (view->mapped && !view->docked &&
		    wm_output_for_view(view) == output)
			wm_view_refit(view);
	}
}

void
wm_output_arrange_all(struct wm_server *server)
{
	struct wm_output *output;
	wl_list_for_each(output, &server->outputs, link)
		wm_output_arrange(output);
}

void
wm_output_apply_config(struct wm_server *server)
{
	if (!server->config)
		return;
	struct wm_output *output;
	wl_list_for_each(output, &server->outputs, link) {
		const struct wm_output_rule *rule = config_find_output_rule(
			server->config, output->wlr_output->name);
		if (!rule)
			continue;
		struct wlr_output_state state;
		wlr_output_state_init(&state);
		if (wm_output_config_build(output->wlr_output, rule, &state) &&
		    !wlr_output_commit_state(output->wlr_output, &state))
			wlr_log(WLR_ERROR, "output %s: cannot apply settings "
				"from init", output->wlr_output->name);
		wlr_output_state_finish(&state);
		wm_output_place(output, rule->has_position, rule->x, rule->y);
	}
}

/* Heads are the outputs in the layout (disabled ones don't count) */
static bool
is_head(struct wm_output *output)
{
	return wlr_output_layout_get(output->server->output_layout,
		output->wlr_output) != NULL;
}

struct wm_output *
wm_output_head(struct wm_server *server, int head)
{
	int n = 1;
	struct wm_output *output;
	wl_list_for_each(output, &server->outputs, link) {
		if (is_head(output) && n++ == head)
			return output;
	}
	return NULL;
}

struct wm_output *
wm_output_by_head(struct wm_server *server, int head)
{
	struct wm_output *output = wm_output_head(server, head);
	/* 0 or out of range: the first head */
	return output ? output : wm_output_head(server, 1);
}

int
wm_output_head_count(struct wm_server *server)
{
	int n = 0;
	struct wm_output *output;
	wl_list_for_each(output, &server->outputs, link) {
		if (is_head(output))
			n++;
	}
	return n;
}

int
wm_output_head_number(struct wm_output *output)
{
	int n = 1;
	struct wm_output *o;
	wl_list_for_each(o, &output->server->outputs, link) {
		if (o == output)
			return is_head(o) ? n : 0;
		if (is_head(o))
			n++;
	}
	return 0;
}

struct wm_output *
wm_output_for_view(struct wm_view *view)
{
	struct wm_server *server = view->server;
	struct wlr_box geo;
	wm_view_get_geometry(view, &geo);
	double cx = view->x + geo.width / 2.0;
	double cy = view->y + geo.height / 2.0;

	struct wlr_output *wlr_output = wlr_output_layout_output_at(
		server->output_layout, cx, cy);
	if (!wlr_output) {
		double lx, ly;
		wlr_output_layout_closest_point(server->output_layout, NULL,
			cx, cy, &lx, &ly);
		wlr_output = wlr_output_layout_output_at(
			server->output_layout, lx, ly);
	}
	struct wm_output *output = wlr_output ?
		wm_output_from_wlr(server, wlr_output) : NULL;
	if (!output && !wl_list_empty(&server->outputs))
		output = wl_container_of(server->outputs.next, output, link);
	return output;
}

static void
handle_new_output(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, new_output);
	struct wlr_output *wlr_output = data;

	/* Initialize rendering for this output */
	wlr_output_init_render(wlr_output, server->allocator,
		server->renderer);

	/* Preferred mode, then the session.output.<name>.* settings */
	const struct wm_output_rule *rule = server->config ?
		config_find_output_rule(server->config, wlr_output->name) :
		NULL;
	struct wlr_output_state state;
	wlr_output_state_init(&state);
	wlr_output_state_set_enabled(&state, true);
	struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
	if (mode) {
		wlr_output_state_set_mode(&state, mode);
	}
	wm_output_config_build(wlr_output, rule, &state);
	if (!wlr_output_commit_state(wlr_output, &state) && rule) {
		wlr_log(WLR_ERROR, "output %s: settings from init failed, "
			"using its preferred mode", wlr_output->name);
		wlr_output_state_finish(&state);
		wlr_output_state_init(&state);
		wlr_output_state_set_enabled(&state, true);
		if (mode)
			wlr_output_state_set_mode(&state, mode);
		wlr_output_commit_state(wlr_output, &state);
	}
	wlr_output_state_finish(&state);

	/* Allocate our output wrapper */
	struct wm_output *output = calloc(1, sizeof(*output));
	if (!output) {
		wlr_log(WLR_ERROR, "%s", "failed to allocate output");
		return;
	}
	output->server = server;
	output->wlr_output = wlr_output;
	output->active_workspace = server->current_workspace;

	/* Connect listeners */
	output->frame.notify = handle_output_frame;
	wl_signal_add(&wlr_output->events.frame, &output->frame);

	output->request_state.notify = handle_output_request_state;
	wl_signal_add(&wlr_output->events.request_state,
		&output->request_state);

	output->destroy.notify = handle_output_destroy;
	wl_signal_add(&wlr_output->events.destroy, &output->destroy);

	/* Connection order: head 1 is the first output connected */
	wl_list_insert(server->outputs.prev, &output->link);

	output->scene_output = wlr_scene_output_create(
		server->scene, wlr_output);

	/* Into the layout at its configured position (or auto) */
	wm_output_place(output, rule && rule->has_position,
		rule ? rule->x : 0, rule ? rule->y : 0);

	wlr_log(WLR_INFO, "new output: %s (%dx%d)",
		wlr_output->name,
		wlr_output->width, wlr_output->height);

	/* Broadcast output add event via IPC */
	{
		char esc_name[128], buf[256];
		wm_json_escape(esc_name, sizeof(esc_name),
			wlr_output->name);
		snprintf(buf, sizeof(buf),
			"{\"event\":\"output_add\","
			"\"name\":\"%s\","
			"\"width\":%d,"
			"\"height\":%d}",
			esc_name,
			wlr_output->width, wlr_output->height);
		wm_ipc_broadcast_event(&server->ipc,
			WM_IPC_EVENT_OUTPUT_ADD, buf);
	}

	/* Offer output for DRM leasing (VR headsets) */
	wm_drm_lease_offer_output(server, wlr_output);
	wm_ext_workspace_output_enter(server, wlr_output);

	/* Layer surfaces, toolbar and slit (may be the first output) */
	handle_layout_change(&server->output_layout_change, NULL);
}

void
wm_output_init(struct wm_server *server)
{
	wl_list_init(&server->outputs);

	server->output_layout = wlr_output_layout_create(
		server->wl_display);
	if (!server->output_layout) {
		wlr_log(WLR_ERROR, "%s", "unable to create output layout");
		exit(EXIT_FAILURE);
	}

	server->scene_layout = wlr_scene_attach_output_layout(
		server->scene, server->output_layout);
	if (!server->scene_layout) {
		wlr_log(WLR_ERROR, "%s", "unable to attach scene to output layout");
		exit(EXIT_FAILURE);
	}

	server->output_layout_change.notify = handle_layout_change;
	wl_signal_add(&server->output_layout->events.change,
		&server->output_layout_change);

	server->new_output.notify = handle_new_output;
	wl_signal_add(&server->backend->events.new_output,
		&server->new_output);
}

void
wm_output_finish(struct wm_server *server)
{
	wl_list_remove(&server->new_output.link);
	wl_list_remove(&server->output_layout_change.link);

	/*
	 * The backend destroys the outputs later, after the toolbar, slit
	 * and session lock are gone: stop following them now.
	 */
	struct wm_output *output, *tmp;
	wl_list_for_each_safe(output, tmp, &server->outputs, link) {
		wl_list_remove(&output->link);
		wl_list_remove(&output->frame.link);
		wl_list_remove(&output->request_state.link);
		wl_list_remove(&output->destroy.link);
		free(output);
	}
}

struct wm_output *
wm_output_from_wlr(struct wm_server *server, struct wlr_output *wlr_output)
{
	struct wm_output *output;
	wl_list_for_each(output, &server->outputs, link) {
		if (output->wlr_output == wlr_output)
			return output;
	}
	return NULL;
}

struct wm_output *
wm_server_get_focused_output(struct wm_server *server)
{
	/* Try output of focused view first */
	if (server->focused_view) {
		struct wm_output *output =
			wm_output_for_view(server->focused_view);
		if (output)
			return output;
	}

	/* Fallback: output under cursor */
	struct wlr_output *wlr_output = wlr_output_layout_output_at(
		server->output_layout,
		server->cursor->x, server->cursor->y);
	if (wlr_output) {
		struct wm_output *output =
			wm_output_from_wlr(server, wlr_output);
		if (output)
			return output;
	}

	/* Last resort: first output */
	if (!wl_list_empty(&server->outputs)) {
		return wl_container_of(server->outputs.next,
			(struct wm_output *)NULL, link);
	}

	return NULL;
}
