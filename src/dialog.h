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
 * Open a dialog (closing one that is open) in the middle of the head
 * under the pointer: a title, and a line to type in, starting with
 * text.  Return runs precommand followed by the line as a command on
 * target (the focused window when NULL); Escape closes it.
 */
void wm_dialog_open(struct wm_server *server, const char *title,
	const char *precommand, const char *text, struct wm_view *target);

void wm_dialog_close(struct wm_server *server);

bool wm_dialog_is_open(struct wm_server *server);

/* A key while a dialog is open: true when it took it (it takes them
 * all); utf8 is the text the key types, if any */
bool wm_dialog_handle_key(struct wm_server *server, xkb_keysym_t sym,
	const char *utf8, uint32_t modifiers);

/* A pointer button (layout coordinates): its title drags it, a click
 * elsewhere closes it.  True when the dialog took the press. */
bool wm_dialog_handle_button(struct wm_server *server, double lx, double ly,
	bool pressed);

/* Pointer motion: moves the dialog while its title is held */
bool wm_dialog_handle_motion(struct wm_server *server, double lx, double ly);

/* A window going away: its SetTitleDialog closes */
void wm_dialog_view_destroyed(struct wm_server *server, struct wm_view *view);

#endif /* WM_DIALOG_H */
