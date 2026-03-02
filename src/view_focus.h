/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * view_focus.h - Internal focus management helpers
 */

#ifndef WM_VIEW_FOCUS_H
#define WM_VIEW_FOCUS_H

#include "view.h"

struct wlr_scene_tree;

/* De-iconify (restore) a minimized view: re-enable, focus, raise. */
void deiconify_view(struct wm_view *view);

/* Look up the scene tree for a given layer. */
struct wlr_scene_tree *get_layer_tree(struct wm_server *server,
	enum wm_view_layer layer);

#endif /* WM_VIEW_FOCUS_H */
