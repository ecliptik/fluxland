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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/edges.h>
#include <wlr/util/log.h>

#include "i18n.h"
#include "ipc.h"
#include "config.h"
#include "decoration.h"
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
#include "workspace.h"

/* ---------- security helpers ---------- */

/* Environment variables that must not be set via IPC or keybinds */
static bool
is_blocked_env_var(const char *name)
{
	static const char *blocked[] = {
		"LD_PRELOAD", "LD_LIBRARY_PATH", "LD_AUDIT",
		"LD_DEBUG", "LD_PROFILE",
		"PATH", "IFS", "SHELL", "HOME",
		"XDG_RUNTIME_DIR", "WAYLAND_DISPLAY",
	};
	for (size_t i = 0; i < sizeof(blocked) / sizeof(blocked[0]); i++) {
		if (strcasecmp(name, blocked[i]) == 0)
			return true;
	}
	return false;
}

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
	if (len > SIZE_MAX / 6 - 1)
		return NULL;
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
				p += snprintf(p, cap - (size_t)(p - out), "\\u%04x", c);
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
						/* Quoted string value — unescape */
						after++;
						const char *end = after;
						while (*end && *end != '"') {
							if (*end == '\\') {
								if (*(end + 1) == '\0')
									break;
								end++;
							}
							end++;
						}
						/* Unterminated string — no closing quote */
						if (*end != '"') {
							return NULL;
						}
						size_t vlen = (size_t)(end - after);
						char *val = malloc(vlen + 1);
						if (!val) return NULL;
						size_t j = 0;
						for (const char *s = after; s < end; s++) {
							if (*s == '\\' && s + 1 < end) {
								s++;
								switch (*s) {
								case '"':  val[j++] = '"';  break;
								case '\\': val[j++] = '\\'; break;
								case '/':  val[j++] = '/';  break;
								case 'n':  val[j++] = '\n'; break;
								case 't':  val[j++] = '\t'; break;
								case 'r':  val[j++] = '\r'; break;
								default:   val[j++] = *s;   break;
								}
							} else {
								val[j++] = *s;
							}
						}
						val[j] = '\0';
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
	char *escaped = json_escape(message);
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
	{"FocusPrev",        WM_ACTION_FOCUS_PREV},
	{"NextWindow",       WM_ACTION_NEXT_WINDOW},
	{"PrevWindow",       WM_ACTION_PREV_WINDOW},
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
	{"Focus",            WM_ACTION_FOCUS},
	{"StartMoving",      WM_ACTION_START_MOVING},
	{"StartResizing",    WM_ACTION_START_RESIZING},
	{"StartTabbing",     WM_ACTION_START_TABBING},
	{"WindowList",       WM_ACTION_WINDOW_LIST},
	{"Deiconify",        WM_ACTION_DEICONIFY},
	{"NextLayout",       WM_ACTION_NEXT_LAYOUT},
	{"PrevLayout",       WM_ACTION_PREV_LAYOUT},
	{"ArrangeWindows",   WM_ACTION_ARRANGE_WINDOWS},
	{"ArrangeWindowsVertical",  WM_ACTION_ARRANGE_VERT},
	{"ArrangeWindowsHorizontal", WM_ACTION_ARRANGE_HORIZ},
	{"CascadeWindows",   WM_ACTION_CASCADE_WINDOWS},
	{"TakeToWorkspace",  WM_ACTION_TAKE_TO_WORKSPACE},
	{"SendToNextWorkspace", WM_ACTION_SEND_TO_NEXT_WORKSPACE},
	{"SendToPrevWorkspace", WM_ACTION_SEND_TO_PREV_WORKSPACE},
	{"TakeToNextWorkspace", WM_ACTION_TAKE_TO_NEXT_WORKSPACE},
	{"TakeToPrevWorkspace", WM_ACTION_TAKE_TO_PREV_WORKSPACE},
	{"AddWorkspace",     WM_ACTION_ADD_WORKSPACE},
	{"RemoveLastWorkspace", WM_ACTION_REMOVE_LAST_WORKSPACE},
	{"LHalf",            WM_ACTION_LHALF},
	{"RHalf",            WM_ACTION_RHALF},
	{"ResizeHorizontal", WM_ACTION_RESIZE_HORIZ},
	{"ResizeVertical",   WM_ACTION_RESIZE_VERT},
	{"FocusLeft",        WM_ACTION_FOCUS_LEFT},
	{"FocusRight",       WM_ACTION_FOCUS_RIGHT},
	{"FocusUp",          WM_ACTION_FOCUS_UP},
	{"FocusDown",        WM_ACTION_FOCUS_DOWN},
	{"SetHead",          WM_ACTION_SET_HEAD},
	{"SendToNextHead",   WM_ACTION_SEND_TO_NEXT_HEAD},
	{"SendToPrevHead",   WM_ACTION_SEND_TO_PREV_HEAD},
	{"ArrangeWindowsStackLeft",   WM_ACTION_ARRANGE_STACK_LEFT},
	{"ArrangeWindowsStackRight",  WM_ACTION_ARRANGE_STACK_RIGHT},
	{"ArrangeWindowsStackTop",    WM_ACTION_ARRANGE_STACK_TOP},
	{"ArrangeWindowsStackBottom", WM_ACTION_ARRANGE_STACK_BOTTOM},
	{"CloseAllWindows",  WM_ACTION_CLOSE_ALL_WINDOWS},
	{"WorkspaceMenu",    WM_ACTION_WORKSPACE_MENU},
	{"ClientMenu",       WM_ACTION_CLIENT_MENU},
	{"CustomMenu",       WM_ACTION_CUSTOM_MENU},
	{"SetStyle",         WM_ACTION_SET_STYLE},
	{"ReloadStyle",      WM_ACTION_RELOAD_STYLE},
	{"RightWorkspace",   WM_ACTION_RIGHT_WORKSPACE},
	{"LeftWorkspace",    WM_ACTION_LEFT_WORKSPACE},
	{"SetWorkspaceName", WM_ACTION_SET_WORKSPACE_NAME},
	{"Remember",         WM_ACTION_REMEMBER},
	{"ToggleSlitAbove",  WM_ACTION_TOGGLE_SLIT_ABOVE},
	{"ToggleSlitHidden", WM_ACTION_TOGGLE_SLIT_HIDDEN},
	{"ToggleToolbarAbove", WM_ACTION_TOGGLE_TOOLBAR_ABOVE},
	{"ToggleToolbarVisible", WM_ACTION_TOGGLE_TOOLBAR_VISIBLE},
	{"ToggleShowPosition", WM_ACTION_TOGGLE_SHOW_POSITION},
	{"MacroCmd",         WM_ACTION_MACRO_CMD},
	{"ToggleCmd",        WM_ACTION_TOGGLE_CMD},
	{"Reconfigure",      WM_ACTION_RECONFIGURE},
	{"Exit",             WM_ACTION_EXIT},
	{"SetAlpha",         WM_ACTION_SET_ALPHA},
	{"SetEnv",           WM_ACTION_SET_ENV},
	{"Export",           WM_ACTION_SET_ENV},
	{"BindKey",          WM_ACTION_BIND_KEY},
	{"GotoWindow",       WM_ACTION_GOTO_WINDOW},
	{"NextGroup",        WM_ACTION_NEXT_GROUP},
	{"PrevGroup",        WM_ACTION_PREV_GROUP},
	{"Unclutter",        WM_ACTION_UNCLUTTER},
	{"If",               WM_ACTION_IF},
	{"ForEach",          WM_ACTION_FOREACH},
	{"Map",              WM_ACTION_MAP},
	{"Delay",            WM_ACTION_DELAY},
	{"FocusToolbar",     WM_ACTION_FOCUS_TOOLBAR},
	{"FocusNextElement", WM_ACTION_FOCUS_NEXT_ELEMENT},
	{"FocusPrevElement", WM_ACTION_FOCUS_PREV_ELEMENT},
	{"FocusActivate",    WM_ACTION_FOCUS_ACTIVATE},
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
	if (pid < 0) return;
	if (pid == 0) {
		sigset_t set;
		sigemptyset(&set);
		sigprocmask(SIG_SETMASK, &set, NULL);
		pid_t g = fork();
		if (g < 0) _exit(1);
		if (g > 0) _exit(0);
		setsid();
		closefrom(STDERR_FILENO + 1);
		execl("/bin/sh", "/bin/sh", "-c", cmd, (char *)NULL);
		_exit(1);
	}
	waitpid(pid, NULL, 0);
}

static bool
ipc_execute_action(struct wm_server *server, enum wm_action action,
	const char *argument)
{
	struct wm_view *view = server->focused_view;

	switch (action) {
	case WM_ACTION_EXEC:
		if (server->ipc_no_exec) {
			wlr_log(WLR_INFO, "%s",
				"IPC Exec blocked by --ipc-no-exec");
			return true;
		}
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
				wl_client_destroy(client);
			}
		}
		return true;

	case WM_ACTION_FOCUS_NEXT:
		wm_focus_next_view(server);
		return true;

	case WM_ACTION_FOCUS_PREV:
		wm_focus_prev_view(server);
		return true;

	case WM_ACTION_NEXT_WINDOW:
		if (argument) {
			struct wm_workspace *ws = wm_workspace_get_active(server);
			struct wm_view *focused = server->focused_view;
			struct wm_view *candidate = NULL;
			bool past_focused = (focused == NULL);
			struct wm_view *first_match = NULL;
			struct wm_view *nwv;
			wl_list_for_each(nwv, &server->views, link) {
				if (nwv->workspace != ws && !nwv->sticky)
					continue;
				bool match = (nwv->title &&
					fnmatch(argument, nwv->title,
						0) == 0) ||
					(nwv->app_id &&
					fnmatch(argument, nwv->app_id,
						0) == 0);
				if (!match)
					continue;
				if (!first_match)
					first_match = nwv;
				if (past_focused) {
					candidate = nwv;
					break;
				}
				if (nwv == focused)
					past_focused = true;
			}
			if (!candidate)
				candidate = first_match;
			if (candidate && candidate != focused)
				wm_focus_view(candidate,
					candidate->xdg_toplevel->base->surface);
		} else {
			wm_view_cycle_next(server);
		}
		return true;

	case WM_ACTION_PREV_WINDOW:
		if (argument) {
			struct wm_workspace *ws = wm_workspace_get_active(server);
			struct wm_view *focused = server->focused_view;
			struct wm_view *candidate = NULL;
			struct wm_view *last_match = NULL;
			struct wm_view *pwv;
			wl_list_for_each(pwv, &server->views, link) {
				if (pwv->workspace != ws && !pwv->sticky)
					continue;
				bool match = (pwv->title &&
					fnmatch(argument, pwv->title,
						0) == 0) ||
					(pwv->app_id &&
					fnmatch(argument, pwv->app_id,
						0) == 0);
				if (!match)
					continue;
				if (pwv == focused) {
					if (candidate)
						break;
					continue;
				}
				candidate = pwv;
				last_match = pwv;
			}
			if (!candidate) {
				wl_list_for_each(pwv, &server->views, link) {
					if (pwv->workspace != ws &&
					    !pwv->sticky)
						continue;
					bool m = (pwv->title &&
						fnmatch(argument,
							pwv->title,
							0) == 0) ||
						(pwv->app_id &&
						fnmatch(argument,
							pwv->app_id,
							0) == 0);
					if (m)
						last_match = pwv;
				}
				candidate = last_match;
			}
			if (candidate && candidate != focused)
				wm_focus_view(candidate,
					candidate->xdg_toplevel->base->surface);
		} else {
			wm_view_cycle_prev(server);
		}
		return true;

	case WM_ACTION_DEICONIFY:
		if (argument) {
			if (strcasecmp(argument, "All") == 0)
				wm_view_deiconify_all(server);
			else if (strcasecmp(argument, "AllWorkspace") == 0)
				wm_view_deiconify_all_workspace(server);
			else if (strcasecmp(argument, "LastWorkspace") == 0)
				wm_view_deiconify_last(server);
			else
				wm_view_deiconify_last(server);
		} else {
			wm_view_deiconify_last(server);
		}
		return true;

	case WM_ACTION_MAXIMIZE:
		if (view) {
			view->request_maximize.notify(
				&view->request_maximize, NULL);
		}
		return true;

	case WM_ACTION_MAXIMIZE_VERT:
		if (view)
			wm_view_maximize_vert(view);
		return true;

	case WM_ACTION_MAXIMIZE_HORIZ:
		if (view)
			wm_view_maximize_horiz(view);
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

	case WM_ACTION_SHADE:
		if (view && view->decoration && server->style)
			wm_decoration_set_shaded(view->decoration,
				!view->decoration->shaded, server->style);
		return true;

	case WM_ACTION_SHADE_ON:
		if (view && view->decoration && server->style)
			wm_decoration_set_shaded(view->decoration,
				true, server->style);
		return true;

	case WM_ACTION_SHADE_OFF:
		if (view && view->decoration && server->style)
			wm_decoration_set_shaded(view->decoration,
				false, server->style);
		return true;

	case WM_ACTION_RAISE:
		if (view)
			wm_view_raise(view);
		return true;

	case WM_ACTION_LOWER:
		if (view)
			wm_view_lower(view);
		return true;

	case WM_ACTION_RAISE_LAYER:
		if (view)
			wm_view_raise_layer(view);
		return true;

	case WM_ACTION_LOWER_LAYER:
		if (view)
			wm_view_lower_layer(view);
		return true;

	case WM_ACTION_SET_LAYER:
		if (view && argument) {
			enum wm_view_layer layer =
				wm_view_layer_from_name(argument);
			wm_view_set_layer(view, layer);
		}
		return true;

	case WM_ACTION_STICK:
		if (view)
			wm_view_set_sticky(view, !view->sticky);
		return true;

	case WM_ACTION_WORKSPACE:
		if (argument) {
			int ws;
			if (safe_atoi(argument, &ws))
				wm_workspace_switch(server, ws - 1);
		}
		return true;

	case WM_ACTION_SEND_TO_WORKSPACE:
		if (argument) {
			int ws;
			if (safe_atoi(argument, &ws))
				wm_view_send_to_workspace(server, ws - 1);
		}
		return true;

	case WM_ACTION_NEXT_WORKSPACE:
		wm_workspace_switch_next(server);
		return true;

	case WM_ACTION_PREV_WORKSPACE:
		wm_workspace_switch_prev(server);
		return true;

	case WM_ACTION_SHOW_DESKTOP: {
		struct wm_view *v;
		wl_list_for_each(v, &server->views, link) {
			if (v->workspace == wm_workspace_get_active(server)) {
				v->request_minimize.notify(
					&v->request_minimize, NULL);
			}
		}
		return true;
	}

	case WM_ACTION_MOVE:
		if (view)
			wm_view_begin_interactive(view, WM_CURSOR_MOVE, 0);
		return true;

	case WM_ACTION_RESIZE:
		if (view)
			wm_view_begin_interactive(view, WM_CURSOR_RESIZE,
				WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT);
		return true;

	case WM_ACTION_MOVE_LEFT:
		if (view && argument) {
			int px;
			if (safe_atoi(argument, &px)) {
				view->x -= px;
				wlr_scene_node_set_position(
					&view->scene_tree->node, view->x, view->y);
			}
		}
		return true;

	case WM_ACTION_MOVE_RIGHT:
		if (view && argument) {
			int px;
			if (safe_atoi(argument, &px)) {
				view->x += px;
				wlr_scene_node_set_position(
					&view->scene_tree->node, view->x, view->y);
			}
		}
		return true;

	case WM_ACTION_MOVE_UP:
		if (view && argument) {
			int px;
			if (safe_atoi(argument, &px)) {
				view->y -= px;
				wlr_scene_node_set_position(
					&view->scene_tree->node, view->x, view->y);
			}
		}
		return true;

	case WM_ACTION_MOVE_DOWN:
		if (view && argument) {
			int px;
			if (safe_atoi(argument, &px)) {
				view->y += px;
				wlr_scene_node_set_position(
					&view->scene_tree->node, view->x, view->y);
			}
		}
		return true;

	case WM_ACTION_MOVE_TO:
		if (view && argument) {
			int x = 0, y = 0;
			sscanf(argument, "%d %d", &x, &y);
			view->x = x;
			view->y = y;
			wlr_scene_node_set_position(
				&view->scene_tree->node, x, y);
		}
		return true;

	case WM_ACTION_RESIZE_TO:
		if (view && argument) {
			int w = 0, h = 0;
			sscanf(argument, "%d %d", &w, &h);
			if (w > 0 && h > 0)
				wlr_xdg_toplevel_set_size(
					view->xdg_toplevel, w, h);
		}
		return true;

	case WM_ACTION_TOGGLE_DECOR:
		if (view)
			wm_view_toggle_decoration(view);
		return true;

	case WM_ACTION_SET_DECOR:
		if (view && view->decoration && server->style && argument) {
			enum wm_decor_preset preset = WM_DECOR_NORMAL;
			if (strcasecmp(argument, "NONE") == 0 ||
			    strcmp(argument, "0") == 0)
				preset = WM_DECOR_NONE;
			else if (strcasecmp(argument, "BORDER") == 0)
				preset = WM_DECOR_BORDER;
			else if (strcasecmp(argument, "TAB") == 0)
				preset = WM_DECOR_TAB;
			else if (strcasecmp(argument, "TINY") == 0)
				preset = WM_DECOR_TINY;
			else if (strcasecmp(argument, "TOOL") == 0)
				preset = WM_DECOR_TOOL;
			wm_decoration_set_preset(view->decoration,
				preset, server->style);
		}
		return true;

	case WM_ACTION_KEY_MODE:
		if (argument) {
			char mode_name[256];
			const char *sp = argument;
			while (*sp && !isspace((unsigned char)*sp))
				sp++;
			size_t len = sp - argument;
			if (len >= sizeof(mode_name))
				len = sizeof(mode_name) - 1;
			memcpy(mode_name, argument, len);
			mode_name[len] = '\0';
			free(server->current_keymode);
			server->current_keymode = strdup(mode_name);
			if (!server->current_keymode)
				server->current_keymode = strdup("default");
			wlr_log(WLR_INFO, "IPC: keymode switched to: %s",
				mode_name);
		}
		return true;

	case WM_ACTION_NEXT_TAB:
		if (view && view->tab_group)
			wm_tab_group_next(view->tab_group);
		return true;

	case WM_ACTION_PREV_TAB:
		if (view && view->tab_group)
			wm_tab_group_prev(view->tab_group);
		return true;

	case WM_ACTION_DETACH_CLIENT:
		if (view && view->tab_group)
			wm_tab_group_remove(view);
		return true;

	case WM_ACTION_MOVE_TAB_LEFT:
		if (view && view->tab_group)
			wm_tab_group_move_left(view->tab_group, view);
		return true;

	case WM_ACTION_MOVE_TAB_RIGHT:
		if (view && view->tab_group)
			wm_tab_group_move_right(view->tab_group, view);
		return true;

	case WM_ACTION_ACTIVATE_TAB:
		if (view && argument) {
			int idx;
			if (safe_atoi(argument, &idx))
				wm_view_activate_tab(view, idx - 1);
		}
		return true;

	case WM_ACTION_START_TABBING:
		/* Tabbing is initiated by mouse action, not useful via IPC */
		return true;

	case WM_ACTION_START_MOVING:
		if (view)
			wm_view_begin_interactive(view, WM_CURSOR_MOVE, 0);
		return true;

	case WM_ACTION_START_RESIZING:
		if (view)
			wm_view_begin_interactive(view, WM_CURSOR_RESIZE,
				WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT);
		return true;

	case WM_ACTION_FOCUS:
		if (view)
			wm_focus_view(view, NULL);
		return true;

	case WM_ACTION_ROOT_MENU:
		wm_menu_show_root(server,
			(int)server->cursor->x, (int)server->cursor->y);
		return true;

	case WM_ACTION_WINDOW_MENU:
		wm_menu_show_window(server,
			(int)server->cursor->x, (int)server->cursor->y);
		return true;

	case WM_ACTION_WINDOW_LIST:
		wm_menu_show_window_list(server,
			(int)server->cursor->x, (int)server->cursor->y);
		return true;

	case WM_ACTION_HIDE_MENUS:
		wm_menu_hide_all(server);
		return true;

	case WM_ACTION_WORKSPACE_MENU:
		wm_menu_show_workspace_menu(server,
			(int)server->cursor->x, (int)server->cursor->y);
		return true;

	case WM_ACTION_CLIENT_MENU:
		wm_menu_show_client_menu(server, argument,
			(int)server->cursor->x, (int)server->cursor->y);
		return true;

	case WM_ACTION_CUSTOM_MENU:
		wm_menu_show_custom(server, argument,
			(int)server->cursor->x, (int)server->cursor->y);
		return true;

	case WM_ACTION_SET_STYLE:
		if (server->ipc_no_exec) {
			wlr_log(WLR_INFO, "%s",
				"IPC SetStyle blocked by --ipc-no-exec");
			return true;
		}
		if (argument && server->config) {
			/* Reject paths with traversal sequences */
			if (strstr(argument, "..")) {
				wlr_log(WLR_ERROR,
					"SetStyle: rejecting path with '..': %s",
					argument);
				return true;
			}
			char *resolved = realpath(argument, NULL);
			if (!resolved) {
				wlr_log(WLR_ERROR,
					"SetStyle: cannot resolve path: %s",
					argument);
				return true;
			}
			/* Only allow style files under home or system dirs */
			const char *home = getenv("HOME");
			bool allowed = false;
			if (home && strncmp(resolved, home,
					strlen(home)) == 0)
				allowed = true;
			if (strncmp(resolved, "/usr/share/", 11) == 0)
				allowed = true;
			if (strncmp(resolved, "/usr/local/share/", 17) == 0)
				allowed = true;
			if (strncmp(resolved, "/etc/", 5) == 0)
				allowed = true;
			if (!allowed) {
				wlr_log(WLR_ERROR,
					"SetStyle: path outside allowed directories: %s",
					resolved);
				free(resolved);
				return true;
			}
			free(server->config->style_file);
			server->config->style_file = resolved;
			if (server->style && server->config->style_file)
				style_load(server->style,
					server->config->style_file);
			if (server->toolbar)
				wm_toolbar_relayout(server->toolbar);
			if (server->style) {
				struct wm_view *v;
				wl_list_for_each(v, &server->views, link) {
					if (v->decoration)
						wm_decoration_update(
							v->decoration,
							server->style);
				}
			}
		}
		return true;

	case WM_ACTION_RELOAD_STYLE:
		if (server->style && server->config &&
		    server->config->style_file)
			style_load(server->style,
				server->config->style_file);
		if (server->toolbar)
			wm_toolbar_relayout(server->toolbar);
		if (server->style) {
			struct wm_view *v;
			wl_list_for_each(v, &server->views, link) {
				if (v->decoration)
					wm_decoration_update(v->decoration,
						server->style);
			}
		}
		return true;

	case WM_ACTION_NEXT_LAYOUT:
		wm_keyboard_next_layout(server);
		return true;

	case WM_ACTION_PREV_LAYOUT:
		wm_keyboard_prev_layout(server);
		return true;

	case WM_ACTION_RECONFIGURE:
		if (server->config) {
			config_reload(server->config);
			server->focus_policy = server->config->focus_policy;
		}
		keybind_destroy_all(&server->keymodes);
		wl_list_init(&server->keymodes);
		if (server->config && server->config->keys_file)
			keybind_load(&server->keymodes,
				server->config->keys_file);
		keybind_destroy_list(&server->keybindings);
		wl_list_init(&server->keybindings);
		free(server->current_keymode);
		server->current_keymode = strdup("default");
		if (!server->current_keymode)
			return false;
		wm_keyboard_apply_config(server);
		wlr_log(WLR_INFO, "%s", "IPC: configuration reloaded");
		return true;

	case WM_ACTION_ARRANGE_WINDOWS:
		wm_arrange_windows_grid(server);
		return true;

	case WM_ACTION_ARRANGE_VERT:
		wm_arrange_windows_vert(server);
		return true;

	case WM_ACTION_ARRANGE_HORIZ:
		wm_arrange_windows_horiz(server);
		return true;

	case WM_ACTION_CASCADE_WINDOWS:
		wm_arrange_windows_cascade(server);
		return true;

	case WM_ACTION_TAKE_TO_WORKSPACE:
		if (argument) {
			int ws;
			if (safe_atoi(argument, &ws))
				wm_view_take_to_workspace(server, ws - 1);
		}
		return true;

	case WM_ACTION_SEND_TO_NEXT_WORKSPACE:
		wm_view_send_to_next_workspace(server);
		return true;

	case WM_ACTION_SEND_TO_PREV_WORKSPACE:
		wm_view_send_to_prev_workspace(server);
		return true;

	case WM_ACTION_TAKE_TO_NEXT_WORKSPACE:
		wm_view_take_to_next_workspace(server);
		return true;

	case WM_ACTION_TAKE_TO_PREV_WORKSPACE:
		wm_view_take_to_prev_workspace(server);
		return true;

	case WM_ACTION_ADD_WORKSPACE:
		wm_workspace_add(server);
		return true;

	case WM_ACTION_REMOVE_LAST_WORKSPACE:
		wm_workspace_remove_last(server);
		return true;

	case WM_ACTION_LHALF:
		if (view)
			wm_view_lhalf(view);
		return true;

	case WM_ACTION_RHALF:
		if (view)
			wm_view_rhalf(view);
		return true;

	case WM_ACTION_RESIZE_HORIZ:
		if (view && argument) {
			int dw;
			if (safe_atoi(argument, &dw))
				wm_view_resize_by(view, dw, 0);
		}
		return true;

	case WM_ACTION_RESIZE_VERT:
		if (view && argument) {
			int dh;
			if (safe_atoi(argument, &dh))
				wm_view_resize_by(view, 0, dh);
		}
		return true;

	case WM_ACTION_FOCUS_LEFT:
		wm_view_focus_direction(server, -1, 0);
		return true;

	case WM_ACTION_FOCUS_RIGHT:
		wm_view_focus_direction(server, 1, 0);
		return true;

	case WM_ACTION_FOCUS_UP:
		wm_view_focus_direction(server, 0, -1);
		return true;

	case WM_ACTION_FOCUS_DOWN:
		wm_view_focus_direction(server, 0, 1);
		return true;

	case WM_ACTION_SET_HEAD:
		if (view && argument) {
			int head;
			if (safe_atoi(argument, &head))
				wm_view_set_head(view, head);
		}
		return true;

	case WM_ACTION_SEND_TO_NEXT_HEAD:
		if (view)
			wm_view_send_to_next_head(view);
		return true;

	case WM_ACTION_SEND_TO_PREV_HEAD:
		if (view)
			wm_view_send_to_prev_head(view);
		return true;

	case WM_ACTION_ARRANGE_STACK_LEFT:
		wm_arrange_windows_stack_left(server);
		return true;

	case WM_ACTION_ARRANGE_STACK_RIGHT:
		wm_arrange_windows_stack_right(server);
		return true;

	case WM_ACTION_ARRANGE_STACK_TOP:
		wm_arrange_windows_stack_top(server);
		return true;

	case WM_ACTION_ARRANGE_STACK_BOTTOM:
		wm_arrange_windows_stack_bottom(server);
		return true;

	case WM_ACTION_CLOSE_ALL_WINDOWS:
		wm_view_close_all(server);
		return true;

	case WM_ACTION_RIGHT_WORKSPACE:
		wm_workspace_switch_right(server);
		return true;

	case WM_ACTION_LEFT_WORKSPACE:
		wm_workspace_switch_left(server);
		return true;

	case WM_ACTION_SET_WORKSPACE_NAME:
		if (argument)
			wm_workspace_set_name(server, argument);
		return true;

	case WM_ACTION_REMEMBER:
		if (view && server->config && server->config->apps_file)
			wm_rules_remember_window(view,
				server->config->apps_file);
		return true;

	case WM_ACTION_TOGGLE_SLIT_ABOVE:
		if (server->slit)
			wm_slit_toggle_above(server->slit);
		return true;

	case WM_ACTION_TOGGLE_SLIT_HIDDEN:
		if (server->slit)
			wm_slit_toggle_hidden(server->slit);
		return true;

	case WM_ACTION_TOGGLE_TOOLBAR_ABOVE:
		if (server->toolbar)
			wm_toolbar_toggle_above(server->toolbar);
		return true;

	case WM_ACTION_TOGGLE_TOOLBAR_VISIBLE:
		if (server->toolbar)
			wm_toolbar_toggle_visible(server->toolbar);
		return true;

	case WM_ACTION_TOGGLE_SHOW_POSITION:
		server->show_position = !server->show_position;
		return true;

	case WM_ACTION_SET_ALPHA:
		if (view && argument) {
			int alpha;
			if (argument[0] == '+' || argument[0] == '-') {
				int offset;
				if (!safe_atoi(argument, &offset))
					return true;
				alpha = view->focus_alpha + offset;
			} else {
				if (!safe_atoi(argument, &alpha))
					return true;
			}
			if (alpha < 0) alpha = 0;
			if (alpha > 255) alpha = 255;
			view->focus_alpha = alpha;
			view->unfocus_alpha = alpha;
			wm_view_set_opacity(view, alpha);
		}
		return true;

	case WM_ACTION_SET_ENV:
		if (argument) {
			char *buf = strdup(argument);
			if (buf) {
				char *eq = strchr(buf, '=');
				if (eq) {
					*eq = '\0';
					if (is_blocked_env_var(buf)) {
						wlr_log(WLR_ERROR,
							"SetEnv: blocked security-sensitive "
							"variable: %s", buf);
					} else {
						setenv(buf, eq + 1, 1);
					}
				} else {
					char *sp = strchr(buf, ' ');
					if (sp) {
						*sp = '\0';
						sp++;
						while (*sp == ' ' || *sp == '\t')
							sp++;
						if (is_blocked_env_var(buf)) {
							wlr_log(WLR_ERROR,
								"SetEnv: blocked security-sensitive "
								"variable: %s", buf);
						} else {
							setenv(buf, sp, 1);
						}
					}
				}
				free(buf);
			}
		}
		return true;

	case WM_ACTION_BIND_KEY: {
		if (server->ipc_no_exec) {
			wlr_log(WLR_INFO, "%s",
				"IPC BindKey blocked by --ipc-no-exec");
			return true;
		}
		if (!argument)
			return true;
		struct wm_keymode *mode = keybind_get_mode(
			&server->keymodes, server->current_keymode ?
			server->current_keymode : "default");
		if (!mode)
			return true;
		keybind_add_from_string(&mode->bindings, argument);
		return true;
	}

	case WM_ACTION_GOTO_WINDOW:
		if (argument) {
			int n;
			if (safe_atoi(argument, &n) && n >= 1) {
				struct wm_workspace *ws =
					wm_workspace_get_active(server);
				int count = 0;
				struct wm_view *gv;
				wl_list_for_each(gv, &server->views, link) {
					if (gv->workspace != ws && !gv->sticky)
						continue;
					count++;
					if (count == n) {
						wm_focus_view(gv,
							gv->xdg_toplevel->base
							->surface);
						break;
					}
				}
			}
		}
		return true;

	case WM_ACTION_NEXT_GROUP:
	case WM_ACTION_PREV_GROUP: {
		struct wm_workspace *ws = wm_workspace_get_active(server);
		struct wm_tab_group *groups[256];
		int ngroups = 0;
		struct wm_view *gv;
		wl_list_for_each(gv, &server->views, link) {
			if (gv->workspace != ws && !gv->sticky)
				continue;
			if (!gv->tab_group)
				continue;
			bool found = false;
			for (int i = 0; i < ngroups; i++) {
				if (groups[i] == gv->tab_group) {
					found = true;
					break;
				}
			}
			if (!found && ngroups < 256)
				groups[ngroups++] = gv->tab_group;
		}
		if (ngroups < 2)
			return true;
		int cur_idx = -1;
		if (view && view->tab_group) {
			for (int i = 0; i < ngroups; i++) {
				if (groups[i] == view->tab_group) {
					cur_idx = i;
					break;
				}
			}
		}
		int next_idx;
		if (cur_idx < 0) {
			next_idx = 0;
		} else if (action == WM_ACTION_NEXT_GROUP) {
			next_idx = (cur_idx + 1) % ngroups;
		} else {
			next_idx = (cur_idx - 1 + ngroups) % ngroups;
		}
		struct wm_view *target = groups[next_idx]->active_view;
		if (target)
			wm_focus_view(target,
				target->xdg_toplevel->base->surface);
		return true;
	}

	case WM_ACTION_UNCLUTTER: {
		struct wm_workspace *ws = wm_workspace_get_active(server);
		struct wlr_box area = {0, 0, 800, 600};
		struct wm_output *output;
		wl_list_for_each(output, &server->outputs, link) {
			area = output->usable_area;
			break;
		}
		int ipc_offset = 0;
		struct wm_view *uv;
		wl_list_for_each(uv, &server->views, link) {
			if (uv->workspace != ws && !uv->sticky)
				continue;
			if (!uv->scene_tree->node.enabled)
				continue;
			uv->x = area.x + ipc_offset;
			uv->y = area.y + ipc_offset;
			wlr_scene_node_set_position(
				&uv->scene_tree->node, uv->x, uv->y);
			ipc_offset += 30;
		}
		return true;
	}

	case WM_ACTION_MACRO_CMD:
	case WM_ACTION_TOGGLE_CMD:
		wlr_log(WLR_INFO,
			"%s", "MacroCmd/ToggleCmd not supported via IPC");
		return false;

	case WM_ACTION_IF:
	case WM_ACTION_FOREACH:
	case WM_ACTION_MAP:
	case WM_ACTION_DELAY:
		wlr_log(WLR_INFO,
			"%s", "If/ForEach/Map/Delay not supported via IPC");
		return false;

	case WM_ACTION_NOP:
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
cmd_command(struct wm_server *server, const char *action_str)
{
	if (!action_str || !*action_str) {
		return make_error(_("missing action argument"));
	}

	enum wm_action action;
	const char *arg;
	if (!parse_action(action_str, &action, &arg)) {
		return make_error(_("unknown action"));
	}

	bool ok = ipc_execute_action(server, action, arg);
	if (!ok) {
		return make_error(_("action not supported via IPC"));
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

/* ---------- main dispatch ---------- */

char *
wm_ipc_handle_command(struct wm_ipc_server *ipc,
	struct wm_ipc_client *client, const char *json_request)
{
	if (!json_request || !*json_request) {
		return make_error(_("empty request"));
	}

	/* Extract "command" field */
	char *command = json_get_string(json_request, "command");
	if (!command) {
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
		result = make_error(_("unknown command"));
	}

	free(command);
	return result;
}
