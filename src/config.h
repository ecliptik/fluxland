/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 *
 * config.h - Configuration struct and loading
 *
 * Loads Fluxbox-compatible init files using X resource database format.
 * Search order:
 *   1. $FLUXLAND_CONFIG_DIR/
 *   2. $XDG_CONFIG_HOME/fluxland/
 *   3. ~/.config/fluxland/
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
	WM_PLACEMENT_ROW_MIN_OVERLAP,
	WM_PLACEMENT_COL_MIN_OVERLAP,
};

enum wm_toolbar_placement {
	WM_TOOLBAR_TOP_LEFT,
	WM_TOOLBAR_TOP_CENTER,
	WM_TOOLBAR_TOP_RIGHT,
	WM_TOOLBAR_BOTTOM_LEFT,
	WM_TOOLBAR_BOTTOM_CENTER,
	WM_TOOLBAR_BOTTOM_RIGHT,
};

enum wm_menu_search {
	WM_MENU_SEARCH_NOWHERE,    /* disabled (default) */
	WM_MENU_SEARCH_ITEMSTART,  /* match from start of item label */
	WM_MENU_SEARCH_SOMEWHERE,  /* match anywhere in item label */
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
	bool row_right_to_left;   /* false = LeftToRight (default) */
	bool col_bottom_to_top;   /* false = TopToBottom (default) */

	/* Toolbar */
	bool toolbar_visible;
	enum wm_toolbar_placement toolbar_placement;
	bool toolbar_auto_hide;
	int toolbar_auto_hide_delay_ms;
	int toolbar_width_percent;
	int toolbar_height;
	int toolbar_layer;
	int toolbar_alpha;
	int toolbar_on_head;

	/* Titlebar button layout */
	char *titlebar_left;
	char *titlebar_right;

	/* Clock format */
	char *clock_format;

	/* Iconbar mode: 0=Workspace, 1=AllWindows, 2=Icons,
	 * 3=NoIcons, 4=WorkspaceIcons */
	int iconbar_mode;

	/* Full maximization (maximize over toolbar/slit) */
	bool full_maximization;

	/* Window opacity (0-255) */
	int window_focus_alpha;
	int window_unfocus_alpha;

	/* Slit configuration */
	bool slit_auto_hide;
	int slit_placement;
	int slit_direction;
	int slit_layer;
	int slit_on_head;

	/* XKB keyboard layout */
	char *xkb_rules;    /* e.g., "evdev" */
	char *xkb_model;    /* e.g., "pc105" */
	char *xkb_layout;   /* e.g., "us,de" (comma-separated for multi) */
	char *xkb_variant;  /* e.g., ",nodeadkeys" */
	char *xkb_options;  /* e.g., "grp:alt_shift_toggle,caps:escape" */

	/* Double-click interval (ms) */
	int double_click_interval;

	/* Root command (run on startup after autostart) */
	char *root_command;

	/* Menu alpha (0-255) */
	int menu_alpha;

	/* Toolbar maximize-over (allow maximize over toolbar) */
	bool toolbar_max_over;

	/* Edge resize snap threshold (pixels) */
	int edge_resize_snap_threshold;

	/* Default decoration preset name */
	char *default_deco;

	/* Manual struts [left, right, top, bottom] in pixels */
	int struts[4];

	/* Menu type-ahead search */
	enum wm_menu_search menu_search;

	/* Submenu hover-open delay (ms) */
	int menu_delay;

	/* Custom window menu file (NULL = use default) */
	char *window_menu_file;

	/* Paths */
	char *config_dir;
	char *keys_file;
	char *apps_file;
	char *menu_file;
	char *style_file;
	char *style_overlay;
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
