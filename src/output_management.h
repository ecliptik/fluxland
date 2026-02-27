/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * output_management.h - wlr-output-management-unstable-v1 protocol support
 *
 * Enables display configuration tools (wlr-randr, kanshi) to query
 * and modify output settings (mode, position, scale, transform).
 */

#ifndef WM_OUTPUT_MANAGEMENT_H
#define WM_OUTPUT_MANAGEMENT_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_output_management_v1.h>

struct wm_server;

struct wm_output_management {
	struct wlr_output_manager_v1 *manager;
	struct wm_server *server;

	struct wl_listener apply;
	struct wl_listener test;
	struct wl_listener destroy;
	struct wl_listener layout_change;
};

/* Initialize output management protocol. Call after output layout is set up. */
void wm_output_management_init(struct wm_server *server);

/* Clean up output management resources. */
void wm_output_management_finish(struct wm_server *server);

/* Send the current output configuration to clients. */
void wm_output_management_update(struct wm_server *server);

#endif /* WM_OUTPUT_MANAGEMENT_H */
