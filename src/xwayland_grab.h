/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * xwayland_grab.h - xwayland-keyboard-grab-unstable-v1
 *
 * On X an active keyboard grab (XGrabKeyboard) sends every key to the
 * grabbing client, which is how xlock keeps the keys it is typed a password
 * with, and what VNC viewers and virtual machines use to get Alt-Tab.  The
 * core Wayland protocol has no such thing, so Xwayland asks the compositor
 * through this protocol (only Xwayland may: the global is hidden from every
 * other client).  The grab puts the keyboard focus on the surface and, while
 * it is there, every key goes to it, with the compositor's own bindings out
 * of the way (as with a shortcuts inhibitor: the VT switch and the
 * emergency exit still work).  It ends when the grab object is destroyed
 * (XUngrabKeyboard, or the program going) or the surface does.
 *
 * session.xwayland.keyboardGrab: false makes the requests be ignored.
 */

#ifndef WM_XWAYLAND_GRAB_H
#define WM_XWAYLAND_GRAB_H

struct wm_server;
struct wlr_surface;

/* After XWayland is set up (the global is for its client alone) */
void wm_xwayland_grab_init(struct wm_server *server);
void wm_xwayland_grab_finish(struct wm_server *server);

/* The surface that has the keyboard grabbed, or NULL */
struct wlr_surface *wm_xwayland_grab_surface(struct wm_server *server);

/* Give the grabbed surface the keyboard focus again if it was moved */
void wm_xwayland_grab_focus(struct wm_server *server);

#endif /* WM_XWAYLAND_GRAB_H */
