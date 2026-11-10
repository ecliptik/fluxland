/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * view_internal.h - windows the compositor draws itself (the backend of
 * the text dialogs)
 *
 * An internal window is a wm_view like any other: it is framed by
 * decoration.c, has a title and the style's buttons, takes focus, and is
 * in the iconbar and the window lists.  Its content is a buffer the owner
 * draws, and while it is focused the keys typed go to its owner, not to a
 * client.
 */

#ifndef WM_VIEW_INTERNAL_H
#define WM_VIEW_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>
#include <xkbcommon/xkbcommon.h>

struct wm_server;
struct wm_view;
struct wlr_buffer;

struct wm_internal_ops {
	/* A key typed while it is focused; true when it took it.  utf8 is
	 * the text the key types, if any */
	bool (*key)(void *data, xkb_keysym_t sym, const char *utf8,
		uint32_t modifiers);
	/* The window was closed (its X, Close): the owner lets go of it;
	 * the window is destroyed after this returns */
	void (*closed)(void *data);
};

/*
 * Make and show a window of content size width x height (it cannot be
 * resized), in the middle of the head under the pointer, and focus it.
 * app_id is what window rules match it by.  NULL if it could not be made.
 */
struct wm_view *wm_internal_view_create(struct wm_server *server,
	const char *title, const char *app_id, int width, int height,
	const struct wm_internal_ops *ops, void *data);

/* The content to show (the window keeps no reference to buffer) */
void wm_internal_view_set_buffer(struct wm_view *view,
	struct wlr_buffer *buffer);

/* Unmap and free it; the owner's closed() is not called */
void wm_internal_view_destroy(struct wm_view *view);

/* A key for view if it is an internal window: true when it was taken.
 * The window may be gone when this returns */
bool wm_internal_view_key(struct wm_view *view, xkb_keysym_t sym,
	const char *utf8, uint32_t modifiers);

#endif /* WM_VIEW_INTERNAL_H */
