/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * atspi_bridge.h - Optional AT-SPI accessibility bridge
 *
 * When enabled (-Datspi=true), bridges compositor events to the
 * AT-SPI D-Bus bus so screen readers (Orca, BRLTTY) can track
 * window focus, menu navigation, and toolbar selection.
 */

#ifndef WM_ATSPI_BRIDGE_H
#define WM_ATSPI_BRIDGE_H

struct wm_server;

struct wm_atspi_bridge;

/* Create an AT-SPI bridge. Returns NULL if AT-SPI is disabled or
 * the D-Bus connection fails (non-fatal). */
struct wm_atspi_bridge *wm_atspi_bridge_create(struct wm_server *server);

/* Destroy the AT-SPI bridge and release D-Bus resources. */
void wm_atspi_bridge_destroy(struct wm_atspi_bridge *bridge);

/* Announce a focus change (window, toolbar, menu, slit).
 * role: short description ("window", "toolbar", "menu", "slit")
 * name: human-readable label (app_id, element name, menu label) */
void wm_atspi_announce_focus(struct wm_atspi_bridge *bridge,
	const char *role, const char *name);

#endif /* WM_ATSPI_BRIDGE_H */
