/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * tooltip.h - Fluxbox's tooltips: the whole title of an iconbar button
 * or tab cut short, over the pointer after tooltipDelay
 */

#ifndef WM_TOOLTIP_H
#define WM_TOOLTIP_H

struct wm_server;
struct wm_view;

/*
 * The pointer moved: show the title under it that is cut short (after
 * session.screen0.tooltipDelay, or at once when it changed while shown),
 * or hide the tooltip when there is none (BScreen::showTooltip).
 */
void wm_tooltip_update(struct wm_server *server);

void wm_tooltip_hide(struct wm_server *server);

/* A window going away: its tooltip goes */
void wm_tooltip_view_destroyed(struct wm_server *server, struct wm_view *view);

void wm_tooltip_finish(struct wm_server *server);

#endif /* WM_TOOLTIP_H */
