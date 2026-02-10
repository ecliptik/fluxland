/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
 * output_management.c - wlr-output-management-unstable-v1 protocol support
 *
 * Enables display configuration tools (wlr-randr, kanshi) to query
 * and modify output settings (mode, position, scale, transform).
 */

#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <wlr/backend.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/util/log.h>

#include "layer_shell.h"
#include "output.h"
#include "output_management.h"
#include "server.h"

/*
 * Build a wlr_output_configuration_v1 reflecting the current state of all
 * outputs, to be sent to clients via the output management protocol.
 */
static struct wlr_output_configuration_v1 *
build_current_configuration(struct wm_server *server)
{
	struct wlr_output_configuration_v1 *config =
		wlr_output_configuration_v1_create();
	if (!config) {
		return NULL;
	}

	struct wm_output *output;
	wl_list_for_each(output, &server->outputs, link) {
		struct wlr_output_configuration_head_v1 *head =
			wlr_output_configuration_head_v1_create(config,
				output->wlr_output);
		if (!head) {
			wlr_output_configuration_v1_destroy(config);
			return NULL;
		}

		/* Fill in the current position from the output layout */
		struct wlr_output_layout_output *lo = wlr_output_layout_get(
			server->output_layout, output->wlr_output);
		if (lo) {
			head->state.x = lo->x;
			head->state.y = lo->y;
		}
	}

	return config;
}

/*
 * Apply or test a requested output configuration from a client.
 * On apply, commits changes and updates the output layout.
 * On test, only validates the configuration without committing.
 */
static void
handle_output_configuration(struct wm_server *server,
	struct wlr_output_configuration_v1 *config, bool test_only)
{
	/* Use the wlroots helper to build per-output state arrays */
	size_t states_len = 0;
	struct wlr_backend_output_state *states =
		wlr_output_configuration_v1_build_state(config, &states_len);
	if (!states) {
		wlr_log(WLR_ERROR, "output management: failed to build state");
		wlr_output_configuration_v1_send_failed(config);
		wlr_output_configuration_v1_destroy(config);
		return;
	}

	/* Test the configuration atomically against the backend */
	bool ok = wlr_backend_test(server->backend, states, states_len);
	if (!ok) {
		wlr_log(WLR_INFO, "output management: configuration test failed");
		wlr_output_configuration_v1_send_failed(config);
		goto cleanup;
	}

	if (test_only) {
		wlr_output_configuration_v1_send_succeeded(config);
		goto cleanup;
	}

	/* Commit the configuration atomically */
	ok = wlr_backend_commit(server->backend, states, states_len);
	if (!ok) {
		wlr_log(WLR_ERROR, "output management: configuration commit failed");
		wlr_output_configuration_v1_send_failed(config);
		goto cleanup;
	}

	/* Apply position changes to the output layout */
	struct wlr_output_configuration_head_v1 *head;
	wl_list_for_each(head, &config->heads, link) {
		struct wlr_output *wlr_output = head->state.output;

		if (head->state.enabled) {
			wlr_output_layout_add(server->output_layout, wlr_output,
				head->state.x, head->state.y);
		}

		/* Re-arrange layer surfaces on the affected output */
		struct wm_output *wm_out;
		wl_list_for_each(wm_out, &server->outputs, link) {
			if (wm_out->wlr_output == wlr_output) {
				wlr_output_effective_resolution(wlr_output,
					&wm_out->usable_area.width,
					&wm_out->usable_area.height);
				wm_layer_shell_arrange(wm_out);
				break;
			}
		}
	}

	wlr_output_configuration_v1_send_succeeded(config);

cleanup:
	for (size_t i = 0; i < states_len; i++) {
		wlr_output_state_finish(&states[i].base);
	}
	free(states);
	wlr_output_configuration_v1_destroy(config);
}

static void
handle_apply(struct wl_listener *listener, void *data)
{
	struct wm_output_management *mgmt =
		wl_container_of(listener, mgmt, apply);
	struct wlr_output_configuration_v1 *config = data;

	wlr_log(WLR_INFO, "output management: applying configuration");
	handle_output_configuration(mgmt->server, config, false);
}

static void
handle_test(struct wl_listener *listener, void *data)
{
	struct wm_output_management *mgmt =
		wl_container_of(listener, mgmt, test);
	struct wlr_output_configuration_v1 *config = data;

	wlr_log(WLR_DEBUG, "output management: testing configuration");
	handle_output_configuration(mgmt->server, config, true);
}

static void
handle_manager_destroy(struct wl_listener *listener, void *data)
{
	struct wm_output_management *mgmt =
		wl_container_of(listener, mgmt, destroy);

	wl_list_remove(&mgmt->apply.link);
	wl_list_remove(&mgmt->test.link);
	wl_list_remove(&mgmt->destroy.link);
	wl_list_remove(&mgmt->layout_change.link);
	mgmt->manager = NULL;
}

static void
handle_layout_change(struct wl_listener *listener, void *data)
{
	struct wm_output_management *mgmt =
		wl_container_of(listener, mgmt, layout_change);

	wm_output_management_update(mgmt->server);
}

void
wm_output_management_init(struct wm_server *server)
{
	struct wm_output_management *mgmt = &server->output_mgmt;
	mgmt->server = server;

	mgmt->manager = wlr_output_manager_v1_create(server->wl_display);
	if (!mgmt->manager) {
		wlr_log(WLR_ERROR, "failed to create output manager");
		return;
	}

	mgmt->apply.notify = handle_apply;
	wl_signal_add(&mgmt->manager->events.apply, &mgmt->apply);

	mgmt->test.notify = handle_test;
	wl_signal_add(&mgmt->manager->events.test, &mgmt->test);

	mgmt->destroy.notify = handle_manager_destroy;
	wl_signal_add(&mgmt->manager->events.destroy, &mgmt->destroy);

	/* Listen for layout changes to push updated configs to clients */
	mgmt->layout_change.notify = handle_layout_change;
	wl_signal_add(&server->output_layout->events.change,
		&mgmt->layout_change);

	wlr_log(WLR_INFO, "output management protocol initialized");
}

void
wm_output_management_finish(struct wm_server *server)
{
	struct wm_output_management *mgmt = &server->output_mgmt;
	if (!mgmt->manager) {
		return;
	}

	wl_list_remove(&mgmt->apply.link);
	wl_list_remove(&mgmt->test.link);
	wl_list_remove(&mgmt->destroy.link);
	wl_list_remove(&mgmt->layout_change.link);
	/* Manager is destroyed by wl_display_destroy */
	mgmt->manager = NULL;
}

void
wm_output_management_update(struct wm_server *server)
{
	struct wm_output_management *mgmt = &server->output_mgmt;
	if (!mgmt->manager) {
		return;
	}

	struct wlr_output_configuration_v1 *config =
		build_current_configuration(server);
	if (!config) {
		wlr_log(WLR_ERROR, "output management: failed to build configuration");
		return;
	}

	wlr_output_manager_v1_set_configuration(mgmt->manager, config);
}
