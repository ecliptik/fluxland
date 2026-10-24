/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * ext_workspace.c - ext-workspace-v1: workspaces for panels and taskbars
 *
 * Mirrors fluxland's workspaces as one workspace group shown on every
 * output: a handle per workspace with its name, position, and whether it
 * is active (shown) or urgent (has a window wanting attention). Panels
 * (waybar's ext/workspaces module, ...) can switch to a workspace, add
 * one, or remove one (its windows join a neighbour).
 *
 * Changes are gathered and sent once from an idle callback:
 * wm_ext_workspace_schedule() is cheap to call from anywhere.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_ext_workspace_v1.h>
#include <wlr/util/log.h>

#include "config.h"
#include "ext_workspace.h"
#include "output.h"
#include "server.h"
#include "view.h"
#include "workspace.h"

#define MAX_HANDLES 64

struct wm_ext_workspace {
	struct wm_server *server;
	struct wlr_ext_workspace_manager_v1 *manager;
	struct wlr_ext_workspace_group_handle_v1 *group;
	/* handles[i] is workspace index i */
	struct wlr_ext_workspace_handle_v1 *handles[MAX_HANDLES];
	int count;
	struct wl_event_source *idle;
	struct wl_listener commit;
};

/* Is a workspace shown on some output? */
static bool
workspace_shown(struct wm_server *server, struct wm_workspace *ws)
{
	if (server->config &&
	    server->config->workspace_mode == WM_WORKSPACE_PER_OUTPUT) {
		struct wm_output *o;
		wl_list_for_each(o, &server->outputs, link) {
			if (o->active_workspace == ws)
				return true;
		}
		return false;
	}
	return server->current_workspace == ws;
}

static bool
workspace_urgent(struct wm_server *server, struct wm_workspace *ws)
{
	struct wm_view *v;
	wl_list_for_each(v, &server->views, link) {
		if (v->mapped && v->urgent && v->workspace == ws)
			return true;
	}
	return false;
}

void
wm_ext_workspace_sync(struct wm_ext_workspace *ew)
{
	struct wm_server *server = ew->server;
	int count = 0;
	struct wm_workspace *ws;
	wl_list_for_each(ws, &server->workspaces, link)
		count++;
	if (count > MAX_HANDLES)
		count = MAX_HANDLES;

	/* Workspaces removed at the end */
	while (ew->count > count)
		wlr_ext_workspace_handle_v1_destroy(ew->handles[--ew->count]);

	wl_list_for_each(ws, &server->workspaces, link) {
		int i = ws->index;
		if (i < 0 || i >= count)
			continue;
		if (i >= ew->count) {
			char id[32];
			snprintf(id, sizeof(id), "fluxland-%d", i + 1);
			/* Capabilities only reach panels when they bind */
			ew->handles[i] = wlr_ext_workspace_handle_v1_create(
				ew->manager, id,
				EXT_WORKSPACE_HANDLE_V1_WORKSPACE_CAPABILITIES_ACTIVATE |
				EXT_WORKSPACE_HANDLE_V1_WORKSPACE_CAPABILITIES_REMOVE);
			if (!ew->handles[i])
				break;
			wlr_ext_workspace_handle_v1_set_group(ew->handles[i],
				ew->group);
			uint32_t coord = (uint32_t)i;
			wlr_ext_workspace_handle_v1_set_coordinates(
				ew->handles[i], &coord, 1);
			ew->count = i + 1;
		}
		struct wlr_ext_workspace_handle_v1 *h = ew->handles[i];
		char fallback[32];
		snprintf(fallback, sizeof(fallback), "%d", i + 1);
		wlr_ext_workspace_handle_v1_set_name(h,
			ws->name && *ws->name ? ws->name : fallback);
		wlr_ext_workspace_handle_v1_set_active(h,
			workspace_shown(server, ws));
		wlr_ext_workspace_handle_v1_set_urgent(h,
			workspace_urgent(server, ws));
	}
}

static void
handle_idle(void *data)
{
	struct wm_ext_workspace *ew = data;
	ew->idle = NULL;
	wm_ext_workspace_sync(ew);
}

void
wm_ext_workspace_schedule(struct wm_server *server)
{
	struct wm_ext_workspace *ew = server->ext_workspace;
	if (!ew || ew->idle)
		return;
	ew->idle = wl_event_loop_add_idle(
		wl_display_get_event_loop(server->wl_display), handle_idle, ew);
}

static int
handle_index(struct wm_ext_workspace *ew,
	struct wlr_ext_workspace_handle_v1 *h)
{
	for (int i = 0; i < ew->count; i++) {
		if (ew->handles[i] == h)
			return i;
	}
	return -1;
}

static void
handle_commit(struct wl_listener *listener, void *data)
{
	struct wm_ext_workspace *ew = wl_container_of(listener, ew, commit);
	struct wm_server *server = ew->server;
	struct wlr_ext_workspace_v1_commit_event *event = data;

	struct wlr_ext_workspace_v1_request *req;
	wl_list_for_each(req, event->requests, link) {
		switch (req->type) {
		case WLR_EXT_WORKSPACE_V1_REQUEST_ACTIVATE: {
			int i = handle_index(ew, req->activate.workspace);
			if (i >= 0)
				wm_workspace_switch(server, i);
			break;
		}
		case WLR_EXT_WORKSPACE_V1_REQUEST_CREATE_WORKSPACE:
			wm_workspace_add(server);
			if (req->create_workspace.name &&
			    *req->create_workspace.name) {
				/* Name the new (last) workspace */
				struct wm_workspace *last = wm_workspace_get(
					server, server->workspace_count - 1);
				if (last) {
					free(last->name);
					last->name =
						strdup(req->create_workspace.name);
				}
			}
			break;
		case WLR_EXT_WORKSPACE_V1_REQUEST_REMOVE: {
			int i = handle_index(ew, req->remove.workspace);
			if (i >= 0)
				wm_workspace_remove(server, i);
			break;
		}
		case WLR_EXT_WORKSPACE_V1_REQUEST_DEACTIVATE:
		case WLR_EXT_WORKSPACE_V1_REQUEST_ASSIGN:
			/* One workspace is always shown; one group */
			break;
		}
	}
	wm_ext_workspace_schedule(server);
}

void
wm_ext_workspace_output_enter(struct wm_server *server,
	struct wlr_output *output)
{
	if (server->ext_workspace)
		wlr_ext_workspace_group_handle_v1_output_enter(
			server->ext_workspace->group, output);
}

void
wm_ext_workspace_output_leave(struct wm_server *server,
	struct wlr_output *output)
{
	if (server->ext_workspace)
		wlr_ext_workspace_group_handle_v1_output_leave(
			server->ext_workspace->group, output);
}

void
wm_ext_workspace_init(struct wm_server *server)
{
	struct wm_ext_workspace *ew = calloc(1, sizeof(*ew));
	if (!ew)
		return;
	ew->server = server;
	ew->manager = wlr_ext_workspace_manager_v1_create(server->wl_display, 1);
	if (!ew->manager) {
		free(ew);
		return;
	}
	ew->group = wlr_ext_workspace_group_handle_v1_create(ew->manager,
		EXT_WORKSPACE_GROUP_HANDLE_V1_GROUP_CAPABILITIES_CREATE_WORKSPACE);
	ew->commit.notify = handle_commit;
	wl_signal_add(&ew->manager->events.commit, &ew->commit);
	server->ext_workspace = ew;
	wm_ext_workspace_sync(ew);
}

void
wm_ext_workspace_finish(struct wm_server *server)
{
	struct wm_ext_workspace *ew = server->ext_workspace;
	if (!ew)
		return;
	if (ew->idle)
		wl_event_source_remove(ew->idle);
	wl_list_remove(&ew->commit.link);
	free(ew);
	server->ext_workspace = NULL;
}
