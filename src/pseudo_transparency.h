/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * pseudo_transparency.h - A translucent window shows the desktop behind it,
 * not the windows
 *
 * A window that is pseudo-transparent has, under its content, a copy of the
 * part of the desktop (the background layer: a wallpaper client such as
 * swaybg, or fluxland's own) that it covers.  What the window draws with
 * some transparency is blended with that and with nothing else, as with
 * Fluxbox's root pixmap, and a window below it is not seen.
 */

#ifndef WM_PSEUDO_TRANSPARENCY_H
#define WM_PSEUDO_TRANSPARENCY_H

#include <stdbool.h>
#include <wlr/util/box.h>

struct wm_view;
struct wm_server;

/* The window's own say (an apps rule): follow the global setting, or not */
enum wm_pseudo_rule {
	WM_PSEUDO_FOLLOW = 0,
	WM_PSEUDO_YES,
	WM_PSEUDO_NO,
};

/*
 * What of a piece of the desktop (a buffer placed at `node`, of which `src`
 * is shown, in buffer pixels) lies under a window: the part inside
 * `window` as a box in layout coordinates, and the part of the buffer
 * that is.  False when none does.
 */
bool wm_pseudo_clip(const struct wlr_box *window, const struct wlr_box *node,
	const struct wlr_fbox *src, struct wlr_box *inter,
	struct wlr_fbox *inter_src);

/* Whether the window is pseudo-transparent (its rule, else the global) */
bool wm_view_pseudo_transparent(const struct wm_view *view);

/* Bring the window's copy of the desktop up to date: made, moved or
 * removed as it should be */
void wm_pseudo_update(struct wm_view *view);

/* The same for every window: the desktop or the setting changed */
void wm_pseudo_update_all(struct wm_server *server);

/*
 * Before each frame: windows that have moved or changed size since their
 * copy of the desktop was made get a new one.  This is what follows a window
 * dragged by hand (the pointer code moves it without going through
 * wm_view_move()), animated, tiled or moved by any other path; a window that
 * has not moved costs a comparison.
 */
void wm_pseudo_sync(struct wm_server *server);

#endif /* WM_PSEUDO_TRANSPARENCY_H */
