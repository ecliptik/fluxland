/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * session_env.c - Desktop session environment
 *
 * Desktop portals (screen sharing, file choosers) and other D-Bus or
 * systemd user services are not children of the compositor: they learn
 * WAYLAND_DISPLAY and friends from the activation environment, and pick
 * their backends from XDG_CURRENT_DESKTOP (fluxland-portals.conf).
 */

#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wlr/util/log.h>

#include "session_env.h"
#include "util.h"

/* Variables services need to reach this session */
static const char *const exported[] = {
	"WAYLAND_DISPLAY",
	"DISPLAY",
	"XDG_CURRENT_DESKTOP",
	"XDG_SESSION_TYPE",
	"FLUXLAND_SOCK",
};
#define EXPORTED_COUNT (sizeof(exported) / sizeof(exported[0]))

void
wm_session_env_defaults(void)
{
	/* Desktop names: portals look up fluxland-, then wlroots-portals.conf */
	setenv("XDG_CURRENT_DESKTOP", "fluxland:wlroots", false);
	setenv("XDG_SESSION_TYPE", "wayland", false);
	/* Toolkits: prefer Wayland, fall back to X11 */
	setenv("QT_QPA_PLATFORM", "wayland;xcb", false);
	setenv("SDL_VIDEODRIVER", "wayland,x11", false);
	setenv("_JAVA_AWT_WM_NONREPARENTING", "1", false);
}

bool
wm_session_env_should_export(enum wm_activation_env mode, bool nested,
	bool headless)
{
	switch (mode) {
	case WM_ACTIVATION_ENV_ALWAYS:
		return true;
	case WM_ACTIVATION_ENV_NEVER:
		return false;
	case WM_ACTIVATION_ENV_AUTO:
	default:
		/*
		 * A nested or headless instance must not point the user's
		 * services (and their real session's portals) at itself.
		 */
		return !nested && !headless;
	}
}

int
wm_session_env_argv(bool clear, char **argv, char *buf, size_t buflen,
	int max)
{
	int n = 0;
	if (max < (int)EXPORTED_COUNT + 3)
		return -1;
	argv[n++] = "dbus-update-activation-environment";
	argv[n++] = "--systemd";
	size_t used = 0;
	for (size_t i = 0; i < EXPORTED_COUNT; i++) {
		if (clear) {
			/* NAME= empties it for services started later */
			int len = snprintf(buf + used, buflen - used, "%s=",
				exported[i]);
			if (len < 0 || (size_t)len >= buflen - used)
				return -1;
			argv[n++] = buf + used;
			used += (size_t)len + 1;
		} else if (getenv(exported[i])) {
			/* NAME alone exports its current value */
			argv[n++] = (char *)exported[i];
		}
	}
	argv[n] = NULL;
	return n;
}

static void
exec_argv(void *data)
{
	char **argv = data;
	execvp(argv[0], argv);
	_exit(127);
}

void
wm_session_env_export(bool clear)
{
	char *argv[EXPORTED_COUNT + 3];
	char buf[256];
	if (wm_session_env_argv(clear, argv, buf, sizeof(buf),
			(int)(sizeof(argv) / sizeof(argv[0]))) < 0)
		return;
	wlr_log(WLR_INFO, "%s the D-Bus/systemd activation environment",
		clear ? "clearing" : "updating");
	wm_spawn(exec_argv, argv);
}
