/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
 *
 * config.h - Configuration struct and loading
 *
 * Loads Fluxbox-compatible init files using X resource database format.
 * Search order:
 *   1. $WM_WAYLAND_CONFIG_DIR/
 *   2. $XDG_CONFIG_HOME/wm-wayland/
 *   3. ~/.config/wm-wayland/
 *   4. ~/.fluxbox/ (Fluxbox compat fallback)
 */

#ifndef WM_CONFIG_H
#define WM_CONFIG_H

#include <stdbool.h>

enum wm_focus_policy {
	WM_FOCUS_CLICK,
	WM_FOCUS_SLOPPY,
	WM_FOCUS_STRICT_MOUSE,
};

enum wm_placement_policy {
	WM_PLACEMENT_ROW_SMART,
	WM_PLACEMENT_COL_SMART,
	WM_PLACEMENT_CASCADE,
	WM_PLACEMENT_UNDER_MOUSE,
};

struct wm_config {
	/* Workspaces */
	int workspace_count;
	char **workspace_names;
	int workspace_name_count;
	bool workspace_warping;

	/* Focus */
	enum wm_focus_policy focus_policy;
	bool raise_on_focus;
	int auto_raise_delay_ms;
	bool click_raises;
	bool focus_new_windows;

	/* Window behavior */
	bool opaque_move;
	bool opaque_resize;
	int edge_snap_threshold;

	/* Placement */
	enum wm_placement_policy placement_policy;

	/* Toolbar (basic for now) */
	bool toolbar_visible;

	/* XKB keyboard layout */
	char *xkb_rules;    /* e.g., "evdev" */
	char *xkb_model;    /* e.g., "pc105" */
	char *xkb_layout;   /* e.g., "us,de" (comma-separated for multi) */
	char *xkb_variant;  /* e.g., ",nodeadkeys" */
	char *xkb_options;  /* e.g., "grp:alt_shift_toggle,caps:escape" */

	/* Paths */
	char *config_dir;
	char *keys_file;
	char *apps_file;
	char *menu_file;
	char *style_file;
};

/* Create a config struct with sensible defaults */
struct wm_config *config_create(void);

/* Load configuration from init file. Returns 0 on success, -1 on error.
 * If path is NULL, searches standard locations for init file. */
int config_load(struct wm_config *config, const char *path);

/* Re-read the configuration file */
int config_reload(struct wm_config *config);

/* Free all resources */
void config_destroy(struct wm_config *config);

#endif /* WM_CONFIG_H */
