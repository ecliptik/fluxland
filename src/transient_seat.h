/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * transient_seat.h - ext-transient-seat-v1 protocol support
 */

#ifndef WM_TRANSIENT_SEAT_H
#define WM_TRANSIENT_SEAT_H

struct wm_server;

/* Initialize the transient seat protocol */
void wm_transient_seat_init(struct wm_server *server);

/* Clean up transient seat resources */
void wm_transient_seat_finish(struct wm_server *server);

#endif /* WM_TRANSIENT_SEAT_H */
