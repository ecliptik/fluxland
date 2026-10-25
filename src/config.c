/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * config.c - Configuration loading and management
 */

#define _POSIX_C_SOURCE 200809L

#include "config.h"
#include "mousebind.h"
#include "i18n.h"
#include "rcparser.h"
#include "util.h"
#include <linux/input-event-codes.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

static void
free_workspace_names(struct wm_config *config)
{
	if (config->workspace_names) {
		for (int i = 0; i < config->workspace_name_count; i++)
			free(config->workspace_names[i]);
		free(config->workspace_names);
		config->workspace_names = NULL;
		config->workspace_name_count = 0;
	}
}

static void
set_default_workspace_names(struct wm_config *config)
{
	free_workspace_names(config);
	config->workspace_name_count = config->workspace_count;
	config->workspace_names =
		calloc(config->workspace_count, sizeof(char *));
	if (!config->workspace_names) {
		config->workspace_name_count = 0;
		return;
	}
	for (int i = 0; i < config->workspace_count; i++) {
		char buf[32];
		snprintf(buf, sizeof(buf), _("Workspace %d"), i + 1);
		config->workspace_names[i] = strdup(buf);
		if (!config->workspace_names[i]) {
			config->workspace_name_count = i;
			return;
		}
	}
}

static void
parse_workspace_names(struct wm_config *config, const char *value)
{
	free_workspace_names(config);

	/* Count commas to determine number of names */
	int count = 1;
	for (const char *p = value; *p; p++) {
		if (*p == ',')
			count++;
	}

	config->workspace_names = calloc(count, sizeof(char *));
	if (!config->workspace_names)
		return;

	char *copy = strdup(value);
	if (!copy) {
		free(config->workspace_names);
		config->workspace_names = NULL;
		return;
	}

	int i = 0;
	char *tok = strtok(copy, ",");
	while (tok && i < count) {
		/* Trim leading/trailing whitespace */
		while (*tok == ' ' || *tok == '\t')
			tok++;
		char *end = tok + strlen(tok) - 1;
		while (end > tok && (*end == ' ' || *end == '\t'))
			*end-- = '\0';
		config->workspace_names[i] = strdup(tok);
		if (!config->workspace_names[i]) {
			config->workspace_name_count = i;
			free(copy);
			return;
		}
		i++;
		tok = strtok(NULL, ",");
	}
	config->workspace_name_count = i;

	free(copy);
}

static enum wm_focus_policy
parse_focus_model(const char *value)
{
	if (!value)
		return WM_FOCUS_CLICK;
	if (strcasecmp(value, "MouseFocus") == 0)
		return WM_FOCUS_SLOPPY;
	if (strcasecmp(value, "StrictMouseFocus") == 0)
		return WM_FOCUS_STRICT_MOUSE;
	return WM_FOCUS_CLICK;
}

static enum wm_toolbar_placement
parse_toolbar_placement(const char *value)
{
	if (!value)
		return WM_TOOLBAR_BOTTOM_CENTER;
	if (strcasecmp(value, "TopLeft") == 0)
		return WM_TOOLBAR_TOP_LEFT;
	if (strcasecmp(value, "TopCenter") == 0)
		return WM_TOOLBAR_TOP_CENTER;
	if (strcasecmp(value, "TopRight") == 0)
		return WM_TOOLBAR_TOP_RIGHT;
	if (strcasecmp(value, "BottomLeft") == 0)
		return WM_TOOLBAR_BOTTOM_LEFT;
	if (strcasecmp(value, "BottomRight") == 0)
		return WM_TOOLBAR_BOTTOM_RIGHT;
	return WM_TOOLBAR_BOTTOM_CENTER;
}

static enum wm_placement_policy
parse_placement(const char *value)
{
	if (!value)
		return WM_PLACEMENT_ROW_SMART;
	if (strcasecmp(value, "ColSmartPlacement") == 0)
		return WM_PLACEMENT_COL_SMART;
	if (strcasecmp(value, "CascadePlacement") == 0)
		return WM_PLACEMENT_CASCADE;
	if (strcasecmp(value, "UnderMousePlacement") == 0)
		return WM_PLACEMENT_UNDER_MOUSE;
	if (strcasecmp(value, "RowMinOverlapPlacement") == 0)
		return WM_PLACEMENT_ROW_MIN_OVERLAP;
	if (strcasecmp(value, "ColMinOverlapPlacement") == 0)
		return WM_PLACEMENT_COL_MIN_OVERLAP;
	return WM_PLACEMENT_ROW_SMART;
}

/*
 * iconbar.mode: Fluxbox's window list ("{static groups} (workspace)"),
 * or a Fluxbox 1.0 name, as fluxbox-update_configs turns them into
 * lists.
 */
static char *
iconbar_mode_spec(const char *value)
{
	static const struct { const char *name, *spec; } names[] = {
		{ "Workspace",        "{static groups} (workspace)" },
		{ "AllWindows",       "{static groups}" },
		{ "Icons",            "{static groups} (minimized=yes)" },
		{ "NoIcons",          "{static groups} (minimized=no)" },
		{ "WorkspaceIcons",   "{static groups} (minimized=yes) (workspace)" },
		{ "WorkspaceNoIcons", "{static groups} (minimized=no) (workspace)" },
	};
	if (!value)
		return strdup(names[0].spec);
	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
		if (strcasecmp(value, names[i].name) == 0)
			return strdup(names[i].spec);
	}
	return strdup(value);
}

static int
parse_iconbar_mode(const char *value)
{
	if (!value)
		return 0;
	if (strcasecmp(value, "AllWindows") == 0)
		return 1;
	if (strcasecmp(value, "Icons") == 0)
		return 2;
	if (strcasecmp(value, "NoIcons") == 0)
		return 3;
	if (strcasecmp(value, "WorkspaceIcons") == 0)
		return 4;
	return 0; /* Workspace (default) */
}

static int
parse_slit_placement(const char *value)
{
	if (!value)
		return 4; /* RightCenter */
	if (strcasecmp(value, "TopLeft") == 0) return 0;
	if (strcasecmp(value, "TopCenter") == 0) return 1;
	if (strcasecmp(value, "TopRight") == 0) return 2;
	if (strcasecmp(value, "RightTop") == 0) return 3;
	if (strcasecmp(value, "RightCenter") == 0) return 4;
	if (strcasecmp(value, "RightBottom") == 0) return 5;
	if (strcasecmp(value, "BottomLeft") == 0) return 6;
	if (strcasecmp(value, "BottomCenter") == 0) return 7;
	if (strcasecmp(value, "BottomRight") == 0) return 8;
	if (strcasecmp(value, "LeftTop") == 0) return 9;
	if (strcasecmp(value, "LeftCenter") == 0) return 10;
	if (strcasecmp(value, "LeftBottom") == 0) return 11;
	return 4;
}

static int
parse_slit_direction(const char *value)
{
	if (!value)
		return 0; /* Vertical */
	if (strcasecmp(value, "Horizontal") == 0)
		return 1;
	return 0; /* Vertical */
}

/*
 * The slit's and the toolbar's layer (Fluxbox's ResourceLayer): a name
 * or a Fluxbox layer number, as one of the three scene layers they can
 * be in: 6 above windows (Menu, AboveDock), 4 the top layer (Dock, Top,
 * Normal; the default), 2 below windows (Bottom, Desktop).
 */
static int
parse_dock_layer(const char *value)
{
	if (!value)
		return 4;
	static const struct { const char *name; int layer; } names[] = {
		{ "Menu", 6 }, { "AboveDock", 6 }, { "Above", 6 },
		{ "Dock", 4 }, { "Top", 4 }, { "Normal", 4 },
		{ "Bottom", 2 }, { "Below", 2 }, { "Desktop", 2 },
	};
	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
		if (strcasecmp(value, names[i].name) == 0)
			return names[i].layer;
	}
	int n;
	if (safe_atoi(value, &n))
		return n < 4 ? 6 : n < 10 ? 4 : 2;
	return 4;
}

/* "l, r, t, b" or "l r t b"; values not given are left as they are */
static void
parse_struts(const char *value, int out[4])
{
	if (!value)
		return;
	char buf[128];
	snprintf(buf, sizeof(buf), "%s", value);
	char *save = NULL;
	int i = 0;
	for (char *tok = strtok_r(buf, ", \t", &save); tok && i < 4;
	     tok = strtok_r(NULL, ", \t", &save), i++) {
		int v;
		if (safe_atoi(tok, &v))
			out[i] = v > 0 ? v : 0;
	}
}

/* Fluxbox's names (ScreenResource.cc), with Center spelled out too */
static int
parse_tab_placement(const char *value)
{
	static const struct { const char *name; int place; } names[] = {
		{ "TopLeft", WM_TAB_TOP_LEFT }, { "Top", WM_TAB_TOP },
		{ "TopCenter", WM_TAB_TOP }, { "TopRight", WM_TAB_TOP_RIGHT },
		{ "BottomLeft", WM_TAB_BOTTOM_LEFT },
		{ "Bottom", WM_TAB_BOTTOM }, { "BottomCenter", WM_TAB_BOTTOM },
		{ "BottomRight", WM_TAB_BOTTOM_RIGHT },
		{ "LeftTop", WM_TAB_LEFT_TOP }, { "Left", WM_TAB_LEFT },
		{ "LeftCenter", WM_TAB_LEFT },
		{ "LeftBottom", WM_TAB_LEFT_BOTTOM },
		{ "RightTop", WM_TAB_RIGHT_TOP }, { "Right", WM_TAB_RIGHT },
		{ "RightCenter", WM_TAB_RIGHT },
		{ "RightBottom", WM_TAB_RIGHT_BOTTOM },
	};
	for (size_t i = 0; value && i < sizeof(names) / sizeof(names[0]); i++) {
		if (strcasecmp(value, names[i].name) == 0)
			return names[i].place;
	}
	return WM_TAB_TOP_LEFT;
}

static enum wm_workspace_transition
parse_workspace_transition(const char *value)
{
	if (!value)
		return WM_TRANSITION_NONE;
	if (strcasecmp(value, "Fade") == 0)
		return WM_TRANSITION_FADE;
	if (strcasecmp(value, "Slide") == 0)
		return WM_TRANSITION_SLIDE;
	return WM_TRANSITION_NONE;
}

static enum wm_wallpaper_mode
parse_wallpaper_mode(const char *value)
{
	if (!value)
		return WM_WALLPAPER_STRETCH;
	if (strcasecmp(value, "Center") == 0)
		return WM_WALLPAPER_CENTER;
	if (strcasecmp(value, "Tile") == 0)
		return WM_WALLPAPER_TILE;
	if (strcasecmp(value, "Fill") == 0)
		return WM_WALLPAPER_FILL;
	if (strcasecmp(value, "Fit") == 0 || strcasecmp(value, "Aspect") == 0)
		return WM_WALLPAPER_FIT;
	return WM_WALLPAPER_STRETCH;
}

static void
free_wallpaper_paths(struct wm_config *config)
{
	if (config->wallpaper_paths) {
		for (int i = 0; i < config->wallpaper_path_count; i++)
			free(config->wallpaper_paths[i]);
		free(config->wallpaper_paths);
		config->wallpaper_paths = NULL;
		config->wallpaper_path_count = 0;
	}
}

static bool
path_exists(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0;
}

static bool
dir_exists(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

/*
 * Validate config directory ownership and permissions.
 * Returns true if safe to use.
 * For env-provided dirs (from_env=true), returns false on ownership or
 * permission failures so the caller can fall back to the default path.
 * For default dirs, only warns but still returns true.
 * Path traversal ("..") always causes rejection.
 */
static bool
validate_config_dir(const char *path, bool from_env)
{
	if (!path)
		return false;

	/* Reject path traversal components */
	if (strstr(path, "..")) {
		fprintf(stderr,
			_("[fluxland] config directory path contains '..': %s\n"),
			path);
		return false;
	}

	struct stat st;
	if (stat(path, &st) != 0) {
		/* Directory doesn't exist yet — can't validate further */
		return true;
	}

	if (!S_ISDIR(st.st_mode)) {
		fprintf(stderr,
			_("[fluxland] config path is not a directory: %s\n"),
			path);
		return false;
	}

	bool safe = true;

	if (st.st_uid != getuid()) {
		fprintf(stderr,
			_("[fluxland] config directory not owned by current "
			"user: %s (owned by uid %d, current uid %d)\n"),
			path, (int)st.st_uid, (int)getuid());
		if (from_env)
			safe = false;
	}

	if (st.st_mode & (S_IWGRP | S_IWOTH)) {
		fprintf(stderr,
			_("[fluxland] config directory has insecure permissions "
			"(group/world writable): %s\n"), path);
		if (from_env)
			safe = false;
	}

	return safe;
}

/*
 * Find the config directory, searching in order:
 *   1. $FLUXLAND_CONFIG_DIR (validated for ownership/permissions)
 *   2. $XDG_CONFIG_HOME/fluxland
 *   3. ~/.config/fluxland
 *   4. ~/.fluxbox (Fluxbox compat fallback)
 */
static char *
find_config_dir(void)
{
	const char *env;
	char path[4096];

	env = getenv("FLUXLAND_CONFIG_DIR");
	if (env) {
		if (!validate_config_dir(env, true)) {
			fprintf(stderr, "%s",
				_("[fluxland] FLUXLAND_CONFIG_DIR failed "
				"validation, falling back to default "
				"config path\n"));
		} else if (dir_exists(env)) {
			return strdup(env);
		}
	}

	env = getenv("XDG_CONFIG_HOME");
	if (env) {
		snprintf(path, sizeof(path), "%s/fluxland", env);
		if (dir_exists(path)) {
			validate_config_dir(path, false);
			return strdup(path);
		}
	}

	const char *home = getenv("HOME");
	if (!home)
		return NULL;

	snprintf(path, sizeof(path), "%s/.config/fluxland", home);
	if (dir_exists(path)) {
		validate_config_dir(path, false);
		return strdup(path);
	}

	snprintf(path, sizeof(path), "%s/.fluxbox", home);
	if (dir_exists(path)) {
		validate_config_dir(path, false);
		return strdup(path);
	}

	/* Default to XDG location even if it doesn't exist yet */
	snprintf(path, sizeof(path), "%s/.config/fluxland", home);
	return strdup(path);
}

static char *
resolve_path(const char *config_dir, const char *value,
	     const char *default_name)
{
	if (value && value[0] == '/')
		return strdup(value);
	if (value && value[0] == '~') {
		const char *home = getenv("HOME");
		if (home) {
			char buf[4096];
			snprintf(buf, sizeof(buf), "%s%s", home, value + 1);
			return strdup(buf);
		}
	}
	if (value)
		return strdup(value);

	char buf[4096];
	snprintf(buf, sizeof(buf), "%s/%s", config_dir, default_name);
	return strdup(buf);
}

struct wm_config *
config_create(void)
{
	struct wm_config *config = calloc(1, sizeof(*config));
	if (!config)
		return NULL;

	/* Set defaults */
	config->workspace_count = 4;
	config->workspace_warping = true;
	config->workspace_warping_horizontal = true;
	config->workspace_warping_vertical = true;
	config->workspace_warping_horizontal_offset = 1;
	config->workspace_warping_vertical_offset = 1;
	config->workspace_mode = WM_WORKSPACE_GLOBAL;
	config->focus_policy = WM_FOCUS_CLICK;
	config->raise_on_focus = true;
	config->auto_raise_delay_ms = 250;
	config->click_raises = true;
	config->focus_new_windows = true;
	config->focus_same_head = false;
	config->no_focus_while_typing_ms = 0;
	config->ignore_border = false;
	config->tabs_attach_titlebar = false;
	config->opaque_move = true;
	config->opaque_resize = false;
	config->edge_snap_threshold = 10;
	config->placement_policy = WM_PLACEMENT_ROW_SMART;
	config->toolbar_visible = true;
	config->toolbar_placement = WM_TOOLBAR_BOTTOM_CENTER;
	config->toolbar_auto_hide = false;
	config->toolbar_auto_raise = false;
	config->toolbar_auto_hide_delay_ms = 500;
	config->toolbar_width_percent = 72;
	config->toolbar_height = 0;
	config->toolbar_layer = 4; /* Dock */
	config->toolbar_alpha = 255;
	config->update_activation_env = -1;
	config->toolbar_on_head = 0;
	config->toolbar_tools = strdup(
		"workspacename prevworkspace nextworkspace iconbar clock");

	config->titlebar_left = strdup("Stick");
	config->titlebar_right = strdup("Minimize Maximize Close");
	config->clock_format = strdup("%H:%M");
	config->iconbar_mode = 0;
	config->iconbar_mode_spec = iconbar_mode_spec(NULL);
	config->iconbar_alignment = 1; /* relative */
	config->iconbar_icon_width = 0; /* auto */
	config->iconbar_use_pixmap = true;
	config->iconbar_wheel_mode = 0; /* screen (workspace change) */
	config->auto_tab_placement = false;
	config->tab_focus_model = 0; /* click */
	config->tabs_intitlebar = true;
	config->tab_placement = WM_TAB_TOP_LEFT;
	config->tab_width = 64;
	config->tab_padding = 0;

	config->full_maximization = false;
	config->window_focus_alpha = 255;
	config->window_unfocus_alpha = 255;
	config->enable_window_snapping = false;
	config->snap_zone_threshold = 10;
	config->animate_window_map = false;
	config->animate_window_unmap = false;
	config->animate_minimize = false;
	config->animation_duration_ms = 300;
	config->workspace_transition = WM_TRANSITION_NONE;
	config->workspace_transition_duration_ms = 300;

	config->slit_auto_hide = false;
	config->slit_auto_raise = false;
	config->slit_placement = 4; /* RightCenter */
	config->slit_direction = 0; /* Vertical */
	config->slit_layer = 4; /* Dock */
	config->slit_on_head = 0;
	config->slit_alpha = 255;
	config->slit_max_over = false;

	config->double_click_interval = 300;
	config->menu_alpha = 255;
	config->toolbar_max_over = false;
	config->edge_resize_snap_threshold = 0;
	config->menu_search = WM_MENU_SEARCH_ITEMSTART; /* Fluxbox's */
	config->menu_delay = 200;

	/* Default mouse button mappings (identity map) */
	config->mouse_button_map[0] = 0;
	config->mouse_button_map[1] = BTN_LEFT;
	config->mouse_button_map[2] = BTN_MIDDLE;
	config->mouse_button_map[3] = BTN_RIGHT;
	/* Fluxbox's (X's) buttons 4 and 5 are the wheel */
	config->mouse_button_map[4] = WM_BUTTON_WHEEL_UP;
	config->mouse_button_map[5] = WM_BUTTON_WHEEL_DOWN;

	config->gesture_workspace_switch = true;
	config->gesture_overview = false;
	config->gesture_workspace_fingers = 3;
	config->gesture_swipe_threshold = 100.0;

	config->config_dir = find_config_dir();

	set_default_workspace_names(config);

	/* Set default file paths */
	if (config->config_dir) {
		config->keys_file =
			resolve_path(config->config_dir, NULL, "keys");
		config->apps_file =
			resolve_path(config->config_dir, NULL, "apps");
		config->menu_file =
			resolve_path(config->config_dir, NULL, "menu");
		config->style_file =
			resolve_path(config->config_dir, NULL, "styles/default");
		config->slitlist_file =
			resolve_path(config->config_dir, NULL, "slitlist");
	}

	return config;
}

static int parse_tristate_bool(const char *val);

static void
apply_workspace_config(struct wm_config *config, struct rc_database *db)
{
	const char *val;

	config->workspace_count =
		rc_get_int(db, "session.screen0.workspaces", 4);
	if (config->workspace_count < 1)
		config->workspace_count = 1;
	if (config->workspace_count > 32)
		config->workspace_count = 32;

	val = rc_get_string(db, "session.screen0.workspaceNames");
	if (val)
		parse_workspace_names(config, val);
	else
		set_default_workspace_names(config);

	config->workspace_warping =
		rc_get_bool(db, "session.screen0.workspacewarping", true);
	config->workspace_warping_horizontal = rc_get_bool(db,
		"session.screen0.workspacewarpinghorizontal", true);
	config->workspace_warping_vertical = rc_get_bool(db,
		"session.screen0.workspacewarpingvertical", true);
	config->workspace_warping_horizontal_offset = rc_get_int(db,
		"session.screen0.workspacewarpinghorizontaloffset", 1);
	config->workspace_warping_vertical_offset = rc_get_int(db,
		"session.screen0.workspacewarpingverticaloffset", 1);

	val = rc_get_string(db, "session.screen0.workspaceMode");
	if (val && strcasecmp(val, "per-output") == 0)
		config->workspace_mode = WM_WORKSPACE_PER_OUTPUT;
	else
		config->workspace_mode = WM_WORKSPACE_GLOBAL;

	val = rc_get_string(db, "session.screen0.workspaceTransition");
	config->workspace_transition = parse_workspace_transition(val);

	config->workspace_transition_duration_ms =
		rc_get_int(db, "session.screen0.workspaceTransitionDuration",
			   300);
	if (config->workspace_transition_duration_ms < 0)
		config->workspace_transition_duration_ms = 0;
	if (config->workspace_transition_duration_ms > 5000)
		config->workspace_transition_duration_ms = 5000;
}

static void
apply_focus_config(struct wm_config *config, struct rc_database *db)
{
	const char *val;

	val = rc_get_string(db, "session.screen0.focusModel");
	config->focus_policy = parse_focus_model(val);

	config->raise_on_focus =
		rc_get_bool(db, "session.screen0.autoRaise", true);
	config->auto_raise_delay_ms =
		rc_get_int(db, "session.autoRaiseDelay", 250);
	config->click_raises =
		rc_get_bool(db, "session.screen0.clickRaises", true);
	config->focus_new_windows =
		rc_get_bool(db, "session.screen0.focusNewWindows", true);
	config->focus_same_head =
		rc_get_bool(db, "session.screen0.focusSameHead", false);
	config->no_focus_while_typing_ms = rc_get_int(db,
		"session.screen0.noFocusWhileTypingDelay", 0);
	config->ignore_border =
		rc_get_bool(db, "session.ignoreBorder", false);
	const char *area = rc_get_string(db, "session.tabsAttachArea");
	config->tabs_attach_titlebar = area &&
		strcasecmp(area, "Titlebar") == 0;
}

static void
apply_window_config(struct wm_config *config, struct rc_database *db)
{
	const char *val;

	/* Move/resize behavior */
	config->opaque_move =
		rc_get_bool(db, "session.screen0.opaqueMove", true);
	config->opaque_resize =
		rc_get_bool(db, "session.screen0.opaqueResize", false);
	config->edge_snap_threshold =
		rc_get_int(db, "session.screen0.edgeSnapThreshold", 10);

	/* Placement */
	val = rc_get_string(db, "session.screen0.windowPlacement");
	config->placement_policy = parse_placement(val);

	val = rc_get_string(db, "session.screen0.rowPlacementDirection");
	config->row_right_to_left = (val &&
		strcasecmp(val, "RightToLeft") == 0);

	val = rc_get_string(db, "session.screen0.colPlacementDirection");
	config->col_bottom_to_top = (val &&
		strcasecmp(val, "BottomToTop") == 0);

	/* Position overlay during move/resize */
	config->show_window_position =
		rc_get_bool(db, "session.screen0.showWindowPosition", false);

	/* Maximization */
	config->full_maximization =
		rc_get_bool(db, "session.screen0.fullMaximization", false);
	config->max_disable_move =
		rc_get_bool(db, "session.screen0.maxDisableMove", false);
	config->max_disable_resize =
		rc_get_bool(db, "session.screen0.maxDisableResize", false);

	/* Opacity */
	config->window_focus_alpha =
		rc_get_int(db, "session.screen0.window.focus.alpha", 255);
	if (config->window_focus_alpha < 0) config->window_focus_alpha = 0;
	if (config->window_focus_alpha > 255) config->window_focus_alpha = 255;
	config->window_unfocus_alpha =
		rc_get_int(db, "session.screen0.window.unfocus.alpha", 255);
	if (config->window_unfocus_alpha < 0) config->window_unfocus_alpha = 0;
	if (config->window_unfocus_alpha > 255) config->window_unfocus_alpha = 255;

	/* Animations */
	config->animate_window_map =
		rc_get_bool(db, "session.screen0.animateWindowMap", false);
	config->animate_window_unmap =
		rc_get_bool(db, "session.screen0.animateWindowUnmap", false);
	config->animate_minimize =
		rc_get_bool(db, "session.screen0.animateMinimize", false);
	config->animation_duration_ms =
		rc_get_int(db, "session.screen0.animationDuration", 300);
	if (config->animation_duration_ms < 0)
		config->animation_duration_ms = 0;
	if (config->animation_duration_ms > 5000)
		config->animation_duration_ms = 5000;

	/* Snap zones */
	config->enable_window_snapping =
		rc_get_bool(db, "session.screen0.enableWindowSnapping", false);
	config->snap_zone_threshold =
		rc_get_int(db, "session.screen0.snapZoneThreshold", 10);
	if (config->snap_zone_threshold < 1)
		config->snap_zone_threshold = 1;
	if (config->snap_zone_threshold > 100)
		config->snap_zone_threshold = 100;

	/* Edge resize snap */
	config->edge_resize_snap_threshold =
		rc_get_int(db, "session.screen0.edgeResizeSnapThreshold", 0);
	if (config->edge_resize_snap_threshold < 0)
		config->edge_resize_snap_threshold = 0;

	/* Default decoration preset */
	val = rc_get_string(db, "session.screen0.defaultDeco");
	if (val) {
		free(config->default_deco);
		config->default_deco = strdup(val);
	}
}

static void
free_toolbar_buttons(struct wm_config *config)
{
	for (int i = 0; i < config->toolbar_button_count; i++) {
		free(config->toolbar_buttons[i].name);
		free(config->toolbar_buttons[i].label);
		free(config->toolbar_buttons[i].commands);
	}
	free(config->toolbar_buttons);
	config->toolbar_buttons = NULL;
	config->toolbar_button_count = 0;
}

static void
apply_toolbar_config(struct wm_config *config, struct rc_database *db)
{
	const char *val;

	config->toolbar_visible =
		rc_get_bool(db, "session.screen0.toolbar.visible", true);

	val = rc_get_string(db, "session.screen0.toolbar.placement");
	config->toolbar_placement = parse_toolbar_placement(val);

	config->toolbar_auto_hide =
		rc_get_bool(db, "session.screen0.toolbar.autoHide", false);
	config->toolbar_auto_raise =
		rc_get_bool(db, "session.screen0.toolbar.autoRaise", false);
	config->toolbar_auto_hide_delay_ms =
		rc_get_int(db, "session.screen0.toolbar.autoHideDelay", 500);

	config->toolbar_width_percent =
		rc_get_int(db, "session.screen0.toolbar.widthPercent", 100);
	if (config->toolbar_width_percent < 10)
		config->toolbar_width_percent = 10;
	if (config->toolbar_width_percent > 100)
		config->toolbar_width_percent = 100;

	config->toolbar_height =
		rc_get_int(db, "session.screen0.toolbar.height", 0);
	config->toolbar_layer = parse_dock_layer(
		rc_get_string(db, "session.screen0.toolbar.layer"));
	config->toolbar_alpha =
		rc_get_int(db, "session.screen0.toolbar.alpha", 255);
	if (config->toolbar_alpha < 0) config->toolbar_alpha = 0;
	if (config->toolbar_alpha > 255) config->toolbar_alpha = 255;
	config->toolbar_on_head =
		rc_get_int(db, "session.screen0.toolbar.onhead", 0);

	/* Toolbar tools layout */
	val = rc_get_string(db, "session.screen0.toolbar.tools");
	if (val) {
		free(config->toolbar_tools);
		config->toolbar_tools = strdup(val);
	}

	/* Fluxbox's button.NAME tools: their label and commands */
	free_toolbar_buttons(config);
	const char *prefix = "session.screen0.toolbar.button.";
	size_t plen = strlen(prefix);
	for (int i = 0; i < db->count; i++) {
		const char *key = db->entries[i].key;
		size_t klen = strlen(key);
		if (strncasecmp(key, prefix, plen) != 0 || klen <= plen + 6 ||
		    strcasecmp(key + klen - 6, ".label") != 0)
			continue;
		char name[64];
		snprintf(name, sizeof(name), "%.*s", (int)(klen - plen - 6),
			key + plen);
		struct wm_toolbar_button_cfg *b = realloc(
			config->toolbar_buttons,
			(config->toolbar_button_count + 1) * sizeof(*b));
		if (!b)
			break;
		config->toolbar_buttons = b;
		b += config->toolbar_button_count++;
		b->name = strdup(name);
		b->label = strdup(db->entries[i].value);
		char ckey[160];
		snprintf(ckey, sizeof(ckey), "%s%s.commands", prefix, name);
		const char *cmds = rc_get_string(db, ckey);
		b->commands = strdup(cmds ? cmds : "");
	}

	/* Titlebar button layout */
	/* Fluxbox saves these per screen; its manual has session.titlebar */
	val = rc_get_string(db, "session.screen0.titlebar.left");
	if (!val)
		val = rc_get_string(db, "session.titlebar.left");
	if (val) {
		free(config->titlebar_left);
		config->titlebar_left = strdup(val);
	}
	val = rc_get_string(db, "session.screen0.titlebar.right");
	if (!val)
		val = rc_get_string(db, "session.titlebar.right");
	if (val) {
		free(config->titlebar_right);
		config->titlebar_right = strdup(val);
	}

	/* Maximize-over */
	config->toolbar_max_over =
		rc_get_bool(db, "session.screen0.toolbar.maxOver", false);
}

static void
apply_iconbar_config(struct wm_config *config, struct rc_database *db)
{
	const char *val;

	/* Clock format */
	val = rc_get_string(db, "session.screen0.strftimeFormat");
	if (val) {
		free(config->clock_format);
		config->clock_format = strdup(val);
	}

	/* Iconbar mode */
	val = rc_get_string(db, "session.screen0.iconbar.mode");
	config->iconbar_mode = parse_iconbar_mode(val);
	free(config->iconbar_mode_spec);
	config->iconbar_mode_spec = iconbar_mode_spec(val);

	/* Iconbar alignment */
	val = rc_get_string(db, "session.screen0.iconbar.alignment");
	/* Fluxbox: Left and Right use iconWidth, Relative (and
	 * RelativeSmart) share the iconbar out; fluxland 1.x: Center */
	if (val) {
		if (strcasecmp(val, "Left") == 0)
			config->iconbar_alignment = 0;
		else if (strcasecmp(val, "Right") == 0)
			config->iconbar_alignment = 2;
		else if (strcasecmp(val, "Center") == 0)
			config->iconbar_alignment = 3;
		else
			config->iconbar_alignment = 1; /* Relative */
	}
	config->iconbar_icon_width =
		rc_get_int(db, "session.screen0.iconbar.iconWidth", 0);
	if (config->iconbar_icon_width < 0)
		config->iconbar_icon_width = 0;
	config->iconbar_use_pixmap =
		rc_get_bool(db, "session.screen0.iconbar.usePixmap", true);
	val = rc_get_string(db, "session.screen0.iconbar.wheelMode");
	if (val) {
		if (strcasecmp(val, "Off") == 0)
			config->iconbar_wheel_mode = 1;
		else
			config->iconbar_wheel_mode = 0; /* Screen */
	}
	val = rc_get_string(db, "session.screen0.iconbar.iconifiedPattern");
	if (val) {
		free(config->iconbar_iconified_pattern);
		config->iconbar_iconified_pattern = strdup(val);
	}
}

static void
apply_tab_config(struct wm_config *config, struct rc_database *db)
{
	const char *val;

	config->auto_tab_placement =
		rc_get_bool(db, "session.screen0.autoTabPlacement", false);

	/* Tab focus model */
	val = rc_get_string(db, "session.screen0.tabFocusModel");
	if (val) {
		if (strcasecmp(val, "MouseTabFocus") == 0)
			config->tab_focus_model = 1;
		else
			config->tab_focus_model = 0; /* ClickTabFocus */
	}

	/* External tab bar */
	config->tabs_intitlebar =
		rc_get_bool(db, "session.screen0.tabs.intitlebar", true);
	config->tab_placement = parse_tab_placement(
		rc_get_string(db, "session.screen0.tab.placement"));
	config->tab_width =
		rc_get_int(db, "session.screen0.tab.width", 64);
	if (config->tab_width <= 0)
		config->tab_width = 64;
	config->tab_padding =
		rc_get_int(db, "session.tabPadding",
			rc_get_int(db, "session.screen0.tabPadding", 0));
	if (config->tab_padding < 0)
		config->tab_padding = 0;
}

static void
apply_slit_config(struct wm_config *config, struct rc_database *db)
{
	const char *val;

	config->slit_auto_hide =
		rc_get_bool(db, "session.screen0.slit.autoHide", false);
	config->slit_auto_raise =
		rc_get_bool(db, "session.screen0.slit.autoRaise", false);
	val = rc_get_string(db, "session.screen0.slit.placement");
	config->slit_placement = parse_slit_placement(val);
	val = rc_get_string(db, "session.screen0.slit.direction");
	config->slit_direction = parse_slit_direction(val);
	config->slit_layer = parse_dock_layer(
		rc_get_string(db, "session.screen0.slit.layer"));
	config->slit_on_head =
		rc_get_int(db, "session.screen0.slit.onhead", 0);
	config->slit_alpha =
		rc_get_int(db, "session.screen0.slit.alpha", 255);
	if (config->slit_alpha < 0) config->slit_alpha = 0;
	if (config->slit_alpha > 255) config->slit_alpha = 255;
	config->slit_max_over =
		rc_get_bool(db, "session.screen0.slit.maxOver", false);

	/* Slitlist file path */
	val = rc_get_string(db, "session.slitlistFile");
	if (val) {
		free(config->slitlist_file);
		config->slitlist_file =
			resolve_path(config->config_dir, val, "slitlist");
	}
}

static void
apply_menu_config(struct wm_config *config, struct rc_database *db)
{
	const char *val;

	config->menu_alpha =
		rc_get_int(db, "session.screen0.menu.alpha", 255);
	if (config->menu_alpha < 0) config->menu_alpha = 0;
	if (config->menu_alpha > 255) config->menu_alpha = 255;

	static const char *const switch_keys[WM_SWITCH_EVENT_COUNT] = {
		"session.switch.lidClose",
		"session.switch.lidOpen",
		"session.switch.tabletModeOn",
		"session.switch.tabletModeOff",
	};
	for (int i = 0; i < WM_SWITCH_EVENT_COUNT; i++) {
		free(config->switch_commands[i]);
		val = rc_get_string(db, switch_keys[i]);
		config->switch_commands[i] = val && *val ? strdup(val) : NULL;
	}

	val = rc_get_string(db, "session.updateActivationEnvironment");
	if (val) {
		if (strcasecmp(val, "auto") == 0)
			config->update_activation_env = -1;
		else
			config->update_activation_env =
				parse_tristate_bool(val) == 1 ? 1 :
				parse_tristate_bool(val) == 0 ? 0 : -1;
	}

	/* Type-ahead search */
	val = rc_get_string(db, "session.menuSearch");
	config->menu_search = WM_MENU_SEARCH_ITEMSTART;
	if (val) {
		if (strcasecmp(val, "itemstart") == 0)
			config->menu_search = WM_MENU_SEARCH_ITEMSTART;
		else if (strcasecmp(val, "somewhere") == 0)
			config->menu_search = WM_MENU_SEARCH_SOMEWHERE;
		else
			config->menu_search = WM_MENU_SEARCH_NOWHERE;
	}

	/* Submenu hover-open delay (ms) */
	config->menu_delay =
		rc_get_int(db, "session.screen0.menuDelay", 200);
	if (config->menu_delay < 0) config->menu_delay = 0;
	if (config->menu_delay > 5000) config->menu_delay = 5000;

	/* Window menu file: the init's, else a windowmenu file in the
	 * config directory (Fluxbox's ~/.fluxbox/windowmenu), else the
	 * built-in copy of Fluxbox's */
	free(config->window_menu_file);
	config->window_menu_file = NULL;
	val = rc_get_string(db, "session.screen0.windowMenu");
	if (val) {
		config->window_menu_file = resolve_path(config->config_dir,
			val, "windowmenu");
	} else if (config->config_dir) {
		char *path = resolve_path(config->config_dir, NULL,
			"windowmenu");
		if (path && access(path, R_OK) == 0)
			config->window_menu_file = path;
		else
			free(path);
	}
}

static void
apply_xkb_config(struct wm_config *config, struct rc_database *db)
{
	const char *val;

	val = rc_get_string(db, "session.xkb.rules");
	if (val) {
		char *dup = strdup(val);
		if (dup) {
			free(config->xkb_rules);
			config->xkb_rules = dup;
		}
	}
	val = rc_get_string(db, "session.xkb.model");
	if (val) {
		char *dup = strdup(val);
		if (dup) {
			free(config->xkb_model);
			config->xkb_model = dup;
		}
	}
	val = rc_get_string(db, "session.xkb.layout");
	if (val) {
		char *dup = strdup(val);
		if (dup) {
			free(config->xkb_layout);
			config->xkb_layout = dup;
		}
	}
	val = rc_get_string(db, "session.xkb.variant");
	if (val) {
		char *dup = strdup(val);
		if (dup) {
			free(config->xkb_variant);
			config->xkb_variant = dup;
		}
	}
	val = rc_get_string(db, "session.xkb.options");
	if (val) {
		char *dup = strdup(val);
		if (dup) {
			free(config->xkb_options);
			config->xkb_options = dup;
		}
	}
}

/* --- libinput settings (session.input.*) --- */

static void
input_rule_init(struct wm_input_rule *r)
{
	memset(r, 0, sizeof(*r));
	r->tap = r->tap_drag = r->tap_drag_lock = -1;
	r->tap_button_map = -1;
	r->natural_scroll = r->left_handed = -1;
	r->disable_while_typing = r->middle_emulation = -1;
	r->accel_profile = -1;
	r->accel_speed = NAN;
	r->scroll_method = -1;
	r->scroll_button = -1;
	r->click_method = -1;
	r->send_events = -1;
}

static void
free_input_rules(struct wm_config *config)
{
	for (int i = 0; i < config->input_rule_count; i++)
		free(config->input_rules[i].device);
	free(config->input_rules);
	config->input_rules = NULL;
	config->input_rule_count = 0;
}

/* Find or add the rule for a scope (and device name) */
static struct wm_input_rule *
input_rule_for(struct wm_config *config, enum wm_input_scope scope,
	const char *device, size_t device_len)
{
	for (int i = 0; i < config->input_rule_count; i++) {
		struct wm_input_rule *r = &config->input_rules[i];
		if (r->scope != scope)
			continue;
		if (scope != WM_INPUT_SCOPE_DEVICE ||
		    (strlen(r->device) == device_len &&
		     strncmp(r->device, device, device_len) == 0))
			return r;
	}
	struct wm_input_rule *rules = realloc(config->input_rules,
		sizeof(*rules) * (size_t)(config->input_rule_count + 1));
	if (!rules)
		return NULL;
	config->input_rules = rules;
	struct wm_input_rule *r = &rules[config->input_rule_count];
	input_rule_init(r);
	r->scope = scope;
	if (scope == WM_INPUT_SCOPE_DEVICE) {
		r->device = strndup(device, device_len);
		if (!r->device)
			return NULL;
	}
	config->input_rule_count++;
	return r;
}

static int
parse_tristate_bool(const char *val)
{
	if (strcasecmp(val, "true") == 0 || strcasecmp(val, "yes") == 0 ||
	    strcmp(val, "1") == 0 || strcasecmp(val, "on") == 0)
		return 1;
	if (strcasecmp(val, "false") == 0 || strcasecmp(val, "no") == 0 ||
	    strcmp(val, "0") == 0 || strcasecmp(val, "off") == 0)
		return 0;
	return -1;
}

static int
parse_choice(const char *val, const char *const *names, int count)
{
	for (int i = 0; i < count; i++) {
		if (strcasecmp(val, names[i]) == 0)
			return i;
	}
	return -1;
}

/* Returns false for an unknown setting name */
static bool
set_input_setting(struct wm_input_rule *r, const char *name,
	const char *val)
{
	static const char *const tap_maps[] = { "lrm", "lmr" };
	static const char *const profiles[] = { "flat", "adaptive" };
	static const char *const scroll[] = {
		"none", "twoFinger", "edge", "button" };
	static const char *const clicks[] = {
		"none", "buttonAreas", "clickFinger" };
	static const char *const sends[] = {
		"enabled", "disabled", "disabledOnExternalMouse" };

	if (strcasecmp(name, "tap") == 0)
		r->tap = parse_tristate_bool(val);
	else if (strcasecmp(name, "tapDrag") == 0)
		r->tap_drag = parse_tristate_bool(val);
	else if (strcasecmp(name, "tapDragLock") == 0)
		r->tap_drag_lock = parse_tristate_bool(val);
	else if (strcasecmp(name, "tapButtonMap") == 0)
		r->tap_button_map = parse_choice(val, tap_maps, 2);
	else if (strcasecmp(name, "naturalScroll") == 0)
		r->natural_scroll = parse_tristate_bool(val);
	else if (strcasecmp(name, "leftHanded") == 0)
		r->left_handed = parse_tristate_bool(val);
	else if (strcasecmp(name, "disableWhileTyping") == 0)
		r->disable_while_typing = parse_tristate_bool(val);
	else if (strcasecmp(name, "middleEmulation") == 0)
		r->middle_emulation = parse_tristate_bool(val);
	else if (strcasecmp(name, "accelProfile") == 0)
		r->accel_profile = parse_choice(val, profiles, 2);
	else if (strcasecmp(name, "accelSpeed") == 0) {
		char *end;
		double v = strtod(val, &end);
		if (*end == '\0' && v >= -1.0 && v <= 1.0)
			r->accel_speed = v;
	} else if (strcasecmp(name, "scrollMethod") == 0)
		r->scroll_method = parse_choice(val, scroll, 4);
	else if (strcasecmp(name, "scrollButton") == 0) {
		char *end;
		long b = strtol(val, &end, 10);
		if (*end == '\0' && b > 0 && b < 0x300)
			r->scroll_button = (int)b;
	} else if (strcasecmp(name, "clickMethod") == 0)
		r->click_method = parse_choice(val, clicks, 3);
	else if (strcasecmp(name, "sendEvents") == 0)
		r->send_events = parse_choice(val, sends, 3);
	else
		return false;
	return true;
}

static void
apply_input_config(struct wm_config *config, struct rc_database *db)
{
	free_input_rules(config);

	static const char prefix[] = "session.input.";
	const size_t plen = sizeof(prefix) - 1;
	for (int i = 0; i < db->count; i++) {
		const char *key = db->entries[i].key;
		const char *val = db->entries[i].value;
		if (strncmp(key, prefix, plen) != 0 || !val)
			continue;
		const char *rest = key + plen;

		enum wm_input_scope scope = WM_INPUT_SCOPE_ALL;
		const char *device = NULL;
		size_t device_len = 0;
		if (strncmp(rest, "touchpad.", 9) == 0) {
			scope = WM_INPUT_SCOPE_TOUCHPAD;
			rest += 9;
		} else if (strncmp(rest, "pointer.", 8) == 0) {
			scope = WM_INPUT_SCOPE_POINTER;
			rest += 8;
		} else if (strncmp(rest, "device.", 7) == 0) {
			/* device.<Name>.<setting>: the setting is the last part */
			device = rest + 7;
			const char *dot = strrchr(device, '.');
			if (!dot || dot == device)
				continue;
			scope = WM_INPUT_SCOPE_DEVICE;
			device_len = (size_t)(dot - device);
			rest = dot + 1;
		}

		struct wm_input_rule *r =
			input_rule_for(config, scope, device, device_len);
		if (r && !set_input_setting(r, rest, val))
			fprintf(stderr, "fluxland: unknown input setting: %s\n",
				key);
	}
}

/* --- Output settings (session.output.<Connector>.*) --- */

static void
free_output_rules(struct wm_config *config)
{
	for (int i = 0; i < config->output_rule_count; i++)
		free(config->output_rules[i].name);
	free(config->output_rules);
	config->output_rules = NULL;
	config->output_rule_count = 0;
}

static struct wm_output_rule *
output_rule_for(struct wm_config *config, const char *name, size_t len)
{
	for (int i = 0; i < config->output_rule_count; i++) {
		struct wm_output_rule *r = &config->output_rules[i];
		if (strlen(r->name) == len && strncmp(r->name, name, len) == 0)
			return r;
	}
	struct wm_output_rule *rules = realloc(config->output_rules,
		sizeof(*rules) * (size_t)(config->output_rule_count + 1));
	if (!rules)
		return NULL;
	config->output_rules = rules;
	struct wm_output_rule *r = &rules[config->output_rule_count];
	memset(r, 0, sizeof(*r));
	r->name = strndup(name, len);
	if (!r->name)
		return NULL;
	r->enabled = -1;
	r->scale = NAN;
	r->transform = -1;
	r->adaptive_sync = -1;
	config->output_rule_count++;
	return r;
}

/* "1920x1080" or "1920x1080@60" / "@59.94" */
bool
config_parse_output_mode(const char *val, int *w, int *h, int *mhz)
{
	char *end;
	long pw = strtol(val, &end, 10);
	if (end == val || (*end != 'x' && *end != 'X') || pw <= 0 ||
	    pw > 32767)
		return false;
	const char *p = end + 1;
	long ph = strtol(p, &end, 10);
	if (end == p || ph <= 0 || ph > 32767)
		return false;
	double hz = 0;
	if (*end == '@') {
		p = end + 1;
		hz = strtod(p, &end);
		if (end == p || hz <= 0 || hz > 1000)
			return false;
		if (strcasecmp(end, "Hz") == 0)
			end += 2;
	}
	if (*end != '\0')
		return false;
	*w = (int)pw;
	*h = (int)ph;
	*mhz = (int)(hz * 1000 + 0.5);
	return true;
}

/* Index into wl_output_transform order, -1 if unknown */
int
config_parse_output_transform(const char *val)
{
	static const char *const names[] = {
		"normal", "90", "180", "270",
		"flipped", "flipped-90", "flipped-180", "flipped-270",
	};
	for (int i = 0; i < 8; i++) {
		if (strcasecmp(val, names[i]) == 0)
			return i;
	}
	return -1;
}

static bool
set_output_setting(struct wm_output_rule *r, const char *name,
	const char *val)
{
	if (strcasecmp(name, "enabled") == 0) {
		r->enabled = parse_tristate_bool(val);
	} else if (strcasecmp(name, "mode") == 0) {
		int w, h, mhz;
		if (config_parse_output_mode(val, &w, &h, &mhz)) {
			r->width = w;
			r->height = h;
			r->refresh_mhz = mhz;
		}
	} else if (strcasecmp(name, "scale") == 0) {
		char *end;
		double v = strtod(val, &end);
		if (*end == '\0' && v >= 0.25 && v <= 10.0)
			r->scale = v;
	} else if (strcasecmp(name, "position") == 0) {
		char *end;
		long x = strtol(val, &end, 10);
		if (end != val && *end == ',') {
			const char *p = end + 1;
			long y = strtol(p, &end, 10);
			if (end != p && *end == '\0' && labs(x) < 100000 &&
			    labs(y) < 100000) {
				r->has_position = true;
				r->x = (int)x;
				r->y = (int)y;
			}
		}
	} else if (strcasecmp(name, "transform") == 0) {
		r->transform = config_parse_output_transform(val);
	} else if (strcasecmp(name, "adaptiveSync") == 0) {
		r->adaptive_sync = parse_tristate_bool(val);
	} else {
		return false;
	}
	return true;
}

static void
apply_output_config(struct wm_config *config, struct rc_database *db)
{
	free_output_rules(config);

	static const char prefix[] = "session.output.";
	const size_t plen = sizeof(prefix) - 1;
	for (int i = 0; i < db->count; i++) {
		const char *key = db->entries[i].key;
		const char *val = db->entries[i].value;
		if (strncmp(key, prefix, plen) != 0 || !val)
			continue;
		const char *name = key + plen;
		const char *dot = strrchr(name, '.');
		if (!dot || dot == name)
			continue;
		struct wm_output_rule *r =
			output_rule_for(config, name, (size_t)(dot - name));
		if (r && !set_output_setting(r, dot + 1, val))
			fprintf(stderr, "fluxland: unknown output setting: %s\n",
				key);
	}
}

const struct wm_output_rule *
config_find_output_rule(const struct wm_config *config, const char *name)
{
	if (!config || !name)
		return NULL;
	for (int i = 0; i < config->output_rule_count; i++) {
		if (strcmp(config->output_rules[i].name, name) == 0)
			return &config->output_rules[i];
	}
	return NULL;
}

static void
apply_mouse_config(struct wm_config *config, struct rc_database *db)
{
	const char *val;
	static const char *button_keys[5] = {
		"session.mouse.button1.map",
		"session.mouse.button2.map",
		"session.mouse.button3.map",
		"session.mouse.button4.map",
		"session.mouse.button5.map",
	};
	static const uint32_t button_defaults[5] = {
		BTN_LEFT, BTN_MIDDLE, BTN_RIGHT, WM_BUTTON_WHEEL_UP,
		WM_BUTTON_WHEEL_DOWN,
	};
	for (int i = 0; i < 5; i++) {
		val = rc_get_string(db, button_keys[i]);
		if (val) {
			if (strcasecmp(val, "BTN_LEFT") == 0)
				config->mouse_button_map[i + 1] = BTN_LEFT;
			else if (strcasecmp(val, "BTN_MIDDLE") == 0)
				config->mouse_button_map[i + 1] = BTN_MIDDLE;
			else if (strcasecmp(val, "BTN_RIGHT") == 0)
				config->mouse_button_map[i + 1] = BTN_RIGHT;
			else if (strcasecmp(val, "BTN_SIDE") == 0)
				config->mouse_button_map[i + 1] = BTN_SIDE;
			else if (strcasecmp(val, "BTN_EXTRA") == 0)
				config->mouse_button_map[i + 1] = BTN_EXTRA;
			else if (strcasecmp(val, "WHEEL_UP") == 0)
				config->mouse_button_map[i + 1] =
					WM_BUTTON_WHEEL_UP;
			else if (strcasecmp(val, "WHEEL_DOWN") == 0)
				config->mouse_button_map[i + 1] =
					WM_BUTTON_WHEEL_DOWN;
			else
				config->mouse_button_map[i + 1] =
					button_defaults[i];
		}
	}
}

static void
apply_misc_config(struct wm_config *config, struct rc_database *db)
{
	const char *val;

	/* Double-click interval */
	config->double_click_interval =
		rc_get_int(db, "session.doubleClickInterval", 250);
	if (config->double_click_interval < 50)
		config->double_click_interval = 50;
	if (config->double_click_interval > 2000)
		config->double_click_interval = 2000;

	/* Root command */
	val = rc_get_string(db, "session.screen0.rootCommand");
	if (val) {
		free(config->root_command);
		config->root_command = strdup(val);
	}

	/* struts: left, right, top, bottom (BScreen::reconfigureStruts) */
	memset(config->struts, 0, sizeof(config->struts));
	parse_struts(rc_get_string(db, "session.screen0.struts"),
		config->struts);
	for (int i = 0; i < WM_STRUT_HEADS; i++) {
		char key[64];
		snprintf(key, sizeof(key), "session.screen0.struts.%d", i + 1);
		memcpy(config->head_struts[i], config->struts,
			sizeof(config->struts));
		parse_struts(rc_get_string(db, key), config->head_struts[i]);
	}

	/* Gesture settings */
	config->gesture_workspace_switch =
		rc_get_bool(db, "session.screen0.gestureWorkspaceSwitch", true);
	config->gesture_overview =
		rc_get_bool(db, "session.screen0.gestureOverview", false);
	config->gesture_workspace_fingers =
		rc_get_int(db, "session.screen0.gestureWorkspaceFingers", 3);
	if (config->gesture_workspace_fingers < 2)
		config->gesture_workspace_fingers = 2;
	if (config->gesture_workspace_fingers > 5)
		config->gesture_workspace_fingers = 5;
	double thresh = rc_get_int(db, "session.screen0.gestureSwipeThreshold", 100);
	if (thresh < 10) thresh = 10;
	if (thresh > 1000) thresh = 1000;
	config->gesture_swipe_threshold = thresh;
}

static void
apply_file_paths_config(struct wm_config *config, struct rc_database *db)
{
	const char *val;

	val = rc_get_string(db, "session.keyFile");
	if (val) {
		free(config->keys_file);
		config->keys_file =
			resolve_path(config->config_dir, val, "keys");
	}

	val = rc_get_string(db, "session.appsFile");
	if (val) {
		free(config->apps_file);
		config->apps_file =
			resolve_path(config->config_dir, val, "apps");
	}

	val = rc_get_string(db, "session.menuFile");
	if (val) {
		free(config->menu_file);
		config->menu_file =
			resolve_path(config->config_dir, val, "menu");
	}

	val = rc_get_string(db, "session.styleFile");
	if (val) {
		free(config->style_file);
		config->style_file =
			resolve_path(config->config_dir, val, "styles/default");
	}

	val = rc_get_string(db, "session.styleOverlay");
	if (val) {
		free(config->style_overlay);
		config->style_overlay =
			resolve_path(config->config_dir, val, "overlay");
	}
}

static void
apply_wallpaper_config(struct wm_config *config, struct rc_database *db)
{
	const char *val;

	val = rc_get_string(db, "session.screen0.wallpaper");
	if (val) {
		free(config->wallpaper);
		config->wallpaper = resolve_path(config->config_dir,
						 val, "wallpaper.png");
	}

	val = rc_get_string(db, "session.screen0.wallpaperMode");
	config->wallpaper_mode = parse_wallpaper_mode(val);

	/* None in the init: the last one SetWallpaper set (fbsetbg -l) */
	if (!config->wallpaper && config->config_dir) {
		char *last = resolve_path(config->config_dir, NULL,
			"lastwallpaper");
		FILE *f = last ? fopen_config(last, "r") : NULL;
		char line[4096], mode[64];
		if (f && fgets(line, sizeof(line), f)) {
			line[strcspn(line, "\n")] = '\0';
			if (*line) {
				config->wallpaper = strdup(line);
				config->wallpaper_from_last = true;
			}
			if (fgets(mode, sizeof(mode), f)) {
				mode[strcspn(mode, "\n")] = '\0';
				config->wallpaper_mode =
					parse_wallpaper_mode(mode);
			}
		}
		if (f)
			fclose(f);
		free(last);
	}

	val = rc_get_string(db, "session.screen0.wallpaperColor");
	if (val) {
		free(config->wallpaper_color);
		config->wallpaper_color = strdup(val);
	}

	/* Per-workspace wallpaper overrides */
	free_wallpaper_paths(config);
	if (config->workspace_count > 0) {
		config->wallpaper_paths =
			calloc(config->workspace_count, sizeof(char *));
		if (config->wallpaper_paths) {
			config->wallpaper_path_count = config->workspace_count;
			for (int i = 0; i < config->workspace_count; i++) {
				char key[128];
				snprintf(key, sizeof(key),
					"session.screen0.workspace%d.wallpaper",
					i);
				val = rc_get_string(db, key);
				if (val) {
					config->wallpaper_paths[i] =
						resolve_path(
							config->config_dir,
							val, "wallpaper.png");
				}
			}
		}
	}
}

static void
apply_rc_to_config(struct wm_config *config, struct rc_database *db)
{
	apply_workspace_config(config, db);
	apply_focus_config(config, db);
	apply_window_config(config, db);
	apply_toolbar_config(config, db);
	apply_iconbar_config(config, db);
	apply_tab_config(config, db);
	apply_slit_config(config, db);
	apply_menu_config(config, db);
	apply_xkb_config(config, db);
	apply_mouse_config(config, db);
	apply_input_config(config, db);
	apply_output_config(config, db);
	apply_misc_config(config, db);
	apply_wallpaper_config(config, db);
	apply_file_paths_config(config, db);
}

int
config_load(struct wm_config *config, const char *path)
{
	char init_path[4096];

	if (path) {
		snprintf(init_path, sizeof(init_path), "%s", path);
	} else if (config->config_dir) {
		snprintf(init_path, sizeof(init_path), "%s/init",
			 config->config_dir);
	} else {
		return -1;
	}

	if (!path_exists(init_path)) {
		/* No config file is okay - defaults will be used */
		return 0;
	}

	struct rc_database *db = rc_create();
	if (!db)
		return -1;

	if (rc_load(db, init_path) < 0) {
		rc_destroy(db);
		return -1;
	}

	apply_rc_to_config(config, db);

	rc_destroy(db);
	return 0;
}

/*
 * One resource of the init file set, as Fluxbox's SetResourceValue and
 * its saving of what its menus change do: the lines with the key (any
 * case; the spelling kept) get the value, a key that isn't there is
 * added.  The file is replaced whole.
 */
int
config_set_resource(struct wm_config *config, const char *key,
	const char *value)
{
	if (!config || !config->config_dir || !key || !*key || !value)
		return -1;
	char path[4096], tmp[4200];
	snprintf(path, sizeof(path), "%s/init", config->config_dir);
	snprintf(tmp, sizeof(tmp), "%s.tmp", path);

	FILE *in = fopen_config(path, "r");
	int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW |
		O_CLOEXEC, 0644);
	FILE *out = fd >= 0 ? fdopen(fd, "w") : NULL;
	if (!out) {
		if (fd >= 0)
			close(fd);
		if (in)
			fclose(in);
		return -1;
	}
	bool found = false;
	char line[4096];
	size_t klen = strlen(key);
	while (in && fgets(line, sizeof(line), in)) {
		const char *p = line;
		while (*p == ' ' || *p == '\t')
			p++;
		const char *colon = strchr(p, ':');
		size_t n = colon ? (size_t)(colon - p) : 0;
		while (n > 0 && (p[n - 1] == ' ' || p[n - 1] == '\t'))
			n--;
		if (colon && *p != '#' && *p != '!' && n == klen &&
		    strncasecmp(p, key, klen) == 0) {
			fprintf(out, "%.*s:\t%s\n", (int)n, p, value);
			found = true;
		} else {
			fputs(line, out);
			if (!strchr(line, '\n'))
				fputc('\n', out);
		}
	}
	if (!found)
		fprintf(out, "%s:\t%s\n", key, value);
	if (in)
		fclose(in);
	bool ok = fflush(out) == 0;
	ok &= fclose(out) == 0;
	if (ok)
		ok = rename(tmp, path) == 0;
	if (!ok) {
		unlink(tmp);
		return -1;
	}
	return 0;
}

int
config_reload(struct wm_config *config)
{
	return config_load(config, NULL);
}

bool
config_file_changed(const char *path, time_t *cached_mtime)
{
	if (!path)
		return true;

	struct stat st;
	if (stat(path, &st) != 0)
		return true; /* force reload on error */

	if (st.st_mtime != *cached_mtime) {
		*cached_mtime = st.st_mtime;
		return true;
	}

	return false;
}


void
config_destroy(struct wm_config *config)
{
	if (!config)
		return;

	free_workspace_names(config);
	free_toolbar_buttons(config);
	free_wallpaper_paths(config);
	free_input_rules(config);
	free_output_rules(config);
	for (int i = 0; i < WM_SWITCH_EVENT_COUNT; i++)
		free(config->switch_commands[i]);
	free(config->wallpaper);
	free(config->wallpaper_color);
	free(config->titlebar_left);
	free(config->titlebar_right);
	free(config->clock_format);
	free(config->xkb_rules);
	free(config->xkb_model);
	free(config->xkb_layout);
	free(config->xkb_variant);
	free(config->xkb_options);
	free(config->toolbar_tools);
	free(config->root_command);
	free(config->default_deco);
	free(config->window_menu_file);
	free(config->iconbar_iconified_pattern);
	free(config->iconbar_mode_spec);
	free(config->slitlist_file);
	free(config->config_dir);
	free(config->keys_file);
	free(config->apps_file);
	free(config->menu_file);
	free(config->style_file);
	free(config->style_overlay);
	free(config);
}
