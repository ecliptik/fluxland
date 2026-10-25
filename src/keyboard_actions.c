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
#include <signal.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <stdio.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/edges.h>
#include <wlr/util/log.h>

#include "animation.h"
#include "client_pattern.h"
#include "command.h"
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
#include "view_geometry.h"
#include "wallpaper.h"
#include "workspace.h"

/* Security helpers are in util.h: wm_is_blocked_env_var(), wm_spawn_command() */

/* --- Key binding execution --- */

bool
wm_execute_keybind_action(struct wm_server *server,
	struct wm_keybind *bind)
{
	if (bind->command)
		return wm_command_execute(server, bind->command, NULL);
	/* Bindings built in code may carry just an action + argument */
	return wm_execute_action(server, bind->action, bind->argument);
}

/*
 * Check a keybind's guard condition against the focused view.
 * Returns true if the binding has no guard or the guard holds.
 */
bool
wm_keybind_check_condition(struct wm_server *server,
	struct wm_keybind *bind)
{
	if (!bind->guard)
		return true;
	return wm_condition_evaluate(server, bind->guard,
		server->focused_view);
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
		wm_view_close(view);
	return true;
}

static bool
handle_kill(struct wm_view *view)
{
	if (view)
		wm_view_kill(view);
	return true;
}

static bool
handle_cycle_window(struct wm_server *server, const char *argument,
	bool reverse, bool groups)
{
	struct wm_window_list_spec spec;
	if (!wm_window_list_parse(argument, &spec)) {
		wlr_log(WLR_ERROR, "bad window pattern: %s", argument);
		return false;
	}
	if (groups)
		spec.groups = true;
	wm_focus_cycle(server, &spec, reverse);
	wm_window_list_finish(&spec);
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
	if (view)
		wm_view_toggle_maximize(view);
	return true;
}

static bool
handle_fullscreen(struct wm_view *view)
{
	if (view)
		wm_view_toggle_fullscreen(view);
	return true;
}

static bool
handle_minimize(struct wm_view *view)
{
	if (view)
		wm_view_minimize(view);
	return true;
}

/*
 * The leading number of an argument, or dflt: Fluxbox reads "N" of
 * "Workspace N" with an istream, so "2 more" is 2 and "" is the default.
 */
static int
arg_int(const char *argument, int dflt)
{
	if (!argument)
		return dflt;
	char *end;
	long v = strtol(argument, &end, 10);
	if (end == argument || v < -100000 || v > 100000)
		return dflt;
	return (int)v;
}

/* A workspace number counting from 1, negative from the end, clamped */
static int
workspace_number(struct wm_server *server, int num)
{
	int total = server->workspace_count;
	if (num < 0)
		num += total + 1;
	if (num < 1)
		num = 1;
	if (num > total)
		num = total;
	return num - 1;
}

static bool
handle_workspace_switch(struct wm_server *server, const char *argument)
{
	wm_workspace_switch(server,
		workspace_number(server, arg_int(argument, 1)));
	return true;
}

static bool
handle_send_to_workspace(struct wm_server *server, struct wm_view *view,
	const char *argument)
{
	wm_view_send_to_workspace(server, view,
		workspace_number(server, arg_int(argument, 1)));
	return true;
}

/* NextWorkspace/PrevWorkspace n (wrapping; 0 = the one before) */
static bool
handle_workspace_offset(struct wm_server *server, const char *argument,
	int sign)
{
	int delta = arg_int(argument, 1);
	int total = server->workspace_count;
	if (total <= 0)
		return true;
	if (delta == 0) {
		if (server->former_workspace >= 0 &&
		    server->former_workspace < total)
			wm_workspace_switch(server, server->former_workspace);
		return true;
	}
	struct wm_workspace *active = wm_workspace_get_active(server);
	int cur = active ? active->index : 0;
	int target = ((cur + sign * delta) % total + total) % total;
	wm_workspace_switch(server, target);
	return true;
}

/* RightWorkspace/LeftWorkspace n (no wrapping) */
static bool
handle_workspace_step(struct wm_server *server, const char *argument,
	int sign)
{
	struct wm_workspace *active = wm_workspace_get_active(server);
	int target = (active ? active->index : 0) +
		sign * arg_int(argument, 1);
	if (target >= 0 && target < server->workspace_count)
		wm_workspace_switch(server, target);
	return true;
}

/* Send/TakeToNext/PrevWorkspace n: from the window's workspace */
static bool
handle_send_offset(struct wm_server *server, struct wm_view *view,
	const char *argument, int sign, bool take)
{
	int total = server->workspace_count;
	if (!view || total <= 0)
		return true;
	struct wm_workspace *active = wm_workspace_get_active(server);
	int cur = view->workspace ? view->workspace->index :
		active ? active->index : 0;
	int delta = sign * arg_int(argument, 1);
	int target = ((cur + delta % total) % total + total) % total;
	if (take)
		wm_view_take_to_workspace(server, view, target);
	else
		wm_view_send_to_workspace(server, view, target);
	return true;
}

/* Tab N: the Nth tab, from the end when negative */
static bool
handle_tab(struct wm_view *view, const char *argument)
{
	if (!view || !view->tab_group)
		return true;
	int count = view->tab_group->count;
	int num = arg_int(argument, 1);
	if (num < 0)
		num += count + 1;
	if (num < 1)
		num = 1;
	if (num > count)
		num = count;
	wm_view_activate_tab(view, num - 1);
	return true;
}

/*
 * Moving or resizing a maximized or fullscreen window un-maximizes it
 * first (disableMaximizationIfNeeded), unless maxDisableMove or
 * maxDisableResize forbid changing it at all. Returns false then.
 */
static bool
leave_maximized(struct wm_view *view, bool resizing)
{
	bool full = view->maximized || view->fullscreen;
	if (full && view->server->config &&
	    (resizing ? view->server->config->max_disable_resize :
	     view->server->config->max_disable_move))
		return false;
	if (view->fullscreen)
		wm_view_toggle_fullscreen(view);
	if (view->maximized)
		wm_view_toggle_maximize(view);
	if (view->maximized_horiz)
		wm_view_maximize_horiz(view);
	if (view->maximized_vert)
		wm_view_maximize_vert(view);
	return true;
}

/* The usable area of the window's head, for % and corners */
static bool
view_head_area(struct wm_view *view, struct wlr_box *area)
{
	struct wm_output *out = wm_output_for_view(view);
	if (!out)
		return false;
	*area = out->usable_area;
	if (wlr_box_empty(area))
		wm_output_layout_box(out, area);
	return !wlr_box_empty(area);
}

/* Up to three words of an argument */
static int
split_args(const char *argument, char *buf, size_t len, char *words[3])
{
	int n = 0;
	words[0] = words[1] = words[2] = NULL;
	if (!argument)
		return 0;
	snprintf(buf, len, "%s", argument);
	char *save = NULL;
	for (char *w = strtok_r(buf, " \t", &save); w && n < 3;
	     w = strtok_r(NULL, " \t", &save))
		words[n++] = w;
	return n;
}

/* MoveRight/Left/Up/Down d, and Move dx dy */
static bool
handle_move_by(struct wm_view *view, int dx, int dy)
{
	if (!view || !leave_maximized(view, false))
		return true;
	wm_view_move(view, view->x + dx, view->y + dy);
	return true;
}

static bool
handle_move_directional(struct wm_view *view, const char *argument,
	int dx, int dy)
{
	int d = arg_int(argument, 0);
	return handle_move_by(view, dx * d, dy * d);
}

static bool
handle_move(struct wm_view *view, const char *argument)
{
	if (!view)
		return true;
	char buf[128], *w[3];
	int n = split_args(argument, buf, sizeof(buf), w);
	if (n == 0) {
		/* fluxland: no deltas, a mouse move as StartMoving */
		wm_view_begin_interactive(view, WM_CURSOR_MOVE, 0);
		return true;
	}
	return handle_move_by(view, arg_int(w[0], 0),
		n > 1 ? arg_int(w[1], 0) : 0);
}

/* MoveTo x y [corner]: pixels or %, "*" keeps a coordinate */
static bool
handle_move_to(struct wm_view *view, const char *argument)
{
	char buf[128], *w[3];
	struct wm_size_token x, y;
	struct wlr_box area;
	if (!view || split_args(argument, buf, sizeof(buf), w) < 2 ||
	    !wm_size_token_parse(w[0], &x) || !wm_size_token_parse(w[1], &y) ||
	    !view_head_area(view, &area))
		return true;
	enum wm_corner corner = WM_CORNER_TOPLEFT;
	if (w[2] && !wm_corner_from_name(w[2], &corner))
		corner = WM_CORNER_TOPLEFT;
	if (!leave_maximized(view, false))
		return true;
	struct wlr_box frame;
	wm_view_frame_box(view, &frame);
	int nx = x.ignore ? frame.x : wm_corner_x(corner, &area, frame.width,
		wm_size_token_pixels(&x, area.width));
	int ny = y.ignore ? frame.y : wm_corner_y(corner, &area, frame.height,
		wm_size_token_pixels(&y, area.height));
	wm_view_move(view, nx, ny);
	return true;
}

/* ResizeTo w h: the window's size, pixels or % of the head; 0 keeps */
static bool
handle_resize_to(struct wm_view *view, const char *argument)
{
	char buf[128], *w[3];
	struct wm_size_token tw, th;
	struct wlr_box area;
	if (!view || split_args(argument, buf, sizeof(buf), w) < 2 ||
	    !wm_size_token_parse(w[0], &tw) || !wm_size_token_parse(w[1], &th) ||
	    !view_head_area(view, &area) || !leave_maximized(view, true))
		return true;
	struct wlr_box frame;
	wm_view_frame_box(view, &frame);
	int fw = wm_size_token_pixels(&tw, area.width);
	int fh = wm_size_token_pixels(&th, area.height);
	if (fw > 0)
		frame.width = fw;
	if (fh > 0)
		frame.height = fh;
	wm_view_set_frame_box(view, &frame);
	return true;
}

/*
 * Resize dw dh, ResizeHorizontal dw, ResizeVertical dh: pixels, or % of
 * the window's own size; never smaller than Fluxbox allows.
 */
static bool
handle_resize_by(struct wm_view *view, const char *argument, bool horiz,
	bool vert)
{
	char buf[128], *w[3];
	int n = split_args(argument, buf, sizeof(buf), w);
	if (!view)
		return true;
	if (n == 0 && horiz && vert) {
		/* fluxland: no deltas, a mouse resize from the corner */
		wm_view_begin_interactive(view, WM_CURSOR_RESIZE,
			WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT);
		return true;
	}
	struct wm_size_token dw = {0}, dh = {0};
	if (horiz && vert) {
		if (n < 2 || !wm_size_token_parse(w[0], &dw) ||
		    !wm_size_token_parse(w[1], &dh))
			return true;
	} else if (n < 1 || !wm_size_token_parse(w[0], horiz ? &dw : &dh)) {
		return true;
	}
	if (!leave_maximized(view, true))
		return true;
	struct wlr_box frame;
	wm_view_frame_box(view, &frame);
	int title = view->decoration ? view->decoration->titlebar_height : 0;
	int nw = frame.width + wm_size_token_pixels(&dw, frame.width);
	int nh = frame.height + wm_size_token_pixels(&dh, frame.height);
	if (nw < title * 2 + 10)
		nw = title * 2 + 10;
	if (nh < title + 10)
		nh = title + 10;
	frame.width = nw;
	frame.height = nh;
	wm_view_set_frame_box(view, &frame);
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
		/* Save focused view for restore on toggle-off */
		server->show_desktop_saved_view = server->focused_view;

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

		/* Restore focus to previously focused view if still valid */
		if (server->show_desktop_saved_view) {
			struct wm_view *saved =
				server->show_desktop_saved_view;
			server->show_desktop_saved_view = NULL;
			wl_list_for_each(v, &server->views, link) {
				if (v == saved) {
					wm_focus_view(saved,
						wm_view_surface(saved));
					break;
				}
			}
		}
	}
	return true;
}

/*
 * KeyMode mode [end keys] (FbCommands::KeyModeCmd): the mode gets a
 * binding of its end keys ("None Escape" unless given) back to the
 * default mode; a mode the keys file doesn't have is the default one.
 */
static bool
handle_key_mode(struct wm_server *server, const char *argument)
{
	if (!argument)
		return true;
	char mode_name[256];
	const char *sp = argument;
	while (*sp && !isspace((unsigned char)*sp))
		sp++;
	size_t len = sp - argument;
	if (len >= sizeof(mode_name))
		len = sizeof(mode_name) - 1;
	memcpy(mode_name, argument, len);
	mode_name[len] = '\0';
	while (isspace((unsigned char)*sp))
		sp++;

	struct wm_keymode *mode = NULL, *m;
	wl_list_for_each(m, &server->keymodes, link) {
		if (strcasecmp(m->name, mode_name) == 0)
			mode = m;
	}
	if (!mode || strcasecmp(mode_name, "default") == 0) {
		snprintf(mode_name, sizeof(mode_name), "default");
	} else if (!mode->end_bound) {
		char line[512];
		snprintf(line, sizeof(line), "%s :KeyMode default",
			*sp ? sp : "None Escape");
		keybind_add_from_string(&mode->bindings, line);
		mode->end_bound = true;
	}

	free(server->current_keymode);
	server->current_keymode = strdup(mode_name);
	wm_chain_reset(server);
	wlr_log(WLR_INFO, "keymode switched to: %s", mode_name);
	return true;
}

static bool
handle_set_style(struct wm_server *server, const char *argument)
{
	if (argument && server->config) {
		/*
		 * Only regular files: reading a device or FIFO would hang
		 * the compositor. (Any source may set a style: key and menu
		 * styles live wherever [stylesdir] points.)
		 */
		char expanded[4096];
		const char *home = getenv("HOME");
		if (argument[0] == '~' && home)
			snprintf(expanded, sizeof(expanded), "%s%s", home,
				argument + 1);
		else
			snprintf(expanded, sizeof(expanded), "%s", argument);
		/* A style directory (theme.cfg or style.cfg inside) too */
		char *resolved = realpath(expanded, NULL);
		struct stat st;
		bool ok = resolved && stat(resolved, &st) == 0 &&
			S_ISREG(st.st_mode);
		if (resolved && !ok && S_ISDIR(st.st_mode)) {
			static const char *const cfg[] = {
				"theme.cfg", "style.cfg" };
			for (int i = 0; i < 2 && !ok; i++) {
				char file[4096];
				struct stat fst;
				snprintf(file, sizeof(file), "%s/%s", resolved,
					cfg[i]);
				ok = stat(file, &fst) == 0 &&
					S_ISREG(fst.st_mode);
			}
		}
		if (!ok) {
			wlr_log(WLR_ERROR, "SetStyle: not a style file: %s",
				argument);
			free(resolved);
			return false;
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
}

/*
 * SetWallpaper [-f|-c|-t|-a] file (fluxland; fbsetbg's options: full,
 * center, tile, aspect): the wallpaper of every workspace without its
 * own, remembered in the config directory's lastwallpaper as fbsetbg
 * remembers its last one.
 */
static bool
handle_set_wallpaper(struct wm_server *server, const char *argument)
{
	struct wm_config *config = server->config;
	if (!argument || !config)
		return true;
	const char *p = argument;
	enum wm_wallpaper_mode mode = config->wallpaper_mode;
	if (p[0] == '-' && p[1] && isspace((unsigned char)p[2])) {
		switch (p[1]) {
		case 'f': mode = WM_WALLPAPER_STRETCH; break;
		case 'c': mode = WM_WALLPAPER_CENTER; break;
		case 't': mode = WM_WALLPAPER_TILE; break;
		case 'a': mode = WM_WALLPAPER_FILL; break;
		default: break;
		}
		p += 2;
		while (isspace((unsigned char)*p))
			p++;
	}
	char path[4096];
	const char *home = getenv("HOME");
	if (p[0] == '~' && home)
		snprintf(path, sizeof(path), "%s%s", home, p + 1);
	else
		snprintf(path, sizeof(path), "%s", p);
	struct stat st;
	if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
		wlr_log(WLR_ERROR, "SetWallpaper: not a file: %s", path);
		return false;
	}
	free(config->wallpaper);
	config->wallpaper = strdup(path);
	config->wallpaper_mode = mode;
	if (server->wallpaper)
		wm_wallpaper_reload(server->wallpaper);
	else
		server->wallpaper = wm_wallpaper_create(server);

	if (config->config_dir) {
		static const char *const names[] = {
			[WM_WALLPAPER_STRETCH] = "Stretch",
			[WM_WALLPAPER_CENTER] = "Center",
			[WM_WALLPAPER_TILE] = "Tile",
			[WM_WALLPAPER_FILL] = "Fill",
		};
		char last[4096];
		snprintf(last, sizeof(last), "%s/lastwallpaper",
			config->config_dir);
		FILE *f = fopen_config(last, "w");
		if (f) {
			fprintf(f, "%s\n%s\n", path, names[mode]);
			fclose(f);
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
		enum wm_decor_preset preset;
		if (!wm_decor_parse(argument, &preset))
			preset = WM_DECOR_NORMAL;
		wm_decoration_set_preset(view->decoration,
			preset, server->style);
	}
	return true;
}

static bool
handle_take_to_workspace(struct wm_server *server, struct wm_view *view,
	const char *argument)
{
	wm_view_take_to_workspace(server, view,
		workspace_number(server, arg_int(argument, 1)));
	return true;
}


/* One SetAlpha value: absolute, or relative with + or - */
static int
alpha_value(const char *word, int current)
{
	int v;
	if (!safe_atoi(word, &v))
		return current;
	if (word[0] == '+' || word[0] == '-')
		v += current;
	return v < 0 ? 0 : v > 255 ? 255 : v;
}

/*
 * SetAlpha [a [u]] (CurrentWindowCmd SetAlphaCmd): no value goes back to
 * the init file's alphas, one sets both, two the focused and unfocused.
 */
static bool
handle_set_alpha(struct wm_view *view, const char *argument)
{
	if (!view)
		return true;
	char buf[64], *w[3];
	int n = split_args(argument, buf, sizeof(buf), w);
	struct wm_config *config = view->server->config;
	if (n == 0) {
		view->focus_alpha = config ? config->window_focus_alpha : 255;
		view->unfocus_alpha = config ?
			config->window_unfocus_alpha : 255;
	} else if (n == 1) {
		view->focus_alpha = alpha_value(w[0], view->focus_alpha);
		view->unfocus_alpha = alpha_value(w[0], view->unfocus_alpha);
	} else {
		view->focus_alpha = alpha_value(w[0], view->focus_alpha);
		view->unfocus_alpha = alpha_value(w[1], view->unfocus_alpha);
	}
	wm_view_set_opacity(view, view == view->server->focused_view ?
		view->focus_alpha : view->unfocus_alpha);
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

/* GoToWindow N [{options}] [pattern] */
static bool
handle_goto_window(struct wm_server *server, const char *argument)
{
	int num = arg_int(argument, 0);
	const char *rest = argument ? strpbrk(argument, "({") : NULL;
	struct wm_window_list_spec spec;
	if (!wm_window_list_parse(rest, &spec)) {
		wlr_log(WLR_ERROR, "bad window pattern: %s", rest);
		return false;
	}
	wm_focus_goto_window(server, &spec, num);
	wm_window_list_finish(&spec);
	return true;
}

/* Focus / Activate [pattern]: with a pattern, GoToWindow 1 (pattern) */
static bool
handle_focus(struct wm_server *server, struct wm_view *view,
	const char *argument)
{
	if (argument && *argument) {
		char buf[1024];
		snprintf(buf, sizeof(buf), "1 %s", argument);
		return handle_goto_window(server, buf);
	}
	if (view)
		wm_focus_view(view, NULL);
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

/*
 * Edges to resize from for StartResizing, following Fluxbox's
 * getResizeDirection(): an explicit corner/edge, NearestCorner,
 * NearestEdge or NearestCornerOrEdge [px [pct]] / [pct%]. With no
 * argument Fluxbox resizes from the bottom-right corner.
 */
static bool
near_corner(int xy, int wh, int px, int pc)
{
	return xy < px || 100 * xy < pc * wh;
}

static uint32_t
resize_edges_for(struct wm_server *server, struct wm_view *view,
	const char *argument)
{
	char mode[32] = "bottomright";
	int px = 0, pc = 0;
	if (argument) {
		char extra1[16] = "", extra2[16] = "";
		int n = sscanf(argument, "%31s %15s %15s", mode, extra1, extra2);
		if (strcasecmp(mode, "NearestCorner") == 0) {
			pc = 100;
		} else if (strcasecmp(mode, "NearestCornerOrEdge") == 0) {
			if (n >= 2) {
				size_t l = strlen(extra1);
				if (l > 0 && extra1[l - 1] == '%') {
					extra1[l - 1] = '\0';
					safe_atoi(extra1, &pc);
				} else {
					safe_atoi(extra1, &px);
					if (n >= 3)
						safe_atoi(extra2, &pc);
				}
			} else {
				px = 50;
				pc = 30;
			}
		}
	}

	static const struct {
		const char *name;
		uint32_t edges;
	} fixed[] = {
		{ "TopLeft",     WLR_EDGE_TOP | WLR_EDGE_LEFT },
		{ "Top",         WLR_EDGE_TOP },
		{ "TopRight",    WLR_EDGE_TOP | WLR_EDGE_RIGHT },
		{ "Left",        WLR_EDGE_LEFT },
		{ "Right",       WLR_EDGE_RIGHT },
		{ "BottomLeft",  WLR_EDGE_BOTTOM | WLR_EDGE_LEFT },
		{ "Bottom",      WLR_EDGE_BOTTOM },
		{ "BottomRight", WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT },
	};
	for (size_t i = 0; i < sizeof(fixed) / sizeof(fixed[0]); i++) {
		if (strcasecmp(mode, fixed[i].name) == 0)
			return fixed[i].edges;
	}

	bool center = strcasecmp(mode, "Center") == 0;
	if (!center && strcasecmp(mode, "NearestCorner") != 0 &&
	    strcasecmp(mode, "NearestEdge") != 0 &&
	    strcasecmp(mode, "NearestCornerOrEdge") != 0)
		return WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT;
	if (center)
		pc = 100; /* not supported yet: nearest corner instead */

	struct wlr_box geo;
	wm_view_get_geometry(view, &geo);
	int w = geo.width > 0 ? geo.width : 1;
	int h = geo.height > 0 ? geo.height : 1;
	int x = (int)server->cursor->x - view->x;
	int y = (int)server->cursor->y - view->y;
	int cx = w / 2, cy = h / 2;

	if (x < cx && near_corner(x, cx, px, pc)) {
		if (y < cy && near_corner(y, cy, px, pc))
			return WLR_EDGE_TOP | WLR_EDGE_LEFT;
		if (near_corner(h - y - 1, h - cy, px, pc))
			return WLR_EDGE_BOTTOM | WLR_EDGE_LEFT;
	} else if (near_corner(w - x - 1, w - cx, px, pc)) {
		if (y < cy && near_corner(y, cy, px, pc))
			return WLR_EDGE_TOP | WLR_EDGE_RIGHT;
		if (near_corner(h - y - 1, h - cy, px, pc))
			return WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT;
	}
	/* Not a corner: the nearest edge */
	if (cy - abs(y - cy) < cx - abs(x - cx))
		return y > cy ? WLR_EDGE_BOTTOM : WLR_EDGE_TOP;
	return x > cx ? WLR_EDGE_RIGHT : WLR_EDGE_LEFT;
}

/* Activate the tab under the cursor (mouse bindings without a number) */
static void
activate_tab_under_cursor(struct wm_server *server, struct wm_view *view)
{
	if (!view->tab_group || view->tab_group->count <= 1 ||
	    !view->decoration)
		return;
	double dx = server->cursor->x - view->x;
	double dy = server->cursor->y - view->y;
	int tab_idx = wm_decoration_tab_at(view->decoration, dx, dy);
	if (tab_idx < 0)
		return;
	int i = 0;
	struct wm_view *tab_view;
	wl_list_for_each(tab_view, &view->tab_group->views, tab_link) {
		if (i++ == tab_idx) {
			wm_tab_group_activate(view->tab_group, tab_view);
			return;
		}
	}
}

/* --- Action dispatch --- */

bool
wm_execute_action(struct wm_server *server,
	enum wm_action action, const char *argument)
{
	return wm_execute_action_on(server, action, argument, NULL);
}

bool
wm_execute_action_on(struct wm_server *server, enum wm_action action,
	const char *argument, struct wm_view *target)
{
	struct wm_view *view = target ? target : server->focused_view;

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
		return handle_cycle_window(server, argument, false, false);
	case WM_ACTION_PREV_WINDOW:
		return handle_cycle_window(server, argument, true, false);
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
		return handle_send_to_workspace(server, view, argument);
	case WM_ACTION_NEXT_WORKSPACE:
		return handle_workspace_offset(server, argument, 1);
	case WM_ACTION_PREV_WORKSPACE:
		return handle_workspace_offset(server, argument, -1);
	case WM_ACTION_RIGHT_WORKSPACE:
		return handle_workspace_step(server, argument, 1);
	case WM_ACTION_LEFT_WORKSPACE:
		return handle_workspace_step(server, argument, -1);
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
		return handle_move(view, argument);
	case WM_ACTION_RESIZE:
		return handle_resize_by(view, argument, true, true);
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
		if (view)
			wm_view_begin_interactive(view, WM_CURSOR_TABBING, 0);
		return true;
	case WM_ACTION_FOCUS:
		return handle_focus(server, view, argument);
	case WM_ACTION_START_MOVING:
		if (view)
			wm_view_begin_interactive(view, WM_CURSOR_MOVE, 0);
		return true;
	case WM_ACTION_START_RESIZING:
		if (view)
			wm_view_begin_interactive(view, WM_CURSOR_RESIZE,
				resize_edges_for(server, view, argument));
		return true;

	/* Menu actions */
	case WM_ACTION_ROOT_MENU:
		wm_menu_show_root(server,
			(int)server->cursor->x, (int)server->cursor->y);
		return true;
	case WM_ACTION_WINDOW_MENU:
		server->menu_view = view;
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
	case WM_ACTION_SET_WALLPAPER:
		return handle_set_wallpaper(server, argument);
	case WM_ACTION_TAB:
		return handle_tab(view, argument);
	case WM_ACTION_ACTIVATE_TAB:
		if (view && argument) {
			int idx;
			if (safe_atoi(argument, &idx))
				wm_view_activate_tab(view, idx - 1);
		} else if (view) {
			activate_tab_under_cursor(server, view);
		}
		return true;
	case WM_ACTION_LOWER:
		if (view)
			wm_view_lower(view);
		return true;

	/* Layer actions */
	case WM_ACTION_RAISE_LAYER:
	case WM_ACTION_LOWER_LAYER:
		/* By n Fluxbox layers (2 by default): steps of fluxland's */
		if (view) {
			int steps = (arg_int(argument, 2) + 1) / 2;
			for (int i = 0; i < (steps > 0 ? steps : 1); i++) {
				if (action == WM_ACTION_RAISE_LAYER)
					wm_view_raise_layer(view);
				else
					wm_view_lower_layer(view);
			}
		}
		return true;
	case WM_ACTION_SET_LAYER:
		if (view && argument) {
			enum wm_view_layer layer;
			if (wm_layer_parse(argument, &layer))
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
		return handle_take_to_workspace(server, view, argument);
	case WM_ACTION_SEND_TO_NEXT_WORKSPACE:
		return handle_send_offset(server, view, argument, 1, false);
	case WM_ACTION_SEND_TO_PREV_WORKSPACE:
		return handle_send_offset(server, view, argument, -1, false);
	case WM_ACTION_TAKE_TO_NEXT_WORKSPACE:
		return handle_send_offset(server, view, argument, 1, true);
	case WM_ACTION_TAKE_TO_PREV_WORKSPACE:
		return handle_send_offset(server, view, argument, -1, true);
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
		return handle_resize_by(view, argument, true, false);
	case WM_ACTION_RESIZE_VERT:
		return handle_resize_by(view, argument, false, true);

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
	case WM_ACTION_SEND_TO_PREV_HEAD: {
		int offset = 1;
		if (argument && *argument && !safe_atoi(argument, &offset))
			offset = 1;
		if (action == WM_ACTION_SEND_TO_PREV_HEAD)
			offset = -offset;
		wm_view_send_to_head_offset(view, offset);
		return true;
	}

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
		return handle_cycle_window(server, argument, false, true);
	case WM_ACTION_PREV_GROUP:
		return handle_cycle_window(server, argument, true, true);
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
	case WM_ACTION_IF:
	case WM_ACTION_FOREACH:
	case WM_ACTION_MAP:
	case WM_ACTION_DELAY: {
		/* Compound commands given as action + argument text */
		struct wm_command *cmd = wm_command_create(action, argument);
		bool ok = cmd && wm_command_execute(server, cmd, target);
		wm_command_destroy(cmd);
		return ok;
	}

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
	case WM_ACTION_FOCUS_SLIT:
		wm_focus_nav_enter_slit(server);
		return true;

	}

	return false;
}
