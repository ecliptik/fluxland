/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * menu_parse.c - Fluxbox menu file parser
 *
 * Parses the Fluxbox menu file format including:
 *   [begin], [end], [exec], [submenu], [separator], [nop],
 *   [include], [config], [workspaces], [reconfig], [restart],
 *   [exit], [style], [stylesdir], [stylesmenu], [wallpapers],
 *   [command], [encoding]/[endencoding], and window menu items.
 */

#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <iconv.h>
#include <unistd.h>
#include <wlr/util/log.h>

#include "config.h"
#include "i18n.h"
#include "menu.h"
#include "menu_parse.h"
#include "server.h"
#include "util.h"

/* --- String helpers --- */

static char *
strip_whitespace(char *str)
{
	while (*str && isspace((unsigned char)*str)) {
		str++;
	}
	char *end = str + strlen(str) - 1;
	while (end > str && isspace((unsigned char)*end)) {
		*end-- = '\0';
	}
	return str;
}

/* Expand ~ to home directory */
static char *
expand_path(const char *path)
{
	if (!path) {
		return NULL;
	}
	if (path[0] == '~' && (path[1] == '/' || path[1] == '\0')) {
		const char *home = getenv("HOME");
		if (!home) {
			home = "/tmp";
		}
		char *expanded;
		if (asprintf(&expanded, "%s%s", home, path + 1) < 0) {
			return strdup(path);
		}
		return expanded;
	}
	return strdup(path);
}

/*
 * Parse a parenthesized value: "(text)" -> "text"
 * Advances *pos past the closing paren.
 * Returns allocated string or NULL.
 */
static char *
parse_paren(const char **pos)
{
	const char *p = *pos;
	while (*p && isspace((unsigned char)*p)) {
		p++;
	}
	if (*p != '(') {
		return NULL;
	}
	p++;
	const char *start = p;
	int depth = 1;
	while (*p && depth > 0) {
		if (*p == '(') {
			depth++;
		} else if (*p == ')') {
			depth--;
		}
		if (depth > 0) {
			p++;
		}
	}
	char *result = strndup(start, p - start);
	if (*p == ')') {
		p++;
	}
	*pos = p;
	return result;
}

/*
 * Parse a brace-enclosed value: "{text}" -> "text"
 * Advances *pos past the closing brace.
 */
static char *
parse_brace(const char **pos)
{
	const char *p = *pos;
	while (*p && isspace((unsigned char)*p)) {
		p++;
	}
	if (*p != '{') {
		return NULL;
	}
	p++;
	const char *start = p;
	int depth = 1;
	while (*p && depth > 0) {
		if (*p == '{') {
			depth++;
		} else if (*p == '}') {
			depth--;
		}
		if (depth > 0) {
			p++;
		}
	}
	char *result = strndup(start, p - start);
	if (*p == '}') {
		p++;
	}
	*pos = p;
	return result;
}

/*
 * Parse an angle-bracket-enclosed value: "<text>" -> "text"
 */
static char *
parse_angle(const char **pos)
{
	const char *p = *pos;
	while (*p && isspace((unsigned char)*p)) {
		p++;
	}
	if (*p != '<') {
		return NULL;
	}
	p++;
	const char *start = p;
	while (*p && *p != '>') {
		p++;
	}
	char *result = strndup(start, p - start);
	if (*p == '>') {
		p++;
	}
	*pos = p;
	return result;
}

/* --- Encoding conversion --- */

/*
 * If an [encoding] block is active, convert label from that encoding to UTF-8.
 * Takes ownership of label; returns converted string (caller must free).
 * If no conversion needed, returns label unchanged.
 */
static char *
maybe_convert_label(char *label, const char *encoding)
{
	if (!label || !encoding)
		return label;
	if (strcasecmp(encoding, "UTF-8") == 0)
		return label;

	iconv_t cd = iconv_open("UTF-8", encoding);
	if (cd == (iconv_t)-1) {
		wlr_log(WLR_ERROR, "menu: unknown encoding '%s'", encoding);
		return label;
	}

	size_t inlen = strlen(label);
	size_t outlen = inlen * 4 + 1;
	char *outbuf = malloc(outlen);
	if (!outbuf) {
		iconv_close(cd);
		return label;
	}

	char *inp = label;
	char *outp = outbuf;
	size_t inleft = inlen;
	size_t outleft = outlen - 1;

	if (iconv(cd, &inp, &inleft, &outp, &outleft) == (size_t)-1) {
		iconv_close(cd);
		free(outbuf);
		return label;
	}

	*outp = '\0';
	iconv_close(cd);
	free(label);
	return outbuf;
}

/* --- Menu allocation --- */

struct wm_menu *
menu_create(struct wm_server *server, const char *title)
{
	struct wm_menu *menu = calloc(1, sizeof(*menu));
	if (!menu) {
		return NULL;
	}
	menu->title = title ? strdup(title) : NULL;
	wl_list_init(&menu->items);
	menu->server = server;
	menu->selected_index = -1;
	return menu;
}

struct wm_menu_item *
menu_item_create(enum wm_menu_item_type type, const char *label)
{
	struct wm_menu_item *item = calloc(1, sizeof(*item));
	if (!item) {
		return NULL;
	}
	item->type = type;
	item->label = label ? strdup(label) : NULL;
	wl_list_init(&item->link);
	return item;
}

void
menu_item_destroy(struct wm_menu_item *item)
{
	if (!item) {
		return;
	}
	free(item->label);
	free(item->command);
	free(item->icon_path);
	if (item->submenu) {
		wm_menu_destroy(item->submenu);
	}
	free(item);
}

/* --- Stylesdir: scan directory for style files --- */

static void
add_stylesdir_entries(struct wm_menu *submenu, const char *raw_path)
{
	char *dir_path = expand_path(raw_path);
	if (!dir_path) {
		return;
	}

	DIR *dir = opendir(dir_path);
	if (!dir) {
		free(dir_path);
		return;
	}

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_name[0] == '.') {
			continue;
		}
		struct wm_menu_item *item =
			menu_item_create(WM_MENU_STYLE, entry->d_name);
		if (item) {
			if (asprintf(&item->command, "%s/%s", dir_path,
				     entry->d_name) < 0) {
				item->command = NULL;
			}
			wl_list_insert(submenu->items.prev, &item->link);
		}
	}

	closedir(dir);
	free(dir_path);
}

/* --- Directory scanning helpers --- */

static int
strcmp_qsort(const void *a, const void *b)
{
	return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static bool
is_image_file(const char *name)
{
	const char *ext = strrchr(name, '.');
	if (!ext)
		return false;
	return strcasecmp(ext, ".png") == 0 ||
	       strcasecmp(ext, ".jpg") == 0 ||
	       strcasecmp(ext, ".jpeg") == 0 ||
	       strcasecmp(ext, ".bmp") == 0 ||
	       strcasecmp(ext, ".gif") == 0 ||
	       strcasecmp(ext, ".webp") == 0;
}

static void
add_wallpaper_entries(struct wm_menu *submenu, const char *raw_path)
{
	char *dir_path = expand_path(raw_path);
	if (!dir_path) {
		struct wm_menu_item *item =
			menu_item_create(WM_MENU_NOP, "(empty)");
		if (item)
			wl_list_insert(submenu->items.prev, &item->link);
		return;
	}

	/* Reject directory paths containing single quotes to prevent
	 * shell escape from the single-quoted swaybg command */
	if (strchr(dir_path, '\'')) {
		wlr_log(WLR_ERROR,
			"wallpaper directory path contains single quote, "
			"skipping: %s", dir_path);
		struct wm_menu_item *item =
			menu_item_create(WM_MENU_NOP, "(invalid path)");
		if (item)
			wl_list_insert(submenu->items.prev, &item->link);
		free(dir_path);
		return;
	}

	DIR *dir = opendir(dir_path);
	if (!dir) {
		struct wm_menu_item *item =
			menu_item_create(WM_MENU_NOP, "(empty)");
		if (item)
			wl_list_insert(submenu->items.prev, &item->link);
		free(dir_path);
		return;
	}

	/* Collect image filenames */
	char **names = NULL;
	size_t count = 0;
	size_t cap = 0;
	struct dirent *entry;

	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_name[0] == '.')
			continue;
		if (!is_image_file(entry->d_name))
			continue;
		if (count >= cap) {
			cap = cap ? cap * 2 : 32;
			char **tmp = realloc(names, cap * sizeof(*names));
			if (!tmp)
				break;
			names = tmp;
		}
		names[count] = strdup(entry->d_name);
		if (names[count])
			count++;
	}
	closedir(dir);

	if (count == 0) {
		struct wm_menu_item *item =
			menu_item_create(WM_MENU_NOP, "(empty)");
		if (item)
			wl_list_insert(submenu->items.prev, &item->link);
		free(names);
		free(dir_path);
		return;
	}

	qsort(names, count, sizeof(*names), strcmp_qsort);

	for (size_t i = 0; i < count; i++) {
		struct wm_menu_item *item =
			menu_item_create(WM_MENU_EXEC, names[i]);
		if (item) {
			/* Shell-escape path components to prevent
			 * command injection via malicious filenames */
			if (asprintf(&item->command,
				     "swaybg -i '%s/%s' -m fill",
				     dir_path, names[i]) < 0)
				item->command = NULL;
			/* Reject filenames containing single quotes to
			 * prevent shell escape from the quoting */
			if (item->command && strchr(names[i], '\'')) {
				free(item->command);
				item->command = NULL;
			}
			wl_list_insert(submenu->items.prev, &item->link);
		}
		free(names[i]);
	}
	free(names);
	free(dir_path);
}

static void
add_stylesmenu_entries(struct wm_menu *submenu, const char *raw_path)
{
	char *dir_path = expand_path(raw_path);
	if (!dir_path) {
		wlr_log(WLR_ERROR, "%s", "menu: failed to expand stylesmenu path");
		return;
	}

	DIR *dir = opendir(dir_path);
	if (!dir) {
		wlr_log(WLR_ERROR, "menu: cannot open styles directory: %s",
			dir_path);
		free(dir_path);
		return;
	}

	/* Collect entry names */
	char **names = NULL;
	size_t count = 0;
	size_t cap = 0;
	struct dirent *entry;

	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_name[0] == '.')
			continue;
		if (count >= cap) {
			cap = cap ? cap * 2 : 32;
			char **tmp = realloc(names, cap * sizeof(*names));
			if (!tmp)
				break;
			names = tmp;
		}
		names[count] = strdup(entry->d_name);
		if (names[count])
			count++;
	}
	closedir(dir);

	if (count > 0) {
		qsort(names, count, sizeof(*names), strcmp_qsort);
		for (size_t i = 0; i < count; i++) {
			struct wm_menu_item *item =
				menu_item_create(WM_MENU_STYLE, names[i]);
			if (item) {
				if (asprintf(&item->command, "%s/%s",
					     dir_path, names[i]) < 0)
					item->command = NULL;
				wl_list_insert(submenu->items.prev,
					&item->link);
			}
			free(names[i]);
		}
	}

	free(names);
	free(dir_path);
}

/* --- Parser --- */

/*
 * Recursively parse menu items from a file stream.
 * Stops at [end] or EOF.  Depth is limited to prevent stack overflow
 * from circular [include] or deeply nested submenus.
 */
#define MENU_MAX_DEPTH 16

static void
parse_menu_items_depth(struct wm_menu *menu, FILE *fp,
	struct wm_server *server, int depth)
{
	if (depth > MENU_MAX_DEPTH) {
		wlr_log(WLR_ERROR, "%s", "menu: max include/submenu depth exceeded");
		return;
	}

	char line[1024];
	char *encoding = NULL;

	while (fgets(line, sizeof(line), fp)) {
		/* Skip remainder of lines longer than buffer */
		size_t len = strlen(line);
		if (len > 0 && line[len - 1] != '\n' && !feof(fp)) {
			int ch;
			while ((ch = fgetc(fp)) != EOF && ch != '\n')
				;
			continue;
		}

		char *trimmed = strip_whitespace(line);
		if (*trimmed == '\0' || *trimmed == '#') {
			continue;
		}

		/* Extract tag: [tagname] */
		if (*trimmed != '[') {
			continue;
		}
		char *tag_start = trimmed + 1;
		char *tag_end = strchr(tag_start, ']');
		if (!tag_end) {
			continue;
		}
		*tag_end = '\0';
		char *tag = tag_start;
		const char *rest = tag_end + 1;

		if (strcasecmp(tag, "end") == 0) {
			free(encoding);
			return;
		}

		if (strcasecmp(tag, "begin") == 0) {
			/* Already in a menu; update title */
			char *title = parse_paren(&rest);
			if (title) {
				free(menu->title);
				menu->title = title;
			}
			continue;
		}

		if (strcasecmp(tag, "encoding") == 0) {
			char *enc = parse_brace(&rest);
			free(encoding);
			encoding = enc;
			continue;
		}

		if (strcasecmp(tag, "endencoding") == 0) {
			free(encoding);
			encoding = NULL;
			continue;
		}

		if (strcasecmp(tag, "exec") == 0) {
			char *label = parse_paren(&rest);
			label = maybe_convert_label(label, encoding);
			char *command = parse_brace(&rest);
			char *icon = parse_angle(&rest);
			struct wm_menu_item *item =
				menu_item_create(WM_MENU_EXEC, label);
			if (item) {
				item->command = command;
				item->icon_path = icon;
				wl_list_insert(menu->items.prev, &item->link);
			} else {
				free(command);
				free(icon);
			}
			free(label);
			continue;
		}

		if (strcasecmp(tag, "submenu") == 0) {
			char *label = parse_paren(&rest);
			label = maybe_convert_label(label, encoding);
			char *sub_title = parse_brace(&rest);
			char *icon = parse_angle(&rest);
			struct wm_menu_item *item =
				menu_item_create(WM_MENU_SUBMENU, label);
			if (item) {
				item->icon_path = icon;
				item->submenu = menu_create(server,
					sub_title ? sub_title : label);
				if (item->submenu) {
					item->submenu->parent = menu;
					parse_menu_items_depth(item->submenu,
						fp, server, depth + 1);
				}
				wl_list_insert(menu->items.prev, &item->link);
			} else {
				free(icon);
			}
			free(label);
			free(sub_title);
			continue;
		}

		if (strcasecmp(tag, "separator") == 0) {
			struct wm_menu_item *item =
				menu_item_create(WM_MENU_SEPARATOR, NULL);
			if (item) {
				wl_list_insert(menu->items.prev, &item->link);
			}
			continue;
		}

		if (strcasecmp(tag, "nop") == 0) {
			char *label = parse_paren(&rest);
			label = maybe_convert_label(label, encoding);
			struct wm_menu_item *item =
				menu_item_create(WM_MENU_NOP, label);
			if (item) {
				wl_list_insert(menu->items.prev, &item->link);
			}
			free(label);
			continue;
		}

		if (strcasecmp(tag, "include") == 0) {
			char *raw_path = parse_paren(&rest);
			if (raw_path) {
				char *path = expand_path(raw_path);
				if (path) {
					/* Validate include path to prevent traversal */
					if (strstr(path, "..")) {
						wlr_log(WLR_ERROR,
							"menu: rejecting include path with '..': %s",
							path);
						free(path);
						free(raw_path);
						continue;
					}
					char *resolved = realpath(path, NULL);
					if (!resolved) {
						wlr_log(WLR_ERROR,
							"menu: cannot resolve include path: %s",
							path);
						free(path);
						free(raw_path);
						continue;
					}
					/* Only allow includes under home, config, or system dirs */
					const char *home = getenv("HOME");
					bool allowed = false;
					if (home && strncmp(resolved, home,
							strlen(home)) == 0)
						allowed = true;
					if (server && server->config &&
					    server->config->config_dir &&
					    strncmp(resolved,
						server->config->config_dir,
						strlen(server->config->config_dir)) == 0)
						allowed = true;
					if (strncmp(resolved, "/usr/share/", 11) == 0)
						allowed = true;
					if (strncmp(resolved, "/usr/local/share/", 17) == 0)
						allowed = true;
					if (strncmp(resolved, "/etc/", 5) == 0)
						allowed = true;
					if (strncmp(resolved, "/tmp/", 5) == 0)
						allowed = true;
					if (!allowed) {
						wlr_log(WLR_ERROR,
							"menu: include path outside allowed directories: %s",
							resolved);
						free(resolved);
						free(path);
						free(raw_path);
						continue;
					}
					FILE *inc_fp = fopen_nofollow(resolved, "r");
					if (inc_fp) {
						parse_menu_items_depth(menu,
							inc_fp, server,
							depth + 1);
						fclose(inc_fp);
					}
					free(resolved);
					free(path);
				}
				free(raw_path);
			}
			continue;
		}

		if (strcasecmp(tag, "stylesdir") == 0) {
			char *dir = parse_paren(&rest);
			if (!dir) {
				dir = parse_brace(&rest);
			}
			if (dir) {
				add_stylesdir_entries(menu, dir);
				free(dir);
			}
			continue;
		}

		if (strcasecmp(tag, "wallpapers") == 0) {
			char *label = parse_paren(&rest);
			label = maybe_convert_label(label, encoding);
			char *dir = parse_brace(&rest);
			struct wm_menu_item *item =
				menu_item_create(WM_MENU_SUBMENU,
					label ? label : _("Wallpapers"));
			if (item) {
				item->submenu = menu_create(server,
					label ? label : _("Wallpapers"));
				if (item->submenu) {
					item->submenu->parent = menu;
					add_wallpaper_entries(
						item->submenu,
						dir ? dir : "");
				}
				wl_list_insert(menu->items.prev, &item->link);
			}
			free(label);
			free(dir);
			continue;
		}

		if (strcasecmp(tag, "stylesmenu") == 0) {
			char *label = parse_paren(&rest);
			label = maybe_convert_label(label, encoding);
			char *dir = parse_brace(&rest);
			struct wm_menu_item *item =
				menu_item_create(WM_MENU_SUBMENU,
					label ? label : _("Styles"));
			if (item) {
				item->submenu = menu_create(server,
					label ? label : _("Styles"));
				if (item->submenu) {
					item->submenu->parent = menu;
					if (dir)
						add_stylesmenu_entries(
							item->submenu, dir);
				}
				wl_list_insert(menu->items.prev, &item->link);
			}
			free(label);
			free(dir);
			continue;
		}

		if (strcasecmp(tag, "style") == 0) {
			char *label = parse_paren(&rest);
			label = maybe_convert_label(label, encoding);
			char *filename = parse_brace(&rest);
			char *icon = parse_angle(&rest);
			struct wm_menu_item *item =
				menu_item_create(WM_MENU_STYLE, label);
			if (item) {
				item->command = filename;
				item->icon_path = icon;
				wl_list_insert(menu->items.prev, &item->link);
			} else {
				free(filename);
				free(icon);
			}
			free(label);
			continue;
		}

		/* Simple tag types with just a label */
		struct {
			const char *name;
			enum wm_menu_item_type type;
		} simple_tags[] = {
			{"config",     WM_MENU_CONFIG},
			{"workspaces", WM_MENU_WORKSPACES},
			{"reconfig",   WM_MENU_RECONFIG},
			{"exit",       WM_MENU_EXIT},
			/* Window menu items */
			{"shade",      WM_MENU_SHADE},
			{"stick",      WM_MENU_STICK},
			{"maximize",   WM_MENU_MAXIMIZE},
			{"iconify",    WM_MENU_ICONIFY},
			{"close",      WM_MENU_CLOSE},
			{"kill",       WM_MENU_KILL},
			{"raise",      WM_MENU_RAISE},
			{"lower",      WM_MENU_LOWER},
			{"sendto",     WM_MENU_SENDTO},
			{"layer",      WM_MENU_LAYER},
			{NULL, 0},
		};

		bool matched = false;
		for (int i = 0; simple_tags[i].name; i++) {
			if (strcasecmp(tag, simple_tags[i].name) == 0) {
				char *label = parse_paren(&rest);
				label = maybe_convert_label(label, encoding);
				struct wm_menu_item *item = menu_item_create(
					simple_tags[i].type,
					label ? label : simple_tags[i].name);
				if (item) {
					wl_list_insert(menu->items.prev,
						&item->link);
				}
				free(label);
				matched = true;
				break;
			}
		}
		if (matched) {
			continue;
		}

		if (strcasecmp(tag, "restart") == 0) {
			char *label = parse_paren(&rest);
			label = maybe_convert_label(label, encoding);
			char *command = parse_brace(&rest);
			struct wm_menu_item *item = menu_item_create(
				WM_MENU_RESTART, label ? label : _("Restart"));
			if (item) {
				item->command = command;
				wl_list_insert(menu->items.prev, &item->link);
			} else {
				free(command);
			}
			free(label);
			continue;
		}

		if (strcasecmp(tag, "command") == 0) {
			char *label = parse_paren(&rest);
			label = maybe_convert_label(label, encoding);
			char *command = parse_brace(&rest);
			struct wm_menu_item *item =
				menu_item_create(WM_MENU_COMMAND, label);
			if (item) {
				item->command = command;
				wl_list_insert(menu->items.prev, &item->link);
			} else {
				free(command);
			}
			free(label);
			continue;
		}
	}

	free(encoding);
}

/* --- Public: Load menu --- */

struct wm_menu *
wm_menu_load(struct wm_server *server, const char *path)
{
	if (!path) {
		return NULL;
	}

	char *expanded = expand_path(path);
	if (!expanded) {
		return NULL;
	}

	FILE *fp = fopen_nofollow(expanded, "r");
	if (!fp) {
		wlr_log(WLR_ERROR, "failed to open menu file: %s", expanded);
		free(expanded);
		return NULL;
	}

	struct wm_menu *menu = menu_create(server, _("Menu"));
	if (!menu) {
		fclose(fp);
		free(expanded);
		return NULL;
	}

	parse_menu_items_depth(menu, fp, server, 0);

	fclose(fp);
	free(expanded);

	wlr_log(WLR_INFO, "loaded menu from %s", path);
	return menu;
}
