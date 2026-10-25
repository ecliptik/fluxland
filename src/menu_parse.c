/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * menu_parse.c - Fluxbox menu file parser
 *
 * Reads menu files as Fluxbox's MenuCreator and FbMenuParser do. Each
 * line is [tag] (label) {command} <icon>, any field but the tag left
 * out. The file starts at [begin] (title); [end] closes it and each
 * [submenu]. Fluxbox's tags are listed in parse_items(); any other tag
 * is a command with the {command} as its arguments ("[MaximizeWindow]",
 * "[SetStyle] (Bloe) {~/.fluxbox/styles/Bloe}"). [encoding] {charset}
 * ... [endencoding] marks labels written in another character set; it
 * carries into submenus and included files.
 */

#define _GNU_SOURCE
#include <cairo.h>
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <iconv.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wlr/util/log.h>

#include "command.h"
#include "config.h"
#include "i18n.h"
#include "menu.h"
#include "validate.h"
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
	char *end = str + strlen(str);
	while (end > str && isspace((unsigned char)end[-1])) {
		*--end = '\0';
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
 * A field between open and close (FbTk::StringUtil::getStringBetween):
 * after blanks, from open to its close. A backslash before either
 * bracket makes it literal and goes; other backslashes stay. Brackets
 * nest when they balance ("(vim (as root))", "{MacroCmd {Raise}
 * {Focus}}"); when they don't, the first unescaped close ends the
 * field, as in Fluxbox. NULL (and *pos unchanged) if there is no field.
 */
static char *
parse_field(const char **pos, char open, char close)
{
	const char *p = *pos;
	while (*p == ' ' || *p == '\t') {
		p++;
	}
	if (*p != open) {
		return NULL;
	}
	const char *start = p + 1;
	const char *end = NULL;
	int depth = 1;
	for (const char *q = start; *q && !end; q++) {
		if (*q == '\\' && (q[1] == open || q[1] == close)) {
			q++;
		} else if (*q == open) {
			depth++;
		} else if (*q == close && --depth == 0) {
			end = q;
		}
	}
	for (const char *q = start; *q && !end; q++) {
		if (*q == '\\' && q[1] == close) {
			q++;
		} else if (*q == close) {
			end = q;
		}
	}
	if (!end) {
		return NULL;
	}
	char *out = malloc((size_t)(end - start) + 1);
	if (!out) {
		return NULL;
	}
	size_t n = 0;
	for (const char *q = start; q < end; q++) {
		if (*q == '\\' && (q[1] == open || q[1] == close)) {
			q++;
		}
		out[n++] = *q;
	}
	out[n] = '\0';
	*pos = end + 1;
	return out;
}

/* One line: [tag] (label) {command} <icon> (FbMenuParser) */
struct menu_line {
	char *tag, *label, *cmd, *icon;
};

static bool
menu_line_parse(const char *text, struct menu_line *ml)
{
	memset(ml, 0, sizeof(*ml));
	const char *pos = text;
	ml->tag = parse_field(&pos, '[', ']');
	if (!ml->tag) {
		return false;
	}
	ml->label = parse_field(&pos, '(', ')');
	ml->cmd = parse_field(&pos, '{', '}');
	ml->icon = parse_field(&pos, '<', '>');
	return true;
}

static void
menu_line_finish(struct menu_line *ml)
{
	free(ml->tag);
	free(ml->label);
	free(ml->cmd);
	free(ml->icon);
	memset(ml, 0, sizeof(*ml));
}

/* --- Encoding conversion --- */

/*
 * A label written in encoding, as UTF-8. Takes ownership of label;
 * returns it unchanged when there's nothing to convert.
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
	if (item->icon)
		cairo_surface_destroy(item->icon);
	if (item->submenu) {
		wm_menu_destroy(item->submenu);
	}
	free(item);
}

/* --- Parser state --- */

#define MENU_MAX_DEPTH 16      /* submenus and includes */
#define MENU_MAX_INCLUDES 10   /* Fluxbox's include limit */
#define MENU_MAX_ENCODINGS 8

struct parse_ctx {
	struct wm_server *server;
	int depth;
	int includes;
	/* [encoding] blocks: a stack, carried into submenus and includes */
	char *encodings[MENU_MAX_ENCODINGS];
	int encoding_count;
	/* What was read, so a change can be noticed */
	char **sources;
	int source_count;
	/* Where we are, for messages; and a checker to tell (--check-config) */
	const char *path;
	int line;
	void (*report)(void *data, const char *path, int line, bool fatal,
		const char *msg);
	void *report_data;
};

/* Log a problem, or tell the checker */
__attribute__((format(printf, 3, 4)))
static void
diag(struct parse_ctx *ctx, bool fatal, const char *fmt, ...)
{
	char msg[512];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);
	if (ctx->report)
		ctx->report(ctx->report_data, ctx->path ? ctx->path : "menu",
			ctx->line, fatal, msg);
	else
		wlr_log(fatal ? WLR_ERROR : WLR_INFO, "menu: %s:%d: %s",
			ctx->path ? ctx->path : "(text)", ctx->line, msg);
}

static const char *
current_encoding(struct parse_ctx *ctx)
{
	return ctx->encoding_count > 0 ?
		ctx->encodings[ctx->encoding_count - 1] : NULL;
}

static bool parse_items(struct wm_menu *menu, FILE *fp,
	struct parse_ctx *ctx);

/* Append an item; label "" or NULL takes the default label */
static struct wm_menu_item *
add_item(struct wm_menu *menu, enum wm_menu_item_type type,
	const char *label, const char *dflt)
{
	struct wm_menu_item *item = menu_item_create(type,
		label && *label ? label : dflt);
	if (item) {
		wl_list_insert(menu->items.prev, &item->link);
	}
	return item;
}

/* Append an item with a submenu */
static struct wm_menu_item *
add_submenu(struct wm_menu *menu, enum wm_menu_item_type type,
	const char *label, const char *dflt, struct wm_menu *submenu)
{
	struct wm_menu_item *item = add_item(menu, type, label, dflt);
	if (item) {
		item->submenu = submenu;
		if (submenu) {
			submenu->parent = menu;
		}
	} else if (submenu) {
		wm_menu_destroy(submenu);
	}
	return item;
}

/* --- Directory listings --- */

static int
strcmp_qsort(const void *a, const void *b)
{
	return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/*
 * The names in a directory, sorted (FbTk::Directory, sort): the caller
 * frees each and the array.
 */
static char **
sorted_entries(const char *dir_path, size_t *count)
{
	*count = 0;
	DIR *dir = opendir(dir_path);
	if (!dir) {
		return NULL;
	}
	char **names = NULL;
	size_t cap = 0;
	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, ".") == 0 ||
		    strcmp(entry->d_name, "..") == 0) {
			continue;
		}
		if (*count >= cap) {
			cap = cap ? cap * 2 : 32;
			char **tmp = realloc(names, cap * sizeof(*names));
			if (!tmp) {
				break;
			}
			names = tmp;
		}
		names[*count] = strdup(entry->d_name);
		if (names[*count]) {
			(*count)++;
		}
	}
	closedir(dir);
	if (*count > 1) {
		qsort(names, *count, sizeof(*names), strcmp_qsort);
	}
	return names;
}

static bool
is_regular(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

/* A file in a listing: not a dotfile or a backup~ (createStyleMenu) */
static bool
listable(const char *name)
{
	size_t len = strlen(name);
	return name[0] != '.' && len > 0 && name[len - 1] != '~';
}

/*
 * [stylesdir] (dir) / [stylesmenu] {dir}: the styles in a directory, in
 * this menu (createStyleMenu): files, and directories with a theme.cfg
 * or style.cfg.
 */
static void
add_style_entries(struct wm_menu *menu, const char *raw_path)
{
	char *dir_path = expand_path(raw_path);
	if (!dir_path) {
		return;
	}
	size_t count;
	char **names = sorted_entries(dir_path, &count);
	if (!names) {
		wlr_log(WLR_INFO, "menu: no styles directory %s", dir_path);
	}
	for (size_t i = 0; i < count; i++) {
		char *path = NULL, *cfg1 = NULL, *cfg2 = NULL;
		if (asprintf(&path, "%s/%s", dir_path, names[i]) < 0) {
			path = NULL;
		}
		if (path && (asprintf(&cfg1, "%s/theme.cfg", path) < 0 ||
		    asprintf(&cfg2, "%s/style.cfg", path) < 0)) {
			cfg1 = cfg2 = NULL;
		}
		bool style = path && ((listable(names[i]) && is_regular(path)) ||
			(cfg1 && is_regular(cfg1)) || (cfg2 && is_regular(cfg2)));
		if (style) {
			struct wm_menu_item *item =
				add_item(menu, WM_MENU_STYLE, names[i], NULL);
			if (item) {
				item->command = path;
				path = NULL;
			}
		}
		free(path);
		free(cfg1);
		free(cfg2);
		free(names[i]);
	}
	free(names);
	free(dir_path);
}

/* An X11 wallpaper setter, which can't work on Wayland */
static bool
is_x11_setter(const char *cmd)
{
	static const char *const setters[] = {
		"fbsetbg", "feh", "Esetroot", "xsetbg", "wmsetbg", "hsetroot",
		"nitrogen", "xwallpaper", "display", "xv", "habak", NULL,
	};
	size_t n = strcspn(cmd, " \t");
	const char *slash = memrchr(cmd, '/', n);
	const char *name = slash ? slash + 1 : cmd;
	size_t len = (size_t)(cmd + n - name);
	for (int i = 0; setters[i]; i++) {
		if (strlen(setters[i]) == len &&
		    strncmp(name, setters[i], len) == 0) {
			return true;
		}
	}
	return false;
}

/* path in single quotes for a shell */
static char *
shell_quote(const char *path)
{
	size_t len = 3;
	for (const char *p = path; *p; p++) {
		len += *p == '\'' ? 4 : 1;
	}
	char *out = malloc(len);
	if (!out) {
		return NULL;
	}
	char *o = out;
	*o++ = '\'';
	for (const char *p = path; *p; p++) {
		if (*p == '\'') {
			memcpy(o, "'\\''", 4);
			o += 4;
		} else {
			*o++ = *p;
		}
	}
	*o++ = '\'';
	*o = '\0';
	return out;
}

/*
 * [wallpapers] (dir) {command}: the files of a directory, in this menu,
 * each running "command file" (createRootCmdMenu). With no command, or
 * an X11 setter such as Fluxbox's default fbsetbg, the file becomes the
 * wallpaper (SetWallpaper).
 */
static void
add_wallpaper_entries(struct wm_menu *menu, const char *raw_path,
	const char *cmd)
{
	char *dir_path = expand_path(raw_path);
	if (!dir_path) {
		return;
	}
	bool internal = !cmd || !*cmd || is_x11_setter(cmd);
	size_t count;
	char **names = sorted_entries(dir_path, &count);
	for (size_t i = 0; i < count; i++) {
		char *path = NULL;
		if (asprintf(&path, "%s/%s", dir_path, names[i]) < 0) {
			path = NULL;
		}
		if (path && listable(names[i]) && is_regular(path)) {
			char *line = NULL;
			if (internal) {
				if (asprintf(&line, "SetWallpaper %s", path) < 0)
					line = NULL;
			} else {
				char *quoted = shell_quote(path);
				if (quoted && asprintf(&line, "%s %s", cmd,
				    quoted) < 0)
					line = NULL;
				free(quoted);
			}
			struct wm_menu_item *item = line ? add_item(menu,
				internal ? WM_MENU_COMMAND : WM_MENU_EXEC,
				names[i], NULL) : NULL;
			if (item) {
				item->command = line;
			} else {
				free(line);
			}
		}
		free(path);
		free(names[i]);
	}
	free(names);
	free(dir_path);
}

/* --- What a menu was read from --- */

static long long
mtime_of(const char *path)
{
	struct stat st;
	if (stat(path, &st) != 0)
		return -1;
	return (long long)st.st_mtim.tv_sec * 1000000000LL +
		st.st_mtim.tv_nsec;
}

static void
add_source(struct parse_ctx *ctx, const char *path)
{
	if (ctx->source_count >= 256)
		return;
	char **list = realloc(ctx->sources,
		sizeof(*list) * (size_t)(ctx->source_count + 1));
	if (!list)
		return;
	ctx->sources = list;
	list[ctx->source_count] = strdup(path);
	if (list[ctx->source_count])
		ctx->source_count++;
}

/* The newest change of the sources, -1 if one is gone */
static long long
sources_mtime(char **sources, int count)
{
	long long newest = 0;
	for (int i = 0; i < count; i++) {
		long long t = mtime_of(sources[i]);
		if (t < 0)
			return -1;
		if (t > newest)
			newest = t;
	}
	return newest;
}

bool
wm_menu_sources_changed(const struct wm_menu *menu)
{
	if (!menu || menu->source_count == 0)
		return false;
	return sources_mtime(menu->sources, menu->source_count) !=
		menu->sources_mtime;
}

/* --- Includes --- */

static void
include_file(struct wm_menu *menu, const char *path, struct parse_ctx *ctx)
{
	FILE *fp = fopen_config(path, "r");
	if (!fp) {
		diag(ctx, true, "cannot include %s", path);
		return;
	}
	add_source(ctx, path);
	/* An included file is a list of items; [end] stops it */
	const char *outer_path = ctx->path;
	int outer_line = ctx->line;
	ctx->path = path;
	ctx->line = 0;
	parse_items(menu, fp, ctx);
	ctx->path = outer_path;
	ctx->line = outer_line;
	fclose(fp);
}

/* [include] (file or directory): its items go into this menu */
static void
include_path(struct wm_menu *menu, const char *raw_path,
	struct parse_ctx *ctx)
{
	if (ctx->includes >= MENU_MAX_INCLUDES) {
		diag(ctx, true, "too many nested includes at %s", raw_path);
		return;
	}
	char *path = expand_path(raw_path);
	if (!path) {
		return;
	}
	ctx->includes++;
	struct stat st;
	if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
		/* Every regular file of the directory, sorted */
		add_source(ctx, path);
		size_t count;
		char **names = sorted_entries(path, &count);
		for (size_t i = 0; i < count; i++) {
			char *file = NULL;
			if (listable(names[i]) &&
			    asprintf(&file, "%s/%s", path, names[i]) >= 0 &&
			    is_regular(file)) {
				include_file(menu, file, ctx);
			}
			free(file);
			free(names[i]);
		}
		free(names);
	} else {
		include_file(menu, path, ctx);
	}
	ctx->includes--;
	free(path);
}

/* --- Window menu items --- */

/* [layer]: Fluxbox's LayerMenu */
static struct wm_menu *
create_layer_menu(struct wm_server *server)
{
	struct wm_menu *menu = menu_create(server, _("Layer"));
	if (!menu) {
		return NULL;
	}
	static const struct { const char *label, *layer; } layers[] = {
		{ N_("Above Dock"), "AboveDock" }, { N_("Dock"), "Dock" },
		{ N_("Top"), "Top" }, { N_("Normal"), "Normal" },
		{ N_("Bottom"), "Bottom" }, { N_("Desktop"), "Desktop" },
	};
	for (size_t i = 0; i < sizeof(layers) / sizeof(layers[0]); i++) {
		struct wm_menu_item *item = add_item(menu, WM_MENU_COMMAND,
			_(layers[i].label), NULL);
		if (item && asprintf(&item->command, "SetLayer %s",
		    layers[i].layer) < 0) {
			item->command = NULL;
		}
	}
	return menu;
}

/* [alpha]: the window's opacity (Fluxbox's AlphaMenu, as steps) */
static struct wm_menu *
create_alpha_menu(struct wm_server *server)
{
	struct wm_menu *menu = menu_create(server, _("Transparency"));
	if (!menu) {
		return NULL;
	}
	static const struct { const char *label; const char *alpha; } steps[] = {
		{ N_("Opaque"), "255" }, { "90%", "230" }, { "75%", "191" },
		{ "50%", "128" }, { N_("Use Defaults"), "" },
	};
	for (size_t i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
		struct wm_menu_item *item = add_item(menu, WM_MENU_COMMAND,
			_(steps[i].label), NULL);
		if (item && asprintf(&item->command, "SetAlpha %s",
		    steps[i].alpha) < 0) {
			item->command = NULL;
		}
	}
	return menu;
}

/* The window menu's tags (MenuCreator::createWindowMenuItem) */
static bool
window_menu_item(struct wm_menu *menu, const char *tag, const char *label,
	struct parse_ctx *ctx)
{
	static const struct {
		const char *tag;
		enum wm_menu_item_type type;
		const char *label;
	} items[] = {
		{ "shade",      WM_MENU_SHADE,    N_("Shade") },
		{ "stick",      WM_MENU_STICK,    N_("Stick") },
		{ "maximize",   WM_MENU_MAXIMIZE, N_("Maximize") },
		{ "iconify",    WM_MENU_ICONIFY,  N_("Iconify") },
		{ "close",      WM_MENU_CLOSE,    N_("Close") },
		{ "kill",       WM_MENU_KILL,     N_("Kill") },
		{ "killwindow", WM_MENU_KILL,     N_("Kill") },
		{ "raise",      WM_MENU_RAISE,    N_("Raise") },
		{ "lower",      WM_MENU_LOWER,    N_("Lower") },
		{ "extramenus", WM_MENU_REMEMBER, N_("Remember...") },
	};
	for (size_t i = 0; i < sizeof(items) / sizeof(items[0]); i++) {
		if (strcasecmp(tag, items[i].tag) == 0) {
			add_item(menu, items[i].type, label, _(items[i].label));
			return true;
		}
	}
	if (strcasecmp(tag, "sendto") == 0) {
		/* Its workspaces are listed when the menu is shown */
		add_submenu(menu, WM_MENU_SENDTO, label, _("Send To..."),
			menu_create(ctx->server, _("Send To...")));
	} else if (strcasecmp(tag, "layer") == 0) {
		add_submenu(menu, WM_MENU_LAYER, label, _("Layer..."),
			create_layer_menu(ctx->server));
	} else if (strcasecmp(tag, "alpha") == 0) {
		add_submenu(menu, WM_MENU_SUBMENU, label, _("Transparency"),
			create_alpha_menu(ctx->server));
	} else if (strcasecmp(tag, "settitledialog") == 0) {
		struct wm_menu_item *item = add_item(menu, WM_MENU_COMMAND,
			label, _("Set Window Title"));
		if (item)
			item->command = strdup("SetTitleDialog");
	} else {
		return false;
	}
	return true;
}

/* --- Parser --- */

/* Read a line (any length); false at the end */
static bool
read_line(FILE *fp, char **line, size_t *cap)
{
	ssize_t n = getline(line, cap, fp);
	return n >= 0;
}

/* The item one line makes; returns false at [end] */
static bool
parse_line(struct wm_menu *menu, FILE *fp, struct menu_line *ml,
	struct parse_ctx *ctx)
{
	const char *tag = ml->tag;
	char *label = maybe_convert_label(ml->label ? strdup(ml->label) :
		NULL, current_encoding(ctx));
	const char *cmd = ml->cmd;
	struct wm_server *server = ctx->server;
	struct wm_menu_item *last = menu->items.prev != &menu->items ?
		wl_container_of(menu->items.prev, last, link) : NULL;
	bool more = true;

	if (strcasecmp(tag, "end") == 0) {
		more = false;
	} else if (strcasecmp(tag, "begin") == 0) {
		/* Only the file's first [begin] counts */
	} else if (strcasecmp(tag, "encoding") == 0) {
		if (cmd && ctx->encoding_count < MENU_MAX_ENCODINGS) {
			ctx->encodings[ctx->encoding_count++] = strdup(cmd);
		}
	} else if (strcasecmp(tag, "endencoding") == 0) {
		if (ctx->encoding_count > 0) {
			free(ctx->encodings[--ctx->encoding_count]);
		}
	} else if (strcasecmp(tag, "exec") == 0) {
		struct wm_menu_item *item =
			add_item(menu, WM_MENU_EXEC, label, "");
		if (item && cmd) {
			item->command = strdup(cmd);
		}
	} else if (strcasecmp(tag, "macrocmd") == 0) {
		struct wm_menu_item *item =
			add_item(menu, WM_MENU_COMMAND, label, "");
		if (item && cmd && asprintf(&item->command, "MacroCmd %s",
		    cmd) < 0) {
			item->command = NULL;
		}
	} else if (strcasecmp(tag, "command") == 0) {
		/* fluxland: [command] (label) {any command} */
		struct wm_menu_item *item =
			add_item(menu, WM_MENU_COMMAND, label, "");
		if (item && cmd) {
			item->command = strdup(cmd);
		}
	} else if (strcasecmp(tag, "nop") == 0) {
		add_item(menu, WM_MENU_NOP, label, "");
	} else if (strcasecmp(tag, "separator") == 0) {
		add_item(menu, WM_MENU_SEPARATOR, NULL, NULL);
	} else if (strcasecmp(tag, "submenu") == 0) {
		struct wm_menu *sub = menu_create(server,
			cmd && *cmd ? cmd : label);
		if (sub && ctx->depth < MENU_MAX_DEPTH) {
			int start = ctx->line;
			ctx->depth++;
			if (!parse_items(sub, fp, ctx)) {
				int at = ctx->line;
				ctx->line = start;
				diag(ctx, false, "[submenu] (%s) has no [end] "
					"(the file ends at line %d)",
					label ? label : "", at);
				ctx->line = at;
			}
			ctx->depth--;
		}
		add_submenu(menu, WM_MENU_SUBMENU, label, "", sub);
	} else if (strcasecmp(tag, "include") == 0) {
		const char *path = ml->label ? ml->label : cmd;
		if (path) {
			include_path(menu, path, ctx);
		}
	} else if (strcasecmp(tag, "style") == 0) {
		struct wm_menu_item *item =
			add_item(menu, WM_MENU_STYLE, label, "");
		if (item && cmd) {
			item->command = strdup(cmd);
		}
	} else if (strcasecmp(tag, "stylesdir") == 0 ||
		   strcasecmp(tag, "themesdir") == 0 ||
		   strcasecmp(tag, "stylesmenu") == 0 ||
		   strcasecmp(tag, "themesmenu") == 0) {
		bool is_dir = tolower((unsigned char)tag[6]) == 'd';
		const char *dir = is_dir ? (ml->label ? ml->label : cmd) :
			(cmd ? cmd : ml->label);
		if (dir) {
			add_style_entries(menu, dir);
			char *expanded = expand_path(dir);
			if (expanded && mtime_of(expanded) >= 0)
				add_source(ctx, expanded);
			free(expanded);
		}
	} else if (strcasecmp(tag, "wallpapers") == 0 ||
		   strcasecmp(tag, "wallpapermenu") == 0 ||
		   strcasecmp(tag, "rootcommands") == 0) {
		if (ml->label) {
			add_wallpaper_entries(menu, ml->label, cmd);
			char *expanded = expand_path(ml->label);
			if (expanded && mtime_of(expanded) >= 0)
				add_source(ctx, expanded);
			free(expanded);
		}
	} else if (strcasecmp(tag, "workspaces") == 0) {
		/* Its workspaces are listed when the menu is shown */
		add_submenu(menu, WM_MENU_WORKSPACES, label, _("Workspaces"),
			menu_create(server, label && *label ? label :
				_("Workspaces")));
	} else if (strcasecmp(tag, "icons") == 0) {
		add_submenu(menu, WM_MENU_ICONS, label, _("Icons"),
			menu_create(server, label && *label ? label :
				_("Icons")));
	} else if (strcasecmp(tag, "config") == 0) {
		add_submenu(menu, WM_MENU_CONFIG, label, _("Configuration"),
			server ? wm_menu_create_config_menu(server) : NULL);
	} else if (strcasecmp(tag, "exit") == 0) {
		add_item(menu, WM_MENU_EXIT, label, _("Exit"));
	} else if (strcasecmp(tag, "reconfig") == 0 ||
		   strcasecmp(tag, "reconfigure") == 0) {
		add_item(menu, WM_MENU_RECONFIG, label, _("Reload Config"));
	} else if (strcasecmp(tag, "restart") == 0) {
		struct wm_menu_item *item =
			add_item(menu, WM_MENU_RESTART, label, _("Restart"));
		if (item && cmd) {
			item->command = strdup(cmd);
		}
	} else if (!window_menu_item(menu, tag, label, ctx)) {
		/*
		 * Any other tag is a command, its {command} the arguments:
		 * "[SetStyle] (Bloe) {path}" runs "SetStyle path".
		 */
		char *line = NULL;
		if (asprintf(&line, "%s%s%s", tag, cmd ? " " : "",
		    cmd ? cmd : "") >= 0) {
			struct wm_command *parsed = wm_command_parse(line);
			if (parsed) {
				wm_command_destroy(parsed);
				struct wm_menu_item *item = add_item(menu,
					WM_MENU_COMMAND, label, tag);
				if (item) {
					item->command = line;
					line = NULL;
				}
			} else {
				diag(ctx, false, "unknown tag [%s]", tag);
			}
		}
		free(line);
	}

	/* The <icon> is the icon of the item the line made */
	struct wm_menu_item *made = menu->items.prev != &menu->items ?
		wl_container_of(menu->items.prev, made, link) : NULL;
	if (ml->icon && made && made != last && !made->icon_path) {
		made->icon_path = expand_path(ml->icon);
	}
	free(label);
	return more;
}

/* Items up to [end] or the end of the file; true if [end] ended them */
static bool
parse_items(struct wm_menu *menu, FILE *fp, struct parse_ctx *ctx)
{
	bool ended = false;
	char *line = NULL;
	size_t cap = 0;
	while (read_line(fp, &line, &cap)) {
		ctx->line++;
		char *trimmed = strip_whitespace(line);
		if (*trimmed != '[') {
			continue; /* comments (#, !) and text that isn't a tag */
		}
		struct menu_line ml;
		if (!menu_line_parse(trimmed, &ml)) {
			diag(ctx, true, "no ']' after the tag");
			continue;
		}
		bool more = parse_line(menu, fp, &ml, ctx);
		menu_line_finish(&ml);
		if (!more) {
			ended = true;
			break;
		}
	}
	free(line);
	return ended;
}

/*
 * A menu from a stream: from its [begin] (title), or, in a file that has
 * none, from the start (fluxland 1.x read menus without one).
 */
static struct wm_menu *
menu_from_stream(struct wm_server *server, FILE *fp, struct parse_ctx *ctx_in)
{
	struct wm_menu *menu = menu_create(server, _("Menu"));
	if (!menu) {
		return NULL;
	}
	struct parse_ctx local = { .server = server };
	struct parse_ctx *ctxp = ctx_in ? ctx_in : &local;
	struct parse_ctx ctx_copy = *ctxp;
	struct parse_ctx *c = &ctx_copy;

	char *line = NULL;
	size_t cap = 0;
	bool begun = false;
	while (!begun && read_line(fp, &line, &cap)) {
		c->line++;
		char *trimmed = strip_whitespace(line);
		struct menu_line ml;
		if (*trimmed == '[' && menu_line_parse(trimmed, &ml)) {
			if (strcasecmp(ml.tag, "begin") == 0) {
				begun = true;
				if (ml.label && *ml.label) {
					free(menu->title);
					menu->title = strdup(ml.label);
				}
			}
			menu_line_finish(&ml);
		}
	}
	free(line);
	if (!begun) {
		/* Fluxbox wouldn't load it; fluxland 1.x read it all */
		c->line = 0;
		diag(c, false, "no [begin]: read from the start");
		rewind(fp);
	}
	parse_items(menu, fp, c);
	for (int i = 0; i < c->encoding_count; i++) {
		free(c->encodings[i]);
	}
	menu->sources = c->sources;
	menu->source_count = c->source_count;
	menu->sources_mtime = sources_mtime(c->sources, c->source_count);
	return menu;
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

	FILE *fp = fopen_config(expanded, "r");
	if (!fp) {
		wlr_log(WLR_ERROR, "failed to open menu file: %s", expanded);
		free(expanded);
		return NULL;
	}

	struct parse_ctx ctx = { .server = server, .path = expanded };
	add_source(&ctx, expanded);
	struct wm_menu *menu = menu_from_stream(server, fp, &ctx);
	fclose(fp);
	free(expanded);
	wlr_log(WLR_INFO, "loaded menu from %s", path);
	return menu;
}

struct wm_menu *
wm_menu_load_string(struct wm_server *server, const char *text)
{
	FILE *fp = fmemopen((void *)text, strlen(text), "r");
	if (!fp) {
		return NULL;
	}
	struct wm_menu *menu = menu_from_stream(server, fp, NULL);
	fclose(fp);
	return menu;
}

bool
wm_menu_check_file(const char *path,
	void (*report)(void *data, const char *path, int line, bool fatal,
		const char *msg),
	void *data)
{
	char *expanded = expand_path(path);
	FILE *fp = expanded ? fopen_config(expanded, "r") : NULL;
	if (!fp) {
		free(expanded);
		return false;
	}
	struct parse_ctx ctx = { .path = expanded, .report = report,
		.report_data = data };
	struct wm_menu *menu = menu_from_stream(NULL, fp, &ctx);
	wm_menu_destroy(menu);
	fclose(fp);
	free(expanded);
	return true;
}
