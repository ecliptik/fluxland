/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
 * gamma_control.c - Gamma control protocol for color temperature tools
 *
 * Enables tools like gammastep, wlsunset, and redshift-wayland to
 * adjust display color temperature via wlr-gamma-control-unstable-v1.
 *
 * wlroots handles the protocol details and gamma ramp storage; the
 * compositor hooks the set_gamma event to apply gamma ramps during
 * output commits.
 */

#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>

#include "gamma_control.h"
#include "output.h"
#include "server.h"

static void
handle_set_gamma(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, gamma_set_gamma);
	const struct wlr_gamma_control_manager_v1_set_gamma_event *event = data;

	struct wlr_output *output = event->output;

	/*
	 * Build an output state from the scene, apply the gamma ramp,
	 * and commit. This integrates with the scene graph so damage
	 * tracking and other state remain consistent.
	 */
	struct wlr_output_state state;
	wlr_output_state_init(&state);

	struct wm_output *wm_output;
	wl_list_for_each(wm_output, &server->outputs, link) {
		if (wm_output->wlr_output == output) {
			if (!wlr_scene_output_build_state(
					wm_output->scene_output,
					&state, NULL)) {
				wlr_log(WLR_ERROR,
					"%s", "gamma: failed to build output state");
				goto finish;
			}
			break;
		}
	}

	if (event->control) {
		if (!wlr_gamma_control_v1_apply(event->control, &state)) {
			wlr_log(WLR_ERROR, "%s", "gamma: failed to apply gamma ramp");
			goto finish;
		}
	}

	if (!wlr_output_commit_state(output, &state)) {
		wlr_log(WLR_ERROR, "%s", "gamma: failed to commit output state");
		if (event->control) {
			wlr_gamma_control_v1_send_failed_and_destroy(
				event->control);
		}
	}

finish:
	wlr_output_state_finish(&state);
}

void
wm_gamma_control_init(struct wm_server *server)
{
	server->gamma_control_mgr =
		wlr_gamma_control_manager_v1_create(server->wl_display);
	if (!server->gamma_control_mgr) {
		wlr_log(WLR_ERROR, "%s", "failed to create gamma control manager");
		return;
	}

	server->gamma_set_gamma.notify = handle_set_gamma;
	wl_signal_add(&server->gamma_control_mgr->events.set_gamma,
		&server->gamma_set_gamma);

	wlr_log(WLR_INFO, "%s", "gamma control protocol enabled");
}
