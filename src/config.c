/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * config.c - Configuration loading and management
 */

#define _POSIX_C_SOURCE 200809L

#include "config.h"
#include "i18n.h"
#include "rcparser.h"
#include <linux/input-event-codes.h>
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

static int
parse_slit_layer(const char *value)
{
	if (!value)
		return 4; /* Dock (Normal/Top) */
	if (strcasecmp(value, "AboveDock") == 0 ||
	    strcasecmp(value, "Above") == 0)
		return 6; /* Overlay */
	if (strcasecmp(value, "Bottom") == 0)
		return 2; /* Bottom */
	/* "Normal" or "Dock" */
	if (strcasecmp(value, "Normal") == 0 ||
	    strcasecmp(value, "Dock") == 0)
		return 4;
	/* Try numeric */
	char *end;
	long n = strtol(value, &end, 10);
	if (end != value && *end == '\0')
		return (int)n;
	return 4; /* Dock (Top layer) */
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
	config->workspace_mode = WM_WORKSPACE_GLOBAL;
	config->focus_policy = WM_FOCUS_CLICK;
	config->raise_on_focus = false;
	config->auto_raise_delay_ms = 250;
	config->click_raises = true;
	config->focus_new_windows = true;
	config->opaque_move = true;
	config->opaque_resize = false;
	config->edge_snap_threshold = 10;
	config->placement_policy = WM_PLACEMENT_ROW_SMART;
	config->toolbar_visible = true;
	config->toolbar_placement = WM_TOOLBAR_BOTTOM_CENTER;
	config->toolbar_auto_hide = false;
	config->toolbar_auto_hide_delay_ms = 500;
	config->toolbar_width_percent = 72;
	config->toolbar_height = 0;
	config->toolbar_layer = 4; /* Dock */
	config->toolbar_alpha = 255;
	config->toolbar_on_head = 0;
	config->toolbar_tools = strdup(
		"workspacename prevworkspace nextworkspace iconbar clock");

	config->titlebar_left = strdup("Stick");
	config->titlebar_right = strdup("Shade Minimize Maximize Close");
	config->clock_format = strdup("%H:%M");
	config->iconbar_mode = 0;
	config->iconbar_alignment = 1; /* center */
	config->iconbar_icon_width = 0; /* auto */
	config->iconbar_use_pixmap = true;
	config->iconbar_wheel_mode = 0; /* screen (workspace change) */
	config->auto_tab_placement = false;
	config->tab_focus_model = 0; /* click */
	config->tabs_intitlebar = true;
	config->tab_placement = 0; /* Top */
	config->tab_width = 0; /* auto from font */
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
	config->menu_search = WM_MENU_SEARCH_NOWHERE;
	config->menu_delay = 200;

	/* Default mouse button mappings (identity map) */
	config->mouse_button_map[0] = 0;
	config->mouse_button_map[1] = BTN_LEFT;
	config->mouse_button_map[2] = BTN_MIDDLE;
	config->mouse_button_map[3] = BTN_RIGHT;
	config->mouse_button_map[4] = BTN_SIDE;
	config->mouse_button_map[5] = BTN_EXTRA;

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
		rc_get_bool(db, "session.screen0.autoRaise", false);
	config->auto_raise_delay_ms =
		rc_get_int(db, "session.autoRaiseDelay", 250);
	config->click_raises =
		rc_get_bool(db, "session.screen0.clickRaises", true);
	config->focus_new_windows =
		rc_get_bool(db, "session.screen0.focusNewWindows", true);
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
apply_toolbar_config(struct wm_config *config, struct rc_database *db)
{
	const char *val;

	config->toolbar_visible =
		rc_get_bool(db, "session.screen0.toolbar.visible", true);

	val = rc_get_string(db, "session.screen0.toolbar.placement");
	config->toolbar_placement = parse_toolbar_placement(val);

	config->toolbar_auto_hide =
		rc_get_bool(db, "session.screen0.toolbar.autoHide", false);
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
	config->toolbar_layer =
		rc_get_int(db, "session.screen0.toolbar.layer", 4);
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

	/* Titlebar button layout */
	val = rc_get_string(db, "session.titlebar.left");
	if (val) {
		free(config->titlebar_left);
		config->titlebar_left = strdup(val);
	}
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

	/* Iconbar alignment */
	val = rc_get_string(db, "session.screen0.iconbar.alignment");
	if (val) {
		if (strcasecmp(val, "Left") == 0)
			config->iconbar_alignment = 0;
		else if (strcasecmp(val, "Right") == 0)
			config->iconbar_alignment = 2;
		else
			config->iconbar_alignment = 1; /* Center */
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
	val = rc_get_string(db, "session.screen0.tab.placement");
	if (val) {
		if (strcasecmp(val, "Bottom") == 0)
			config->tab_placement = 1;
		else if (strcasecmp(val, "Left") == 0)
			config->tab_placement = 2;
		else if (strcasecmp(val, "Right") == 0)
			config->tab_placement = 3;
		else
			config->tab_placement = 0; /* Top */
	}
	config->tab_width =
		rc_get_int(db, "session.screen0.tab.width", 0);
	if (config->tab_width < 0)
		config->tab_width = 0;
	config->tab_padding =
		rc_get_int(db, "session.screen0.tabPadding", 0);
	if (config->tab_padding < 0)
		config->tab_padding = 0;
}

static void
apply_slit_config(struct wm_config *config, struct rc_database *db)
{
	const char *val;

	config->slit_auto_hide =
		rc_get_bool(db, "session.screen0.slit.autoHide", false);
	val = rc_get_string(db, "session.screen0.slit.placement");
	config->slit_placement = parse_slit_placement(val);
	val = rc_get_string(db, "session.screen0.slit.direction");
	config->slit_direction = parse_slit_direction(val);
	val = rc_get_string(db, "session.screen0.slit.layer");
	if (val)
		config->slit_layer = parse_slit_layer(val);
	else
		config->slit_layer =
			rc_get_int(db, "session.screen0.slit.layer", 4);
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

	/* Type-ahead search */
	val = rc_get_string(db, "session.menuSearch");
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

	/* Custom window menu file */
	val = rc_get_string(db, "session.screen0.windowMenu");
	if (val) {
		free(config->window_menu_file);
		config->window_menu_file = strdup(val);
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
		BTN_LEFT, BTN_MIDDLE, BTN_RIGHT, BTN_SIDE, BTN_EXTRA,
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
		rc_get_int(db, "session.doubleClickInterval", 300);
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

	/* Manual struts [left right top bottom] */
	val = rc_get_string(db, "session.screen0.struts");
	if (val) {
		sscanf(val, "%d %d %d %d",
			&config->struts[0], &config->struts[1],
			&config->struts[2], &config->struts[3]);
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
	free_wallpaper_paths(config);
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
	free(config->slitlist_file);
	free(config->config_dir);
	free(config->keys_file);
	free(config->apps_file);
	free(config->menu_file);
	free(config->style_file);
	free(config->style_overlay);
	free(config);
}
