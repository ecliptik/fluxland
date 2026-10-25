/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * rules_parse.c - Reading the Fluxbox apps file
 *
 * Follows Remember.cc's Remember::reconfigure and parseApp: [app] and
 * [transient] entries, [group]s whose [app] lines share one set of
 * settings, and [startup] commands. Problems are logged and, when a
 * checker asked for them (rules->report), reported with their line.
 */

#define _GNU_SOURCE

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <wlr/util/log.h>

#include "client_pattern.h"
#include "fbcompat.h"
#include "rules.h"
#include "util.h"
#include "validate.h"

/* --- Lines --- */

static char *
strip(char *s)
{
	while (*s && isspace((unsigned char)*s))
		s++;
	char *end = s + strlen(s);
	while (end > s && isspace((unsigned char)end[-1]))
		end--;
	*end = '\0';
	return s;
}

/*
 * FbTk::StringUtil::getStringBetween: after optional blanks, the text
 * between open and close; a backslash escapes close (and is dropped).
 * Returns the offset just past close, or -1.
 */
static int
between(const char *s, char open, char close, char *out, size_t len)
{
	const char *p = s;
	while (*p == ' ' || *p == '\t')
		p++;
	if (*p != open)
		return -1;
	p++;
	size_t n = 0;
	for (; *p && *p != close; p++) {
		if (*p == '\\' && p[1] == close)
			p++;
		if (n + 1 < len)
			out[n++] = *p;
	}
	if (*p != close)
		return -1;
	out[n] = '\0';
	return (int)(p + 1 - s);
}

static bool
is_comment(const char *s)
{
	return *s == '\0' || *s == '#' || *s == '!';
}

/* Fluxbox compares lowercased values with "yes" */
static bool
yes(const char *s)
{
	return strcasecmp(s, "yes") == 0 || strcasecmp(s, "true") == 0 ||
		strcmp(s, "1") == 0;
}

/* Log a problem, and report it to a checker (--check-config) */
__attribute__((format(printf, 5, 6)))
static void
diag(struct wm_rules *rules, const char *path, int row, bool fatal,
	const char *fmt, ...)
{
	char msg[512];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);
	if (rules->report)
		rules->report(rules->report_data, path, row, fatal, msg);
	else
		wlr_log(fatal ? WLR_ERROR : WLR_INFO, "%s:%d: %s",
			path, row, msg);
}

/* --- Parsing --- */

static struct wm_rule_app *
app_new(struct wm_rules *rules, bool transient, bool grouped)
{
	struct wm_rule_app *app = calloc(1, sizeof(*app));
	if (!app)
		return NULL;
	app->transient = transient;
	app->grouped = grouped;
	wl_list_insert(rules->apps.prev, &app->link);
	return app;
}

static void
add_rule(struct wm_rules *rules, struct wm_client_pattern *pattern,
	struct wm_rule_app *app)
{
	struct wm_window_rule *rule = calloc(1, sizeof(*rule));
	if (!rule) {
		wm_client_pattern_destroy(pattern);
		return;
	}
	rule->pattern = pattern;
	rule->app = app;
	wl_list_insert(rules->window_rules.prev, &rule->link);
}

/*
 * One "[Key] (option) {value}" line of an entry (Remember.cc parseApp).
 * Returns false on [end].
 */
static bool
parse_setting(struct wm_rules *rules, const char *line,
	struct wm_rule_app *app, const char *path, int row)
{
	char key[64], option[64] = "", value[256] = "";
	int pos = between(line, '[', ']', key, sizeof(key));
	if (pos < 0)
		return true;
	int opt = between(line + pos, '(', ')', option, sizeof(option));
	if (opt > 0)
		pos += opt;
	between(line + pos, '{', '}', value, sizeof(value));
	char *val = strip(value);
	bool ok = true;

	if (strcasecmp(key, "end") == 0) {
		return false;
	} else if (strcasecmp(key, "Workspace") == 0) {
		ok = safe_atoi(val, &app->workspace) && app->workspace >= 0;
		app->has_workspace = ok;
	} else if (strcasecmp(key, "Head") == 0) {
		ok = safe_atoi(val, &app->head);
		app->has_head = ok;
	} else if (strcasecmp(key, "Layer") == 0) {
		ok = wm_layer_parse(val, &app->layer);
		app->has_layer = ok;
	} else if (strcasecmp(key, "Dimensions") == 0) {
		ok = wm_size_pair_parse(val, &app->width, &app->height);
		app->has_dimensions = ok;
	} else if (strcasecmp(key, "Position") == 0) {
		ok = wm_corner_from_name(option, &app->pos_corner) &&
			wm_size_pair_parse(val, &app->pos_x, &app->pos_y);
		app->has_position = ok;
	} else if (strcasecmp(key, "Deco") == 0) {
		ok = wm_decor_parse(val, &app->deco);
		app->has_deco = ok;
	} else if (strcasecmp(key, "Alpha") == 0) {
		int f, u;
		int n = sscanf(val, "%i %i", &f, &u);
		if (n == 1)
			u = f;
		ok = n >= 1;
		if (ok) {
			app->has_alpha = true;
			app->focus_alpha = f < 0 ? 0 : f > 255 ? 255 : f;
			app->unfocus_alpha = u < 0 ? 0 : u > 255 ? 255 : u;
		}
	} else if (strcasecmp(key, "Sticky") == 0) {
		app->has_sticky = true;
		app->sticky = yes(val);
	} else if (strcasecmp(key, "Shaded") == 0) {
		app->has_shaded = true;
		app->shaded = yes(val);
	} else if (strcasecmp(key, "Tab") == 0) {
		/* Fluxbox reads it but has no external tabs to apply it to */
	} else if (strcasecmp(key, "FocusHidden") == 0) {
		app->has_focus_hidden = true;
		app->focus_hidden = yes(val);
	} else if (strcasecmp(key, "IconHidden") == 0) {
		app->has_icon_hidden = true;
		app->icon_hidden = yes(val);
	} else if (strcasecmp(key, "Hidden") == 0) {
		app->has_focus_hidden = app->has_icon_hidden = true;
		app->focus_hidden = app->icon_hidden = yes(val);
	} else if (strcasecmp(key, "FocusNewWindow") == 0) {
		/* Fluxbox folds it into the protection flags */
		app->has_focus_protection = true;
		if (!(app->focus_protection &
		      (WM_FOCUS_PROT_GAIN | WM_FOCUS_PROT_REFUSE)))
			app->focus_protection |= yes(val) ?
				WM_FOCUS_PROT_GAIN : WM_FOCUS_PROT_REFUSE;
	} else if (strcasecmp(key, "FocusProtection") == 0) {
		app->has_focus_protection = true;
		ok = wm_focus_protection_parse(val, &app->focus_protection);
	} else if (strcasecmp(key, "Minimized") == 0) {
		app->has_minimized = true;
		app->minimized = yes(val);
	} else if (strcasecmp(key, "Maximized") == 0) {
		app->has_maximized = true;
		if (yes(val))
			app->maximized = WM_RULE_MAX_FULL;
		else if (strcasecmp(val, "horz") == 0)
			app->maximized = WM_RULE_MAX_HORZ;
		else if (strcasecmp(val, "vert") == 0)
			app->maximized = WM_RULE_MAX_VERT;
		else
			app->maximized = WM_RULE_MAX_NONE;
	} else if (strcasecmp(key, "Fullscreen") == 0) {
		app->has_fullscreen = true;
		app->fullscreen = yes(val);
	} else if (strcasecmp(key, "Jump") == 0) {
		app->has_jump = true;
		app->jump = yes(val);
	} else if (strcasecmp(key, "Close") == 0) {
		app->has_close = true;
		app->save_on_close = yes(val);
	} else if (strcasecmp(key, "IgnoreSizeHints") == 0) {
		app->has_ignore_size_hints = true;
		app->ignore_size_hints = yes(val);
	} else if (strcasecmp(key, "Dock") == 0) {
		app->has_dock = true;
		app->dock = yes(val);
	} else {
		diag(rules, path, row, false, "unknown apps key '%s'", key);
	}
	if (!ok)
		diag(rules, path, row, true, "bad value for [%s]: '%s'",
			key, val);
	return true;
}

/* "[startup] (screen=N) {command}" */
static void
parse_startup(struct wm_rules *rules, const char *rest, const char *path,
	int row)
{
	char opts[64], cmd[1024];
	int pos = between(rest, '(', ')', opts, sizeof(opts));
	if (pos < 0)
		pos = 0;
	if (between(rest + pos, '{', '}', cmd, sizeof(cmd)) < 0) {
		diag(rules, path, row, true, "[startup] expects {command}");
		return;
	}
	char **list = realloc(rules->startup,
		sizeof(*list) * (size_t)(rules->startup_count + 1));
	if (!list)
		return;
	rules->startup = list;
	list[rules->startup_count] = strdup(cmd);
	if (list[rules->startup_count])
		rules->startup_count++;
}

void
wm_rules_init(struct wm_rules *rules)
{
	wl_list_init(&rules->window_rules);
	wl_list_init(&rules->apps);
	rules->startup = NULL;
	rules->startup_count = 0;
	rules->report = NULL;
	rules->report_data = NULL;
}

void
wm_rules_finish(struct wm_rules *rules)
{
	struct wm_window_rule *rule, *rtmp;
	wl_list_for_each_safe(rule, rtmp, &rules->window_rules, link) {
		wl_list_remove(&rule->link);
		wm_client_pattern_destroy(rule->pattern);
		free(rule);
	}
	struct wm_rule_app *app, *atmp;
	wl_list_for_each_safe(app, atmp, &rules->apps, link) {
		wl_list_remove(&app->link);
		wm_client_pattern_destroy(app->group_pattern);
		free(app);
	}
	for (int i = 0; i < rules->startup_count; i++)
		free(rules->startup[i]);
	free(rules->startup);
	rules->startup = NULL;
	rules->startup_count = 0;
}

bool
wm_rules_load(struct wm_rules *rules, const char *path)
{
	FILE *f = path ? fopen_nofollow(path, "r") : NULL;
	if (!f) {
		wlr_log(WLR_INFO, "rules: no apps file at %s",
			path ? path : "(none)");
		return false;
	}

	char *line = NULL;
	size_t cap = 0;
	int row = 0;
	struct wm_rule_app *app = NULL;  /* entry whose settings follow */
	bool in_group = false;
	struct wm_client_pattern *group_pattern = NULL;
	struct wm_client_pattern **members = NULL;
	int member_count = 0;

	while (getline(&line, &cap, f) >= 0) {
		row++;
		char *s = strip(line);
		if (is_comment(s))
			continue;

		if (app) {
			if (!parse_setting(rules, s, app, path, row))
				app = NULL;
			continue;
		}

		char key[32];
		int pos = between(s, '[', ']', key, sizeof(key));
		bool is_app = pos > 0 && strcasecmp(key, "app") == 0;
		bool is_transient = pos > 0 &&
			strcasecmp(key, "transient") == 0;

		if (is_app || is_transient) {
			struct wm_client_pattern *pat =
				wm_client_pattern_parse(s + pos, NULL);
			if (!pat) {
				diag(rules, path, row, true,
					"bad client pattern: %s", s);
				/* Its settings belong to nothing */
				app = in_group ? NULL :
					app_new(rules, is_transient, false);
				continue;
			}
			if (in_group) {
				struct wm_client_pattern **m = realloc(members,
					sizeof(*m) * (size_t)(member_count + 1));
				if (!m) {
					wm_client_pattern_destroy(pat);
					continue;
				}
				members = m;
				members[member_count++] = pat;
			} else {
				app = app_new(rules, is_transient, false);
				if (app)
					add_rule(rules, pat, app);
				else
					wm_client_pattern_destroy(pat);
			}
		} else if (pos > 0 && strcasecmp(key, "startup") == 0) {
			parse_startup(rules, s + pos, path, row);
		} else if (pos > 0 && strcasecmp(key, "group") == 0) {
			in_group = true;
			wm_client_pattern_destroy(group_pattern);
			group_pattern = strchr(s + pos, '(') ?
				wm_client_pattern_parse(s + pos, NULL) : NULL;
		} else if (in_group) {
			/* The members are listed; the group's settings start */
			struct wm_rule_app *gapp = app_new(rules, false, true);
			for (int i = 0; i < member_count; i++) {
				if (gapp)
					add_rule(rules, members[i], gapp);
				else
					wm_client_pattern_destroy(members[i]);
			}
			member_count = 0;
			if (gapp) {
				gapp->group_pattern = group_pattern;
				group_pattern = NULL;
			}
			in_group = false;
			if (gapp && !(pos > 0 && strcasecmp(key, "end") == 0) &&
			    parse_setting(rules, s, gapp, path, row))
				app = gapp;
		} else {
			diag(rules, path, row, true,
				"not an apps entry: %s", s);
		}
	}
	free(line);
	fclose(f);

	if (app)
		diag(rules, path, row, false, "last entry has no [end]");
	if (in_group)
		diag(rules, path, row, false, "[group] has no [end]");
	/* A [group] that never ended */
	for (int i = 0; i < member_count; i++)
		wm_client_pattern_destroy(members[i]);
	free(members);
	wm_client_pattern_destroy(group_pattern);

	int count = 0;
	struct wm_window_rule *rule;
	wl_list_for_each(rule, &rules->window_rules, link)
		count++;
	wlr_log(WLR_INFO, "rules: %d pattern(s) from %s", count, path);
	return true;
}

bool
wm_rules_check_file(const char *path,
	void (*report)(void *data, const char *path, int line, bool fatal,
		const char *msg),
	void *data)
{
	struct wm_rules rules;
	wm_rules_init(&rules);
	rules.report = report;
	rules.report_data = data;
	bool ok = wm_rules_load(&rules, path);
	wm_rules_finish(&rules);
	return ok;
}
