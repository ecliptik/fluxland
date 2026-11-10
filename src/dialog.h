/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * dialog.h - Fluxbox's text dialogs: CommandDialog, SetTitleDialog,
 * SetWorkspaceNameDialog, SetResourceValueDialog
 */

#ifndef WM_DIALOG_H
#define WM_DIALOG_H

#include <stdbool.h>
#include <stdint.h>
#include <xkbcommon/xkbcommon.h>

struct wm_server;
struct wm_view;

/*
 * Open a dialog (closing one that is open), a window in the middle of the
 * head under the pointer: a title, and a line to type in, starting with
 * text.  Return runs precommand followed by the line as a command on
 * target (the focused window when NULL); Escape closes it.
 */
void wm_dialog_open(struct wm_server *server, const char *title,
	const char *precommand, const char *text, struct wm_view *target);

void wm_dialog_close(struct wm_server *server);

/* A window going away: its SetTitleDialog closes */
void wm_dialog_view_destroyed(struct wm_server *server, struct wm_view *view);

#endif /* WM_DIALOG_H */
