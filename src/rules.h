/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * rules.h - Per-window rules (Fluxbox apps file)
 *
 * Reads Fluxbox's apps file as Fluxbox's Remember does: [app] and
 * [transient] entries with client patterns, [group] entries whose
 * windows tab together, and [startup] commands.
 */

#ifndef WM_RULES_H
#define WM_RULES_H

#include <stdbool.h>
#include <wayland-server-core.h>

#include "decoration.h"
#include "fbcompat.h"

struct wm_view;
struct wm_server;
struct wm_client_pattern;

enum wm_rule_maximize {
	WM_RULE_MAX_NONE,   /* [Maximized] {no} */
	WM_RULE_MAX_FULL,   /* {yes} */
	WM_RULE_MAX_HORZ,   /* {horz} */
	WM_RULE_MAX_VERT,   /* {vert} */
};

/*
 * What to do with a window (Fluxbox's Application). Every [app] pattern
 * of a [group] shares one.
 */
struct wm_rule_app {
	bool transient;   /* [transient]: matches dialogs only */
	bool grouped;     /* [group]: its windows tab together */
	/* [group] (pattern): which open window of the group to join */
	struct wm_client_pattern *group_pattern;

	/* Settings; each has a "has_" flag saying whether it was given */
	bool has_workspace;     int workspace;
	bool has_jump;          bool jump;
	bool has_head;          int head;
	bool has_dimensions;    struct wm_size_token width, height;
	bool has_position;      struct wm_size_token pos_x, pos_y;
	enum wm_corner pos_corner;
	bool has_deco;          enum wm_decor_preset deco;
	bool has_sticky;        bool sticky;
	bool has_layer;         enum wm_view_layer layer;
	bool has_alpha;         int focus_alpha, unfocus_alpha;
	bool has_maximized;     enum wm_rule_maximize maximized;
	bool has_minimized;     bool minimized;
	bool has_fullscreen;    bool fullscreen;
	bool has_shaded;        bool shaded;
	bool has_focus_protection; int focus_protection;
	bool has_focus_hidden;  bool focus_hidden;
	bool has_icon_hidden;   bool icon_hidden;
	bool has_ignore_size_hints; bool ignore_size_hints;
	bool has_close;         bool save_on_close;  /* [Close] */
	bool has_dock;          bool dock; /* fluxland: into the slit */

	struct wl_list link;    /* wm_rules.apps */
};

/* One [app] pattern and the settings it selects */
struct wm_window_rule {
	struct wm_client_pattern *pattern;
	struct wm_rule_app *app;
	int matches;            /* open windows it matched, for {N} */
	struct wl_list link;    /* wm_rules.window_rules */
};

struct wm_rules {
	struct wl_list window_rules;  /* wm_window_rule.link, file order */
	struct wl_list apps;          /* wm_rule_app.link */
	char **startup;               /* [startup] commands */
	int startup_count;
	/* Optional: told about each problem in the file (--check-config) */
	void (*report)(void *data, const char *path, int line, bool fatal,
		const char *msg);
	void *report_data;
};

/* rules_parse.c */
void wm_rules_init(struct wm_rules *rules);
void wm_rules_finish(struct wm_rules *rules);

/* Read an apps file (adds to rules); false if it can't be read */
bool wm_rules_load(struct wm_rules *rules, const char *path);

/*
 * The rule for a window: the one it matched before, or the first that
 * matches it now (which then counts the window against its {N}).
 */
struct wm_window_rule *wm_rules_match(struct wm_rules *rules,
	struct wm_view *view);

/* Apply the window's rule (at map, or when its app_id arrives late) */
void wm_rules_apply(struct wm_rules *rules, struct wm_view *view);

/* The window has closed: uncount it; [Close] {yes} saves its state */
void wm_rules_release(struct wm_rules *rules, struct wm_view *view,
	const char *apps_path);

/* Before rules are reloaded: windows forget the rules they matched */
void wm_rules_forget_views(struct wm_server *server);

/* A [group] window to tab a new window with, or NULL */
struct wm_view *wm_rules_find_group(struct wm_rules *rules,
	struct wm_view *view, struct wm_server *server);

/* Should this window go into the slit? ([Dock] {yes}) */
bool wm_rules_should_dock(struct wm_rules *rules, struct wm_view *view);

/* Write the window's current state as an [app] entry (Remember) */
int wm_rules_remember_window(struct wm_view *view, const char *apps_path);

/* Fluxbox's Remember menu, one attribute at a time (Remember.cc) */
enum wm_remember_attr {
	WM_REM_WORKSPACE,
	WM_REM_JUMP,
	WM_REM_HEAD,
	WM_REM_DIMENSIONS,
	WM_REM_POSITION,
	WM_REM_STICKY,
	WM_REM_DECO,
	WM_REM_SHADED,
	WM_REM_MINIMIZED,
	WM_REM_MAXIMIZED,
	WM_REM_FULLSCREEN,
	WM_REM_ALPHA,
	WM_REM_LAYER,
	WM_REM_CLOSE,
	WM_REM_COUNT,
};

/* Does the window's entry in the apps file have the attribute? */
bool wm_rules_is_remembered(struct wm_view *view, const char *apps_path,
	enum wm_remember_attr attr);

/* Remember the attribute as it is now, or forget it when remembered;
 * written to the apps file (an entry left empty goes). 0, or -1. */
int wm_rules_toggle_remembered(struct wm_view *view, const char *apps_path,
	enum wm_remember_attr attr);

#endif /* WM_RULES_H */
