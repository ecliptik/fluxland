/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * rules.c - Per-window rules parser and engine (Fluxbox apps file)
 */

#define _GNU_SOURCE

#include <ctype.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>

#include "rules.h"
#include "decoration.h"
#include "util.h"
#include "output.h"
#include "view.h"
#include "server.h"
#include "workspace.h"
#include "tabgroup.h"

/* --- Helpers --- */

/* Strip leading/trailing whitespace in-place, return pointer into buf */
static char *strip(char *s) {
	while (*s && isspace((unsigned char)*s))
		s++;
	char *end = s + strlen(s);
	while (end > s && isspace((unsigned char)end[-1]))
		end--;
	*end = '\0';
	return s;
}

/* Case-insensitive string comparison */
static bool streqi(const char *a, const char *b) {
	return strcasecmp(a, b) == 0;
}

/* Parse a boolean value: "yes"/"true"/"1" -> true, else false */
static bool parse_bool(const char *s) {
	return streqi(s, "yes") || streqi(s, "true") || streqi(s, "1");
}

/* Parse a decoration preset string */
static bool parse_deco_preset(const char *s, enum wm_decor_preset *out) {
	if (streqi(s, "NORMAL") || streqi(s, "FULL")) {
		*out = WM_DECOR_NORMAL;
	} else if (streqi(s, "NONE") || streqi(s, "0")) {
		*out = WM_DECOR_NONE;
	} else if (streqi(s, "BORDER")) {
		*out = WM_DECOR_BORDER;
	} else if (streqi(s, "TAB")) {
		*out = WM_DECOR_TAB;
	} else if (streqi(s, "TINY")) {
		*out = WM_DECOR_TINY;
	} else if (streqi(s, "TOOL")) {
		*out = WM_DECOR_TOOL;
	} else {
		return false;
	}
	return true;
}

/*
 * Parse match patterns from an [app] or [group] header line.
 * Format: [app] (property=regex) (property!=regex) ...
 * Or:     [group] (workspace)
 *
 * Returns number of patterns parsed. Patterns are allocated and stored
 * in *out_patterns. Caller must free.
 */
static int parse_patterns(const char *line, struct wm_match_pattern **out_patterns) {
	int capacity = 4;
	int count = 0;
	struct wm_match_pattern *pats = calloc(capacity, sizeof(*pats));
	if (!pats)
		return 0;

	const char *p = line;
	while ((p = strchr(p, '(')) != NULL) {
		p++; /* skip '(' */
		const char *close = strchr(p, ')');
		if (!close)
			break;

		/* Extract content between parens */
		int len = (int)(close - p);
		char *content = strndup(p, len);
		if (!content)
			break;

		/* Check for property=value or property!=value */
		char *eq = strstr(content, "!=");
		bool negate = false;
		char *prop = NULL;
		char *regex = NULL;

		if (eq) {
			negate = true;
			*eq = '\0';
			prop = strip(content);
			regex = strip(eq + 2);
		} else {
			eq = strchr(content, '=');
			if (eq) {
				*eq = '\0';
				prop = strip(content);
				regex = strip(eq + 1);
			} else {
				/* No '=' — skip (could be a group modifier like "workspace") */
				free(content);
				p = close + 1;
				continue;
			}
		}

		if (prop[0] == '\0' || regex[0] == '\0') {
			free(content);
			p = close + 1;
			continue;
		}

		/* Grow array if needed */
		if (count >= capacity) {
			capacity *= 2;
			struct wm_match_pattern *tmp = realloc(pats, capacity * sizeof(*pats));
			if (!tmp) {
				free(content);
				break;
			}
			pats = tmp;
		}

		struct wm_match_pattern *pat = &pats[count];
		memset(pat, 0, sizeof(*pat));
		pat->property = strdup(prop);
		pat->regex_str = strdup(regex);
		if (!pat->property || !pat->regex_str) {
			free(pat->property);
			free(pat->regex_str);
			free(content);
			p = close + 1;
			continue;
		}
		pat->negate = negate;

		/* Cap regex length to prevent ReDoS / excessive compilation cost */
		if (strlen(regex) > 1024) {
			wlr_log(WLR_ERROR, "rules: regex too long (%zu bytes): %.32s...",
				strlen(regex), regex);
			free(pat->property);
			free(pat->regex_str);
			free(content);
			p = close + 1;
			continue;
		}

		int rc = regcomp(&pat->compiled, regex, REG_EXTENDED | REG_NOSUB);
		if (rc != 0) {
			char errbuf[128];
			regerror(rc, &pat->compiled, errbuf, sizeof(errbuf));
			wlr_log(WLR_ERROR, "rules: bad regex '%s': %s", regex, errbuf);
			free(pat->property);
			free(pat->regex_str);
			free(content);
			p = close + 1;
			continue;
		}

		count++;
		free(content);
		p = close + 1;
	}

	if (count == 0) {
		free(pats);
		*out_patterns = NULL;
		return 0;
	}

	*out_patterns = pats;
	return count;
}

/* Free an array of match patterns */
static void free_patterns(struct wm_match_pattern *pats, int count) {
	for (int i = 0; i < count; i++) {
		free(pats[i].property);
		free(pats[i].regex_str);
		regfree(&pats[i].compiled);
	}
	free(pats);
}

/*
 * Parse a setting line inside an [app] block.
 * Format: [Setting] {value} or [Setting] (modifier) {value}
 */
static void parse_rule_setting(const char *line, struct wm_window_rule *rule) {
	/* Find [Setting] */
	const char *open = strchr(line, '[');
	if (!open)
		return;
	const char *close = strchr(open, ']');
	if (!close)
		return;

	int name_len = (int)(close - open - 1);
	char name[64];
	if (name_len <= 0 || name_len >= (int)sizeof(name))
		return;
	memcpy(name, open + 1, name_len);
	name[name_len] = '\0';

	/* Find {value} */
	const char *brace_open = strchr(close, '{');
	const char *brace_close = brace_open ? strchr(brace_open, '}') : NULL;
	char value[256] = "";
	if (brace_open && brace_close) {
		int val_len = (int)(brace_close - brace_open - 1);
		if (val_len > 0 && val_len < (int)sizeof(value)) {
			memcpy(value, brace_open + 1, val_len);
			value[val_len] = '\0';
		}
	}

	/* Find optional (modifier) between ] and { */
	char modifier[64] = "";
	const char *mod_open = strchr(close, '(');
	if (mod_open && (!brace_open || mod_open < brace_open)) {
		const char *mod_close = strchr(mod_open, ')');
		if (mod_close) {
			int mod_len = (int)(mod_close - mod_open - 1);
			if (mod_len > 0 && mod_len < (int)sizeof(modifier)) {
				memcpy(modifier, mod_open + 1, mod_len);
				modifier[mod_len] = '\0';
			}
		}
	}

	char *val = strip(value);

	if (streqi(name, "Workspace")) {
		int ws;
		if (safe_atoi(val, &ws)) {
			rule->has_workspace = true;
			rule->workspace = ws;
		}
	} else if (streqi(name, "Dimensions")) {
		int w = 0, h = 0;
		if (sscanf(val, "%d %d", &w, &h) == 2) {
			rule->has_dimensions = true;
			rule->width = w;
			rule->height = h;
		}
	} else if (streqi(name, "Position")) {
		int x = 0, y = 0;
		if (sscanf(val, "%d %d", &x, &y) == 2) {
			rule->has_position = true;
			rule->pos_x = x;
			rule->pos_y = y;
		}
		if (modifier[0]) {
			free(rule->pos_anchor);
			rule->pos_anchor = strdup(modifier);
		}
	} else if (streqi(name, "Deco")) {
		enum wm_decor_preset preset;
		if (parse_deco_preset(val, &preset)) {
			rule->has_deco = true;
			rule->deco = preset;
		}
	} else if (streqi(name, "Sticky")) {
		rule->has_sticky = true;
		rule->sticky = parse_bool(val);
	} else if (streqi(name, "Layer")) {
		int layer;
		if (safe_atoi(val, &layer)) {
			rule->has_layer = true;
			rule->layer = layer;
		}
	} else if (streqi(name, "Alpha")) {
		int fa = 255, ua = 255;
		if (sscanf(val, "%d %d", &fa, &ua) >= 1) {
			rule->has_alpha = true;
			rule->focus_alpha = fa;
			rule->unfocus_alpha = (ua != 255 || sscanf(val, "%*d %d", &ua) == 1) ? ua : fa;
		}
	} else if (streqi(name, "Maximized")) {
		rule->has_maximized = true;
		rule->maximized = parse_bool(val);
	} else if (streqi(name, "Minimized")) {
		rule->has_minimized = true;
		rule->minimized = parse_bool(val);
	} else if (streqi(name, "Fullscreen")) {
		rule->has_fullscreen = true;
		rule->fullscreen = parse_bool(val);
	} else if (streqi(name, "Shaded")) {
		rule->has_shaded = true;
		rule->shaded = parse_bool(val);
	} else if (streqi(name, "FocusNewWindow")) {
		rule->has_focus_new = true;
		rule->focus_new = parse_bool(val);
	} else if (streqi(name, "Head")) {
		int head;
		if (safe_atoi(val, &head)) {
			rule->has_head = true;
			rule->head = head;
		}
	} else if (streqi(name, "FocusProtection")) {
		rule->has_focus_protection = true;
		rule->focus_protection = 0;
		/* Parse comma-separated flags */
		char buf[256];
		strncpy(buf, val, sizeof(buf) - 1);
		buf[sizeof(buf) - 1] = '\0';
		char *saveptr = NULL;
		char *tok = strtok_r(buf, ",", &saveptr);
		while (tok) {
			char *t = strip(tok);
			if (streqi(t, "Gain"))
				rule->focus_protection |= WM_FOCUS_PROT_GAIN;
			else if (streqi(t, "Refuse"))
				rule->focus_protection |= WM_FOCUS_PROT_REFUSE;
			else if (streqi(t, "Deny"))
				rule->focus_protection |= WM_FOCUS_PROT_DENY;
			else if (streqi(t, "Lock"))
				rule->focus_protection |= WM_FOCUS_PROT_LOCK;
			else
				wlr_log(WLR_ERROR,
					"rules: unknown FocusProtection value '%s'", t);
			tok = strtok_r(NULL, ",", &saveptr);
		}
	} else if (streqi(name, "IgnoreSizeHints")) {
		rule->has_ignore_size_hints = true;
		rule->ignore_size_hints = parse_bool(val);
	}
}

/* --- Public API --- */

void wm_rules_init(struct wm_rules *rules) {
	wl_list_init(&rules->window_rules);
	wl_list_init(&rules->group_rules);
}

void wm_rules_finish(struct wm_rules *rules) {
	struct wm_window_rule *wr, *wr_tmp;
	wl_list_for_each_safe(wr, wr_tmp, &rules->window_rules, link) {
		wl_list_remove(&wr->link);
		free_patterns(wr->patterns, wr->pattern_count);
		free(wr->pos_anchor);
		free(wr);
	}

	struct wm_group_rule *gr, *gr_tmp;
	wl_list_for_each_safe(gr, gr_tmp, &rules->group_rules, link) {
		wl_list_remove(&gr->link);
		free_patterns(gr->patterns, gr->pattern_count);
		free(gr);
	}
}

bool wm_rules_load(struct wm_rules *rules, const char *path) {
	FILE *f = fopen_nofollow(path, "r");
	if (!f) {
		wlr_log(WLR_INFO, "rules: no apps file at %s", path);
		return false;
	}

	wlr_log(WLR_INFO, "rules: loading %s", path);

	char line[1024];
	struct wm_window_rule *current_rule = NULL;
	bool in_group = false;
	struct wm_group_rule *current_group = NULL;

	while (fgets(line, sizeof(line), f)) {
		/* Skip remainder of lines longer than buffer */
		size_t len = strlen(line);
		if (len > 0 && line[len - 1] != '\n' && !feof(f)) {
			int ch;
			while ((ch = fgetc(f)) != EOF && ch != '\n')
				;
			continue;
		}

		char *s = strip(line);

		/* Skip blank lines and comments */
		if (s[0] == '\0' || s[0] == '#' || s[0] == '!')
			continue;

		/* [end] closes the current block */
		if (strncasecmp(s, "[end]", 5) == 0) {
			if (current_rule) {
				wl_list_insert(rules->window_rules.prev, &current_rule->link);
				current_rule = NULL;
			}
			if (in_group && current_group) {
				wl_list_insert(rules->group_rules.prev, &current_group->link);
				current_group = NULL;
				in_group = false;
			}
			continue;
		}

		/* [group] header */
		if (strncasecmp(s, "[group]", 7) == 0) {
			in_group = true;
			current_group = calloc(1, sizeof(*current_group));
			if (!current_group)
				continue;
			/* Check for (workspace) modifier */
			if (strcasestr(s, "(workspace)"))
				current_group->workspace_scope = true;
			continue;
		}

		/* [app] header */
		if (strncasecmp(s, "[app]", 5) == 0) {
			if (in_group && current_group) {
				/* Inside a [group] block: add patterns to group rule */
				struct wm_match_pattern *pats = NULL;
				int count = parse_patterns(s, &pats);
				if (count > 0) {
					int old_count = current_group->pattern_count;
					int new_count = old_count + count;
					struct wm_match_pattern *merged = realloc(
						current_group->patterns,
						new_count * sizeof(*merged));
					if (merged) {
						memcpy(&merged[old_count], pats,
							count * sizeof(*pats));
						current_group->patterns = merged;
						current_group->pattern_count = new_count;
						/* Free the temporary array (not the contents,
						 * which are now owned by merged) */
						free(pats);
					} else {
						free_patterns(pats, count);
					}
				}
			} else {
				/* Standalone [app] block: create window rule */
				current_rule = calloc(1, sizeof(*current_rule));
				if (!current_rule)
					continue;
				current_rule->pattern_count = parse_patterns(s,
					&current_rule->patterns);
			}
			continue;
		}

		/* Setting line inside an [app] block */
		if (current_rule && s[0] == '[') {
			parse_rule_setting(s, current_rule);
		}
	}

	/* Handle unterminated blocks */
	if (current_rule) {
		wlr_log(WLR_ERROR, "rules: unterminated [app] block%s", "");
		free_patterns(current_rule->patterns, current_rule->pattern_count);
		free(current_rule->pos_anchor);
		free(current_rule);
	}
	if (current_group) {
		wlr_log(WLR_ERROR, "rules: unterminated [group] block%s", "");
		free_patterns(current_group->patterns, current_group->pattern_count);
		free(current_group);
	}

	fclose(f);

	int wcount = 0, gcount = 0;
	struct wm_window_rule *wr;
	wl_list_for_each(wr, &rules->window_rules, link)
		wcount++;
	struct wm_group_rule *gr;
	wl_list_for_each(gr, &rules->group_rules, link)
		gcount++;
	wlr_log(WLR_INFO, "rules: loaded %d window rule(s), %d group rule(s)",
		wcount, gcount);

	return true;
}

/*
 * Get the value of a property from a view for matching purposes.
 * Returns a static/view-owned string (do not free).
 */
static const char *view_get_property(struct wm_view *view,
		const char *property) {
	if (streqi(property, "class") || streqi(property, "name")) {
		return view->app_id ? view->app_id : "";
	} else if (streqi(property, "title")) {
		return view->title ? view->title : "";
	} else if (streqi(property, "transient")) {
		bool is_transient = view->xdg_toplevel &&
			view->xdg_toplevel->parent != NULL;
		return is_transient ? "yes" : "no";
	}
	return "";
}

/*
 * Test whether a single pattern matches a view.
 */
static bool pattern_matches(struct wm_match_pattern *pat,
		struct wm_view *view) {
	const char *val = view_get_property(view, pat->property);
	int rc = regexec(&pat->compiled, val, 0, NULL, 0);
	bool matched = (rc == 0);
	return pat->negate ? !matched : matched;
}

/*
 * Test whether all patterns in a rule match a view (AND-combined).
 */
static bool rule_matches(struct wm_match_pattern *patterns, int count,
		struct wm_view *view) {
	for (int i = 0; i < count; i++) {
		if (!pattern_matches(&patterns[i], view))
			return false;
	}
	return count > 0;
}

void wm_rules_apply(struct wm_rules *rules, struct wm_view *view) {
	struct wm_window_rule *rule;
	wl_list_for_each(rule, &rules->window_rules, link) {
		if (!rule_matches(rule->patterns, rule->pattern_count, view))
			continue;

		wlr_log(WLR_DEBUG, "rules: matched rule for app_id='%s' title='%s'",
			view->app_id ? view->app_id : "",
			view->title ? view->title : "");

		/* Apply workspace */
		if (rule->has_workspace && view->server) {
			struct wm_workspace *ws = wm_workspace_get(
				view->server, rule->workspace);
			if (ws) {
				wm_view_move_to_workspace(view, ws);
			}
		}

		/* Apply dimensions */
		if (rule->has_dimensions && view->xdg_toplevel) {
			wlr_xdg_toplevel_set_size(view->xdg_toplevel,
				rule->width, rule->height);
		}

		/* Apply position */
		if (rule->has_position) {
			int px = rule->pos_x;
			int py = rule->pos_y;

			if (rule->pos_anchor && view->server) {
				/* Get output area for anchor calculation */
				struct wlr_box area = {0};
				struct wlr_output *wlr_output =
					wlr_output_layout_output_at(
						view->server->output_layout,
						view->server->cursor->x,
						view->server->cursor->y);
				if (wlr_output) {
					struct wm_output *out;
					wl_list_for_each(out,
					    &view->server->outputs, link) {
						if (out->wlr_output ==
						    wlr_output) {
							if (out->usable_area
							    .width > 0)
								area = out
								    ->usable_area;
							else
								wlr_output_layout_get_box(
								    view->server
								    ->output_layout,
								    wlr_output,
								    &area);
							break;
						}
					}
				}

				if (area.width > 0 && area.height > 0) {
					/* Get view dimensions */
					int vw = 0, vh = 0;
					if (view->xdg_toplevel) {
						struct wlr_box geo;
						wlr_xdg_surface_get_geometry(
							view->xdg_toplevel
							    ->base,
							&geo);
						vw = geo.width;
						vh = geo.height;
					}

					if (strcasecmp(rule->pos_anchor,
					    "Center") == 0) {
						px = area.x +
						    (area.width - vw) / 2 +
						    rule->pos_x;
						py = area.y +
						    (area.height - vh) / 2 +
						    rule->pos_y;
					} else if (strcasecmp(
					    rule->pos_anchor,
					    "UpperRight") == 0) {
						px = area.x + area.width -
						    vw + rule->pos_x;
						py = area.y + rule->pos_y;
					} else if (strcasecmp(
					    rule->pos_anchor,
					    "LowerLeft") == 0) {
						px = area.x + rule->pos_x;
						py = area.y + area.height -
						    vh + rule->pos_y;
					} else if (strcasecmp(
					    rule->pos_anchor,
					    "LowerRight") == 0) {
						px = area.x + area.width -
						    vw + rule->pos_x;
						py = area.y + area.height -
						    vh + rule->pos_y;
					} else {
						/* UpperLeft or default */
						px = area.x + rule->pos_x;
						py = area.y + rule->pos_y;
					}
				}
			}

			view->x = px;
			view->y = py;
			if (view->scene_tree) {
				wlr_scene_node_set_position(
					&view->scene_tree->node,
					view->x, view->y);
			}
		}

		/* Apply decoration preset */
		if (rule->has_deco && view->decoration) {
			wm_decoration_set_preset(view->decoration,
				rule->deco, view->server->style);
		}

		/* Apply sticky */
		if (rule->has_sticky) {
			wm_view_set_sticky(view, rule->sticky);
		}

		/* Apply maximized */
		if (rule->has_maximized && rule->maximized &&
		    view->xdg_toplevel && view->server) {
			/* Save current geometry for later restore */
			wm_view_get_geometry(view, &view->saved_geometry);

			/* Find output to maximize onto */
			struct wlr_output *max_output =
				wlr_output_layout_output_at(
					view->server->output_layout,
					view->server->cursor->x,
					view->server->cursor->y);
			if (max_output) {
				struct wlr_box max_box;
				wlr_output_layout_get_box(
					view->server->output_layout,
					max_output, &max_box);
				wlr_xdg_toplevel_set_size(
					view->xdg_toplevel,
					max_box.width, max_box.height);
				view->x = max_box.x;
				view->y = max_box.y;
			}

			view->maximized = true;
			wlr_xdg_toplevel_set_maximized(
				view->xdg_toplevel, true);
		}

		/* Apply fullscreen */
		if (rule->has_fullscreen && rule->fullscreen) {
			view->fullscreen = true;
			if (view->xdg_toplevel) {
				wlr_xdg_toplevel_set_fullscreen(
					view->xdg_toplevel, true);
			}
		}

		/* Apply shaded */
		if (rule->has_shaded && rule->shaded && view->decoration) {
			wm_decoration_set_shaded(view->decoration, true,
				view->server->style);
		}

		/* Apply alpha/opacity */
		if (rule->has_alpha) {
			view->focus_alpha = rule->focus_alpha;
			view->unfocus_alpha = rule->unfocus_alpha;
		}

		/* Apply focus protection */
		if (rule->has_focus_protection) {
			view->focus_protection = rule->focus_protection;
		}

		/* Apply ignore size hints */
		if (rule->has_ignore_size_hints) {
			view->ignore_size_hints = rule->ignore_size_hints;
		}

		/* First match wins */
		return;
	}
}

static const char *
deco_preset_name(enum wm_decor_preset preset) {
	switch (preset) {
	case WM_DECOR_NONE:   return "NONE";
	case WM_DECOR_BORDER: return "BORDER";
	case WM_DECOR_TAB:    return "TAB";
	case WM_DECOR_TINY:   return "TINY";
	case WM_DECOR_TOOL:   return "TOOL";
	default:              return "NORMAL";
	}
}

static const char *
layer_name(enum wm_view_layer layer) {
	switch (layer) {
	case WM_LAYER_DESKTOP: return "Desktop";
	case WM_LAYER_BELOW:   return "Below";
	case WM_LAYER_ABOVE:   return "Above";
	default:               return "Normal";
	}
}

int wm_rules_remember_window(struct wm_view *view, const char *apps_path) {
	if (!view || !apps_path)
		return -1;

	FILE *f = fopen_nofollow(apps_path, "a");
	if (!f) {
		wlr_log(WLR_ERROR, "rules: cannot open %s for appending",
			apps_path);
		return -1;
	}

	const char *app_id = view->app_id ? view->app_id : "unknown";

	/* Get current geometry */
	struct wlr_box geo;
	wm_view_get_geometry(view, &geo);

	int ws_index = view->workspace ? view->workspace->index : 0;

	fprintf(f, "\n[app] (class=%s)\n", app_id);
	fprintf(f, "  [Workspace] {%d}\n", ws_index);
	fprintf(f, "  [Position] {%d %d}\n", geo.x, geo.y);
	fprintf(f, "  [Dimensions] {%d %d}\n", geo.width, geo.height);
	fprintf(f, "  [Layer] {%s}\n", layer_name(view->layer));
	fprintf(f, "  [Alpha] {%d %d}\n",
		view->focus_alpha, view->unfocus_alpha);
	fprintf(f, "  [Sticky] {%s}\n", view->sticky ? "yes" : "no");
	fprintf(f, "  [Maximized] {%s}\n", view->maximized ? "yes" : "no");
	fprintf(f, "  [Fullscreen] {%s}\n", view->fullscreen ? "yes" : "no");

	/* Decoration preset */
	if (view->decoration) {
		fprintf(f, "  [Deco] {%s}\n",
			deco_preset_name(view->decoration->preset));
	}

	fprintf(f, "[end]\n");
	fclose(f);

	wlr_log(WLR_INFO, "rules: remembered window '%s' to %s",
		app_id, apps_path);
	return 0;
}

struct wm_view *wm_rules_find_group(struct wm_rules *rules,
		struct wm_view *view, struct wm_server *server) {
	struct wm_group_rule *group;
	wl_list_for_each(group, &rules->group_rules, link) {
		/* Check if the new view matches any pattern in this group */
		bool new_view_matches = false;
		for (int i = 0; i < group->pattern_count; i++) {
			if (pattern_matches(&group->patterns[i], view)) {
				new_view_matches = true;
				break;
			}
		}
		if (!new_view_matches)
			continue;

		/* Find an existing view that also matches this group */
		struct wm_view *candidate;
		wl_list_for_each(candidate, &server->views, link) {
			if (candidate == view)
				continue;

			/* If workspace-scoped, only match on same workspace */
			if (group->workspace_scope &&
			    candidate->workspace != view->workspace)
				continue;

			for (int i = 0; i < group->pattern_count; i++) {
				if (pattern_matches(&group->patterns[i], candidate)) {
					wlr_log(WLR_DEBUG,
						"rules: auto-group '%s' with '%s'",
						view->app_id ? view->app_id : "",
						candidate->app_id ? candidate->app_id : "");
					return candidate;
				}
			}
		}
	}
	return NULL;
}
