/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * viewporter.c - Viewporter and single-pixel-buffer protocol support
 *
 * Viewporter allows clients to crop and scale their surface buffers.
 * Single-pixel-buffer allows clients to create solid-color buffers efficiently.
 * Both are pure protocol support — wlroots handles all buffer operations.
 */

#include <wlr/types/wlr_single_pixel_buffer_v1.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/util/log.h>

#include "server.h"

void
wm_viewporter_init(struct wm_server *server)
{
	server->viewporter = wlr_viewporter_create(server->wl_display);
	if (!server->viewporter) {
		wlr_log(WLR_ERROR, "%s", "failed to create viewporter");
		return;
	}

	server->single_pixel_buffer_mgr =
		wlr_single_pixel_buffer_manager_v1_create(server->wl_display);
	if (!server->single_pixel_buffer_mgr) {
		wlr_log(WLR_ERROR, "%s", "failed to create single-pixel-buffer manager");
		return;
	}

	wlr_log(WLR_INFO, "%s", "viewporter and single-pixel-buffer protocols enabled");
}
