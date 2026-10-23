/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * session_env.h - Desktop session environment
 */

#ifndef WM_SESSION_ENV_H
#define WM_SESSION_ENV_H

#include <stdbool.h>
#include <stddef.h>

/* session.updateActivationEnvironment */
enum wm_activation_env {
	WM_ACTIVATION_ENV_AUTO,   /* only for a real (not nested) session */
	WM_ACTIVATION_ENV_ALWAYS,
	WM_ACTIVATION_ENV_NEVER,
};

/*
 * XDG_CURRENT_DESKTOP=fluxland:wlroots, XDG_SESSION_TYPE=wayland and
 * toolkit backend hints, each only when not already set.
 */
void wm_session_env_defaults(void);

/* Whether to export to the D-Bus/systemd activation environment */
bool wm_session_env_should_export(enum wm_activation_env mode, bool nested,
	bool headless);

/*
 * dbus-update-activation-environment --systemd command line: the set
 * session variables, or (clear) all of them emptied. Strings for clear
 * go in buf. Returns argc, or -1 if argv/buf are too small.
 */
int wm_session_env_argv(bool clear, char **argv, char *buf, size_t buflen,
	int max);

/* Run it (double-forked, not waited for) */
void wm_session_env_export(bool clear);

#endif /* WM_SESSION_ENV_H */
