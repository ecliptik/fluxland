/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
 * output.c - Output (display/monitor) management
 */

#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <time.h>
#include <wlr/backend/wayland.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/util/log.h>

#include "ipc.h"
#include "layer_shell.h"
#include "output.h"
#include "server.h"

static void
handle_output_frame(struct wl_listener *listener, void *data)
{
	struct wm_output *output = wl_container_of(listener, output, frame);
	struct wlr_scene_output *scene_output = output->scene_output;

	wlr_scene_output_commit(scene_output, NULL);

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
		char buf[256];
		snprintf(buf, sizeof(buf),
			"{\"event\":\"output_remove\","
			"\"name\":\"%s\"}",
			output->wlr_output->name ?
				output->wlr_output->name : "");
		wm_ipc_broadcast_event(&output->server->ipc,
			WM_IPC_EVENT_OUTPUT_REMOVE, buf);
	}

	wl_list_remove(&output->link);
	wl_list_remove(&output->frame.link);
	wl_list_remove(&output->request_state.link);
	wl_list_remove(&output->destroy.link);
	free(output);
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

	/* Set preferred mode */
	struct wlr_output_state state;
	wlr_output_state_init(&state);
	wlr_output_state_set_enabled(&state, true);

	struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
	if (mode) {
		wlr_output_state_set_mode(&state, mode);
	}

	wlr_output_commit_state(wlr_output, &state);
	wlr_output_state_finish(&state);

	/* Allocate our output wrapper */
	struct wm_output *output = calloc(1, sizeof(*output));
	if (!output) {
		wlr_log(WLR_ERROR, "failed to allocate output");
		return;
	}
	output->server = server;
	output->wlr_output = wlr_output;

	/* Connect listeners */
	output->frame.notify = handle_output_frame;
	wl_signal_add(&wlr_output->events.frame, &output->frame);

	output->request_state.notify = handle_output_request_state;
	wl_signal_add(&wlr_output->events.request_state,
		&output->request_state);

	output->destroy.notify = handle_output_destroy;
	wl_signal_add(&wlr_output->events.destroy, &output->destroy);

	wl_list_insert(&server->outputs, &output->link);

	/* Add output to layout (auto-arranged) */
	struct wlr_output_layout_output *lo =
		wlr_output_layout_add_auto(server->output_layout, wlr_output);

	/* Create scene output and attach to layout */
	output->scene_output = wlr_scene_output_create(
		server->scene, wlr_output);
	wlr_scene_output_layout_add_output(
		server->scene_layout, lo, output->scene_output);

	/* Initialize usable area to full output dimensions */
	wlr_output_effective_resolution(wlr_output,
		&output->usable_area.width,
		&output->usable_area.height);

	wlr_log(WLR_INFO, "new output: %s (%dx%d)",
		wlr_output->name,
		wlr_output->width, wlr_output->height);

	/* Broadcast output add event via IPC */
	{
		char buf[256];
		snprintf(buf, sizeof(buf),
			"{\"event\":\"output_add\","
			"\"name\":\"%s\","
			"\"width\":%d,"
			"\"height\":%d}",
			wlr_output->name ? wlr_output->name : "",
			wlr_output->width, wlr_output->height);
		wm_ipc_broadcast_event(&server->ipc,
			WM_IPC_EVENT_OUTPUT_ADD, buf);
	}

	/* Arrange any existing layer surfaces on this output */
	wm_layer_shell_arrange(output);
}

void
wm_output_init(struct wm_server *server)
{
	wl_list_init(&server->outputs);

	server->output_layout = wlr_output_layout_create(
		server->wl_display);
	if (!server->output_layout) {
		wlr_log(WLR_ERROR, "unable to create output layout");
		exit(EXIT_FAILURE);
	}

	server->scene_layout = wlr_scene_attach_output_layout(
		server->scene, server->output_layout);
	if (!server->scene_layout) {
		wlr_log(WLR_ERROR, "unable to attach scene to output layout");
		exit(EXIT_FAILURE);
	}

	server->new_output.notify = handle_new_output;
	wl_signal_add(&server->backend->events.new_output,
		&server->new_output);
}

void
wm_output_finish(struct wm_server *server)
{
	wl_list_remove(&server->new_output.link);
}
