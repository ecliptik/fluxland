/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * eis.h - Input emulation for X11 programs (an EIS server for XWayland)
 *
 * X11 programs inject pointer motion, buttons and keys with the XTest
 * extension: Steam Input's desktop controls, xdotool, remote desktops.
 * Under Xorg that is the real input; under Xwayland it can reach only
 * Xwayland itself, unless the compositor takes it.  Xwayland sends it
 * to an EIS server (libei's compositor side) when it finds one at
 * $LIBEI_SOCKET; this is that server, as KWin's and GNOME's are.
 *
 * What arrives is emulated input devices of the compositor's: a pointer
 * and a keyboard, so it reaches Wayland windows and fluxland's own
 * bindings like any other input.  Which programs may do it is
 * session.xwayland.inputEmulation: ask (the first time a program does,
 * a menu asks), allow, or deny, with allow and deny lists of names.
 *
 * Without libei (or XWayland) these functions do nothing.
 */

#ifndef WM_EIS_H
#define WM_EIS_H

struct wm_server;
struct wm_eis;

/* Before XWayland can start: it finds the server through the environment */
void wm_eis_init(struct wm_server *server);

void wm_eis_finish(struct wm_server *server);

/* The session locked or unlocked: emulated input stops while locked */
void wm_eis_session_lock_changed(struct wm_server *server);

/* The outputs changed: the area absolute motion may reach is theirs */
void wm_eis_layout_changed(struct wm_server *server);

#endif /* WM_EIS_H */
