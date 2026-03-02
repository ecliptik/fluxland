/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * rules.h - Per-window rules and auto-grouping (Fluxbox apps file)
 *
 * Parses the Fluxbox-compatible apps file to define per-window rules
 * (workspace, geometry, decoration preset, etc.) and auto-grouping rules
 * for tab grouping.
 */

#ifndef WM_RULES_H
#define WM_RULES_H

#include <regex.h>
#include <stdbool.h>
#include <wayland-server-core.h>

#include "decoration.h"

struct wm_view;
struct wm_server;

/* Focus protection flags (bitmask) */
#define WM_FOCUS_PROT_GAIN   0x01
#define WM_FOCUS_PROT_REFUSE 0x02
#define WM_FOCUS_PROT_DENY   0x04
#define WM_FOCUS_PROT_LOCK   (WM_FOCUS_PROT_REFUSE | WM_FOCUS_PROT_DENY)

/* --- Match pattern (single property=regex clause) --- */

struct wm_match_pattern {
	char *property;    /* "class", "name", "title", "role", "transient" */
	char *regex_str;   /* raw regex string for diagnostics / serialization */
	regex_t compiled;  /* compiled POSIX regex */
	bool negate;       /* true for != (negated match) */
};

/* --- Window rule (one [app] ... [end] block) --- */

struct wm_window_rule {
	/* Match criteria (AND-combined) */
	struct wm_match_pattern *patterns;
	int pattern_count;

	/* Actions — each has a "has_" flag to indicate whether it was set */
	bool has_workspace;     int workspace;
	bool has_dimensions;    int width, height;
	bool has_position;      int pos_x, pos_y;
	char *pos_anchor;       /* "Center", "UpperLeft", etc. (NULL = default) */
	bool has_deco;          enum wm_decor_preset deco;
	bool has_sticky;        bool sticky;
	bool has_layer;         int layer;
	bool has_alpha;         int focus_alpha, unfocus_alpha;
	bool has_maximized;     bool maximized;
	bool has_minimized;     bool minimized;
	bool has_fullscreen;    bool fullscreen;
	bool has_shaded;        bool shaded;
	bool has_focus_new;     bool focus_new;
	bool has_head;          int head;
	bool has_focus_protection; int focus_protection;
	bool has_ignore_size_hints; bool ignore_size_hints;
	bool has_dock;          bool dock; /* route to slit instead of workspace */

	struct wl_list link;    /* wm_rules.window_rules */
};

/* --- Group rule (one [group] ... [end] block) --- */

struct wm_group_rule {
	struct wm_match_pattern *patterns;  /* OR-combined: any match joins group */
	int pattern_count;
	bool workspace_scope;  /* if true, only group on same workspace */

	struct wl_list link;   /* wm_rules.group_rules */
};

/* --- Top-level rules container --- */

struct wm_rules {
	struct wl_list window_rules;  /* wm_window_rule.link */
	struct wl_list group_rules;   /* wm_group_rule.link */
};

/*
 * Load rules from a Fluxbox-compatible apps file.
 * Returns true on success. On error, logs a warning and returns false.
 * The rules struct should be initialized with wm_rules_init() first.
 */
bool wm_rules_load(struct wm_rules *rules, const char *path);

/*
 * Initialize rules lists (call before wm_rules_load).
 */
void wm_rules_init(struct wm_rules *rules);

/*
 * Free all rules and reset lists.
 */
void wm_rules_finish(struct wm_rules *rules);

/*
 * Apply the first matching window rule to a newly-mapped view.
 * Sets workspace, geometry, decoration preset, state flags, etc.
 * Should be called after the view is mapped, before initial focus.
 */
void wm_rules_apply(struct wm_rules *rules, struct wm_view *view);

/*
 * Find an existing view that matches the same group rule as `view`.
 * Returns a view to tab with, or NULL if no group rule matches.
 * The caller should use wm_tab_group_add() to group the views.
 */
struct wm_view *wm_rules_find_group(struct wm_rules *rules,
	struct wm_view *view, struct wm_server *server);

/*
 * Check whether a newly-mapped view should be routed to the slit (dockapp).
 * Returns true if a matching window rule has [Dock] {yes}.
 */
bool wm_rules_should_dock(struct wm_rules *rules, struct wm_view *view);

/*
 * Save the current window properties to the apps file.
 * Appends a new [app] (class=APP_ID) block with the view's current
 * workspace, position, dimensions, layer, alpha, sticky, maximized,
 * fullscreen, and decoration state.
 * Returns 0 on success, -1 on error.
 */
int wm_rules_remember_window(struct wm_view *view, const char *apps_path);

#endif /* WM_RULES_H */
