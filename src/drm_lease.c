/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * drm_lease.c - wp-drm-lease-v1 protocol support
 *
 * Allows clients (VR runtimes like Monado/OpenXR) to lease DRM
 * connectors for direct display access. The compositor grants or
 * rejects lease requests for individual outputs.
 */

#define _POSIX_C_SOURCE 200809L
#include <wlr/types/wlr_drm_lease_v1.h>
#include <wlr/util/log.h>

#include "drm_lease.h"
#include "server.h"

static void
handle_drm_lease_request(struct wl_listener *listener, void *data)
{
	struct wm_server *server =
		wl_container_of(listener, server, drm_lease_request);
	struct wlr_drm_lease_request_v1 *request = data;

	/*
	 * Grant the lease request. The client will get direct access
	 * to the DRM connector. The output will be destroyed and
	 * re-created when the lease ends.
	 */
	struct wlr_drm_lease_v1 *lease =
		wlr_drm_lease_request_v1_grant(request);
	if (!lease) {
		wlr_log(WLR_ERROR, "%s",
			"drm lease: failed to grant lease request");
		wlr_drm_lease_request_v1_reject(request);
		return;
	}

	wlr_log(WLR_INFO, "drm lease: granted lease with %zu connectors",
		lease->n_connectors);
}

void
wm_drm_lease_init(struct wm_server *server)
{
	/*
	 * Create the DRM lease manager. This returns NULL if the
	 * backend doesn't support DRM (e.g. when running nested
	 * in a Wayland or X11 session).
	 */
	server->drm_lease_mgr = wlr_drm_lease_v1_manager_create(
		server->wl_display, server->backend);
	if (!server->drm_lease_mgr) {
		wlr_log(WLR_INFO, "%s",
			"drm lease: not available (no DRM backend)");
		return;
	}

	server->drm_lease_request.notify = handle_drm_lease_request;
	wl_signal_add(&server->drm_lease_mgr->events.request,
		&server->drm_lease_request);

	wlr_log(WLR_INFO, "%s",
		"initialized wp-drm-lease-v1 protocol");
}

void
wm_drm_lease_finish(struct wm_server *server)
{
	if (server->drm_lease_mgr) {
		wl_list_remove(&server->drm_lease_request.link);
	}
}

void
wm_drm_lease_offer_output(struct wm_server *server,
	struct wlr_output *output)
{
	if (!server->drm_lease_mgr) {
		return;
	}

	if (!wlr_drm_lease_v1_manager_offer_output(
			server->drm_lease_mgr, output)) {
		wlr_log(WLR_DEBUG, "drm lease: cannot offer output '%s'",
			output->name ? output->name : "?");
	}
}
