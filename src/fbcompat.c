/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * fbcompat.c - Fluxbox argument formats shared by the apps file and commands
 *
 * Each parser follows the Fluxbox function it is named after in the
 * comment: same words, same aliases, same arithmetic.
 */

#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "fbcompat.h"

/* --- Sizes (FbTk::StringUtil::parseSizeToken) --- */

bool
wm_size_token_parse(const char *str, struct wm_size_token *out)
{
	out->value = 0;
	out->relative = false;
	out->ignore = false;
	if (!str || !*str)
		return false;
	if (str[0] == '*' && str[1] == '\0') {
		out->ignore = true;
		return true;
	}
	char *end;
	errno = 0;
	long v = strtol(str, &end, 10);
	if (end == str || errno || v < INT_MIN || v > INT_MAX)
		return false;
	if (*end == '%') {
		out->relative = true;
		end++;
	}
	if (*end != '\0')
		return false;
	out->value = (int)v;
	return true;
}

int
wm_size_token_pixels(const struct wm_size_token *tok, int extent)
{
	if (!tok->relative)
		return tok->value;
	/* FbTk::RelCalcHelper::calPercentageValueOf: rounded */
	long long v = (long long)tok->value * extent;
	return (int)(v >= 0 ? (v + 50) / 100 : (v - 50) / 100);
}

bool
wm_size_pair_parse(const char *str, struct wm_size_token *a,
	struct wm_size_token *b)
{
	if (!str)
		return false;
	char buf[128];
	snprintf(buf, sizeof(buf), "%s", str);
	char *save = NULL;
	char *t1 = strtok_r(buf, " \t", &save);
	char *t2 = t1 ? strtok_r(NULL, " \t", &save) : NULL;
	char *t3 = t2 ? strtok_r(NULL, " \t", &save) : NULL;
	if (!t2 || t3)
		return false;
	return wm_size_token_parse(t1, a) && wm_size_token_parse(t2, b);
}

/* --- Reference corners (FluxboxWindow::getCorner, translateCoords) --- */

static const struct {
	const char *name;
	enum wm_corner corner;
} corner_names[] = {
	{ "TopLeft",      WM_CORNER_TOPLEFT },
	{ "LeftTop",      WM_CORNER_TOPLEFT },
	{ "UpperLeft",    WM_CORNER_TOPLEFT },
	{ "Top",          WM_CORNER_TOP },
	{ "Upper",        WM_CORNER_TOP },
	{ "TopCenter",    WM_CORNER_TOP },
	{ "TopRight",     WM_CORNER_TOPRIGHT },
	{ "RightTop",     WM_CORNER_TOPRIGHT },
	{ "UpperRight",   WM_CORNER_TOPRIGHT },
	{ "Left",         WM_CORNER_LEFT },
	{ "LeftCenter",   WM_CORNER_LEFT },
	{ "Center",       WM_CORNER_CENTER },
	{ "WinCenter",    WM_CORNER_CENTER },
	{ "Right",        WM_CORNER_RIGHT },
	{ "RightCenter",  WM_CORNER_RIGHT },
	{ "BottomLeft",   WM_CORNER_BOTTOMLEFT },
	{ "LeftBottom",   WM_CORNER_BOTTOMLEFT },
	{ "LowerLeft",    WM_CORNER_BOTTOMLEFT },
	{ "Bottom",       WM_CORNER_BOTTOM },
	{ "Lower",        WM_CORNER_BOTTOM },
	{ "BottomCenter", WM_CORNER_BOTTOM },
	{ "BottomRight",  WM_CORNER_BOTTOMRIGHT },
	{ "RightBottom",  WM_CORNER_BOTTOMRIGHT },
	{ "LowerRight",   WM_CORNER_BOTTOMRIGHT },
};

bool
wm_corner_from_name(const char *name, enum wm_corner *out)
{
	if (!name || !*name) {
		*out = WM_CORNER_TOPLEFT;
		return true;
	}
	for (size_t i = 0; i < sizeof(corner_names) / sizeof(corner_names[0]);
	     i++) {
		if (strcasecmp(name, corner_names[i].name) == 0) {
			*out = corner_names[i].corner;
			return true;
		}
	}
	return false;
}

const char *
wm_corner_name(enum wm_corner corner)
{
	for (size_t i = 0; i < sizeof(corner_names) / sizeof(corner_names[0]);
	     i++) {
		if (corner_names[i].corner == corner)
			return corner_names[i].name;
	}
	return "TopLeft";
}

int
wm_corner_x(enum wm_corner corner, const struct wlr_box *area, int w, int x)
{
	switch (corner) {
	case WM_CORNER_TOPLEFT:
	case WM_CORNER_LEFT:
	case WM_CORNER_BOTTOMLEFT:
		return area->x + x;
	case WM_CORNER_TOPRIGHT:
	case WM_CORNER_RIGHT:
	case WM_CORNER_BOTTOMRIGHT:
		return area->x + area->width - w - x;
	default:
		return area->x + x + (area->width - w) / 2;
	}
}

int
wm_corner_y(enum wm_corner corner, const struct wlr_box *area, int h, int y)
{
	switch (corner) {
	case WM_CORNER_TOPLEFT:
	case WM_CORNER_TOP:
	case WM_CORNER_TOPRIGHT:
		return area->y + y;
	case WM_CORNER_BOTTOMLEFT:
	case WM_CORNER_BOTTOM:
	case WM_CORNER_BOTTOMRIGHT:
		return area->y + area->height - h - y;
	default:
		return area->y + y + (area->height - h) / 2;
	}
}

/* --- Layers (ResourceLayer::getNumFromString) --- */

static enum wm_view_layer
layer_from_number(int n)
{
	/* Fluxbox: 0 Menu, 2 AboveDock, 4 Dock, 6 Top, 8 Normal,
	 * 10 Bottom, 12 Desktop. fluxland has one layer above Normal. */
	if (n < 8)
		return WM_LAYER_ABOVE;
	if (n < 10)
		return WM_LAYER_NORMAL;
	if (n < 12)
		return WM_LAYER_BELOW;
	return WM_LAYER_DESKTOP;
}

bool
wm_layer_parse(const char *str, enum wm_view_layer *out)
{
	if (!str)
		return false;
	while (isspace((unsigned char)*str))
		str++;
	char *end;
	errno = 0;
	long n = strtol(str, &end, 10);
	if (end != str && !errno) {
		while (isspace((unsigned char)*end))
			end++;
		if (*end == '\0') {
			*out = layer_from_number(n < 0 ? 0 :
				n > INT_MAX ? INT_MAX : (int)n);
			return true;
		}
	}
	static const struct {
		const char *name;
		enum wm_view_layer layer;
	} names[] = {
		{ "Menu",       WM_LAYER_ABOVE },
		{ "AboveDock",  WM_LAYER_ABOVE },
		{ "Above Dock", WM_LAYER_ABOVE },
		{ "Dock",       WM_LAYER_ABOVE },
		{ "Top",        WM_LAYER_ABOVE },
		{ "Above",      WM_LAYER_ABOVE },
		{ "Normal",     WM_LAYER_NORMAL },
		{ "Bottom",     WM_LAYER_BELOW },
		{ "Below",      WM_LAYER_BELOW },
		{ "Desktop",    WM_LAYER_DESKTOP },
	};
	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
		if (strcasecmp(str, names[i].name) == 0) {
			*out = names[i].layer;
			return true;
		}
	}
	return false;
}

int
wm_layer_fluxbox_number(enum wm_view_layer layer)
{
	switch (layer) {
	case WM_LAYER_DESKTOP: return 12;
	case WM_LAYER_BELOW:   return 10;
	case WM_LAYER_ABOVE:   return 6;
	default:               return 8;
	}
}

/* --- Decorations (WindowState::getDecoMaskFromString) --- */

/* Fluxbox's DECORM_* bits */
#define FB_DECORM_TITLEBAR (1 << 0)
#define FB_DECORM_HANDLE   (1 << 1)
#define FB_DECORM_BORDER   (1 << 2)
#define FB_DECORM_ICONIFY  (1 << 3)
#define FB_DECORM_TAB      (1 << 9)

bool
wm_decor_parse(const char *str, enum wm_decor_preset *out)
{
	if (!str || !*str)
		return false;
	if (strcasecmp(str, "NORMAL") == 0 || strcasecmp(str, "FULL") == 0) {
		*out = WM_DECOR_NORMAL;
	} else if (strcasecmp(str, "NONE") == 0) {
		*out = WM_DECOR_NONE;
	} else if (strcasecmp(str, "BORDER") == 0) {
		*out = WM_DECOR_BORDER;
	} else if (strcasecmp(str, "TAB") == 0) {
		*out = WM_DECOR_TAB;
	} else if (strcasecmp(str, "TINY") == 0) {
		*out = WM_DECOR_TINY;
	} else if (strcasecmp(str, "TOOL") == 0) {
		*out = WM_DECOR_TOOL;
	} else {
		/* A mask: decimal, 0x hex or 0 octal (extractNumber) */
		char *end;
		errno = 0;
		long mask = strtol(str, &end, 0);
		if (end == str || *end != '\0' || errno || mask < 0)
			return false;
		/* The nearest preset fluxland can draw */
		if (mask & FB_DECORM_TITLEBAR) {
			if (mask & FB_DECORM_HANDLE)
				*out = WM_DECOR_NORMAL;
			else if (mask & FB_DECORM_ICONIFY)
				*out = WM_DECOR_TINY;
			else
				*out = WM_DECOR_TOOL;
		} else if (mask & FB_DECORM_TAB) {
			*out = WM_DECOR_TAB;
		} else if (mask & (FB_DECORM_BORDER | FB_DECORM_HANDLE)) {
			*out = WM_DECOR_BORDER;
		} else {
			*out = WM_DECOR_NONE;
		}
	}
	return true;
}

/* --- Focus protection (Focus::parseProtection) --- */

bool
wm_focus_protection_parse(const char *str, int *prot)
{
	if (!str)
		return false;
	char buf[256];
	snprintf(buf, sizeof(buf), "%s", str);
	bool ok = true;
	char *save = NULL;
	for (char *tok = strtok_r(buf, ", \t", &save); tok;
	     tok = strtok_r(NULL, ", \t", &save)) {
		if (strcasecmp(tok, "Lock") == 0)
			*prot = (*prot & ~WM_FOCUS_PROT_DENY) | WM_FOCUS_PROT_LOCK;
		else if (strcasecmp(tok, "Deny") == 0)
			*prot = (*prot & ~WM_FOCUS_PROT_LOCK) | WM_FOCUS_PROT_DENY;
		else if (strcasecmp(tok, "Gain") == 0)
			*prot = (*prot & ~WM_FOCUS_PROT_REFUSE) | WM_FOCUS_PROT_GAIN;
		else if (strcasecmp(tok, "Refuse") == 0)
			*prot = (*prot & ~WM_FOCUS_PROT_GAIN) | WM_FOCUS_PROT_REFUSE;
		else if (strcasecmp(tok, "None") == 0)
			*prot = 0;
		else
			ok = false;
	}
	return ok;
}
