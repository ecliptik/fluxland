/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * fuzz_stubs.c - Execution-side symbols for parser fuzz targets
 *
 * The key/mouse binding parsers link command.c and client_pattern.c,
 * which reference the action dispatcher and a few view helpers. The
 * fuzzers only parse, so these are never called.
 */

#include <stdbool.h>
#include <stddef.h>

#include "keybind.h"

struct wm_server;
struct wm_view;
struct wm_workspace;
struct wlr_box;

bool wm_execute_action_on(struct wm_server *server, enum wm_action action,
	const char *argument, struct wm_view *target);
void wm_view_get_geometry(struct wm_view *view, struct wlr_box *box);
struct wm_workspace *wm_workspace_get_active(struct wm_server *server);
struct wm_view *wm_view_parent(struct wm_view *view);

bool
wm_execute_action_on(struct wm_server *server, enum wm_action action,
	const char *argument, struct wm_view *target)
{
	(void)server; (void)action; (void)argument; (void)target;
	return true;
}

void
wm_view_get_geometry(struct wm_view *view, struct wlr_box *box)
{
	(void)view; (void)box;
}

struct wm_workspace *
wm_workspace_get_active(struct wm_server *server)
{
	(void)server;
	return NULL;
}

struct wm_view *
wm_view_parent(struct wm_view *view)
{
	(void)view;
	return NULL;
}
