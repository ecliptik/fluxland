/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * tray_grant.h - Windows that may take the focus because of a tray click
 *
 * An application that gets no xdg-activation token (an appindicator item has
 * no method for one) asks for its window to be raised when the user chooses
 * something in its tray menu, and nothing tells the compositor that the
 * request is the answer to the click.  With session.screen0.systray.
 * activationGrace set, the compositor allows a request from the item's
 * program, or one it started, from the click until it has been answered
 * (the reply to the call that passed the click on) and a short grace after:
 * the time the program takes to ask on its own channel.
 */

#ifndef WM_TRAY_GRANT_H
#define WM_TRAY_GRANT_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

/* The longest a click stays unanswered and still counts (ms) */
#define WM_TRAY_GRANT_PENDING_MAX_MS 5000

/*
 * A click was passed on to the program with this pid: returns the id of the
 * grant, 0 for none (grace_ms <= 0: off, or no pid).  Until it is ended
 * the window it allows lasts WM_TRAY_GRANT_PENDING_MAX_MS.
 */
uint64_t wm_tray_grant_begin(pid_t pid, int64_t now_ms, int grace_ms);

/* The call that passed the click on was answered: grace_ms more from now */
void wm_tray_grant_end(uint64_t id, int64_t now_ms);

/*
 * May a window of this pid take the focus: it or an ancestor of it is the
 * program a click was passed on to, and the grant has not run out.
 */
bool wm_tray_grant_allows(pid_t window_pid, int64_t now_ms);

/* The same time as the above takes, milliseconds on the monotonic clock */
int64_t wm_tray_grant_now_ms(void);

/* Is pid the same as ancestor, or one of its descendants (/proc) */
bool wm_pid_is_descendant(pid_t pid, pid_t ancestor);

/* Forget every grant (tests, and on shutdown) */
void wm_tray_grant_clear(void);

#endif /* WM_TRAY_GRANT_H */
