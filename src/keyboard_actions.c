/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * keyboard_actions.c - Keyboard action dispatch
 *
 * All action execution, condition evaluation, and security helpers.
 * Split from keyboard.c for maintainability and testability.
 */

#define _GNU_SOURCE
#include <ctype.h>
#include <fnmatch.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/edges.h>
#include <wlr/util/log.h>

#include "animation.h"
#include "foreign_toplevel.h"
#include "i18n.h"
#include "keyboard_actions.h"
#include "keyboard.h"
#include "server.h"
#include "config.h"
#include "util.h"
#include "focus_nav.h"
#include "output.h"
#include "decoration.h"
#include "keybind.h"
#include "menu.h"
#include "placement.h"
#include "rules.h"
#include "slit.h"
#include "tabgroup.h"
#include "style.h"
#include "toolbar.h"
#include "view.h"
#include "workspace.h"

/* Security helpers are in util.h: wm_is_blocked_env_var(), wm_spawn_command() */

/* --- Condition evaluation for If/ForEach --- */

/*
 * Check if a boolean property string matches "yes", "true", or "1".
 */
static bool
bool_match(const char *pattern, bool value)
{
	bool pat_true = (strcasecmp(pattern, "yes") == 0 ||
			 strcasecmp(pattern, "true") == 0 ||
			 strcmp(pattern, "1") == 0);
	return value == pat_true;
}

/*
 * Layer name string for a view layer enum value.
 */
static const char *
layer_name(enum wm_view_layer layer)
{
	switch (layer) {
	case WM_LAYER_DESKTOP: return "Desktop";
	case WM_LAYER_BELOW:   return "Below";
	case WM_LAYER_NORMAL:  return "Normal";
	case WM_LAYER_ABOVE:   return "Above";
	default:               return "Normal";
	}
}

/*
 * Match a view property against a fnmatch pattern.
 */
static bool
match_property(struct wm_view *view, const char *property,
	       const char *pattern)
{
	if (!view || !property || !pattern)
		return false;

	if (strcasecmp(property, "title") == 0) {
		return view->title &&
		       fnmatch(pattern, view->title, 0) == 0;
	}
	if (strcasecmp(property, "class") == 0 ||
	    strcasecmp(property, "name") == 0) {
		return view->app_id &&
		       fnmatch(pattern, view->app_id, 0) == 0;
	}
	if (strcasecmp(property, "workspace") == 0) {
		if (view->workspace && view->workspace->name &&
		    fnmatch(pattern, view->workspace->name, 0) == 0)
			return true;
		/* Also try matching workspace index as string */
		if (view->workspace) {
			char idx[16];
			snprintf(idx, sizeof(idx), "%d",
				 view->workspace->index + 1);
			return fnmatch(pattern, idx, 0) == 0;
		}
		return false;
	}
	if (strcasecmp(property, "maximized") == 0)
		return bool_match(pattern, view->maximized);
	if (strcasecmp(property, "minimized") == 0)
		return bool_match(pattern,
			!view->scene_tree->node.enabled);
	if (strcasecmp(property, "fullscreen") == 0)
		return bool_match(pattern, view->fullscreen);
	if (strcasecmp(property, "shaded") == 0)
		return bool_match(pattern,
			view->decoration && view->decoration->shaded);
	if (strcasecmp(property, "sticky") == 0)
		return bool_match(pattern, view->sticky);
	if (strcasecmp(property, "layer") == 0)
		return fnmatch(pattern, layer_name(view->layer), 0) == 0;

	wlr_log(WLR_ERROR, "unknown condition property: %s", property);
	return false;
}

/*
 * Recursively evaluate a condition tree against a view.
 */
static bool
evaluate_condition(struct wm_server *server,
		   struct wm_condition *cond, struct wm_view *view)
{
	if (!cond)
		return false;

	switch (cond->type) {
	case WM_COND_MATCHES:
		return match_property(view, cond->property, cond->pattern);

	case WM_COND_SOME: {
		struct wm_view *v;
		wl_list_for_each(v, &server->views, link) {
			if (match_property(v, cond->property, cond->pattern))
				return true;
		}
		return false;
	}

	case WM_COND_EVERY: {
		struct wm_view *v;
		wl_list_for_each(v, &server->views, link) {
			if (!match_property(v, cond->property, cond->pattern))
				return false;
		}
		return true;
	}

	case WM_COND_NOT:
		return !evaluate_condition(server, cond->child, view);

	case WM_COND_AND:
		return evaluate_condition(server, cond->left, view) &&
		       evaluate_condition(server, cond->right, view);

	case WM_COND_OR:
		return evaluate_condition(server, cond->left, view) ||
		       evaluate_condition(server, cond->right, view);

	case WM_COND_XOR:
		return evaluate_condition(server, cond->left, view) ^
		       evaluate_condition(server, cond->right, view);
	}

	return false;
}

/* --- Delay timer callback data --- */

struct delay_cb_data {
	struct wm_server *server;
	enum wm_action action;
	char *argument;
	struct wl_event_source *timer;
};

static int
delay_timer_cb(void *data)
{
	struct delay_cb_data *cb = data;
	wm_execute_action(cb->server, cb->action, cb->argument);
	wl_event_source_remove(cb->timer);
	free(cb->argument);
	free(cb);
	return 0;
}

/*
 * Execute a keybind, including MacroCmd/ToggleCmd dispatch.
 */
bool
wm_execute_keybind_action(struct wm_server *server,
	struct wm_keybind *bind)
{
	if (bind->action == WM_ACTION_MACRO_CMD) {
		struct wm_subcmd *cmd = bind->subcmds;
		while (cmd) {
			wm_execute_action(server, cmd->action, cmd->argument);
			cmd = cmd->next;
		}
		return true;
	}

	if (bind->action == WM_ACTION_TOGGLE_CMD) {
		if (bind->subcmd_count > 0) {
			struct wm_subcmd *cmd = bind->subcmds;
			int idx = bind->toggle_index % bind->subcmd_count;
			for (int i = 0; i < idx && cmd; i++)
				cmd = cmd->next;
			if (cmd) {
				wm_execute_action(server, cmd->action,
					cmd->argument);
			}
			bind->toggle_index =
				(bind->toggle_index + 1) % bind->subcmd_count;
		}
		return true;
	}

	if (bind->action == WM_ACTION_IF) {
		struct wm_view *view = server->focused_view;
		bool result = evaluate_condition(server,
			bind->condition, view);
		if (result && bind->subcmds) {
			wm_execute_action(server, bind->subcmds->action,
				bind->subcmds->argument);
		} else if (!result && bind->else_cmd) {
			wm_execute_action(server, bind->else_cmd->action,
				bind->else_cmd->argument);
		}
		return true;
	}

	if (bind->action == WM_ACTION_FOREACH) {
		if (!bind->subcmds)
			return true;
		struct wm_view *saved = server->focused_view;
		struct wm_view *v, *tmp;
		wl_list_for_each_safe(v, tmp, &server->views, link) {
			if (evaluate_condition(server, bind->condition, v)) {
				server->focused_view = v;
				wm_execute_action(server,
					bind->subcmds->action,
					bind->subcmds->argument);
			}
		}
		server->focused_view = saved;
		return true;
	}

	if (bind->action == WM_ACTION_MAP) {
		if (!bind->subcmds)
			return true;
		struct wm_view *saved = server->focused_view;
		struct wm_view *v, *tmp;
		wl_list_for_each_safe(v, tmp, &server->views, link) {
			server->focused_view = v;
			wm_execute_action(server,
				bind->subcmds->action,
				bind->subcmds->argument);
		}
		server->focused_view = saved;
		return true;
	}

	if (bind->action == WM_ACTION_DELAY) {
		if (!bind->subcmds)
			return true;
		struct delay_cb_data *cb = calloc(1, sizeof(*cb));
		if (!cb)
			return false;
		cb->server = server;
		cb->action = bind->subcmds->action;
		if (bind->subcmds->argument)
			cb->argument = strdup(bind->subcmds->argument);
		cb->timer = wl_event_loop_add_timer(
			server->wl_event_loop, delay_timer_cb, cb);
		if (!cb->timer) {
			free(cb->argument);
			free(cb);
			return false;
		}
		/* delay_us is in microseconds, timer wants milliseconds */
		int ms = bind->delay_us / 1000;
		if (ms < 1)
			ms = 1;
		wl_event_source_timer_update(cb->timer, ms);
		return true;
	}

	return wm_execute_action(server, bind->action, bind->argument);
}

/* --- Action handler functions --- */

static bool
handle_exec(const char *argument)
{
	wm_spawn_command(argument);
	return true;
}

static bool
handle_exit(struct wm_server *server)
{
	wl_display_terminate(server->wl_display);
	return true;
}

static bool
handle_close(struct wm_view *view)
{
	if (view)
		wlr_xdg_toplevel_send_close(view->xdg_toplevel);
	return true;
}

static bool
handle_kill(struct wm_view *view)
{
	if (view) {
		struct wl_resource *resource =
			view->xdg_toplevel->base->resource;
		struct wl_client *client =
			wl_resource_get_client(resource);
		if (client) {
			pid_t pid;
			wl_client_get_credentials(client,
				&pid, NULL, NULL);
			if (pid > 0)
				kill(pid, SIGKILL);
		}
	}
	return true;
}

static bool
handle_next_window(struct wm_server *server, const char *argument)
{
	if (argument) {
		/* Filtered cycling: only windows matching pattern */
		struct wm_workspace *ws = wm_workspace_get_active(server);
		struct wm_view *focused = server->focused_view;
		struct wm_view *candidate = NULL;
		bool past_focused = (focused == NULL);
		struct wm_view *first_match = NULL;
		struct wm_view *v;
		wl_list_for_each(v, &server->views, link) {
			if (v->workspace != ws && !v->sticky)
				continue;
			bool match = (v->title &&
				fnmatch(argument, v->title, 0) == 0) ||
				(v->app_id &&
				fnmatch(argument, v->app_id, 0) == 0);
			if (!match)
				continue;
			if (!first_match)
				first_match = v;
			if (past_focused) {
				candidate = v;
				break;
			}
			if (v == focused)
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
}

static bool
handle_prev_window(struct wm_server *server, const char *argument)
{
	if (argument) {
		/* Filtered cycling: only windows matching pattern */
		struct wm_workspace *ws = wm_workspace_get_active(server);
		struct wm_view *focused = server->focused_view;
		struct wm_view *candidate = NULL;
		struct wm_view *last_match = NULL;
		struct wm_view *v;
		wl_list_for_each(v, &server->views, link) {
			if (v->workspace != ws && !v->sticky)
				continue;
			bool match = (v->title &&
				fnmatch(argument, v->title, 0) == 0) ||
				(v->app_id &&
				fnmatch(argument, v->app_id, 0) == 0);
			if (!match)
				continue;
			if (v == focused) {
				if (candidate)
					break;
				continue;
			}
			candidate = v;
			last_match = v;
		}
		if (!candidate) {
			/* Wrap: find last matching view */
			wl_list_for_each(v, &server->views, link) {
				if (v->workspace != ws && !v->sticky)
					continue;
				bool m = (v->title &&
					fnmatch(argument, v->title,
						0) == 0) ||
					(v->app_id &&
					fnmatch(argument, v->app_id,
						0) == 0);
				if (m)
					last_match = v;
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
}

static bool
handle_deiconify(struct wm_server *server, const char *argument)
{
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
}

static bool
handle_maximize(struct wm_view *view)
{
	if (view) {
		struct wl_listener *listener = &view->request_maximize;
		listener->notify(listener, NULL);
	}
	return true;
}

static bool
handle_fullscreen(struct wm_view *view)
{
	if (view) {
		struct wl_listener *listener =
			&view->request_fullscreen;
		listener->notify(listener, NULL);
	}
	return true;
}

static bool
handle_minimize(struct wm_view *view)
{
	if (view) {
		struct wl_listener *listener =
			&view->request_minimize;
		listener->notify(listener, NULL);
	}
	return true;
}

static bool
handle_workspace_switch(struct wm_server *server, const char *argument)
{
	if (argument) {
		int ws;
		if (safe_atoi(argument, &ws))
			wm_workspace_switch(server, ws - 1);
	}
	return true;
}

static bool
handle_send_to_workspace(struct wm_server *server, const char *argument)
{
	if (argument) {
		int ws;
		if (safe_atoi(argument, &ws))
			wm_view_send_to_workspace(server, ws - 1);
	}
	return true;
}

static bool
handle_move_directional(struct wm_view *view, const char *argument,
	int dx, int dy)
{
	if (view && argument) {
		int px;
		if (safe_atoi(argument, &px)) {
			view->x += dx * px;
			view->y += dy * px;
			wlr_scene_node_set_position(
				&view->scene_tree->node, view->x, view->y);
		}
	}
	return true;
}

static bool
handle_move_to(struct wm_view *view, const char *argument)
{
	if (view && argument) {
		int x = 0, y = 0;
		sscanf(argument, "%d %d", &x, &y);
		view->x = x;
		view->y = y;
		wlr_scene_node_set_position(
			&view->scene_tree->node, x, y);
	}
	return true;
}

static bool
handle_resize_to(struct wm_view *view, const char *argument)
{
	if (view && argument) {
		int w = 0, h = 0;
		sscanf(argument, "%d %d", &w, &h);
		if (w > 0 && h > 0)
			wlr_xdg_toplevel_set_size(
				view->xdg_toplevel, w, h);
	}
	return true;
}

static bool
handle_show_desktop(struct wm_server *server)
{
	struct wm_workspace *ws = wm_workspace_get_active(server);
	struct wm_view *v;
	bool any_visible = false;

	/* Check if any non-minimized views exist on workspace */
	wl_list_for_each(v, &server->views, link) {
		if (v->workspace == ws &&
		    v->scene_tree->node.enabled) {
			any_visible = true;
			break;
		}
	}

	if (any_visible) {
		/* Minimize all visible views without triggering
		 * focus changes (which reorder the view list and
		 * corrupt the iteration) */
		wl_list_for_each(v, &server->views, link) {
			if (v->workspace == ws &&
			    v->scene_tree->node.enabled) {
				if (v->animation)
					wm_animation_cancel(
						v->animation);
				wm_foreign_toplevel_set_minimized(
					v, true);
				wlr_scene_node_set_enabled(
					&v->scene_tree->node, false);
			}
		}
		if (server->seat) {
			wlr_seat_keyboard_notify_clear_focus(server->seat);
		}
		server->focused_view = NULL;
		if (server->toolbar) {
			wm_toolbar_update_iconbar(server->toolbar);
		}
	} else {
		/* Restore all minimized views on workspace */
		wm_view_deiconify_all_workspace(server);
	}
	return true;
}

static bool
handle_key_mode(struct wm_server *server, const char *argument)
{
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
		wm_chain_reset(server);
		wlr_log(WLR_INFO, "keymode switched to: %s",
			mode_name);
	}
	return true;
}

static bool
handle_set_style(struct wm_server *server, const char *argument)
{
	if (argument && server->config) {
		free(server->config->style_file);
		server->config->style_file = strdup(argument);
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
}

static bool
handle_reload_style(struct wm_server *server)
{
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
}

static bool
handle_set_decor(struct wm_server *server, struct wm_view *view,
	const char *argument)
{
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
}

static bool
handle_take_to_workspace(struct wm_server *server, const char *argument)
{
	if (argument) {
		int ws;
		if (safe_atoi(argument, &ws))
			wm_view_take_to_workspace(server, ws - 1);
	}
	return true;
}

static bool
handle_resize_delta(struct wm_view *view, const char *argument,
	int horiz, int vert)
{
	if (view && argument) {
		int delta;
		if (safe_atoi(argument, &delta))
			wm_view_resize_by(view, horiz * delta,
				vert * delta);
	}
	return true;
}

static bool
handle_set_alpha(struct wm_view *view, const char *argument)
{
	if (view && argument) {
		int alpha;
		if (argument[0] == '+' || argument[0] == '-') {
			/* Relative adjustment */
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
}

static bool
handle_set_env(const char *argument)
{
	if (argument) {
		char *buf = strdup(argument);
		if (buf) {
			char *eq = strchr(buf, '=');
			if (eq) {
				*eq = '\0';
				if (wm_is_blocked_env_var(buf)) {
					wlr_log(WLR_ERROR,
						"SetEnv: blocked security-sensitive "
						"variable: %s", buf);
				} else {
					setenv(buf, eq + 1, 1);
				}
			} else {
				/* "VAR value" format */
				char *sp = strchr(buf, ' ');
				if (sp) {
					*sp = '\0';
					sp++;
					while (*sp == ' ' || *sp == '\t')
						sp++;
					if (wm_is_blocked_env_var(buf)) {
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
}

static bool
handle_bind_key(struct wm_server *server, const char *argument)
{
	if (!argument)
		return true;
	/* Argument is a keybinding line like "Mod4 t :Exec xterm" */
	struct wl_list *bindings = wm_get_active_bindings(server);
	if (!bindings)
		return true;
	if (keybind_add_from_string(bindings, argument))
		wlr_log(WLR_INFO, "BindKey: added binding for: %s", argument);
	else
		wlr_log(WLR_ERROR, "BindKey: failed to parse: %s",
			argument);
	return true;
}

static bool
handle_goto_window(struct wm_server *server, const char *argument)
{
	if (argument) {
		int n;
		if (!safe_atoi(argument, &n) || n < 1)
			return true;
		struct wm_workspace *ws = wm_workspace_get_active(server);
		int count = 0;
		struct wm_view *v;
		wl_list_for_each(v, &server->views, link) {
			if (v->workspace != ws && !v->sticky)
				continue;
			count++;
			if (count == n) {
				wm_focus_view(v,
					v->xdg_toplevel->base->surface);
				break;
			}
		}
	}
	return true;
}

static bool
handle_next_prev_group(struct wm_server *server, struct wm_view *view,
	enum wm_action action)
{
	struct wm_workspace *ws = wm_workspace_get_active(server);
	/* Build array of unique tab groups on current workspace */
	struct wm_tab_group *groups[256];
	int ngroups = 0;
	struct wm_view *v;
	wl_list_for_each(v, &server->views, link) {
		if (v->workspace != ws && !v->sticky)
			continue;
		if (!v->tab_group)
			continue;
		/* Check if already in our list */
		bool found = false;
		for (int i = 0; i < ngroups; i++) {
			if (groups[i] == v->tab_group) {
				found = true;
				break;
			}
		}
		if (!found && ngroups < 256)
			groups[ngroups++] = v->tab_group;
	}
	if (ngroups < 2)
		return true;
	/* Find the current group */
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

static bool
handle_unclutter(struct wm_server *server)
{
	struct wm_workspace *ws = wm_workspace_get_active(server);
	/* Get usable area from first output */
	struct wlr_box area = {0, 0, 800, 600};
	struct wm_output *output;
	wl_list_for_each(output, &server->outputs, link) {
		area = output->usable_area;
		break;
	}
	int offset = 0;
	struct wm_view *v;
	wl_list_for_each(v, &server->views, link) {
		if (v->workspace != ws && !v->sticky)
			continue;
		if (!v->scene_tree->node.enabled)
			continue;
		v->x = area.x + offset;
		v->y = area.y + offset;
		wlr_scene_node_set_position(
			&v->scene_tree->node, v->x, v->y);
		offset += 30;
	}
	return true;
}

/* --- Main action dispatcher --- */

bool
wm_execute_action(struct wm_server *server,
	enum wm_action action, const char *argument)
{
	struct wm_view *view = server->focused_view;

	switch (action) {
	case WM_ACTION_EXEC:
		return handle_exec(argument);
	case WM_ACTION_EXIT:
		return handle_exit(server);
	case WM_ACTION_CLOSE:
		return handle_close(view);
	case WM_ACTION_KILL:
		return handle_kill(view);
	case WM_ACTION_FOCUS_NEXT:
		wm_focus_next_view(server);
		return true;
	case WM_ACTION_FOCUS_PREV:
		wm_focus_prev_view(server);
		return true;
	case WM_ACTION_NEXT_WINDOW:
		return handle_next_window(server, argument);
	case WM_ACTION_PREV_WINDOW:
		return handle_prev_window(server, argument);
	case WM_ACTION_DEICONIFY:
		return handle_deiconify(server, argument);
	case WM_ACTION_MAXIMIZE:
		return handle_maximize(view);
	case WM_ACTION_FULLSCREEN:
		return handle_fullscreen(view);
	case WM_ACTION_MINIMIZE:
		return handle_minimize(view);
	case WM_ACTION_RAISE:
		if (view)
			wm_view_raise(view);
		return true;
	case WM_ACTION_WORKSPACE:
		return handle_workspace_switch(server, argument);
	case WM_ACTION_SEND_TO_WORKSPACE:
		return handle_send_to_workspace(server, argument);
	case WM_ACTION_NEXT_WORKSPACE:
		wm_workspace_switch_next(server);
		return true;
	case WM_ACTION_PREV_WORKSPACE:
		wm_workspace_switch_prev(server);
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
	case WM_ACTION_STICK:
		if (view)
			wm_view_set_sticky(view, !view->sticky);
		return true;
	case WM_ACTION_RECONFIGURE:
		wm_server_reconfigure(server);
		return true;
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
		return handle_move_directional(view, argument, -1, 0);
	case WM_ACTION_MOVE_RIGHT:
		return handle_move_directional(view, argument, 1, 0);
	case WM_ACTION_MOVE_UP:
		return handle_move_directional(view, argument, 0, -1);
	case WM_ACTION_MOVE_DOWN:
		return handle_move_directional(view, argument, 0, 1);
	case WM_ACTION_MOVE_TO:
		return handle_move_to(view, argument);
	case WM_ACTION_RESIZE_TO:
		return handle_resize_to(view, argument);
	case WM_ACTION_SHOW_DESKTOP:
		return handle_show_desktop(server);
	case WM_ACTION_KEY_MODE:
		return handle_key_mode(server, argument);

	/* Tab group actions */
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
	case WM_ACTION_START_TABBING:
		/* Tabbing is initiated by mouse action, not keyboard */
		return true;
	case WM_ACTION_FOCUS:
		/* Focus is implicit in mouse binding dispatch */
		if (view)
			wm_focus_view(view, NULL);
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

	/* Menu actions */
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

	/* Style actions */
	case WM_ACTION_SET_STYLE:
		return handle_set_style(server, argument);
	case WM_ACTION_RELOAD_STYLE:
		return handle_reload_style(server);

	/* Shade actions */
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

	/* Keyboard layout */
	case WM_ACTION_NEXT_LAYOUT:
		wm_keyboard_next_layout(server);
		return true;
	case WM_ACTION_PREV_LAYOUT:
		wm_keyboard_prev_layout(server);
		return true;

	/* View state actions */
	case WM_ACTION_MAXIMIZE_VERT:
		if (view)
			wm_view_maximize_vert(view);
		return true;
	case WM_ACTION_MAXIMIZE_HORIZ:
		if (view)
			wm_view_maximize_horiz(view);
		return true;
	case WM_ACTION_TOGGLE_DECOR:
		if (view)
			wm_view_toggle_decoration(view);
		return true;
	case WM_ACTION_ACTIVATE_TAB:
		if (view && argument) {
			int idx;
			if (safe_atoi(argument, &idx))
				wm_view_activate_tab(view, idx - 1);
		}
		return true;
	case WM_ACTION_LOWER:
		if (view)
			wm_view_lower(view);
		return true;

	/* Layer actions */
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
	case WM_ACTION_SET_DECOR:
		return handle_set_decor(server, view, argument);

	/* Arrange actions */
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

	/* Workspace send/take */
	case WM_ACTION_TAKE_TO_WORKSPACE:
		return handle_take_to_workspace(server, argument);
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

	/* Half-screen tiling */
	case WM_ACTION_LHALF:
		if (view)
			wm_view_lhalf(view);
		return true;
	case WM_ACTION_RHALF:
		if (view)
			wm_view_rhalf(view);
		return true;

	/* Resize by delta */
	case WM_ACTION_RESIZE_HORIZ:
		return handle_resize_delta(view, argument, 1, 0);
	case WM_ACTION_RESIZE_VERT:
		return handle_resize_delta(view, argument, 0, 1);

	/* Focus direction */
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

	/* Head/output actions */
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

	/* Misc view actions */
	case WM_ACTION_CLOSE_ALL_WINDOWS:
		wm_view_close_all(server);
		return true;
	case WM_ACTION_REMEMBER:
		if (view && server->config && server->config->apps_file)
			wm_rules_remember_window(view,
				server->config->apps_file);
		return true;

	/* Slit/toolbar toggles */
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

	case WM_ACTION_SET_ALPHA:
		return handle_set_alpha(view, argument);
	case WM_ACTION_SET_ENV:
		return handle_set_env(argument);
	case WM_ACTION_BIND_KEY:
		return handle_bind_key(server, argument);
	case WM_ACTION_GOTO_WINDOW:
		return handle_goto_window(server, argument);
	case WM_ACTION_NEXT_GROUP:
	case WM_ACTION_PREV_GROUP:
		return handle_next_prev_group(server, view, action);
	case WM_ACTION_UNCLUTTER:
		return handle_unclutter(server);

	case WM_ACTION_NOP:
		return true;

	case WM_ACTION_TOGGLE_SHOW_POSITION:
		server->show_position = !server->show_position;
		wlr_log(WLR_INFO, "show window position: %s",
			server->show_position ? "on" : "off");
		return true;

	/*
	 * MacroCmd/ToggleCmd are dispatched by wm_execute_keybind_action()
	 * which has access to the keybind's subcmd list and toggle state.
	 * If reached here via a direct wm_execute_action() call (e.g. from a
	 * subcmd), these are invalid — log and ignore.
	 */
	case WM_ACTION_MACRO_CMD:
	case WM_ACTION_TOGGLE_CMD:
		wlr_log(WLR_ERROR,
			"%s", "MacroCmd/ToggleCmd cannot be nested in subcmds");
		return false;

	/* Focus navigation */
	case WM_ACTION_FOCUS_TOOLBAR:
		wm_focus_nav_enter_toolbar(server);
		return true;
	case WM_ACTION_FOCUS_NEXT_ELEMENT:
		wm_focus_nav_next_element(server);
		return true;
	case WM_ACTION_FOCUS_PREV_ELEMENT:
		wm_focus_nav_prev_element(server);
		return true;
	case WM_ACTION_FOCUS_ACTIVATE:
		wm_focus_nav_activate(server);
		return true;

	case WM_ACTION_IF:
	case WM_ACTION_FOREACH:
	case WM_ACTION_MAP:
	case WM_ACTION_DELAY:
		wlr_log(WLR_ERROR,
			"%s", "If/ForEach/Map/Delay require keybind context");
		return false;
	}

	return false;
}
