/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * fractional_scale.h - wp-fractional-scale-v1 protocol support
 */

#ifndef WM_FRACTIONAL_SCALE_H
#define WM_FRACTIONAL_SCALE_H

struct wm_server;

/* Initialize fractional scale manager protocol */
void wm_fractional_scale_init(struct wm_server *server);

#endif /* WM_FRACTIONAL_SCALE_H */
