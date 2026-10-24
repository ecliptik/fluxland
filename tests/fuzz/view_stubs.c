/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * view_stubs.c - View operations referenced by menu.c and rules.c
 *
 * The menu and apps parsers live next to code that acts on windows
 * (menu items, rule application). The fuzzers only parse, so these are
 * never called.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct wlr_box;
struct wlr_surface;
struct wm_view;

struct wlr_surface *wm_view_surface(struct wm_view *view);
void wm_view_content_geometry(struct wm_view *view, struct wlr_box *geo);
void wm_view_set_size(struct wm_view *view, int width, int height);
void wm_view_set_head(struct wm_view *view, int head);
void wm_view_toggle_maximize(struct wm_view *view);
void wm_view_toggle_fullscreen(struct wm_view *view);
void wm_view_minimize(struct wm_view *view);
void wm_view_close(struct wm_view *view);
void wm_view_kill(struct wm_view *view);

struct wlr_surface *
wm_view_surface(struct wm_view *view)
{
	(void)view;
	return NULL;
}

void
wm_view_content_geometry(struct wm_view *view, struct wlr_box *geo)
{
	(void)view; (void)geo;
}

void
wm_view_set_size(struct wm_view *view, int width, int height)
{
	(void)view; (void)width; (void)height;
}

void
wm_view_set_head(struct wm_view *view, int head)
{
	(void)view; (void)head;
}

void
wm_view_toggle_maximize(struct wm_view *view)
{
	(void)view;
}

void
wm_view_toggle_fullscreen(struct wm_view *view)
{
	(void)view;
}

void
wm_view_minimize(struct wm_view *view)
{
	(void)view;
}

void
wm_view_close(struct wm_view *view)
{
	(void)view;
}

void
wm_view_kill(struct wm_view *view)
{
	(void)view;
}
