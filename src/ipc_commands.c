/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
 * ipc_commands.c - IPC command dispatch and handlers
 *
 * Parses incoming JSON requests and builds JSON responses using
 * simple string formatting (no json-c dependency).
 */

#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>

#include "ipc.h"
#include "config.h"
#include "keybind.h"
#include "output.h"
#include "server.h"
#include "view.h"
#include "workspace.h"

/* ---------- minimal JSON helpers ---------- */

/* Escape a string for JSON output. Returns malloc'd string. */
static char *
json_escape(const char *s)
{
	if (!s) {
		return strdup("null");
	}
	/* Worst case: every char needs escaping (\uXXXX = 6 chars) */
	size_t len = strlen(s);
	size_t cap = len * 6 + 3; /* quotes + null */
	char *out = malloc(cap);
	if (!out) return NULL;

	char *p = out;
	*p++ = '"';
	for (size_t i = 0; i < len; i++) {
		unsigned char c = (unsigned char)s[i];
		switch (c) {
		case '"':  *p++ = '\\'; *p++ = '"';  break;
		case '\\': *p++ = '\\'; *p++ = '\\'; break;
		case '\n': *p++ = '\\'; *p++ = 'n';  break;
		case '\r': *p++ = '\\'; *p++ = 'r';  break;
		case '\t': *p++ = '\\'; *p++ = 't';  break;
		default:
			if (c < 0x20) {
				p += sprintf(p, "\\u%04x", c);
			} else {
				*p++ = (char)c;
			}
			break;
		}
	}
	*p++ = '"';
	*p = '\0';
	return out;
}

/* Extract a string value for a given key from a JSON object string.
 * Very minimal: only handles top-level string and bare-word values.
 * Returns malloc'd string or NULL. */
static char *
json_get_string(const char *json, const char *key)
{
	/* Find "key" : */
	size_t klen = strlen(key);
	const char *p = json;
	while ((p = strstr(p, key)) != NULL) {
		/* Check that it's preceded by a quote */
		if (p > json && *(p - 1) == '"') {
			const char *after = p + klen;
			if (*after == '"') {
				after++;
				/* Skip whitespace and colon */
				while (*after && (*after == ' ' || *after == '\t'))
					after++;
				if (*after == ':') {
					after++;
					while (*after && (*after == ' ' || *after == '\t'))
						after++;
					if (*after == '"') {
						/* Quoted string value */
						after++;
						const char *end = after;
						while (*end && *end != '"') {
							if (*end == '\\' && *(end + 1))
								end++;
							end++;
						}
						size_t vlen = (size_t)(end - after);
						char *val = malloc(vlen + 1);
						if (val) {
							memcpy(val, after, vlen);
							val[vlen] = '\0';
						}
						return val;
					}
					/* Unquoted value (number, bool, null) */
					const char *end = after;
					while (*end && *end != ',' && *end != '}'
						&& *end != ' ' && *end != '\n')
						end++;
					size_t vlen = (size_t)(end - after);
					char *val = malloc(vlen + 1);
					if (val) {
						memcpy(val, after, vlen);
						val[vlen] = '\0';
					}
					return val;
				}
			}
		}
		p++;
	}
	return NULL;
}

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
	sb->cap = 256;
	if (sb->data) sb->data[0] = '\0';
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
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	strbuf_append(sb, buf);
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
	char *escaped = json_escape(message);
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

/* ---------- action parsing (mirrors keyboard.c) ---------- */

struct action_entry {
	const char *name;
	enum wm_action action;
};

static const struct action_entry action_table[] = {
	{"Exec",             WM_ACTION_EXEC},
	{"Close",            WM_ACTION_CLOSE},
	{"Kill",             WM_ACTION_KILL},
	{"Maximize",         WM_ACTION_MAXIMIZE},
	{"MaximizeVertical", WM_ACTION_MAXIMIZE_VERT},
	{"MaximizeHorizontal", WM_ACTION_MAXIMIZE_HORIZ},
	{"Fullscreen",       WM_ACTION_FULLSCREEN},
	{"Minimize",         WM_ACTION_MINIMIZE},
	{"Iconify",          WM_ACTION_MINIMIZE},
	{"Shade",            WM_ACTION_SHADE},
	{"ShadeOn",          WM_ACTION_SHADE_ON},
	{"ShadeOff",         WM_ACTION_SHADE_OFF},
	{"Stick",            WM_ACTION_STICK},
	{"Move",             WM_ACTION_MOVE},
	{"Resize",           WM_ACTION_RESIZE},
	{"Raise",            WM_ACTION_RAISE},
	{"Lower",            WM_ACTION_LOWER},
	{"RaiseLayer",       WM_ACTION_RAISE_LAYER},
	{"LowerLayer",       WM_ACTION_LOWER_LAYER},
	{"SetLayer",         WM_ACTION_SET_LAYER},
	{"FocusNext",        WM_ACTION_FOCUS_NEXT},
	{"NextWindow",       WM_ACTION_FOCUS_NEXT},
	{"FocusPrev",        WM_ACTION_FOCUS_PREV},
	{"PrevWindow",       WM_ACTION_FOCUS_PREV},
	{"Workspace",        WM_ACTION_WORKSPACE},
	{"SendToWorkspace",  WM_ACTION_SEND_TO_WORKSPACE},
	{"NextWorkspace",    WM_ACTION_NEXT_WORKSPACE},
	{"PrevWorkspace",    WM_ACTION_PREV_WORKSPACE},
	{"ShowDesktop",      WM_ACTION_SHOW_DESKTOP},
	{"MoveLeft",         WM_ACTION_MOVE_LEFT},
	{"MoveRight",        WM_ACTION_MOVE_RIGHT},
	{"MoveUp",           WM_ACTION_MOVE_UP},
	{"MoveDown",         WM_ACTION_MOVE_DOWN},
	{"MoveTo",           WM_ACTION_MOVE_TO},
	{"ResizeTo",         WM_ACTION_RESIZE_TO},
	{"ToggleDecor",      WM_ACTION_TOGGLE_DECOR},
	{"SetDecor",         WM_ACTION_SET_DECOR},
	{"KeyMode",          WM_ACTION_KEY_MODE},
	{"NextTab",          WM_ACTION_NEXT_TAB},
	{"PrevTab",          WM_ACTION_PREV_TAB},
	{"DetachClient",     WM_ACTION_DETACH_CLIENT},
	{"MoveTabLeft",      WM_ACTION_MOVE_TAB_LEFT},
	{"MoveTabRight",     WM_ACTION_MOVE_TAB_RIGHT},
	{"ActivateTab",      WM_ACTION_ACTIVATE_TAB},
	{"RootMenu",         WM_ACTION_ROOT_MENU},
	{"WindowMenu",       WM_ACTION_WINDOW_MENU},
	{"HideMenus",        WM_ACTION_HIDE_MENUS},
	{"Reconfigure",      WM_ACTION_RECONFIGURE},
	{"Exit",             WM_ACTION_EXIT},
	{NULL, WM_ACTION_NOP},
};

static bool
parse_action(const char *cmd_str, enum wm_action *action, const char **arg)
{
	/* Skip leading whitespace */
	while (*cmd_str && isspace((unsigned char)*cmd_str))
		cmd_str++;

	/* Find end of action name */
	const char *end = cmd_str;
	while (*end && !isspace((unsigned char)*end))
		end++;

	size_t name_len = (size_t)(end - cmd_str);
	if (name_len == 0) return false;

	for (const struct action_entry *e = action_table; e->name; e++) {
		if (strncasecmp(e->name, cmd_str, name_len) == 0 &&
		    strlen(e->name) == name_len) {
			*action = e->action;
			/* Skip whitespace after action name to get argument */
			while (*end && isspace((unsigned char)*end))
				end++;
			*arg = (*end) ? end : NULL;
			return true;
		}
	}
	return false;
}

/* ---------- action execution (simplified from keyboard.c) ---------- */

static void
exec_command(const char *cmd)
{
	if (!cmd || !*cmd) return;
	pid_t pid = fork();
	if (pid == 0) {
		setsid();
		execl("/bin/sh", "/bin/sh", "-c", cmd, (char *)NULL);
		_exit(1);
	}
}

static bool
ipc_execute_action(struct wm_server *server, enum wm_action action,
	const char *argument)
{
	struct wm_view *view = server->focused_view;

	switch (action) {
	case WM_ACTION_EXEC:
		exec_command(argument);
		return true;

	case WM_ACTION_EXIT:
		wl_display_terminate(server->wl_display);
		return true;

	case WM_ACTION_CLOSE:
		if (view)
			wlr_xdg_toplevel_send_close(view->xdg_toplevel);
		return true;

	case WM_ACTION_KILL:
		if (view) {
			struct wl_client *client = wl_resource_get_client(
				view->xdg_toplevel->base->resource);
			if (client) {
				pid_t pid;
				wl_client_get_credentials(client, &pid,
					NULL, NULL);
				if (pid > 0)
					kill(pid, SIGKILL);
			}
		}
		return true;

	case WM_ACTION_FOCUS_NEXT:
	case WM_ACTION_FOCUS_PREV:
		wm_focus_next_view(server);
		return true;

	case WM_ACTION_MAXIMIZE:
		if (view) {
			view->request_maximize.notify(
				&view->request_maximize, NULL);
		}
		return true;

	case WM_ACTION_FULLSCREEN:
		if (view) {
			view->request_fullscreen.notify(
				&view->request_fullscreen, NULL);
		}
		return true;

	case WM_ACTION_MINIMIZE:
		if (view) {
			view->request_minimize.notify(
				&view->request_minimize, NULL);
		}
		return true;

	case WM_ACTION_RAISE:
		if (view)
			wm_view_raise(view);
		return true;

	case WM_ACTION_WORKSPACE:
		if (argument) {
			int ws = atoi(argument) - 1;
			wm_workspace_switch(server, ws);
		}
		return true;

	case WM_ACTION_SEND_TO_WORKSPACE:
		if (argument) {
			int ws = atoi(argument) - 1;
			wm_view_send_to_workspace(server, ws);
		}
		return true;

	case WM_ACTION_NEXT_WORKSPACE:
		wm_workspace_switch_next(server);
		return true;

	case WM_ACTION_PREV_WORKSPACE:
		wm_workspace_switch_prev(server);
		return true;

	case WM_ACTION_STICK:
		if (view)
			wm_view_set_sticky(view, !view->sticky);
		return true;

	case WM_ACTION_MOVE:
		if (view)
			wm_view_begin_interactive(view, WM_CURSOR_MOVE, 0);
		return true;

	case WM_ACTION_MOVE_LEFT:
		if (view && argument) {
			int px = atoi(argument);
			view->x -= px;
			wlr_scene_node_set_position(
				&view->scene_tree->node, view->x, view->y);
		}
		return true;

	case WM_ACTION_MOVE_RIGHT:
		if (view && argument) {
			int px = atoi(argument);
			view->x += px;
			wlr_scene_node_set_position(
				&view->scene_tree->node, view->x, view->y);
		}
		return true;

	case WM_ACTION_MOVE_UP:
		if (view && argument) {
			int px = atoi(argument);
			view->y -= px;
			wlr_scene_node_set_position(
				&view->scene_tree->node, view->x, view->y);
		}
		return true;

	case WM_ACTION_MOVE_DOWN:
		if (view && argument) {
			int px = atoi(argument);
			view->y += px;
			wlr_scene_node_set_position(
				&view->scene_tree->node, view->x, view->y);
		}
		return true;

	default:
		return false;
	}
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

		char *name_esc = json_escape(ws->name);
		bool focused = (ws == server->current_workspace);

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

static char *
cmd_get_windows(struct wm_server *server)
{
	struct strbuf sb;
	strbuf_init(&sb);
	strbuf_append(&sb, "{\"success\":true,\"data\":[");

	struct wm_view *view;
	bool first = true;
	wl_list_for_each(view, &server->views, link) {
		if (!first) strbuf_append(&sb, ",");
		first = false;

		char *title_esc = json_escape(view->title);
		char *app_id_esc = json_escape(view->app_id);
		bool focused = (view == server->focused_view);

		struct wlr_box geo;
		wm_view_get_geometry(view, &geo);

		int ws_index = view->workspace ? view->workspace->index : -1;

		strbuf_appendf(&sb,
			"{\"app_id\":%s,\"title\":%s,"
			"\"workspace\":%d,\"focused\":%s,"
			"\"maximized\":%s,\"fullscreen\":%s,"
			"\"sticky\":%s,"
			"\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d}",
			app_id_esc, title_esc,
			ws_index, focused ? "true" : "false",
			view->maximized ? "true" : "false",
			view->fullscreen ? "true" : "false",
			view->sticky ? "true" : "false",
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

		char *name_esc = json_escape(output->wlr_output->name);

		int width = output->wlr_output->width;
		int height = output->wlr_output->height;

		double ox = 0, oy = 0;
		wlr_output_layout_output_coords(server->output_layout,
			output->wlr_output, &ox, &oy);

		strbuf_appendf(&sb,
			"{\"name\":%s,\"width\":%d,\"height\":%d,"
			"\"x\":%d,\"y\":%d}",
			name_esc, width, height, (int)ox, (int)oy);
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
cmd_command(struct wm_server *server, const char *action_str)
{
	if (!action_str || !*action_str) {
		return make_error("missing action argument");
	}

	enum wm_action action;
	const char *arg;
	if (!parse_action(action_str, &action, &arg)) {
		return make_error("unknown action");
	}

	bool ok = ipc_execute_action(server, action, arg);
	if (!ok) {
		return make_error("action not supported via IPC");
	}
	return make_ok();
}

static char *
cmd_subscribe(struct wm_ipc_client *client, const char *events_str)
{
	if (!events_str || !*events_str) {
		return make_error("missing event types");
	}

	/* Parse comma- or space-separated event names */
	char *copy = strdup(events_str);
	if (!copy) return make_error("out of memory");

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
		}
		token = strtok_r(NULL, ", ", &saveptr);
	}
	free(copy);

	return make_ok();
}

/* ---------- main dispatch ---------- */

char *
wm_ipc_handle_command(struct wm_ipc_server *ipc,
	struct wm_ipc_client *client, const char *json_request)
{
	if (!json_request || !*json_request) {
		return make_error("empty request");
	}

	/* Extract "command" field */
	char *command = json_get_string(json_request, "command");
	if (!command) {
		return make_error("missing 'command' field");
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
		char *action = json_get_string(json_request, "action");
		result = cmd_command(server, action);
		free(action);
	} else if (strcmp(command, "subscribe") == 0) {
		char *events = json_get_string(json_request, "events");
		result = cmd_subscribe(client, events);
		free(events);
	} else if (strcmp(command, "ping") == 0) {
		result = make_ok();
	} else {
		result = make_error("unknown command");
	}

	free(command);
	return result;
}
