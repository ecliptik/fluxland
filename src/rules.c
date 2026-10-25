/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * rules.c - Per-window rules (Fluxbox apps file)
 *
 * Follows Fluxbox's Remember.cc: entries are tried top to bottom and the
 * first whose pattern matches (and whose [app]/[transient] kind matches
 * the window) wins; a window keeps the entry it matched until it closes,
 * which is what {N} counts. Patterns are client patterns (client_pattern.c).
 * Settings are applied in Remember::setupWindow's order: layer and
 * decoration, workspace, head, dimensions, position, then states.
 * Reading the file is rules_parse.c's.
 */

#define _GNU_SOURCE

#include <time.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/util/log.h>

#include "client_pattern.h"
#include "decoration.h"
#include "fbcompat.h"
#include "output.h"
#include "rules.h"
#include "server.h"
#include "util.h"
#include "view.h"
#include "view_geometry.h"
#include "workspace.h"

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

/* --- Matching --- */

struct wm_window_rule *
wm_rules_match(struct wm_rules *rules, struct wm_view *view)
{
	if (!view)
		return NULL;
	if (view->rule)
		return view->rule;
	bool transient = wm_view_parent(view) != NULL;
	struct wm_window_rule *rule;
	wl_list_for_each(rule, &rules->window_rules, link) {
		int limit = rule->pattern->match_limit;
		if (limit > 0 && rule->matches >= limit)
			continue;
		if (rule->app->transient != transient)
			continue;
		if (!wm_client_pattern_match(view->server, rule->pattern, view))
			continue;
		rule->matches++;
		view->rule = rule;
		return rule;
	}
	return NULL;
}

void
wm_rules_release(struct wm_rules *rules, struct wm_view *view,
	const char *apps_path)
{
	(void)rules;
	struct wm_window_rule *rule = view->rule;
	if (!rule)
		return;
	if (rule->app->has_close && rule->app->save_on_close && apps_path)
		wm_rules_remember_window(view, apps_path);
	if (rule->matches > 0)
		rule->matches--;
	view->rule = NULL;
}

void
wm_rules_forget_views(struct wm_server *server)
{
	struct wm_view *view;
	wl_list_for_each(view, &server->views, link)
		view->rule = NULL;
}

bool
wm_rules_should_dock(struct wm_rules *rules, struct wm_view *view)
{
	struct wm_window_rule *rule = wm_rules_match(rules, view);
	return rule && rule->app->has_dock && rule->app->dock;
}

struct wm_view *
wm_rules_find_group(struct wm_rules *rules, struct wm_view *view,
	struct wm_server *server)
{
	struct wm_window_rule *rule = wm_rules_match(rules, view);
	if (!rule || !rule->app->grouped)
		return NULL;
	/* Remember::findGroup: an open window of the same entry */
	struct wm_view *peer;
	wl_list_for_each(peer, &server->views, link) {
		if (peer == view || !peer->mapped || !peer->rule ||
		    peer->rule->app != rule->app)
			continue;
		if (rule->app->group_pattern &&
		    !wm_client_pattern_match(server, rule->app->group_pattern,
			peer))
			continue;
		return peer;
	}
	return NULL;
}

/* --- Applying --- */

/* The usable area of the output a window goes to */
static bool
target_area(struct wm_view *view, const struct wm_rule_app *app, bool shown,
	struct wlr_box *area)
{
	struct wm_server *server = view->server;
	struct wm_output *out = NULL;
	if (app->has_head)
		out = wm_output_head(server, app->head);
	if (!out && shown)
		out = wm_output_for_view(view);
	if (!out) {
		/* Not placed yet: windows open on the pointer's output */
		struct wlr_output *at = wlr_output_layout_output_at(
			server->output_layout, server->cursor->x,
			server->cursor->y);
		struct wm_output *o;
		wl_list_for_each(o, &server->outputs, link) {
			if (!out || o->wlr_output == at)
				out = o;
			if (o->wlr_output == at)
				break;
		}
	}
	if (!out)
		return false;
	*area = out->usable_area;
	if (wlr_box_empty(area))
		wlr_output_layout_get_box(server->output_layout,
			out->wlr_output, area);
	return !wlr_box_empty(area);
}

void
wm_rules_apply(struct wm_rules *rules, struct wm_view *view)
{
	struct wm_window_rule *rule = wm_rules_match(rules, view);
	if (!rule)
		return;
	const struct wm_rule_app *app = rule->app;
	struct wm_server *server = view->server;
	bool shown = view->scene_tree && view->scene_tree->node.enabled;

	wlr_log(WLR_DEBUG, "rules: app_id='%s' title='%s' matched",
		view->app_id ? view->app_id : "",
		view->title ? view->title : "");

	if (app->has_focus_hidden)
		view->focus_hidden = app->focus_hidden;
	if (app->has_icon_hidden)
		view->icon_hidden = app->icon_hidden;
	if (app->has_layer)
		wm_view_set_layer(view, app->layer);
	if (app->has_deco && view->decoration)
		wm_decoration_set_preset(view->decoration, app->deco,
			server->style);
	if (app->has_focus_protection)
		view->focus_protection = app->focus_protection;
	if (app->has_alpha) {
		view->focus_alpha = app->focus_alpha;
		view->unfocus_alpha = app->unfocus_alpha;
	}
	if (app->has_ignore_size_hints)
		view->ignore_size_hints = app->ignore_size_hints;

	if (app->has_workspace) {
		struct wm_workspace *ws = wm_workspace_get(server,
			app->workspace);
		if (ws) {
			wm_view_move_to_workspace(view, ws);
			if (app->has_jump && app->jump)
				wm_workspace_switch(server, app->workspace);
		}
	}

	/* Dimensions and Position are on the head, as frame (outer) sizes */
	struct wlr_box area;
	bool have_area = (app->has_dimensions || app->has_position) &&
		target_area(view, app, shown, &area);
	int top, bottom, left, right;
	wm_view_frame_extents(view, &top, &bottom, &left, &right);
	struct wlr_box geo = {0};
	if (wm_view_surface(view))
		wm_view_content_geometry(view, &geo);
	int frame_w = geo.width + left + right;
	int frame_h = geo.height + top + bottom;

	if (app->has_dimensions && have_area && wm_view_surface(view)) {
		int w = wm_size_token_pixels(&app->width, area.width);
		int h = wm_size_token_pixels(&app->height, area.height);
		if (w > 0)
			frame_w = w;
		if (h > 0)
			frame_h = h;
		int cw = frame_w - left - right;
		int ch = frame_h - top - bottom;
		wm_view_set_size(view, cw > 1 ? cw : 1, ch > 1 ? ch : 1);
	}

	if (app->has_position && have_area) {
		int x = wm_size_token_pixels(&app->pos_x, area.width);
		int y = wm_size_token_pixels(&app->pos_y, area.height);
		view->x = wm_corner_x(app->pos_corner, &area, frame_w, x);
		view->y = wm_corner_y(app->pos_corner, &area, frame_h, y);
		view->rule_placed = true;
		if (shown)
			wm_view_move(view, view->x, view->y);
		else if (view->scene_tree)
			wlr_scene_node_set_position(&view->scene_tree->node,
				view->x, view->y);
	} else if (app->has_head) {
		if (shown)
			wm_view_set_head(view, app->head);
		else
			view->pending_head = app->head;
	}

	if (app->has_shaded && view->decoration &&
	    view->decoration->shaded != app->shaded)
		wm_decoration_set_shaded(view->decoration, app->shaded,
			server->style);
	if (app->has_sticky && view->sticky != app->sticky)
		wm_view_set_sticky(view, app->sticky);
	if (app->has_minimized && app->minimized) {
		if (shown)
			wm_view_minimize(view);
		else
			view->pending_minimize = true;
	}

	/* The states that fill an output wait until the window is placed */
	if (app->has_maximized) {
		switch (app->maximized) {
		case WM_RULE_MAX_FULL:
			if (shown && !view->maximized)
				wm_view_toggle_maximize(view);
			else if (!shown)
				view->pending_maximize = true;
			break;
		case WM_RULE_MAX_HORZ:
			if (shown && !view->maximized_horiz)
				wm_view_maximize_horiz(view);
			else if (!shown)
				view->pending_maximize_horiz = true;
			break;
		case WM_RULE_MAX_VERT:
			if (shown && !view->maximized_vert)
				wm_view_maximize_vert(view);
			else if (!shown)
				view->pending_maximize_vert = true;
			break;
		case WM_RULE_MAX_NONE:
			view->refuse_maximize = true;
			if (shown && view->maximized)
				wm_view_toggle_maximize(view);
			break;
		}
	}
	if (app->has_fullscreen) {
		if (app->fullscreen) {
			if (shown && !view->fullscreen)
				wm_view_toggle_fullscreen(view);
			else if (!shown)
				view->pending_fullscreen = true;
		} else {
			view->refuse_fullscreen = true;
			if (shown && view->fullscreen)
				wm_view_toggle_fullscreen(view);
		}
	}
}

/* --- Remember --- */

/* Escape a string so that it matches itself as an anchored ERE */
static void
regex_escape(FILE *out, const char *s)
{
	for (; *s; s++) {
		if (strchr(".[]()*+?{}|^$\\", *s))
			fputc('\\', out);
		fputc(*s, out);
	}
}

static char *
remember_header(struct wm_view *view)
{
	char *buf = NULL;
	size_t len = 0;
	FILE *mem = open_memstream(&buf, &len);
	if (!mem)
		return NULL;
	/* Fluxbox's own: (name=instance) (class=class) (role=role) */
	fputs("[app] (name=", mem);
	regex_escape(mem, view->instance ? view->instance :
		view->app_id ? view->app_id : "");
	fputs(") (class=", mem);
	regex_escape(mem, view->app_id ? view->app_id : "");
	fputc(')', mem);
	if (view->role && *view->role) {
		fputs(" (role=", mem);
		regex_escape(mem, view->role);
		fputc(')', mem);
	}
	fclose(mem);
	return buf;
}

static char *
remember_block(struct wm_view *view, const char *header)
{
	char *buf = NULL;
	size_t len = 0;
	FILE *mem = open_memstream(&buf, &len);
	if (!mem)
		return NULL;

	fprintf(mem, "%s\n", header);
	fprintf(mem, "  [Workspace]\t{%d}\n",
		view->workspace ? view->workspace->index : 0);
	struct wm_output *out = wm_output_for_view(view);
	if (out && wm_output_head_count(view->server) > 1)
		fprintf(mem, "  [Head]\t{%d}\n", wm_output_head_number(out));
	fprintf(mem, "  [Layer]\t{%d}\n", wm_layer_fluxbox_number(view->layer));
	if (!view->maximized && !view->fullscreen) {
		struct wlr_box frame;
		wm_view_frame_box(view, &frame);
		struct wlr_box area = out ? out->usable_area : (struct wlr_box){0};
		fprintf(mem, "  [Dimensions]\t{%d %d}\n",
			frame.width, frame.height);
		fprintf(mem, "  [Position]\t(UPPERLEFT)\t{%d %d}\n",
			frame.x - area.x, frame.y - area.y);
	}
	fprintf(mem, "  [Alpha]\t{%d %d}\n",
		view->focus_alpha, view->unfocus_alpha);
	fprintf(mem, "  [Sticky]\t{%s}\n", view->sticky ? "yes" : "no");
	fprintf(mem, "  [Maximized]\t{%s}\n", view->maximized ? "yes" :
		view->maximized_horiz ? "horz" :
		view->maximized_vert ? "vert" : "no");
	fprintf(mem, "  [Fullscreen]\t{%s}\n", view->fullscreen ? "yes" : "no");
	if (view->decoration) {
		static const char *const deco[] = {
			[WM_DECOR_NORMAL] = "NORMAL", [WM_DECOR_NONE] = "NONE",
			[WM_DECOR_BORDER] = "BORDER", [WM_DECOR_TAB] = "TAB",
			[WM_DECOR_TINY] = "TINY", [WM_DECOR_TOOL] = "TOOL",
		};
		fprintf(mem, "  [Shaded]\t{%s}\n",
			view->decoration->shaded ? "yes" : "no");
		fprintf(mem, "  [Deco]\t{%s}\n",
			deco[view->decoration->preset]);
	}
	if (view->rule && view->rule->app->has_close)
		fprintf(mem, "  [Close]\t{%s}\n",
			view->rule->app->save_on_close ? "yes" : "no");
	fprintf(mem, "[end]\n");
	fclose(mem);
	return buf;
}

/* The apps keyword of each remembered attribute */
static const char *const remember_keys[WM_REM_COUNT] = {
	"Workspace", "Jump", "Head", "Dimensions", "Position", "Sticky",
	"Deco", "Shaded", "Minimized", "Maximized", "Fullscreen", "Alpha",
	"Layer", "Close",
};

/* The attribute's line as it is now (Remember::rememberAttrib) */
static void
remember_line(struct wm_view *view, enum wm_remember_attr attr, FILE *mem)
{
	struct wm_output *out = wm_output_for_view(view);
	struct wlr_box frame, area = out ? out->usable_area : (struct wlr_box){0};
	wm_view_frame_box(view, &frame);
	static const char *const deco[] = {
		[WM_DECOR_NORMAL] = "NORMAL", [WM_DECOR_NONE] = "NONE",
		[WM_DECOR_BORDER] = "BORDER", [WM_DECOR_TAB] = "TAB",
		[WM_DECOR_TINY] = "TINY", [WM_DECOR_TOOL] = "TOOL",
	};
	switch (attr) {
	case WM_REM_WORKSPACE:
		fprintf(mem, "  [Workspace]\t{%d}\n",
			view->workspace ? view->workspace->index : 0);
		break;
	case WM_REM_JUMP:
		fprintf(mem, "  [Jump]\t{yes}\n");
		break;
	case WM_REM_HEAD:
		fprintf(mem, "  [Head]\t{%d}\n",
			out ? wm_output_head_number(out) : 0);
		break;
	case WM_REM_DIMENSIONS:
		fprintf(mem, "  [Dimensions]\t{%d %d}\n", frame.width,
			frame.height);
		break;
	case WM_REM_POSITION:
		fprintf(mem, "  [Position]\t(UPPERLEFT)\t{%d %d}\n",
			frame.x - area.x, frame.y - area.y);
		break;
	case WM_REM_STICKY:
		fprintf(mem, "  [Sticky]\t{%s}\n", view->sticky ? "yes" : "no");
		break;
	case WM_REM_DECO:
		fprintf(mem, "  [Deco]\t{%s}\n", view->decoration ?
			deco[view->decoration->preset] : "NONE");
		break;
	case WM_REM_SHADED:
		fprintf(mem, "  [Shaded]\t{%s}\n", view->decoration &&
			view->decoration->shaded ? "yes" : "no");
		break;
	case WM_REM_MINIMIZED:
		fprintf(mem, "  [Minimized]\t{%s}\n",
			view->scene_tree && !view->scene_tree->node.enabled ?
			"yes" : "no");
		break;
	case WM_REM_MAXIMIZED:
		fprintf(mem, "  [Maximized]\t{%s}\n", view->maximized ? "yes" :
			view->maximized_horiz ? "horz" :
			view->maximized_vert ? "vert" : "no");
		break;
	case WM_REM_FULLSCREEN:
		fprintf(mem, "  [Fullscreen]\t{%s}\n",
			view->fullscreen ? "yes" : "no");
		break;
	case WM_REM_ALPHA:
		fprintf(mem, "  [Alpha]\t{%d %d}\n", view->focus_alpha,
			view->unfocus_alpha);
		break;
	case WM_REM_LAYER:
		fprintf(mem, "  [Layer]\t{%d}\n",
			wm_layer_fluxbox_number(view->layer));
		break;
	case WM_REM_CLOSE:
		fprintf(mem, "  [Close]\t{yes}\n");
		break;
	case WM_REM_COUNT:
		break;
	}
}

/* An entry line's attribute ("  [Workspace] {1}"), or -1 */
static int
line_attr(const char *line)
{
	while (*line == ' ' || *line == '\t')
		line++;
	if (*line != '[')
		return -1;
	for (int a = 0; a < WM_REM_COUNT; a++) {
		size_t n = strlen(remember_keys[a]);
		if (strncasecmp(line + 1, remember_keys[a], n) == 0 &&
		    line[1 + n] == ']')
			return a;
	}
	return -1;
}

/*
 * The apps file with the window's entry changed: the attribute toggled
 * (toggle), or just looked for. Returns whether the entry had it; the
 * new file in *content when toggling.
 */
static bool
edit_entry(struct wm_view *view, const char *apps_path,
	enum wm_remember_attr attr, bool toggle, char **content, size_t *len)
{
	char *header = remember_header(view);
	char legacy[512];
	snprintf(legacy, sizeof(legacy), "[app] (class=%s)",
		view->app_id ? view->app_id : "unknown");
	FILE *out = toggle ? open_memstream(content, len) : NULL;
	bool had = false, in_entry = false, found = false;
	/* The entry's lines, held until its end */
	char *entry = NULL;
	size_t entry_len = 0;
	FILE *ent = NULL;
	int kept = 0;
	FILE *in = header ? fopen_config(apps_path, "r") : NULL;
	char *line = NULL;
	size_t cap = 0;
	while (in && getline(&line, &cap, in) >= 0) {
		char copy[1024];
		snprintf(copy, sizeof(copy), "%s", line);
		char *s = strip(copy);
		if (!in_entry && !found && (strcmp(s, header) == 0 ||
		    strcmp(s, legacy) == 0)) {
			in_entry = found = true;
			ent = out ? open_memstream(&entry, &entry_len) : NULL;
			if (ent)
				fputs(line, ent);
			continue;
		}
		if (in_entry) {
			if (strncasecmp(s, "[end]", 5) == 0) {
				in_entry = false;
				if (ent) {
					if (!had)
						remember_line(view, attr, ent);
					fputs(line, ent);
					fclose(ent);
					ent = NULL;
					/* Empty now: it goes */
					if (kept > 0 || !had)
						fputs(entry, out);
					free(entry);
					entry = NULL;
				}
				continue;
			}
			if (line_attr(s) == (int)attr) {
				had = true;
				continue;	/* forgotten */
			}
			if (line_attr(s) >= 0)
				kept++;
			if (ent)
				fputs(line, ent);
			continue;
		}
		if (out)
			fputs(line, out);
	}
	free(line);
	if (in)
		fclose(in);
	if (ent) {
		/* No [end]: as it was */
		fclose(ent);
		fputs(entry, out);
		free(entry);
	}
	if (out) {
		if (!found) {
			/* A new entry with just this */
			fprintf(out, "%s\n", header);
			remember_line(view, attr, out);
			fprintf(out, "[end]\n");
		}
		fclose(out);
	}
	free(header);
	return had;
}

bool
wm_rules_is_remembered(struct wm_view *view, const char *apps_path,
	enum wm_remember_attr attr)
{
	if (!view || !apps_path || attr >= WM_REM_COUNT)
		return false;
	return edit_entry(view, apps_path, attr, false, NULL, NULL);
}

int
wm_rules_toggle_remembered(struct wm_view *view, const char *apps_path,
	enum wm_remember_attr attr)
{
	if (!view || !apps_path || attr >= WM_REM_COUNT)
		return -1;
	char *content = NULL;
	size_t len = 0;
	edit_entry(view, apps_path, attr, true, &content, &len);
	if (!content)
		return -1;
	int ret = -1;
	FILE *wf = fopen_config(apps_path, "w");
	if (wf) {
		if (fwrite(content, 1, len, wf) == len)
			ret = 0;
		if (fclose(wf) != 0)
			ret = -1;
	}
	free(content);
	return ret;
}

int
wm_rules_remember_window(struct wm_view *view, const char *apps_path)
{
	if (!view || !apps_path)
		return -1;
	char *header = remember_header(view);
	char *block = header ? remember_block(view, header) : NULL;
	if (!block) {
		free(header);
		return -1;
	}
	/* fluxland 1.x wrote "[app] (class=APP_ID)" */
	char legacy[512];
	snprintf(legacy, sizeof(legacy), "[app] (class=%s)",
		view->app_id ? view->app_id : "unknown");

	/* Replace the window's entry, keeping the rest of the file */
	char *content = NULL;
	size_t content_len = 0;
	FILE *out = open_memstream(&content, &content_len);
	if (!out) {
		free(header);
		free(block);
		return -1;
	}
	bool replaced = false, skipping = false;
	FILE *in = fopen_config(apps_path, "r");
	if (in) {
		char *line = NULL;
		size_t cap = 0;
		while (getline(&line, &cap, in) >= 0) {
			char copy[1024];
			snprintf(copy, sizeof(copy), "%s", line);
			char *s = strip(copy);
			if (skipping) {
				if (strncasecmp(s, "[end]", 5) == 0)
					skipping = false;
				continue;
			}
			if (!replaced && (strcmp(s, header) == 0 ||
			    strcmp(s, legacy) == 0)) {
				fputs(block, out);
				replaced = skipping = true;
				continue;
			}
			fputs(line, out);
		}
		free(line);
		fclose(in);
	}
	if (!replaced) {
		if (content_len > 0)
			fputc('\n', out);
		fputs(block, out);
	}
	fclose(out);

	int ret = -1;
	FILE *wf = fopen_config(apps_path, "w");
	if (wf) {
		if (fwrite(content, 1, content_len, wf) == content_len)
			ret = 0;
		if (fclose(wf) != 0)
			ret = -1;
	}
	if (ret == 0)
		wlr_log(WLR_INFO, "rules: remembered '%s' in %s",
			view->app_id ? view->app_id : "", apps_path);
	else
		wlr_log(WLR_ERROR, "rules: cannot write %s", apps_path);
	free(content);
	free(header);
	free(block);
	return ret;
}

/* view.h's: an apps entry's hold on a new X window's geometry */
void
wm_view_rule_holds_geometry(struct wm_view *view, bool *pos, bool *size)
{
	*pos = *size = false;
	const struct wm_rule_app *app = view->rule ? view->rule->app : NULL;
	if (!app)
		return;
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	int64_t now = (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
	if (now - view->mapped_ms >= 1000)
		return;
	bool all = app->has_maximized || app->has_fullscreen;
	*pos = all || app->has_position;
	*size = all || app->has_dimensions;
}
