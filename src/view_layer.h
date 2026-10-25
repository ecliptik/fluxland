/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * view_layer.h - Window layers
 */

#ifndef WM_VIEW_LAYER_H
#define WM_VIEW_LAYER_H

/* Fluxbox-inspired window layers (bottom to top) */
enum wm_view_layer {
	WM_LAYER_DESKTOP = 0,
	WM_LAYER_BELOW,
	WM_LAYER_NORMAL,
	WM_LAYER_ABOVE,
	WM_VIEW_LAYER_COUNT, /* sentinel */
};

#endif /* WM_VIEW_LAYER_H */
