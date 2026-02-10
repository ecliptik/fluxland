/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 *
 * config.c - Configuration loading and management
 */

#define _POSIX_C_SOURCE 200809L

#include "config.h"
#include "rcparser.h"
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
		snprintf(buf, sizeof(buf), "Workspace %d", i + 1);
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
	return WM_PLACEMENT_ROW_SMART;
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
 * Find the config directory, searching in order:
 *   1. $FLUXLAND_CONFIG_DIR
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
	if (env && dir_exists(env))
		return strdup(env);

	env = getenv("XDG_CONFIG_HOME");
	if (env) {
		snprintf(path, sizeof(path), "%s/fluxland", env);
		if (dir_exists(path))
			return strdup(path);
	}

	const char *home = getenv("HOME");
	if (!home)
		return NULL;

	snprintf(path, sizeof(path), "%s/.config/fluxland", home);
	if (dir_exists(path))
		return strdup(path);

	snprintf(path, sizeof(path), "%s/.fluxbox", home);
	if (dir_exists(path))
		return strdup(path);

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
	config->toolbar_width_percent = 100;

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
	}

	return config;
}

static void
apply_rc_to_config(struct wm_config *config, struct rc_database *db)
{
	const char *val;

	/* Workspaces */
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

	/* Focus */
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

	/* Window behavior */
	config->opaque_move =
		rc_get_bool(db, "session.screen0.opaqueMove", true);
	config->opaque_resize =
		rc_get_bool(db, "session.screen0.opaqueResize", false);
	config->edge_snap_threshold =
		rc_get_int(db, "session.screen0.edgeSnapThreshold", 10);

	/* Placement */
	val = rc_get_string(db, "session.screen0.windowPlacement");
	config->placement_policy = parse_placement(val);

	/* Toolbar */
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

	/* XKB keyboard layout */
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

	/* File paths */
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

void
config_destroy(struct wm_config *config)
{
	if (!config)
		return;

	free_workspace_names(config);
	free(config->xkb_rules);
	free(config->xkb_model);
	free(config->xkb_layout);
	free(config->xkb_variant);
	free(config->xkb_options);
	free(config->config_dir);
	free(config->keys_file);
	free(config->apps_file);
	free(config->menu_file);
	free(config->style_file);
	free(config);
}
