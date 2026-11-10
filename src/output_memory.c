/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * output_memory.c - Windows go back to a monitor that comes back
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/util/box.h>
#include <wlr/util/log.h>

#include "output.h"
#include "output_memory.h"
#include "server.h"
#include "view.h"
#include "view_geometry.h"

/* Monitors remembered at once: more than anyone has, and old ones go */
#define MEMORY_MAX 8

/* A window that was on the monitor */
struct remembered_view {
	struct wm_view *view;	/* only compared, until found in the list */
	int64_t mapped_ms;	/* tells it from another at the same address */
	int rel_x, rel_y;	/* its frame's corner from the monitor's */
	int after_x, after_y;	/* where it was put when the monitor went */
	bool state;		/* maximized, tiled, ...: refit on return */
	struct wl_list link;
};

struct monitor_memory {
	char *key;		/* the port the monitor is on (HDMI-A-1) */
	struct wl_list views;	/* remembered_view.link */
	struct wl_list link;
};

static struct wl_list memories;	/* monitor_memory.link, newest first */
static bool memories_ready;

static void
ensure_list(void)
{
	if (!memories_ready) {
		wl_list_init(&memories);
		memories_ready = true;
	}
}

/* A monitor is told by its port (HDMI-A-1), as the toolbar's place is */
static char *
monitor_key(const struct wlr_output *wlr_output)
{
	return strdup(wlr_output->name);
}

static void
memory_free(struct monitor_memory *mem)
{
	struct remembered_view *rv, *tmp;
	wl_list_for_each_safe(rv, tmp, &mem->views, link) {
		wl_list_remove(&rv->link);
		free(rv);
	}
	wl_list_remove(&mem->link);
	free(mem->key);
	free(mem);
}

static struct monitor_memory *
memory_find(const char *key)
{
	struct monitor_memory *mem;
	wl_list_for_each(mem, &memories, link) {
		if (!strcmp(mem->key, key))
			return mem;
	}
	return NULL;
}

static bool
has_state(const struct wm_view *view)
{
	return view->maximized || view->maximized_vert ||
		view->maximized_horiz || view->lhalf || view->rhalf ||
		view->tile.active || view->fullscreen;
}

void
wm_output_memory_depart(struct wm_server *server,
	struct wlr_output *wlr_output)
{
	ensure_list();
	struct wlr_box box;
	/* (A disabled output is still in the layout until it is taken out) */
	if (!server->output_layout ||
	    !wlr_output_layout_get(server->output_layout, wlr_output))
		return;
	wlr_output_layout_get_box(server->output_layout, wlr_output, &box);
	if (wlr_box_empty(&box))
		return;

	char *key = monitor_key(wlr_output);
	if (!key)
		return;
	/* An older note of the same monitor is out of date */
	struct monitor_memory *old = memory_find(key);
	if (old)
		memory_free(old);
	struct monitor_memory *mem = calloc(1, sizeof(*mem));
	if (!mem) {
		free(key);
		return;
	}
	mem->key = key;
	wl_list_init(&mem->views);

	struct wm_view *view;
	wl_list_for_each(view, &server->views, link) {
		if (!view->mapped || view->docked || !view->scene_tree)
			continue;
		struct wlr_box frame;
		wm_view_frame_box(view, &frame);
		if (!wlr_box_contains_point(&box,
			    frame.x + frame.width / 2.0,
			    frame.y + frame.height / 2.0))
			continue;
		struct remembered_view *rv = calloc(1, sizeof(*rv));
		if (!rv)
			continue;
		rv->view = view;
		rv->mapped_ms = view->mapped_ms;
		rv->rel_x = frame.x - box.x;
		rv->rel_y = frame.y - box.y;
		rv->state = has_state(view);
		rv->after_x = view->x;
		rv->after_y = view->y;
		wl_list_insert(mem->views.prev, &rv->link);
	}

	if (wl_list_empty(&mem->views)) {
		free(mem->key);
		free(mem);
		return;
	}
	wl_list_insert(&memories, &mem->link);
	/* A few monitors at most: the oldest goes */
	int count = 0;
	struct monitor_memory *m, *tmp;
	wl_list_for_each_safe(m, tmp, &memories, link) {
		if (++count > MEMORY_MAX)
			memory_free(m);
	}
	wlr_log(WLR_DEBUG, "output memory: %s left with its windows noted",
		wlr_output->name);
}

void
wm_output_memory_departed(struct wm_server *server,
	struct wlr_output *wlr_output)
{
	ensure_list();
	char *key = monitor_key(wlr_output);
	if (!key)
		return;
	struct monitor_memory *mem = memory_find(key);
	free(key);
	if (!mem)
		return;

	struct remembered_view *rv;
	wl_list_for_each(rv, &mem->views, link) {
		struct wm_view *view;
		wl_list_for_each(view, &server->views, link) {
			if (view == rv->view &&
			    view->mapped_ms == rv->mapped_ms) {
				rv->after_x = view->x;
				rv->after_y = view->y;
				break;
			}
		}
	}
}

void
wm_output_memory_return(struct wm_server *server,
	struct wlr_output *wlr_output)
{
	ensure_list();
	char *key = monitor_key(wlr_output);
	if (!key)
		return;
	struct monitor_memory *mem = memory_find(key);
	free(key);
	if (!mem)
		return;

	struct wlr_box box;
	struct wm_output *output = wm_output_from_wlr(server, wlr_output);
	if (!output || !wlr_output_layout_get(server->output_layout,
		    wlr_output)) {
		return;
	}
	wlr_output_layout_get_box(server->output_layout, wlr_output, &box);
	int head = wm_output_head_number(output);

	struct remembered_view *rv;
	wl_list_for_each(rv, &mem->views, link) {
		struct wm_view *view;
		wl_list_for_each(view, &server->views, link) {
			if (view != rv->view ||
			    view->mapped_ms != rv->mapped_ms)
				continue;
			/* Where it was put when the monitor went: it has been
			 * left alone since */
			if (view->mapped && view->x == rv->after_x &&
			    view->y == rv->after_y) {
				if (rv->state && head > 0)
					wm_view_set_head(view, head);
				else
					wm_view_move(view, box.x + rv->rel_x,
						box.y + rv->rel_y);
				wm_view_keep_on_screen(view);
			}
			break;
		}
	}
	wlr_log(WLR_DEBUG, "output memory: %s is back; its windows with it",
		wlr_output->name);
	memory_free(mem);
}

void
wm_output_memory_finish(void)
{
	if (!memories_ready)
		return;
	struct monitor_memory *mem, *tmp;
	wl_list_for_each_safe(mem, tmp, &memories, link)
		memory_free(mem);
}
