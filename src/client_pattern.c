/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * client_pattern.c - Fluxbox client patterns
 *
 * Semantics follow Fluxbox's ClientPattern.cc: anchored POSIX extended
 * regexes, all terms ANDed, a bare word matches the name, a bare
 * property means [current]. On Wayland, "name" and "class" are both the
 * xdg app_id; X11 windows use WM_CLASS instance and class.
 */

#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <limits.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>

#include "client_pattern.h"
#include "regex_guard.h"
#include "decoration.h"
#include "output.h"
#include "server.h"
#include "view.h"
#include "workspace.h"

/* Cap regex length, as rules.c does, to bound compile cost */
#define MAX_REGEX_LEN 1024

static const struct {
	const char *name;
	enum wm_pattern_prop prop;
} prop_names[] = {
	{ "class",               WM_PROP_CLASS },
	{ "focushidden",         WM_PROP_FOCUSHIDDEN },
	{ "fullscreen",          WM_PROP_FULLSCREEN },
	{ "head",                WM_PROP_HEAD },
	{ "iconhidden",          WM_PROP_ICONHIDDEN },
	{ "layer",               WM_PROP_LAYER },
	{ "maximized",           WM_PROP_MAXIMIZED },
	{ "maximizedhorizontal", WM_PROP_MAXIMIZED_HORIZ },
	{ "maximizedvertical",   WM_PROP_MAXIMIZED_VERT },
	{ "minimized",           WM_PROP_MINIMIZED },
	{ "name",                WM_PROP_NAME },
	{ "role",                WM_PROP_ROLE },
	{ "tag",                 WM_PROP_TAG },
	{ "screen",              WM_PROP_SCREEN },
	{ "shaded",              WM_PROP_SHADED },
	{ "stuck",               WM_PROP_STUCK },
	{ "title",               WM_PROP_TITLE },
	{ "transient",           WM_PROP_TRANSIENT },
	{ "urgent",              WM_PROP_URGENT },
	{ "workspace",           WM_PROP_WORKSPACE },
	{ "workspacename",       WM_PROP_WORKSPACENAME },
	/* fluxland extensions and aliases */
	{ "app_id",              WM_PROP_CLASS },
	{ "focused",             WM_PROP_FOCUSED },
	{ "output",              WM_PROP_OUTPUT },
	{ "sticky",              WM_PROP_STUCK },
};

bool
wm_pattern_prop_from_name(const char *name, enum wm_pattern_prop *prop)
{
	for (size_t i = 0; i < sizeof(prop_names) / sizeof(prop_names[0]);
	     i++) {
		if (strcasecmp(prop_names[i].name, name) == 0) {
			*prop = prop_names[i].prop;
			return true;
		}
	}
	return false;
}

/*
 * Translate a shell glob into an ERE. fluxland 1.x conditions used
 * fnmatch globs ("*vim*"); those are not valid EREs, so a pattern that
 * fails to compile is retried as a glob.
 */
static char *
glob_to_regex(const char *glob)
{
	size_t len = strlen(glob);
	char *out = malloc(len * 2 + 1);
	if (!out)
		return NULL;
	char *o = out;
	for (const char *g = glob; *g; g++) {
		switch (*g) {
		case '*':
			*o++ = '.';
			*o++ = '*';
			break;
		case '?':
			*o++ = '.';
			break;
		case '.': case '+': case '(': case ')': case '{': case '}':
		case '|': case '^': case '$': case '\\':
			*o++ = '\\';
			*o++ = *g;
			break;
		default:
			*o++ = *g;
			break;
		}
	}
	*o = '\0';
	return out;
}

static bool
compile_one(regex_t *re, const char *str)
{
	if (!wm_regex_is_sane(str))
		return false;
	size_t len = strlen(str);
	if (len > MAX_REGEX_LEN * 2)
		return false;
	char *anchored = malloc(len + 5);
	if (!anchored)
		return false;
	snprintf(anchored, len + 5, "^(%s)$", str);
	int ret = regcomp(re, anchored, REG_EXTENDED | REG_NOSUB);
	free(anchored);
	return ret == 0;
}

static bool
compile_anchored(regex_t *re, const char *str)
{
	if (strlen(str) > MAX_REGEX_LEN)
		return false;
	if (compile_one(re, str))
		return true;

	/* One retry as a glob; if that fails too, the pattern is bad */
	char *glob = glob_to_regex(str);
	if (!glob)
		return false;
	bool ok = compile_one(re, glob);
	if (ok)
		wlr_log(WLR_INFO, "pattern '%s' is not a valid regex; "
			"treating it as a glob", str);
	free(glob);
	return ok;
}

/*
 * Extract the text between a '(' at *pos and its matching ')',
 * allowing nested parentheses and backslash escapes in regexes.
 */
static char *
extract_paren(const char **pos)
{
	const char *p = *pos;
	if (*p != '(')
		return NULL;
	p++;
	const char *start = p;
	int depth = 1;
	while (*p) {
		if (*p == '\\' && p[1]) {
			p += 2;
			continue;
		}
		if (*p == '(')
			depth++;
		else if (*p == ')' && --depth == 0)
			break;
		p++;
	}
	if (depth != 0)
		return NULL;
	char *out = strndup(start, (size_t)(p - start));
	*pos = p + 1;
	return out;
}

static char *
trim(char *s)
{
	while (isspace((unsigned char)*s))
		s++;
	char *end = s + strlen(s);
	while (end > s && isspace((unsigned char)end[-1]))
		*--end = '\0';
	return s;
}

static bool
parse_term(char *content, struct wm_pattern_term *term)
{
	char orig[MAX_REGEX_LEN + 1];
	snprintf(orig, sizeof(orig), "%s", content);
	char *eq = strchr(content, '=');
	char *value;
	char *propstr;

	term->negate = false;
	if (!eq) {
		propstr = trim(content);
		value = NULL;
	} else {
		*eq = '\0';
		value = eq + 1;
		propstr = content;
		size_t plen = strlen(propstr);
		if (plen > 0 && propstr[plen - 1] == '!') {
			term->negate = true;
			propstr[plen - 1] = '\0';
		}
		propstr = trim(propstr);
		value = trim(value);
	}

	term->xprop = NULL;
	if (propstr[0] == '@' && propstr[1]) {
		/* @NAME: an X property (ClientPattern's XPROP) */
		term->prop = WM_PROP_XPROP;
		term->xprop = strdup(propstr + 1);
		if (!term->xprop)
			return false;
		if (!value)
			value = "[current]";
	} else if (!wm_pattern_prop_from_name(propstr, &term->prop)) {
		/* Not a property: the whole term is a regex on the name */
		term->prop = WM_PROP_NAME;
		term->negate = false;
		value = trim(orig);
	} else if (!value) {
		value = "[current]";
	}

	term->regex_str = strdup(value);
	if (!term->regex_str) {
		free(term->xprop);
		term->xprop = NULL;
		return false;
	}

	if (strcasecmp(value, "[current]") == 0) {
		term->kind = WM_PATTERN_CURRENT;
		return true;
	}
	if (strcasecmp(value, "[mouse]") == 0 &&
	    term->prop == WM_PROP_HEAD) {
		term->kind = WM_PATTERN_MOUSE;
		return true;
	}
	term->kind = WM_PATTERN_REGEX;
	if (!compile_anchored(&term->regex, value)) {
		wlr_log(WLR_ERROR, "invalid pattern regex: %s", value);
		free(term->regex_str);
		term->regex_str = NULL;
		free(term->xprop);
		term->xprop = NULL;
		return false;
	}
	return true;
}

struct wm_client_pattern *
wm_client_pattern_parse(const char *str, const char **rest)
{
	if (!str)
		return NULL;

	struct wm_client_pattern *pattern = calloc(1, sizeof(*pattern));
	if (!pattern)
		return NULL;

	const char *p = str;
	while (isspace((unsigned char)*p))
		p++;

	while (*p == '(') {
		char *content = extract_paren(&p);
		if (!content)
			goto error;

		struct wm_pattern_term *terms = realloc(pattern->terms,
			sizeof(*terms) * (size_t)(pattern->term_count + 1));
		if (!terms) {
			free(content);
			goto error;
		}
		pattern->terms = terms;
		struct wm_pattern_term *term =
			&pattern->terms[pattern->term_count];
		memset(term, 0, sizeof(*term));
		bool ok = parse_term(content, term);
		free(content);
		if (!ok)
			goto error;
		pattern->term_count++;

		while (isspace((unsigned char)*p))
			p++;
	}

	if (pattern->term_count == 0)
		goto error;

	/* Optional {N} match limit */
	if (*p == '{') {
		const char *close = strchr(p, '}');
		if (close) {
			char num[16] = {0};
			size_t len = (size_t)(close - p - 1);
			if (len < sizeof(num)) {
				memcpy(num, p + 1, len);
				char *end;
				long n = strtol(trim(num), &end, 10);
				if (*end == '\0' && n >= 0 && n <= INT_MAX) {
					pattern->match_limit = (int)n;
					p = close + 1;
				}
			}
		}
	}

	while (isspace((unsigned char)*p))
		p++;
	if (rest)
		*rest = p;
	else if (*p != '\0')
		goto error;

	return pattern;

error:
	wm_client_pattern_destroy(pattern);
	return NULL;
}

void
wm_client_pattern_destroy(struct wm_client_pattern *pattern)
{
	if (!pattern)
		return;
	for (int i = 0; i < pattern->term_count; i++) {
		struct wm_pattern_term *t = &pattern->terms[i];
		if (t->kind == WM_PATTERN_REGEX && t->regex_str)
			regfree(&t->regex);
		free(t->regex_str);
		free(t->xprop);
	}
	free(pattern->terms);
	free(pattern);
}

bool
wm_window_list_parse(const char *args, struct wm_window_list_spec *spec)
{
	memset(spec, 0, sizeof(*spec));
	if (!args)
		return true;

	const char *p = args;
	while (isspace((unsigned char)*p))
		p++;

	/* Options: {static groups} (Fluxbox puts them in one block) */
	while (*p == '{') {
		const char *close = strchr(p, '}');
		if (!close)
			return false;
		char opts[64];
		size_t len = (size_t)(close - p - 1);
		if (len >= sizeof(opts))
			return false;
		memcpy(opts, p + 1, len);
		opts[len] = '\0';
		char *save = NULL;
		for (char *tok = strtok_r(opts, " \t", &save); tok;
		     tok = strtok_r(NULL, " \t", &save)) {
			if (strcasecmp(tok, "static") == 0)
				spec->static_order = true;
			else if (strcasecmp(tok, "groups") == 0)
				spec->groups = true;
		}
		p = close + 1;
		while (isspace((unsigned char)*p))
			p++;
	}

	if (*p == '\0')
		return true;

	if (*p == '(') {
		spec->pattern = wm_client_pattern_parse(p, NULL);
		return spec->pattern != NULL;
	}

	/* fluxland 1.x: a bare glob such as "firefox" or "*term*" */
	size_t len = strlen(p);
	char *wrapped = malloc(len + 3);
	if (!wrapped)
		return false;
	snprintf(wrapped, len + 3, "(%s)", p);
	spec->pattern = wm_client_pattern_parse(wrapped, NULL);
	free(wrapped);
	return spec->pattern != NULL;
}

void
wm_window_list_finish(struct wm_window_list_spec *spec)
{
	wm_client_pattern_destroy(spec->pattern);
	spec->pattern = NULL;
}

/* --- Property values --- */

static const char *
yes_no(bool b)
{
	return b ? "yes" : "no";
}

/* 0-based index of an output, oldest first (like Xinerama heads) */
static int
output_index(struct wm_server *server, struct wlr_output *wlr_output)
{
	int count = 0, idx = -1;
	struct wm_output *o;
	wl_list_for_each_reverse(o, &server->outputs, link) {
		if (o->wlr_output == wlr_output)
			idx = count;
		count++;
	}
	return idx;
}

static struct wlr_output *
view_output(struct wm_server *server, struct wm_view *view)
{
	struct wlr_box geo;
	wm_view_get_geometry(view, &geo);
	return wlr_output_layout_output_at(server->output_layout,
		view->x + geo.width / 2.0, view->y + geo.height / 2.0);
}

static const char *
layer_value(enum wm_view_layer layer)
{
	/* Fluxbox's layer names; fluxland's aliases are tried too */
	switch (layer) {
	case WM_LAYER_DESKTOP: return "Desktop";
	case WM_LAYER_BELOW:   return "Bottom";
	case WM_LAYER_NORMAL:  return "Normal";
	case WM_LAYER_ABOVE:   return "AboveDock";
	case WM_VIEW_LAYER_COUNT: break;
	}
	return "Normal";
}

static const char *
layer_alias(enum wm_view_layer layer)
{
	switch (layer) {
	case WM_LAYER_BELOW: return "Below";
	case WM_LAYER_ABOVE: return "Above";
	default:             return NULL;
	}
}

const char *
wm_client_pattern_property(struct wm_server *server, struct wm_view *view,
	enum wm_pattern_prop prop, char *buf, size_t len)
{
	buf[0] = '\0';
	if (!view) {
		if (prop == WM_PROP_FOCUSED)
			snprintf(buf, len, "no");
		return buf;
	}

	switch (prop) {
	case WM_PROP_NAME:
		/* X11: WM_CLASS instance; xdg-shell only has the app_id */
		snprintf(buf, len, "%s", view->instance ? view->instance :
			view->app_id ? view->app_id : "");
		break;
	case WM_PROP_CLASS:
		snprintf(buf, len, "%s", view->app_id ? view->app_id : "");
		break;
	case WM_PROP_TITLE:
		snprintf(buf, len, "%s", view->title ? view->title : "");
		break;
	case WM_PROP_ROLE:
		/* X11 WM_WINDOW_ROLE; xdg-shell has no role */
		snprintf(buf, len, "%s", view->role ? view->role : "");
		break;
	case WM_PROP_TAG:
		/* xdg-toplevel-tag: a stable name the app gives the window */
		snprintf(buf, len, "%s", view->tag ? view->tag : "");
		break;
	case WM_PROP_TRANSIENT:
		snprintf(buf, len, "%s", yes_no(wm_view_parent(view)));
		break;
	case WM_PROP_MAXIMIZED:
		snprintf(buf, len, "%s", yes_no(view->maximized));
		break;
	case WM_PROP_MAXIMIZED_VERT:
		snprintf(buf, len, "%s",
			yes_no(view->maximized || view->maximized_vert));
		break;
	case WM_PROP_MAXIMIZED_HORIZ:
		snprintf(buf, len, "%s",
			yes_no(view->maximized || view->maximized_horiz));
		break;
	case WM_PROP_MINIMIZED:
		snprintf(buf, len, "%s",
			yes_no(view->scene_tree &&
				!view->scene_tree->node.enabled));
		break;
	case WM_PROP_FULLSCREEN:
		snprintf(buf, len, "%s", yes_no(view->fullscreen));
		break;
	case WM_PROP_SHADED:
		snprintf(buf, len, "%s", yes_no(view->decoration &&
			view->decoration->shaded));
		break;
	case WM_PROP_STUCK:
		snprintf(buf, len, "%s", yes_no(view->sticky));
		break;
	case WM_PROP_FOCUSHIDDEN:
		snprintf(buf, len, "%s", yes_no(view->focus_hidden));
		break;
	case WM_PROP_ICONHIDDEN:
		snprintf(buf, len, "%s", yes_no(view->icon_hidden));
		break;
	case WM_PROP_WORKSPACE:
		/* Fluxbox numbers workspaces from 0 here */
		snprintf(buf, len, "%d",
			view->workspace ? view->workspace->index : 0);
		break;
	case WM_PROP_WORKSPACENAME:
		snprintf(buf, len, "%s", view->workspace &&
			view->workspace->name ? view->workspace->name : "");
		break;
	case WM_PROP_HEAD: {
		struct wlr_output *o = view_output(server, view);
		snprintf(buf, len, "%d", o ? output_index(server, o) : 0);
		break;
	}
	case WM_PROP_OUTPUT: {
		struct wlr_output *o = view_output(server, view);
		snprintf(buf, len, "%s", o && o->name ? o->name : "");
		break;
	}
	case WM_PROP_LAYER:
		snprintf(buf, len, "%s", layer_value(view->layer));
		break;
	case WM_PROP_URGENT:
		snprintf(buf, len, "%s", yes_no(view->urgent));
		break;
	case WM_PROP_SCREEN:
		snprintf(buf, len, "0");
		break;
	case WM_PROP_FOCUSED:
		snprintf(buf, len, "%s",
			yes_no(view == server->focused_view));
		break;
	case WM_PROP_XPROP:
		/* Needs the term's name: term_value */
		break;
	}
	return buf;
}

/* A window's text property, as @NAME reads it: X11's own, or what
 * SetXProp set ("" when none) */
static const char *
view_xprop(struct wm_view *view, const char *name, char *buf, size_t len)
{
	buf[0] = '\0';
	if (view->impl && view->impl->get_xprop &&
	    view->impl->get_xprop(view, name, buf, len))
		return buf;
	for (struct wm_view_prop *p = view->props; p; p = p->next) {
		if (strcmp(p->name, name) == 0) {
			snprintf(buf, len, "%s", p->value);
			break;
		}
	}
	return buf;
}

/* A term's property of a view */
static const char *
term_value(struct wm_server *server, const struct wm_pattern_term *t,
	struct wm_view *view, char *buf, size_t len)
{
	if (t->prop == WM_PROP_XPROP) {
		buf[0] = '\0';
		return view ? view_xprop(view, t->xprop, buf, len) : buf;
	}
	return wm_client_pattern_property(server, view, t->prop, buf, len);
}

static bool
term_matches(struct wm_server *server, const struct wm_pattern_term *t,
	struct wm_view *view)
{
	char buf[512], other[512];

	if (t->kind == WM_PATTERN_CURRENT) {
		if (t->prop == WM_PROP_WORKSPACE ||
		    t->prop == WM_PROP_WORKSPACENAME) {
			struct wm_workspace *ws =
				wm_workspace_get_active(server);
			bool same = view && ws && (view->workspace == ws ||
				view->sticky);
			return t->negate ^ same;
		}
		struct wm_view *focused = server->focused_view;
		if (!focused)
			return false;
		term_value(server, t, view,
			buf, sizeof(buf));
		term_value(server, t, focused,
			other, sizeof(other));
		return t->negate ^ (strcmp(buf, other) == 0);
	}

	if (t->kind == WM_PATTERN_MOUSE) {
		struct wlr_output *at = wlr_output_layout_output_at(
			server->output_layout, server->cursor->x,
			server->cursor->y);
		term_value(server, t, view,
			buf, sizeof(buf));
		snprintf(other, sizeof(other), "%d",
			at ? output_index(server, at) : 0);
		return t->negate ^ (strcmp(buf, other) == 0);
	}

	term_value(server, t, view, buf, sizeof(buf));
	bool hit = regexec(&t->regex, buf, 0, NULL, 0) == 0;
	if (!hit && t->prop == WM_PROP_LAYER && view) {
		const char *alias = layer_alias(view->layer);
		hit = alias && regexec(&t->regex, alias, 0, NULL, 0) == 0;
	}
	return t->negate ^ hit;
}

bool
wm_client_pattern_match(struct wm_server *server,
	const struct wm_client_pattern *pattern, struct wm_view *view)
{
	if (!pattern)
		return false;
	for (int i = 0; i < pattern->term_count; i++) {
		if (!term_matches(server, &pattern->terms[i], view))
			return false;
	}
	return true;
}
