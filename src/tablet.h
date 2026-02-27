/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * tablet.h - Tablet/stylus input support via tablet-v2 protocol
 */

#ifndef WM_TABLET_H
#define WM_TABLET_H

struct wm_server;
struct wlr_input_device;

/* Initialize tablet manager and register tablet event handlers */
void wm_tablet_init(struct wm_server *server);

/* Clean up tablet resources */
void wm_tablet_finish(struct wm_server *server);

/* Set up a new tablet tool (stylus) device */
void wm_tablet_tool_setup(struct wm_server *server,
	struct wlr_input_device *device);

/* Set up a new tablet pad device */
void wm_tablet_pad_setup(struct wm_server *server,
	struct wlr_input_device *device);

#endif /* WM_TABLET_H */
