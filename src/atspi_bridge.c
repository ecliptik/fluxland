/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * atspi_bridge.c - Optional AT-SPI accessibility bridge
 *
 * When compiled with -DWM_HAS_ATSPI, connects to the AT-SPI D-Bus
 * bus and emits focus:object signals. Otherwise provides no-op stubs.
 */

#define _POSIX_C_SOURCE 200809L

#include <stddef.h>
#include "atspi_bridge.h"

#ifdef WM_HAS_ATSPI

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <systemd/sd-bus.h>
#include <wlr/util/log.h>

#include "server.h"

struct wm_atspi_bridge {
	struct wm_server *server;
	sd_bus *bus;
};

struct wm_atspi_bridge *
wm_atspi_bridge_create(struct wm_server *server)
{
	struct wm_atspi_bridge *bridge = calloc(1, sizeof(*bridge));
	if (!bridge) {
		return NULL;
	}
	bridge->server = server;

	int r = sd_bus_open_user(&bridge->bus);
	if (r < 0) {
		wlr_log(WLR_INFO, "AT-SPI: cannot connect to user bus: %s",
			strerror(-r));
		free(bridge);
		return NULL;
	}

	wlr_log(WLR_INFO, "AT-SPI accessibility bridge enabled");
	return bridge;
}

void
wm_atspi_bridge_destroy(struct wm_atspi_bridge *bridge)
{
	if (!bridge) {
		return;
	}
	if (bridge->bus) {
		sd_bus_unref(bridge->bus);
	}
	free(bridge);
}

void
wm_atspi_announce_focus(struct wm_atspi_bridge *bridge,
	const char *role, const char *name)
{
	if (!bridge || !bridge->bus) {
		return;
	}

	/* Emit AT-SPI focus event on the accessibility bus.
	 * The signal path and interface follow the AT-SPI2 spec. */
	int r = sd_bus_emit_signal(
		bridge->bus,
		"/org/a11y/atspi/accessible/fluxland",
		"org.a11y.atspi.Event.Focus",
		"Focus",
		"siiv(so)",
		"focus:", 0, 0, "s", name ? name : "",
		"/org/a11y/atspi/accessible/fluxland",
		"org.a11y.atspi.Accessible");
	if (r < 0) {
		wlr_log(WLR_DEBUG, "AT-SPI: failed to emit focus signal: %s",
			strerror(-r));
	}
}

#else /* !WM_HAS_ATSPI */

struct wm_atspi_bridge {
	int dummy;
};

struct wm_atspi_bridge *
wm_atspi_bridge_create(struct wm_server *server)
{
	(void)server;
	return NULL;
}

void
wm_atspi_bridge_destroy(struct wm_atspi_bridge *bridge)
{
	(void)bridge;
}

void
wm_atspi_announce_focus(struct wm_atspi_bridge *bridge,
	const char *role, const char *name)
{
	(void)bridge;
	(void)role;
	(void)name;
}

#endif /* WM_HAS_ATSPI */
