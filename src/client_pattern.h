/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * client_pattern.h - Fluxbox client patterns
 *
 * A client pattern selects windows, as used by the apps file, the
 * Matches/Some/Every conditions, NextWindow/PrevWindow, ClientMenu and
 * the iconbar mode. Grammar (from Fluxbox's ClientPattern.cc):
 *
 *   PATTERN ::= TERM+ LIMIT?
 *   TERM    ::= '(' regex ')'                 -- matches "name"
 *             | '(' property ')'              -- property=[current]
 *             | '(' property '=' regex ')'
 *             | '(' property '!=' regex ')'
 *   LIMIT   ::= '{' number '}'
 *
 * Regexes are POSIX extended and anchored (^...$). All terms must match.
 * The special values [current] (same as the focused window, or the
 * current workspace) and [mouse] (head under the cursor) are supported.
 */

#ifndef WM_CLIENT_PATTERN_H
#define WM_CLIENT_PATTERN_H

#include <regex.h>
#include <stdbool.h>

struct wm_server;
struct wm_view;

enum wm_pattern_prop {
	WM_PROP_NAME,
	WM_PROP_CLASS,
	WM_PROP_TITLE,
	WM_PROP_ROLE,
	WM_PROP_TRANSIENT,
	WM_PROP_MAXIMIZED,
	WM_PROP_MAXIMIZED_VERT,
	WM_PROP_MAXIMIZED_HORIZ,
	WM_PROP_MINIMIZED,
	WM_PROP_FULLSCREEN,
	WM_PROP_SHADED,
	WM_PROP_STUCK,
	WM_PROP_FOCUSHIDDEN,
	WM_PROP_ICONHIDDEN,
	WM_PROP_WORKSPACE,
	WM_PROP_WORKSPACENAME,
	WM_PROP_HEAD,
	WM_PROP_LAYER,
	WM_PROP_URGENT,
	WM_PROP_SCREEN,
	WM_PROP_FOCUSED,     /* fluxland extension: yes/no */
	WM_PROP_OUTPUT,      /* fluxland extension: output name */
};

enum wm_pattern_value {
	WM_PATTERN_REGEX,
	WM_PATTERN_CURRENT,  /* [current] */
	WM_PATTERN_MOUSE,    /* [mouse] (head only) */
};

struct wm_pattern_term {
	enum wm_pattern_prop prop;
	enum wm_pattern_value kind;
	bool negate;
	char *regex_str;     /* as written, for diagnostics */
	regex_t regex;       /* compiled anchored ERE (kind == REGEX) */
};

struct wm_client_pattern {
	struct wm_pattern_term *terms;
	int term_count;
	int match_limit;     /* {N}; 0 = unlimited */
};

/*
 * Parse a pattern. If `rest` is non-NULL, parsing stops after the
 * pattern and *rest points at the remaining text; otherwise trailing
 * text is an error. Returns NULL on error (no terms, bad regex, ...).
 */
struct wm_client_pattern *wm_client_pattern_parse(const char *str,
	const char **rest);

void wm_client_pattern_destroy(struct wm_client_pattern *pattern);

/* True if every term matches the view. Ignores the {N} limit. */
bool wm_client_pattern_match(struct wm_server *server,
	const struct wm_client_pattern *pattern, struct wm_view *view);

/*
 * Window-list arguments shared by NextWindow, PrevWindow, Attach,
 * ArrangeWindows*, Unclutter, GoToWindow and ClientMenu:
 *   [{static} {groups}] [pattern]
 * A missing pattern matches every window.
 */
struct wm_window_list_spec {
	bool static_order;   /* creation order instead of focus order */
	bool groups;         /* one entry per tab group */
	struct wm_client_pattern *pattern;  /* NULL = all windows */
};

/*
 * Parse window-list arguments (NULL/empty is valid). A bare word
 * without parentheses is accepted as "(word)" for fluxland 1.x configs.
 * Returns false on a malformed pattern.
 */
bool wm_window_list_parse(const char *args,
	struct wm_window_list_spec *spec);

void wm_window_list_finish(struct wm_window_list_spec *spec);

/* Look up a property by (case-insensitive) name. */
bool wm_pattern_prop_from_name(const char *name,
	enum wm_pattern_prop *prop);

/*
 * The string value of a property for a view, as matched by a regex.
 * Writes into buf and returns it.
 */
const char *wm_client_pattern_property(struct wm_server *server,
	struct wm_view *view, enum wm_pattern_prop prop,
	char *buf, size_t len);

#endif /* WM_CLIENT_PATTERN_H */
