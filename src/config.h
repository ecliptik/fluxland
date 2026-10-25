/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
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
#include <stdint.h>
#include <time.h>

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
	WM_PLACEMENT_AUTOTAB,       /* AutotabPlacement: into the focused window */
};

enum wm_toolbar_placement {
	WM_TOOLBAR_TOP_LEFT,
	WM_TOOLBAR_TOP_CENTER,
	WM_TOOLBAR_TOP_RIGHT,
	WM_TOOLBAR_BOTTOM_LEFT,
	WM_TOOLBAR_BOTTOM_CENTER,
	WM_TOOLBAR_BOTTOM_RIGHT,
	/* Up and down a side (Fluxbox's ROT270 on the left, ROT90 on the
	 * right) */
	WM_TOOLBAR_LEFT_TOP,
	WM_TOOLBAR_LEFT_CENTER,
	WM_TOOLBAR_LEFT_BOTTOM,
	WM_TOOLBAR_RIGHT_TOP,
	WM_TOOLBAR_RIGHT_CENTER,
	WM_TOOLBAR_RIGHT_BOTTOM,
};

enum wm_menu_search {
	WM_MENU_SEARCH_NOWHERE,    /* disabled (default) */
	WM_MENU_SEARCH_ITEMSTART,  /* match from start of item label */
	WM_MENU_SEARCH_SOMEWHERE,  /* match anywhere in item label */
};

/* Window snap zones for edge/corner snapping */
enum wm_workspace_mode {
	WM_WORKSPACE_GLOBAL,       /* all outputs share one active workspace (default) */
	WM_WORKSPACE_PER_OUTPUT,   /* each output independently selects its workspace */
};

enum wm_snap_zone {
	WM_SNAP_ZONE_NONE = 0,
	WM_SNAP_ZONE_LEFT_HALF,
	WM_SNAP_ZONE_RIGHT_HALF,
	WM_SNAP_ZONE_TOP_HALF,
	WM_SNAP_ZONE_BOTTOM_HALF,
	WM_SNAP_ZONE_TOPLEFT,
	WM_SNAP_ZONE_TOPRIGHT,
	WM_SNAP_ZONE_BOTTOMLEFT,
	WM_SNAP_ZONE_BOTTOMRIGHT,
};

/* Workspace transition type */
enum wm_workspace_transition {
	WM_TRANSITION_NONE,      /* instant switch (default) */
	WM_TRANSITION_FADE,      /* cross-fade between workspaces */
	WM_TRANSITION_SLIDE,     /* horizontal slide animation */
};

/* Wallpaper display mode */
enum wm_wallpaper_mode {
	WM_WALLPAPER_STRETCH,    /* stretch to fill (default) */
	WM_WALLPAPER_CENTER,     /* center at original size */
	WM_WALLPAPER_TILE,       /* tile to fill */
	WM_WALLPAPER_FILL,       /* scale to fill, maintain aspect ratio */
	WM_WALLPAPER_FIT,        /* scale to fit inside (fbsetbg -a) */
};

/*
 * libinput settings for one scope (init keys, see fluxland-init(5)):
 *   session.input.<key>                  every device
 *   session.input.touchpad.<key>         touchpads
 *   session.input.pointer.<key>          other pointers (mice, trackballs)
 *   session.input.device.<Name>.<key>    one device; non-alphanumeric
 *                                        characters in its name become _
 * More specific scopes win. -1 / NaN means "leave libinput's default".
 */
enum wm_input_scope {
	WM_INPUT_SCOPE_ALL,
	WM_INPUT_SCOPE_TOUCHPAD,
	WM_INPUT_SCOPE_POINTER,
	WM_INPUT_SCOPE_DEVICE,
};

struct wm_input_rule {
	enum wm_input_scope scope;
	char *device;              /* WM_INPUT_SCOPE_DEVICE: sanitized name */
	int tap;                   /* bool */
	int tap_drag;              /* bool */
	int tap_drag_lock;         /* bool */
	int tap_button_map;        /* 0 lrm, 1 lmr */
	int natural_scroll;        /* bool */
	int left_handed;           /* bool */
	int disable_while_typing;  /* bool */
	int middle_emulation;      /* bool */
	int accel_profile;         /* 0 flat, 1 adaptive */
	double accel_speed;        /* -1.0 .. 1.0 */
	int scroll_method;         /* 0 none, 1 twofinger, 2 edge, 3 button */
	int scroll_button;         /* linux button code for scrollMethod button */
	int click_method;          /* 0 none, 1 buttonareas, 2 clickfinger */
	int send_events;           /* 0 enabled, 1 disabled, 2 off w/ ext mouse */
};

/*
 * Output (monitor) settings: session.output.<Connector>.<setting>, where
 * <Connector> is the output name (eDP-1, HDMI-A-1, DP-2, ...). Unset
 * fields keep the output's current/preferred state.
 */
struct wm_output_rule {
	char *name;
	int enabled;        /* -1 unset, else bool */
	int width, height;  /* mode; 0 = unset */
	int refresh_mhz;    /* 0 = highest available for the size */
	double scale;       /* NaN = unset */
	bool has_position;
	int x, y;           /* layout position */
	int transform;      /* -1 unset, else enum wl_output_transform */
	int adaptive_sync;  /* -1 unset, else bool */
};

/* Switch events that can run a command (see switch_commands) */
enum wm_switch_event {
	WM_SWITCH_LID_CLOSE,
	WM_SWITCH_LID_OPEN,
	WM_SWITCH_TABLET_MODE_ON,
	WM_SWITCH_TABLET_MODE_OFF,
	WM_SWITCH_EVENT_COUNT,
};

/* session.screen0.tab.placement, in Fluxbox's order */
enum wm_tab_placement {
	WM_TAB_TOP_LEFT,
	WM_TAB_TOP,
	WM_TAB_TOP_RIGHT,
	WM_TAB_BOTTOM_LEFT,
	WM_TAB_BOTTOM,
	WM_TAB_BOTTOM_RIGHT,
	WM_TAB_LEFT_TOP,
	WM_TAB_LEFT,
	WM_TAB_LEFT_BOTTOM,
	WM_TAB_RIGHT_TOP,
	WM_TAB_RIGHT,
	WM_TAB_RIGHT_BOTTOM,
};

struct wm_config {
	/* Workspaces */
	int workspace_count;
	char **workspace_names;
	int workspace_name_count;
	bool workspace_warping;
	/* Fluxbox's workspacewarping{horizontal,vertical}[offset] */
	bool workspace_warping_horizontal;
	bool workspace_warping_vertical;
	int workspace_warping_horizontal_offset;
	int workspace_warping_vertical_offset;
	enum wm_workspace_mode workspace_mode;

	/* Focus */
	enum wm_focus_policy focus_policy;
	bool raise_on_focus;
	int auto_raise_delay_ms;
	bool click_raises;
	bool focus_new_windows;
	bool focus_same_head;            /* focus reverts on the pointer's head */
	int no_focus_while_typing_ms;    /* noFocusWhileTypingDelay */
	int demands_attention_ms;        /* demandsAttentionTimeout: flashes */
	bool ignore_border;              /* session.ignoreBorder */
	bool tabs_attach_titlebar;       /* session.tabsAttachArea: Titlebar */

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
	bool toolbar_auto_raise;
	int toolbar_auto_hide_delay_ms;
	int toolbar_width_percent;
	int toolbar_height;
	int toolbar_layer;
	int toolbar_alpha;
	int toolbar_on_head;
	char *toolbar_tools; /* configurable tool layout string */
	/* toolbar.button.NAME.label and .commands (a command for each
	 * mouse button, separated by ':') */
	struct wm_toolbar_button_cfg {
		char *name, *label, *commands;
	} *toolbar_buttons;
	int toolbar_button_count;

	/* Titlebar button layout */
	char *titlebar_left;
	char *titlebar_right;

	/* Clock format */
	char *clock_format;

	/* Iconbar mode: 0=Workspace, 1=AllWindows, 2=Icons,
	 * 3=NoIcons, 4=WorkspaceIcons */
	int iconbar_mode;
	/* Which windows: a window list ({static groups} (workspace)) */
	char *iconbar_mode_spec;

	/* Iconbar enhancements */
	int iconbar_alignment;              /* 0=left, 1=relative (default), 2=right, 3=center */
	int iconbar_icon_width;             /* entry width unless relative (0 = 128) */
	int iconbar_icon_text_padding;      /* iconTextPadding: around the title */
	int tooltip_delay_ms;               /* tooltipDelay: -1 none, 0 at once */
	bool iconbar_use_pixmap;            /* show app icon pixmaps (default true) */
	int iconbar_wheel_mode;             /* 0 = screen (ws change), 1 = off */
	char *iconbar_iconified_pattern;    /* e.g. "(%s)" for iconified windows */

	/* Auto-tab placement: group windows with same app_id */
	bool auto_tab_placement;

	/* Tab focus model: 0 = click (default), 1 = mouse hover */
	int tab_focus_model;

	/* External tab bar configuration */
	bool tabs_intitlebar;       /* true = tabs in titlebar (default), false = external */
	bool tabs_max_over;         /* tabs.maxOver: maximized over external tabs */
	bool tabs_use_pixmap;       /* tabs.usePixmap: window icons in tabs */
	bool clientmenu_use_pixmap; /* clientMenu.usePixmap: icons in window lists */
	char *systray_pin_left;     /* systray.pinLeft: ids, the first leftmost */
	char *systray_pin_right;    /* systray.pinRight */
	int tab_placement;          /* enum wm_tab_placement */
	int tab_width;              /* an external tab's length (64) */
	int tab_padding;            /* padding between tabs in pixels */

	/* Full maximization (maximize over toolbar/slit) */
	bool full_maximization;
	bool max_disable_move;     /* maximized windows can't be moved */
	bool max_disable_resize;   /* ...or resized */

	/* Window opacity (0-255) */
	int window_focus_alpha;
	int window_unfocus_alpha;

	/* Window snap zones (edge/corner snapping during move) */
	bool enable_window_snapping;
	int snap_zone_threshold;  /* pixels from edge to trigger snap */

	/* Window animations */
	bool animate_window_map;
	bool animate_window_unmap;
	bool animate_minimize;
	int animation_duration_ms;

	/* Workspace transitions */
	enum wm_workspace_transition workspace_transition;
	int workspace_transition_duration_ms;

	/* Slit configuration */
	bool slit_auto_hide;
	bool slit_auto_raise;
	int slit_placement;
	int slit_direction;
	int slit_layer;
	int slit_on_head;
	int slit_alpha;       /* 0-255, default 255 */
	bool slit_max_over;   /* default false */
	char *slitlist_file;  /* path to slitlist file */

	/* XKB keyboard layout */
	char *xkb_rules;    /* e.g., "evdev" */
	char *xkb_model;    /* e.g., "pc105" */
	char *xkb_layout;   /* e.g., "us,de" (comma-separated for multi) */
	char *xkb_variant;  /* e.g., ",nodeadkeys" */
	char *xkb_options;  /* e.g., "grp:alt_shift_toggle,caps:escape" */

	/* libinput settings, in file order */
	struct wm_input_rule *input_rules;
	int input_rule_count;

	/* Output settings, one per connector */
	struct wm_output_rule *output_rules;
	int output_rule_count;

	/* Double-click interval (ms) */
	int double_click_interval;

	/* Root command (run on startup after autostart) */
	char *root_command;
	/*
	 * session.updateActivationEnvironment: export WAYLAND_DISPLAY etc.
	 * to D-Bus/systemd user services. -1 auto (not when nested or
	 * headless), 0 never, 1 always.
	 */
	int update_activation_env;

	/*
	 * Commands run when a switch flips (session.switch.lidClose,
	 * lidOpen, tabletModeOn, tabletModeOff); NULL = nothing.
	 */
	char *switch_commands[WM_SWITCH_EVENT_COUNT];

	/* Menu alpha (0-255) */
	int menu_alpha;

	/* Toolbar maximize-over (allow maximize over toolbar) */
	bool toolbar_max_over;

	/* Edge resize snap threshold (pixels) */
	int edge_resize_snap_threshold;

	/* Default decoration preset name */
	char *default_deco;

	/* Manual struts [left, right, top, bottom] in pixels: struts for
	 * every head, head_struts[n - 1] for head n (struts.n) */
	int struts[4];
#define WM_STRUT_HEADS 8
	int head_struts[WM_STRUT_HEADS][4];

	/* Show window position/size during move/resize */
	bool show_window_position;

	/* Menu type-ahead search */
	enum wm_menu_search menu_search;

	/* Submenu hover-open delay (ms) */
	int menu_delay;

	/* Custom window menu file (NULL = use default) */
	char *window_menu_file;

	/* Mouse button remapping (index 0 unused, 1-5 = button mappings) */
	uint32_t mouse_button_map[6];

	/* Per-workspace wallpaper */
	char *wallpaper;            /* default wallpaper image path */
	enum wm_wallpaper_mode wallpaper_mode;
	bool wallpaper_from_last;	/* the lastwallpaper file's, not the init's */
	char *wallpaper_color;      /* fallback solid color (#RRGGBB) */
	char **wallpaper_paths;     /* per-workspace overrides (indexed by ws) */
	int wallpaper_path_count;

	/* Touchpad gesture settings */
	bool gesture_workspace_switch;     /* default: true */
	bool gesture_overview;             /* default: false */
	int gesture_workspace_fingers;     /* default: 3 */
	double gesture_swipe_threshold;    /* default: 100.0 pixels */

	/* Paths */
	char *config_dir;
	char *keys_file;
	char *apps_file;
	char *menu_file;
	char *style_file;
	char *style_overlay;

	/* Cached mtimes for conditional reload */
	time_t keys_file_mtime;
	time_t style_file_mtime;
	time_t style_overlay_mtime;
	time_t menu_file_mtime;
	time_t apps_file_mtime;
};

/* Create a config struct with sensible defaults */
struct wm_config *config_create(void);

/* Load configuration from init file. Returns 0 on success, -1 on error.
 * If path is NULL, searches standard locations for init file. */
int config_load(struct wm_config *config, const char *path);

/* Set a resource of the init file (added when it isn't there); 0, or -1
 * when the file can't be written */
int config_set_resource(struct wm_config *config, const char *key,
	const char *value);

/* Re-read the configuration file */
int config_reload(struct wm_config *config);

/* Free all resources */
void config_destroy(struct wm_config *config);

/* The session.output.<name>.* settings for an output, or NULL */
const struct wm_output_rule *config_find_output_rule(
	const struct wm_config *config, const char *name);

/* Parse "WxH[@Hz]" (refresh in mHz, 0 if not given) */
bool config_parse_output_mode(const char *val, int *w, int *h, int *mhz);

/* "normal", "90", ..., "flipped-270" -> wl_output_transform, or -1 */
int config_parse_output_transform(const char *val);

/* Check if a config file's mtime has changed since last check.
 * Updates *cached_mtime on change. Returns true if changed or on error. */
bool config_file_changed(const char *path, time_t *cached_mtime);

#endif /* WM_CONFIG_H */
