/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * output_memory.h - Windows go back to a monitor that comes back
 *
 * When an output leaves the layout (a monitor powered off or unplugged, an
 * output turned off by a profile) its windows are moved to the outputs that
 * remain.  X did not do that: the screen stayed as it was.  So that nothing
 * is lost when a monitor is only off for the night, the windows on it are
 * remembered, with where they were on it; when a monitor is back on the same
 * port, those that have not been moved since go back where they were.
 */

#ifndef WM_OUTPUT_MEMORY_H
#define WM_OUTPUT_MEMORY_H

struct wm_server;
struct wlr_output;

/* The output is about to leave the layout (it is still in it): note its
 * windows and where they are on it */
void wm_output_memory_depart(struct wm_server *server,
	struct wlr_output *wlr_output);

/* It has left, and the windows have been moved off it: note where they are
 * now, to tell later whether they were moved since */
void wm_output_memory_departed(struct wm_server *server,
	struct wlr_output *wlr_output);

/* The output is in the layout again: windows that were on this monitor, and
 * were not moved since, go back to it */
void wm_output_memory_return(struct wm_server *server,
	struct wlr_output *wlr_output);

/* Forget everything (shutdown) */
void wm_output_memory_finish(void);

#endif /* WM_OUTPUT_MEMORY_H */
