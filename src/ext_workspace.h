/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * ext_workspace.h - ext-workspace-v1: workspaces for panels and taskbars
 */

#ifndef WM_EXT_WORKSPACE_H
#define WM_EXT_WORKSPACE_H

struct wm_server;
struct wm_ext_workspace;
struct wlr_output;

void wm_ext_workspace_init(struct wm_server *server);
void wm_ext_workspace_finish(struct wm_server *server);

/*
 * Workspaces, the shown workspace(s), names or urgency changed: tell
 * panels (once, from an idle callback).
 */
void wm_ext_workspace_schedule(struct wm_server *server);

/* Bring the handles up to date now */
void wm_ext_workspace_sync(struct wm_ext_workspace *ew);

/* The workspace group is shown on every output */
void wm_ext_workspace_output_enter(struct wm_server *server,
	struct wlr_output *output);
void wm_ext_workspace_output_leave(struct wm_server *server,
	struct wlr_output *output);

#endif /* WM_EXT_WORKSPACE_H */
