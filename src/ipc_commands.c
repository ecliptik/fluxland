/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * ipc_commands.c - IPC command dispatch and handlers
 *
 * Parses incoming JSON requests and builds JSON responses using
 * simple string formatting (no json-c dependency).
 */

#define _GNU_SOURCE
#include <ctype.h>
#include <fnmatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/edges.h>
#include <wlr/util/log.h>

#include "i18n.h"
#include "ipc.h"
#include "config.h"
#include "perf.h"
#include "decoration.h"
#include "foreign_toplevel.h"
#include "keybind.h"
#include "keyboard.h"
#include "menu.h"
#include "output.h"
#include "placement.h"
#include "server.h"
#include "slit.h"
#include "tabgroup.h"
#include "style.h"
#include "toolbar.h"
#include "rules.h"
#include "util.h"
#include "view.h"
#include "command.h"
#include "ipc_json.h"
#include "workspace.h"
#include "xwayland.h"

/* Security helpers are in util.h: wm_is_blocked_env_var(), wm_spawn_command() */

#ifdef WM_PERF_ENABLE
static struct wm_perf_probe perf_ipc_command;
static bool perf_ipc_inited;
#endif

/* JSON helpers live in ipc_json.c (shared with the fuzz target) */

/* ---------- dynamic string buffer ---------- */

struct strbuf {
	char *data;
	size_t len;
	size_t cap;
};

static void
strbuf_init(struct strbuf *sb)
{
	sb->data = malloc(256);
	sb->len = 0;
	if (!sb->data) {
		sb->cap = 0;
		wlr_log(WLR_ERROR, "%s", "allocation failed");
		return;
	}
	sb->cap = 256;
	sb->data[0] = '\0';
}

static void
strbuf_append(struct strbuf *sb, const char *s)
{
	if (!sb->data) return;
	size_t slen = strlen(s);
	if (sb->len + slen >= sb->cap) {
		size_t new_cap = (sb->len + slen + 1) * 2;
		char *new_data = realloc(sb->data, new_cap);
		if (!new_data) return;
		sb->data = new_data;
		sb->cap = new_cap;
	}
	memcpy(sb->data + sb->len, s, slen + 1);
	sb->len += slen;
}

static void
strbuf_appendf(struct strbuf *sb, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));

static void
strbuf_appendf(struct strbuf *sb, const char *fmt, ...)
{
	char buf[1024];
	va_list ap, ap2;
	va_start(ap, fmt);
	va_copy(ap2, ap);
	int needed = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (needed < 0) {
		va_end(ap2);
		return;
	}
	if ((size_t)needed < sizeof(buf)) {
		strbuf_append(sb, buf);
	} else {
		/* Retry with heap allocation for large strings */
		char *heap = malloc((size_t)needed + 1);
		if (heap) {
			vsnprintf(heap, (size_t)needed + 1, fmt, ap2);
			strbuf_append(sb, heap);
			free(heap);
		}
	}
	va_end(ap2);
}

static char *
strbuf_finish(struct strbuf *sb)
{
	return sb->data; /* caller owns */
}

/* ---------- response helpers ---------- */

static char *
make_error(const char *message)
{
	char *escaped = wm_ipc_json_quote(message);
	if (!escaped) return NULL;
	struct strbuf sb;
	strbuf_init(&sb);
	strbuf_appendf(&sb, "{\"success\":false,\"error\":%s}", escaped);
	free(escaped);
	return strbuf_finish(&sb);
}

static char *
make_ok(void)
{
	return strdup("{\"success\":true}");
}

/* ---------- command handlers ---------- */

static char *
cmd_get_workspaces(struct wm_server *server)
{
	struct strbuf sb;
	strbuf_init(&sb);
	strbuf_append(&sb, "{\"success\":true,\"data\":[");

	struct wm_workspace *ws;
	bool first = true;
	wl_list_for_each(ws, &server->workspaces, link) {
		if (!first) strbuf_append(&sb, ",");
		first = false;

		char *name_esc = wm_ipc_json_quote(ws->name);
		bool focused = (ws == wm_workspace_get_active(server));

		strbuf_appendf(&sb,
			"{\"index\":%d,\"name\":%s,\"focused\":%s,"
			"\"visible\":%s}",
			ws->index, name_esc,
			focused ? "true" : "false",
			focused ? "true" : "false");
		free(name_esc);
	}

	strbuf_append(&sb, "]}");
	return strbuf_finish(&sb);
}

static const char *
view_layer_str(enum wm_view_layer layer)
{
	switch (layer) {
	case WM_LAYER_DESKTOP: return "Desktop";
	case WM_LAYER_BELOW: return "Below";
	case WM_LAYER_NORMAL: return "Normal";
	case WM_LAYER_ABOVE: return "Above";
	default: return "Normal";
	}
}

static char *
cmd_get_windows(struct wm_server *server)
{
	struct strbuf sb;
	strbuf_init(&sb);
	strbuf_append(&sb, "{\"success\":true,\"data\":[");

	struct wm_view *view;
	bool first = true;
	wl_list_for_each(view, &server->views, link) {
		/* Not yet mapped, or managed by the slit */
		if (!view->mapped || view->docked)
			continue;
		if (!first) strbuf_append(&sb, ",");
		first = false;

		char *title_esc = wm_ipc_json_quote(view->title);
		char *app_id_esc = wm_ipc_json_quote(view->app_id);
		bool focused = (view == server->focused_view);
		bool minimized = !view->scene_tree->node.enabled;

		struct wlr_box geo;
		wm_view_get_geometry(view, &geo);

		int ws_index = view->workspace ? view->workspace->index : -1;

		/* Determine role: dialog if has parent, window otherwise */
		const char *role = "window";
		if (wm_view_parent(view))
			role = "dialog";

		pid_t pid = wm_view_pid(view);
		if (pid <= 0)
			pid = -1;

		strbuf_appendf(&sb,
			"{\"id\":%u,\"app_id\":%s,\"title\":%s,"
			"\"workspace\":%d,\"focused\":%s,"
			"\"maximized\":%s,\"fullscreen\":%s,"
			"\"minimized\":%s,\"shaded\":%s,\"sticky\":%s,"
			"\"layer\":\"%s\",\"decorated\":%s,"
			"\"role\":\"%s\",\"pid\":%d,\"xwayland\":%s,"
			"\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d}",
			view->id,
			app_id_esc, title_esc,
			ws_index, focused ? "true" : "false",
			view->maximized ? "true" : "false",
			view->fullscreen ? "true" : "false",
			minimized ? "true" : "false",
			view->decoration && view->decoration->shaded ? "true" : "false",
			view->sticky ? "true" : "false",
			view_layer_str(view->layer),
			view->decoration && view->show_decoration ?
				"true" : "false",
			role, (int)pid,
			view->type == WM_VIEW_XWAYLAND ? "true" : "false",
			geo.x, geo.y, geo.width, geo.height);
		free(title_esc);
		free(app_id_esc);
	}


	strbuf_append(&sb, "]}");
	return strbuf_finish(&sb);
}

static char *
cmd_get_outputs(struct wm_server *server)
{
	struct strbuf sb;
	strbuf_init(&sb);
	strbuf_append(&sb, "{\"success\":true,\"data\":[");

	struct wm_output *output;
	bool first = true;
	wl_list_for_each(output, &server->outputs, link) {
		if (!first) strbuf_append(&sb, ",");
		first = false;

		char *name_esc = wm_ipc_json_quote(output->wlr_output->name);

		struct wlr_output *wo = output->wlr_output;
		/* Position and usable area are layout coordinates */
		struct wlr_box box = {0};
		wm_output_layout_box(output, &box);
		struct wlr_box u = output->usable_area;

		strbuf_appendf(&sb,
			"{\"name\":%s,\"width\":%d,\"height\":%d,"
			"\"x\":%d,\"y\":%d,\"enabled\":%s,"
			"\"scale\":%.3g,\"refresh\":%d,"
			"\"layout\":{\"x\":%d,\"y\":%d,\"width\":%d,"
			"\"height\":%d},"
			"\"usable\":{\"x\":%d,\"y\":%d,\"width\":%d,"
			"\"height\":%d}}",
			name_esc, wo->width, wo->height, box.x, box.y,
			wo->enabled ? "true" : "false", wo->scale, wo->refresh,
			box.x, box.y, box.width, box.height,
			u.x, u.y, u.width, u.height);
		free(name_esc);
	}

	strbuf_append(&sb, "]}");
	return strbuf_finish(&sb);
}

static char *
cmd_get_config(struct wm_server *server)
{
	struct strbuf sb;
	strbuf_init(&sb);
	strbuf_append(&sb, "{\"success\":true,\"data\":{");

	struct wm_config *cfg = server->config;
	if (cfg) {
		const char *focus_str = "click";
		if (cfg->focus_policy == WM_FOCUS_SLOPPY)
			focus_str = "sloppy";
		else if (cfg->focus_policy == WM_FOCUS_STRICT_MOUSE)
			focus_str = "strict_mouse";

		const char *placement_str = "row_smart";
		if (cfg->placement_policy == WM_PLACEMENT_COL_SMART)
			placement_str = "col_smart";
		else if (cfg->placement_policy == WM_PLACEMENT_CASCADE)
			placement_str = "cascade";
		else if (cfg->placement_policy == WM_PLACEMENT_UNDER_MOUSE)
			placement_str = "under_mouse";
		else if (cfg->placement_policy == WM_PLACEMENT_ROW_MIN_OVERLAP)
			placement_str = "row_min_overlap";
		else if (cfg->placement_policy == WM_PLACEMENT_COL_MIN_OVERLAP)
			placement_str = "col_min_overlap";

		strbuf_appendf(&sb,
			"\"workspace_count\":%d,"
			"\"focus_policy\":\"%s\","
			"\"placement_policy\":\"%s\","
			"\"edge_snap_threshold\":%d,"
			"\"toolbar_visible\":%s",
			cfg->workspace_count,
			focus_str, placement_str,
			cfg->edge_snap_threshold,
			cfg->toolbar_visible ? "true" : "false");
	}

	strbuf_append(&sb, "}}");
	return strbuf_finish(&sb);
}

static char *
cmd_command(struct wm_server *server, const char *action_str,
	const char *window_str)
{
	if (!action_str || !*action_str) {
		return make_error(_("missing action argument"));
	}

	/* Optional "window": <id> runs the command on that window */
	struct wm_view *target = NULL;
	if (window_str) {
		int id;
		if (!safe_atoi(window_str, &id) || id <= 0)
			return make_error(_("invalid window id"));
		struct wm_view *v;
		wl_list_for_each(v, &server->views, link) {
			if (v->id == (uint32_t)id && v->mapped) {
				target = v;
				break;
			}
		}
		if (!target)
			return make_error(_("no such window"));
	}

	/* Same command language as key bindings (MacroCmd, If, ...) */
	struct wm_command *cmd = wm_command_parse(action_str);
	if (!cmd) {
		return make_error(_("unknown action"));
	}

	/* --ipc-no-exec: refuse anything that can run programs */
	if (server->ipc_no_exec &&
	    (wm_command_contains(cmd, WM_ACTION_EXEC) ||
	     wm_command_contains(cmd, WM_ACTION_BIND_KEY) ||
	     wm_command_contains(cmd, WM_ACTION_SET_STYLE))) {
		wm_command_destroy(cmd);
		return make_error(_("action blocked by --ipc-no-exec"));
	}

	bool ok = wm_command_execute(server, cmd, target);
	wm_command_destroy(cmd);
	if (!ok) {
		return make_error(_("action failed"));
	}
	return make_ok();
}

static char *
cmd_subscribe(struct wm_ipc_client *client, const char *events_str)
{
	if (!events_str || !*events_str) {
		return make_error(_("missing event types"));
	}

	/* Parse comma- or space-separated event names */
	char *copy = strdup(events_str);
	if (!copy) return make_error(_("out of memory"));

	char *saveptr;
	char *token = strtok_r(copy, ", ", &saveptr);
	while (token) {
		if (strcasecmp(token, "window") == 0 ||
		    strcasecmp(token, "window_open") == 0) {
			client->subscribed_events |= WM_IPC_EVENT_WINDOW_OPEN;
			client->subscribed_events |= WM_IPC_EVENT_WINDOW_CLOSE;
			client->subscribed_events |= WM_IPC_EVENT_WINDOW_FOCUS;
			client->subscribed_events |= WM_IPC_EVENT_WINDOW_TITLE;
		} else if (strcasecmp(token, "window_close") == 0) {
			client->subscribed_events |= WM_IPC_EVENT_WINDOW_CLOSE;
		} else if (strcasecmp(token, "window_focus") == 0) {
			client->subscribed_events |= WM_IPC_EVENT_WINDOW_FOCUS;
		} else if (strcasecmp(token, "window_title") == 0) {
			client->subscribed_events |= WM_IPC_EVENT_WINDOW_TITLE;
		} else if (strcasecmp(token, "workspace") == 0) {
			client->subscribed_events |= WM_IPC_EVENT_WORKSPACE;
		} else if (strcasecmp(token, "output") == 0) {
			client->subscribed_events |= WM_IPC_EVENT_OUTPUT_ADD;
			client->subscribed_events |= WM_IPC_EVENT_OUTPUT_REMOVE;
		} else if (strcasecmp(token, "style_changed") == 0) {
			client->subscribed_events |=
				WM_IPC_EVENT_STYLE_CHANGED;
		} else if (strcasecmp(token, "config_reloaded") == 0) {
			client->subscribed_events |=
				WM_IPC_EVENT_CONFIG_RELOADED;
		} else if (strcasecmp(token, "focus_changed") == 0) {
			client->subscribed_events |=
				WM_IPC_EVENT_FOCUS_CHANGED;
		} else if (strcasecmp(token, "menu") == 0) {
			client->subscribed_events |=
				WM_IPC_EVENT_MENU;
		} else if (strcasecmp(token, "bell") == 0) {
			client->subscribed_events |= WM_IPC_EVENT_BELL;
		} else if (strcasecmp(token, "accessibility") == 0) {
			/* Subscribe to all accessibility events */
			client->subscribed_events |=
				WM_IPC_EVENT_FOCUS_CHANGED;
			client->subscribed_events |=
				WM_IPC_EVENT_MENU;
			client->subscribed_events |=
				WM_IPC_EVENT_WORKSPACE;
			client->subscribed_events |=
				WM_IPC_EVENT_WINDOW_FOCUS;
		}
		token = strtok_r(NULL, ", ", &saveptr);
	}
	free(copy);

	return make_ok();
}

#ifdef WM_PERF_ENABLE
static char *
cmd_get_perf(void)
{
	struct strbuf sb;
	strbuf_init(&sb);
	strbuf_append(&sb, "{\"success\":true,\"data\":{\"probes\":[");

	if (!perf_ipc_inited) {
		strbuf_append(&sb, "]}}");
		return strbuf_finish(&sb);
	}

	uint64_t avg_ns = perf_ipc_command.total_count > 0
		? perf_ipc_command.sum_ns / perf_ipc_command.total_count : 0;
	uint64_t min_ns = perf_ipc_command.min_ns == UINT64_MAX
		? 0 : perf_ipc_command.min_ns;

	strbuf_appendf(&sb,
		"{\"name\":\"ipc_command\",\"count\":%lu,"
		"\"min_ns\":%lu,\"max_ns\":%lu,\"avg_ns\":%lu}",
		(unsigned long)perf_ipc_command.total_count,
		(unsigned long)min_ns,
		(unsigned long)perf_ipc_command.max_ns,
		(unsigned long)avg_ns);

	strbuf_append(&sb, "]}}");
	return strbuf_finish(&sb);
}
#endif

/* ---------- main dispatch ---------- */

char *
wm_ipc_handle_command(struct wm_ipc_server *ipc,
	struct wm_ipc_client *client, const char *json_request)
{
	if (!json_request || !*json_request) {
		return make_error(_("empty request"));
	}

#ifdef WM_PERF_ENABLE
	if (!perf_ipc_inited) {
		wm_perf_probe_init(&perf_ipc_command, "ipc_command");
		perf_ipc_inited = true;
	}
	WM_PERF_BEGIN(ipc_cmd);
#endif

	/* Extract "command" field */
	char *command = wm_ipc_json_get_string(json_request, "command");
	if (!command) {
#ifdef WM_PERF_ENABLE
		WM_PERF_END(ipc_cmd, &perf_ipc_command);
#endif
		return make_error(_("missing 'command' field"));
	}

	struct wm_server *server = ipc->server;
	char *result = NULL;

	if (strcmp(command, "get_workspaces") == 0) {
		result = cmd_get_workspaces(server);
	} else if (strcmp(command, "get_windows") == 0) {
		result = cmd_get_windows(server);
	} else if (strcmp(command, "get_outputs") == 0) {
		result = cmd_get_outputs(server);
	} else if (strcmp(command, "get_config") == 0) {
		result = cmd_get_config(server);
	} else if (strcmp(command, "command") == 0) {
		char *action = wm_ipc_json_get_string(json_request, "action");
		char *window = wm_ipc_json_get_string(json_request, "window");
		result = cmd_command(server, action, window);
		free(action);
		free(window);
	} else if (strcmp(command, "subscribe") == 0) {
		char *events = wm_ipc_json_get_string(json_request, "events");
		result = cmd_subscribe(client, events);
		free(events);
	} else if (strcmp(command, "ping") == 0) {
		result = make_ok();
#ifdef WM_PERF_ENABLE
	} else if (strcmp(command, "get_perf") == 0) {
		result = cmd_get_perf();
#endif
	} else {
		result = make_error(_("unknown command"));
	}

#ifdef WM_PERF_ENABLE
	WM_PERF_END(ipc_cmd, &perf_ipc_command);
#endif

	free(command);
	return result;
}
