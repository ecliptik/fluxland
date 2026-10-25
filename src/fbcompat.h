/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * fbcompat.h - Fluxbox argument formats shared by the apps file and commands
 *
 * Sizes and positions ("100", "50%", "*"), reference corners (TopLeft,
 * Center, BottomRight...), layer names and numbers, decoration presets
 * and masks, and focus protection flags, read the way Fluxbox reads them.
 */

#ifndef WM_FBCOMPAT_H
#define WM_FBCOMPAT_H

#include <stdbool.h>
#include <wlr/util/box.h>

#include "decoration.h"
#include "view_layer.h"

/* A size or position: "100" pixels, "50%" of the head, "*" keep as is */
struct wm_size_token {
	int value;
	bool relative;  /* "%": percent of the head's usable area */
	bool ignore;    /* "*": leave this coordinate alone */
};

/* Parse one token; false if it is not a number, a percentage or "*" */
bool wm_size_token_parse(const char *str, struct wm_size_token *out);

/* The token in pixels, given the extent a percentage refers to */
int wm_size_token_pixels(const struct wm_size_token *tok, int extent);

/* Parse "W H" (two tokens); false unless there are exactly two */
bool wm_size_pair_parse(const char *str, struct wm_size_token *a,
	struct wm_size_token *b);

/* Fluxbox's ReferenceCorner */
enum wm_corner {
	WM_CORNER_TOPLEFT,
	WM_CORNER_TOP,
	WM_CORNER_TOPRIGHT,
	WM_CORNER_LEFT,
	WM_CORNER_CENTER,
	WM_CORNER_RIGHT,
	WM_CORNER_BOTTOMLEFT,
	WM_CORNER_BOTTOM,
	WM_CORNER_BOTTOMRIGHT,
};

/* TopLeft/UpperLeft/LeftTop, Top/Upper/TopCenter, ..., Center/WinCenter */
bool wm_corner_from_name(const char *name, enum wm_corner *out);
const char *wm_corner_name(enum wm_corner corner);

/*
 * Where a frame of width w (or height h) goes for offset x (or y) from a
 * corner of area: offsets from the right or bottom edge count inwards,
 * as in Fluxbox's translateCoords().
 */
int wm_corner_x(enum wm_corner corner, const struct wlr_box *area,
	int w, int x);
int wm_corner_y(enum wm_corner corner, const struct wlr_box *area,
	int h, int y);

/*
 * A Fluxbox layer: a name (Menu, AboveDock, Dock, Top, Normal, Bottom,
 * Desktop; fluxland's Above and Below too) or a Fluxbox layer number
 * (0 Menu ... 8 Normal ... 12 Desktop), mapped onto fluxland's layers.
 */
bool wm_layer_parse(const char *str, enum wm_view_layer *out);
/* The Fluxbox layer number for a fluxland layer (8 for Normal) */
int wm_layer_fluxbox_number(enum wm_view_layer layer);

/* A [Deco] or SetDecor value: a preset name or a Fluxbox bitmask */
bool wm_decor_parse(const char *str, enum wm_decor_preset *out);

/* Fluxbox focus protection flags */
#define WM_FOCUS_PROT_GAIN   0x01  /* takes focus when it opens */
#define WM_FOCUS_PROT_REFUSE 0x02  /* doesn't take focus when it opens */
#define WM_FOCUS_PROT_DENY   0x04  /* never takes focus by itself */
#define WM_FOCUS_PROT_LOCK   0x08  /* no other window takes focus from it */

/*
 * Parse "Gain, Lock" etc. into *prot (contradicting flags: the last one
 * wins; None clears). False if a word is not a flag.
 */
bool wm_focus_protection_parse(const char *str, int *prot);

#endif /* WM_FBCOMPAT_H */
