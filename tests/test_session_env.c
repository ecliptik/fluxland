/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * test_session_env.c - Desktop session environment
 */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "session_env.h"

/* util.c stand-in: record what would be spawned */
static int g_spawn_count;
void
wm_spawn(void (*exec_fn)(void *), void *data)
{
	(void)exec_fn;
	(void)data;
	g_spawn_count++;
}

static void
test_defaults_keep_existing(void)
{
	unsetenv("XDG_CURRENT_DESKTOP");
	setenv("XDG_SESSION_TYPE", "tty", 1);
	unsetenv("QT_QPA_PLATFORM");
	wm_session_env_defaults();
	assert(strcmp(getenv("XDG_CURRENT_DESKTOP"), "fluxland:wlroots") == 0);
	/* Already set by the display manager or user: left alone */
	assert(strcmp(getenv("XDG_SESSION_TYPE"), "tty") == 0);
	assert(strcmp(getenv("QT_QPA_PLATFORM"), "wayland;xcb") == 0);

	setenv("XDG_CURRENT_DESKTOP", "mine", 1);
	wm_session_env_defaults();
	assert(strcmp(getenv("XDG_CURRENT_DESKTOP"), "mine") == 0);
	printf("  PASS: defaults_keep_existing\n");
}

static void
test_should_export(void)
{
	/* Auto: only for a real session */
	assert(wm_session_env_should_export(WM_ACTIVATION_ENV_AUTO,
		false, false));
	assert(!wm_session_env_should_export(WM_ACTIVATION_ENV_AUTO,
		true, false));
	assert(!wm_session_env_should_export(WM_ACTIVATION_ENV_AUTO,
		false, true));
	assert(wm_session_env_should_export(WM_ACTIVATION_ENV_ALWAYS,
		true, true));
	assert(!wm_session_env_should_export(WM_ACTIVATION_ENV_NEVER,
		false, false));
	printf("  PASS: should_export\n");
}

static void
test_argv(void)
{
	char *argv[16];
	char buf[256];

	setenv("WAYLAND_DISPLAY", "wayland-1", 1);
	unsetenv("DISPLAY");
	setenv("XDG_CURRENT_DESKTOP", "fluxland:wlroots", 1);
	setenv("XDG_SESSION_TYPE", "wayland", 1);
	setenv("FLUXLAND_SOCK", "/run/x.sock", 1);

	/* Export: names of the set variables only */
	int n = wm_session_env_argv(false, argv, buf, sizeof(buf), 16);
	assert(n == 6);
	assert(strcmp(argv[0], "dbus-update-activation-environment") == 0);
	assert(strcmp(argv[1], "--systemd") == 0);
	assert(strcmp(argv[2], "WAYLAND_DISPLAY") == 0);
	assert(strcmp(argv[3], "XDG_CURRENT_DESKTOP") == 0);
	assert(strcmp(argv[5], "FLUXLAND_SOCK") == 0);
	assert(argv[6] == NULL);

	/* Clear: every variable emptied */
	n = wm_session_env_argv(true, argv, buf, sizeof(buf), 16);
	assert(n == 7);
	assert(strcmp(argv[2], "WAYLAND_DISPLAY=") == 0);
	assert(strcmp(argv[3], "DISPLAY=") == 0);
	assert(strcmp(argv[6], "FLUXLAND_SOCK=") == 0);

	/* Too small */
	assert(wm_session_env_argv(true, argv, buf, sizeof(buf), 4) == -1);
	assert(wm_session_env_argv(true, argv, buf, 10, 16) == -1);

	g_spawn_count = 0;
	wm_session_env_export(false);
	wm_session_env_export(true);
	assert(g_spawn_count == 2);
	printf("  PASS: argv\n");
}

int
main(void)
{
	printf("session_env tests:\n");
	test_defaults_keep_existing();
	test_should_export();
	test_argv();
	printf("All session_env tests passed.\n");
	return 0;
}
