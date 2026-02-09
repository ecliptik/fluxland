/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
 *
 * menu.c - Fluxbox menu file parser, rendering, and interaction
 */

#define _GNU_SOURCE
#include <cairo.h>
#include <ctype.h>
#include <dirent.h>
#include <drm_fourcc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

#include "config.h"
#include "menu.h"
#include "render.h"
#include "server.h"
#include "style.h"
#include "view.h"
#include "workspace.h"

/* --- Layout constants --- */

#define MENU_MIN_WIDTH     120
#define MENU_MAX_WIDTH     300
#define MENU_ITEM_PADDING  4
#define MENU_TITLE_PADDING 6
#define MENU_SEPARATOR_HEIGHT 7
#define MENU_ARROW_SIZE    8

/* --- Cairo-to-wlr_buffer bridge (same as decoration.c) --- */

struct wm_pixel_buffer {
	struct wlr_buffer base;
	void *data;
	uint32_t format;
	size_t stride;
};

static void pixel_buffer_destroy(struct wlr_buffer *wlr_buffer)
{
	struct wm_pixel_buffer *buffer =
		wl_container_of(wlr_buffer, buffer, base);
	free(buffer->data);
	free(buffer);
}

static bool pixel_buffer_begin_data_ptr_access(struct wlr_buffer *wlr_buffer,
	uint32_t flags, void **data, uint32_t *format, size_t *stride)
{
	struct wm_pixel_buffer *buffer =
		wl_container_of(wlr_buffer, buffer, base);
	if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE) {
		return false;
	}
	*data = buffer->data;
	*format = buffer->format;
	*stride = buffer->stride;
	return true;
}

static void pixel_buffer_end_data_ptr_access(struct wlr_buffer *wlr_buffer)
{
	/* nothing to do */
}

static const struct wlr_buffer_impl pixel_buffer_impl = {
	.destroy = pixel_buffer_destroy,
	.begin_data_ptr_access = pixel_buffer_begin_data_ptr_access,
	.end_data_ptr_access = pixel_buffer_end_data_ptr_access,
};

static struct wlr_buffer *
wlr_buffer_from_cairo(cairo_surface_t *surface)
{
	if (!surface) {
		return NULL;
	}

	cairo_surface_flush(surface);

	int width = cairo_image_surface_get_width(surface);
	int height = cairo_image_surface_get_height(surface);
	int stride = cairo_image_surface_get_stride(surface);
	unsigned char *src = cairo_image_surface_get_data(surface);

	if (width <= 0 || height <= 0 || !src) {
		cairo_surface_destroy(surface);
		return NULL;
	}

	size_t size = (size_t)stride * height;
	void *data = malloc(size);
	if (!data) {
		cairo_surface_destroy(surface);
		return NULL;
	}
	memcpy(data, src, size);

	struct wm_pixel_buffer *buffer = calloc(1, sizeof(*buffer));
	if (!buffer) {
		free(data);
		cairo_surface_destroy(surface);
		return NULL;
	}

	wlr_buffer_init(&buffer->base, &pixel_buffer_impl, width, height);
	buffer->data = data;
	buffer->format = DRM_FORMAT_ARGB8888;
	buffer->stride = stride;

	cairo_surface_destroy(surface);
	return &buffer->base;
}

/* --- Color helpers --- */

static void
wm_color_to_float4(const struct wm_color *c, float out[4])
{
	out[0] = c->r / 255.0f;
	out[1] = c->g / 255.0f;
	out[2] = c->b / 255.0f;
	out[3] = c->a / 255.0f;
}

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

/* --- Menu allocation --- */

static struct wm_menu *
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

static struct wm_menu_item *
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

static void
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

/* --- Parser --- */

/*
 * Recursively parse menu items from a file stream.
 * Stops at [end] or EOF.
 */
static void
parse_menu_items(struct wm_menu *menu, FILE *fp, struct wm_server *server)
{
	char line[1024];

	while (fgets(line, sizeof(line), fp)) {
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

		if (strcasecmp(tag, "exec") == 0) {
			char *label = parse_paren(&rest);
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
					parse_menu_items(item->submenu, fp,
						server);
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
					FILE *inc_fp = fopen(path, "r");
					if (inc_fp) {
						parse_menu_items(menu, inc_fp,
							server);
						fclose(inc_fp);
					}
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

		if (strcasecmp(tag, "style") == 0) {
			char *label = parse_paren(&rest);
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
			char *command = parse_brace(&rest);
			struct wm_menu_item *item = menu_item_create(
				WM_MENU_RESTART, label ? label : "Restart");
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

	FILE *fp = fopen(expanded, "r");
	if (!fp) {
		wlr_log(WLR_ERROR, "failed to open menu file: %s", expanded);
		free(expanded);
		return NULL;
	}

	struct wm_menu *menu = menu_create(server, "Menu");
	if (!menu) {
		fclose(fp);
		free(expanded);
		return NULL;
	}

	parse_menu_items(menu, fp, server);

	fclose(fp);
	free(expanded);

	wlr_log(WLR_INFO, "loaded menu from %s", path);
	return menu;
}

/* --- Window menu --- */

struct wm_menu *
wm_menu_create_window_menu(struct wm_server *server)
{
	struct wm_menu *menu = menu_create(server, "Window");

	struct {
		enum wm_menu_item_type type;
		const char *label;
	} entries[] = {
		{WM_MENU_SHADE,    "Shade"},
		{WM_MENU_STICK,    "Stick"},
		{WM_MENU_MAXIMIZE, "Maximize"},
		{WM_MENU_ICONIFY,  "Iconify"},
		{WM_MENU_SEPARATOR, NULL},
		{WM_MENU_SENDTO,   "Send To..."},
		{WM_MENU_LAYER,    "Layer"},
		{WM_MENU_SEPARATOR, NULL},
		{WM_MENU_RAISE,    "Raise"},
		{WM_MENU_LOWER,    "Lower"},
		{WM_MENU_SEPARATOR, NULL},
		{WM_MENU_CLOSE,    "Close"},
		{WM_MENU_KILL,     "Kill"},
	};

	for (size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); i++) {
		struct wm_menu_item *item = menu_item_create(
			entries[i].type, entries[i].label);
		if (item) {
			/* Create SendTo submenu with workspace list */
			if (entries[i].type == WM_MENU_SENDTO) {
				item->submenu = wm_menu_create_workspace_menu(
					server);
				if (item->submenu) {
					item->submenu->parent = menu;
				}
			}
			/* Create Layer submenu */
			if (entries[i].type == WM_MENU_LAYER) {
				item->submenu = menu_create(server, "Layer");
				if (item->submenu) {
					item->submenu->parent = menu;
					const char *layers[] = {
						"Above Dock", "Dock",
						"Normal", "Bottom", "Desktop"
					};
					for (int j = 0; j < 5; j++) {
						struct wm_menu_item *li =
							menu_item_create(
								WM_MENU_COMMAND,
								layers[j]);
						if (li) {
							char cmd[64];
							snprintf(cmd,
								sizeof(cmd),
								"SetLayer %d",
								j);
							li->command =
								strdup(cmd);
							wl_list_insert(
								item->submenu->items.prev,
								&li->link);
						}
					}
				}
			}
			wl_list_insert(menu->items.prev, &item->link);
		}
	}

	return menu;
}

/* --- Workspace menu --- */

struct wm_menu *
wm_menu_create_workspace_menu(struct wm_server *server)
{
	struct wm_menu *menu = menu_create(server, "Workspaces");

	struct wm_workspace *ws;
	wl_list_for_each(ws, &server->workspaces, link) {
		char label[128];
		snprintf(label, sizeof(label), "%d: %s", ws->index + 1,
			ws->name ? ws->name : "Workspace");
		struct wm_menu_item *item = menu_item_create(
			WM_MENU_COMMAND, label);
		if (item) {
			char cmd[64];
			snprintf(cmd, sizeof(cmd), "SendToWorkspace %d",
				ws->index + 1);
			item->command = strdup(cmd);
			wl_list_insert(menu->items.prev, &item->link);
		}
	}

	return menu;
}

/* --- Rendering --- */

/*
 * Count items and compute menu dimensions.
 */
static void
menu_compute_layout(struct wm_menu *menu, struct wm_style *style)
{
	const struct wm_font *title_font = &style->menu_title_font;
	const struct wm_font *frame_font = &style->menu_frame_font;

	int title_h = title_font->size > 0 ? title_font->size : 12;
	title_h += 2 * MENU_TITLE_PADDING;
	menu->title_height = title_h;

	int item_h = frame_font->size > 0 ? frame_font->size : 12;
	item_h += 2 * MENU_ITEM_PADDING;
	menu->item_height = item_h;

	menu->border_width = style->menu_border_width > 0 ?
		style->menu_border_width : 1;

	/* Compute width based on widest label */
	int max_label_width = MENU_MIN_WIDTH;
	int count = 0;

	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		count++;
		if (item->label) {
			/* Approximate text width: chars * (font_size * 0.6) */
			int approx = (int)(strlen(item->label) *
				(frame_font->size > 0 ? frame_font->size :
				 12) * 0.6);
			/* Add space for submenu arrow */
			if (item->type == WM_MENU_SUBMENU ||
			    item->type == WM_MENU_SENDTO ||
			    item->type == WM_MENU_LAYER) {
				approx += MENU_ARROW_SIZE + 4;
			}
			if (approx > max_label_width) {
				max_label_width = approx;
			}
		}
	}
	menu->item_count = count;

	int bw = menu->border_width;
	menu->width = max_label_width + 2 * MENU_ITEM_PADDING + 2 * bw;
	if (menu->width > MENU_MAX_WIDTH) {
		menu->width = MENU_MAX_WIDTH;
	}

	/* Total height: border + title + border + items + border */
	int items_height = 0;
	wl_list_for_each(item, &menu->items, link) {
		if (item->type == WM_MENU_SEPARATOR) {
			items_height += MENU_SEPARATOR_HEIGHT;
		} else {
			items_height += item_h;
		}
	}

	menu->height = 2 * bw + title_h + items_height;
}

/*
 * Render the menu title bar to a Cairo surface.
 */
static cairo_surface_t *
render_menu_title(struct wm_menu *menu, struct wm_style *style)
{
	int bw = menu->border_width;
	int inner_w = menu->width - 2 * bw;
	int th = menu->title_height;

	if (inner_w <= 0 || th <= 0) {
		return NULL;
	}

	/* Render title background texture */
	cairo_surface_t *bg = wm_render_texture(&style->menu_title,
		inner_w, th, 1.0f);
	if (!bg) {
		bg = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
			inner_w, th);
		if (cairo_surface_status(bg) != CAIRO_STATUS_SUCCESS) {
			cairo_surface_destroy(bg);
			return NULL;
		}
		cairo_t *cr = cairo_create(bg);
		cairo_set_source_rgba(cr, 0.2, 0.2, 0.3, 1.0);
		cairo_paint(cr);
		cairo_destroy(cr);
	}

	/* Render title text */
	if (menu->title && *menu->title) {
		int tw, tth;
		cairo_surface_t *text = wm_render_text(menu->title,
			&style->menu_title_font,
			&style->menu_title_text_color,
			inner_w - 2 * MENU_TITLE_PADDING,
			&tw, &tth, style->menu_title_justify, 1.0f);
		if (text) {
			cairo_t *cr = cairo_create(bg);
			int tx = MENU_TITLE_PADDING;
			int ty = (th - tth) / 2;
			if (style->menu_title_justify == WM_JUSTIFY_CENTER) {
				tx = (inner_w - tw) / 2;
			} else if (style->menu_title_justify ==
				   WM_JUSTIFY_RIGHT) {
				tx = inner_w - tw - MENU_TITLE_PADDING;
			}
			if (ty < 0) {
				ty = 0;
			}
			cairo_set_source_surface(cr, text, tx, ty);
			cairo_paint(cr);
			cairo_destroy(cr);
			cairo_surface_destroy(text);
		}
	}

	return bg;
}

/*
 * Render all menu items to a single Cairo surface.
 */
static cairo_surface_t *
render_menu_items(struct wm_menu *menu, struct wm_style *style)
{
	int bw = menu->border_width;
	int inner_w = menu->width - 2 * bw;
	int item_h = menu->item_height;

	/* Compute total items height */
	int total_h = 0;
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		if (item->type == WM_MENU_SEPARATOR) {
			total_h += MENU_SEPARATOR_HEIGHT;
		} else {
			total_h += item_h;
		}
	}

	if (inner_w <= 0 || total_h <= 0) {
		return NULL;
	}

	/* Render frame background */
	cairo_surface_t *surface = wm_render_texture(&style->menu_frame,
		inner_w, total_h, 1.0f);
	if (!surface) {
		surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
			inner_w, total_h);
		if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
			cairo_surface_destroy(surface);
			return NULL;
		}
		cairo_t *cr = cairo_create(surface);
		cairo_set_source_rgba(cr, 0.15, 0.15, 0.15, 1.0);
		cairo_paint(cr);
		cairo_destroy(cr);
	}

	cairo_t *cr = cairo_create(surface);
	int y = 0;
	int index = 0;

	wl_list_for_each(item, &menu->items, link) {
		if (item->type == WM_MENU_SEPARATOR) {
			/* Draw separator line */
			cairo_set_source_rgba(cr,
				style->menu_border_color.r / 255.0,
				style->menu_border_color.g / 255.0,
				style->menu_border_color.b / 255.0,
				0.6);
			cairo_set_line_width(cr, 1.0);
			int sep_y = y + MENU_SEPARATOR_HEIGHT / 2;
			cairo_move_to(cr, 4, sep_y + 0.5);
			cairo_line_to(cr, inner_w - 4, sep_y + 0.5);
			cairo_stroke(cr);
			y += MENU_SEPARATOR_HEIGHT;
			index++;
			continue;
		}

		bool is_selected = (index == menu->selected_index);

		/* Draw highlight background for selected item */
		if (is_selected) {
			cairo_surface_t *hilite = wm_render_texture(
				&style->menu_hilite, inner_w, item_h, 1.0f);
			if (hilite) {
				cairo_set_source_surface(cr, hilite, 0, y);
				cairo_paint(cr);
				cairo_surface_destroy(hilite);
			} else {
				cairo_set_source_rgba(cr, 0.3, 0.3, 0.5, 1.0);
				cairo_rectangle(cr, 0, y, inner_w, item_h);
				cairo_fill(cr);
			}
		}

		/* Draw label text */
		const struct wm_color *text_color = is_selected ?
			&style->menu_hilite_text_color :
			&style->menu_frame_text_color;

		bool is_disabled = (item->type == WM_MENU_NOP);
		struct wm_color disabled_color;
		if (is_disabled) {
			/* Grey out disabled items */
			disabled_color = *text_color;
			disabled_color.r = (disabled_color.r + 128) / 2;
			disabled_color.g = (disabled_color.g + 128) / 2;
			disabled_color.b = (disabled_color.b + 128) / 2;
			text_color = &disabled_color;
		}

		if (item->label) {
			int tw, th;
			int max_w = inner_w - 2 * MENU_ITEM_PADDING;
			bool has_arrow = (item->type == WM_MENU_SUBMENU ||
				item->type == WM_MENU_SENDTO ||
				item->type == WM_MENU_LAYER);
			if (has_arrow) {
				max_w -= MENU_ARROW_SIZE + 4;
			}
			cairo_surface_t *text = wm_render_text(item->label,
				&style->menu_frame_font, text_color,
				max_w, &tw, &th, WM_JUSTIFY_LEFT, 1.0f);
			if (text) {
				int tx = MENU_ITEM_PADDING;
				int ty = y + (item_h - th) / 2;
				if (ty < y) {
					ty = y;
				}
				cairo_set_source_surface(cr, text, tx, ty);
				cairo_paint(cr);
				cairo_surface_destroy(text);
			}
		}

		/* Draw submenu arrow indicator */
		bool has_submenu = (item->type == WM_MENU_SUBMENU ||
			item->type == WM_MENU_SENDTO ||
			item->type == WM_MENU_LAYER);
		if (has_submenu) {
			double ax = inner_w - MENU_ITEM_PADDING -
				MENU_ARROW_SIZE;
			double ay = y + (item_h - MENU_ARROW_SIZE) / 2.0;

			cairo_set_source_rgba(cr,
				text_color->r / 255.0,
				text_color->g / 255.0,
				text_color->b / 255.0,
				text_color->a / 255.0);
			cairo_move_to(cr, ax, ay);
			cairo_line_to(cr, ax + MENU_ARROW_SIZE,
				ay + MENU_ARROW_SIZE / 2.0);
			cairo_line_to(cr, ax, ay + MENU_ARROW_SIZE);
			cairo_close_path(cr);
			cairo_fill(cr);
		}

		y += item_h;
		index++;
	}

	cairo_destroy(cr);
	return surface;
}

/* --- Display --- */

void
wm_menu_show(struct wm_menu *menu, int x, int y)
{
	if (!menu || !menu->server) {
		return;
	}

	struct wm_server *server = menu->server;
	struct wm_style *style = server->style;

	/* Hide if already visible (re-show) */
	if (menu->visible) {
		wm_menu_hide(menu);
	}

	menu_compute_layout(menu, style);

	/* Clamp to screen bounds if possible */
	/* TODO: get output bounds properly; for now use reasonable defaults */
	menu->x = x;
	menu->y = y;
	menu->selected_index = -1;

	/* Create scene tree for this menu */
	menu->scene_tree = wlr_scene_tree_create(server->scene->tree.node.parent ?
		&server->scene->tree : &server->scene->tree);
	if (!menu->scene_tree) {
		return;
	}
	wlr_scene_node_set_position(&menu->scene_tree->node, x, y);
	/* Raise menu tree above everything else */
	wlr_scene_node_raise_to_top(&menu->scene_tree->node);

	int bw = menu->border_width;

	/* Border background rect */
	float border_color[4];
	wm_color_to_float4(&style->menu_border_color, border_color);
	struct wlr_scene_rect *border_rect = wlr_scene_rect_create(
		menu->scene_tree, menu->width, menu->height, border_color);
	wlr_scene_node_set_position(&border_rect->node, 0, 0);

	/* Title bar */
	cairo_surface_t *title_surf = render_menu_title(menu, style);
	struct wlr_buffer *title_buf = wlr_buffer_from_cairo(title_surf);
	menu->title_buf = wlr_scene_buffer_create(menu->scene_tree, title_buf);
	if (title_buf) {
		wlr_buffer_drop(title_buf);
	}
	wlr_scene_node_set_position(&menu->title_buf->node, bw, bw);

	/* Items area */
	cairo_surface_t *items_surf = render_menu_items(menu, style);
	struct wlr_buffer *items_buf = wlr_buffer_from_cairo(items_surf);
	menu->items_buf = wlr_scene_buffer_create(menu->scene_tree, items_buf);
	if (items_buf) {
		wlr_buffer_drop(items_buf);
	}
	wlr_scene_node_set_position(&menu->items_buf->node,
		bw, bw + menu->title_height);

	menu->visible = true;
}

void
wm_menu_hide(struct wm_menu *menu)
{
	if (!menu || !menu->visible) {
		return;
	}

	/* Hide any open submenu */
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		if (item->submenu && item->submenu->visible) {
			wm_menu_hide(item->submenu);
		}
	}

	if (menu->scene_tree) {
		wlr_scene_node_destroy(&menu->scene_tree->node);
		menu->scene_tree = NULL;
	}

	menu->title_buf = NULL;
	menu->items_buf = NULL;
	menu->hilite_rect = NULL;
	menu->visible = false;
	menu->selected_index = -1;
}

void
wm_menu_hide_all(struct wm_server *server)
{
	if (!server) {
		return;
	}

	/* We track root_menu and window_menu on the server via the
	 * action system. For now, iterate all known menus. The
	 * integration module will store them on the server struct. */
	/* This is a stub that will be wired up in the integration task. */
	(void)server;
}

/* --- Re-render items for selection change --- */

static void
menu_update_render(struct wm_menu *menu)
{
	if (!menu || !menu->visible || !menu->scene_tree) {
		return;
	}

	struct wm_style *style = menu->server->style;

	cairo_surface_t *items_surf = render_menu_items(menu, style);
	struct wlr_buffer *items_buf = wlr_buffer_from_cairo(items_surf);
	if (menu->items_buf) {
		wlr_scene_buffer_set_buffer(menu->items_buf, items_buf);
	}
	if (items_buf) {
		wlr_buffer_drop(items_buf);
	}
}

/* --- Hit testing --- */

/*
 * Given layout coordinates, determine if they hit this menu.
 * Returns the item index (0-based) or -1 if not hit.
 */
static int
menu_item_at(struct wm_menu *menu, double lx, double ly)
{
	if (!menu || !menu->visible) {
		return -1;
	}

	int bw = menu->border_width;
	double rx = lx - menu->x - bw;
	double ry = ly - menu->y - bw - menu->title_height;

	if (rx < 0 || rx >= menu->width - 2 * bw) {
		return -1;
	}
	if (ry < 0) {
		return -1;
	}

	int y = 0;
	int index = 0;
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		int ih = (item->type == WM_MENU_SEPARATOR) ?
			MENU_SEPARATOR_HEIGHT : menu->item_height;
		if (ry >= y && ry < y + ih) {
			return index;
		}
		y += ih;
		index++;
	}

	return -1;
}

/*
 * Check if layout coordinates are within this menu's bounds (including border).
 */
static bool
menu_contains_point(struct wm_menu *menu, double lx, double ly)
{
	if (!menu || !menu->visible) {
		return false;
	}
	return lx >= menu->x && lx < menu->x + menu->width &&
	       ly >= menu->y && ly < menu->y + menu->height;
}

/*
 * Get the nth item from the menu.
 */
static struct wm_menu_item *
menu_get_item(struct wm_menu *menu, int index)
{
	int i = 0;
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		if (i == index) {
			return item;
		}
		i++;
	}
	return NULL;
}

/* --- Find deepest visible menu --- */

static struct wm_menu *
find_deepest_visible(struct wm_menu *menu)
{
	if (!menu || !menu->visible) {
		return NULL;
	}

	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		if (item->submenu && item->submenu->visible) {
			struct wm_menu *deeper =
				find_deepest_visible(item->submenu);
			if (deeper) {
				return deeper;
			}
		}
	}

	return menu;
}

/* --- Find visible menu containing point --- */

static struct wm_menu *
find_menu_at(struct wm_menu *menu, double lx, double ly)
{
	if (!menu || !menu->visible) {
		return NULL;
	}

	/* Check submenus first (they're on top) */
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		if (item->submenu && item->submenu->visible) {
			struct wm_menu *hit = find_menu_at(item->submenu,
				lx, ly);
			if (hit) {
				return hit;
			}
		}
	}

	if (menu_contains_point(menu, lx, ly)) {
		return menu;
	}

	return NULL;
}

/* --- Action execution --- */

static void
execute_menu_item(struct wm_menu *menu, struct wm_menu_item *item)
{
	if (!menu || !item) {
		return;
	}

	struct wm_server *server = menu->server;

	switch (item->type) {
	case WM_MENU_EXEC:
		if (item->command) {
			wlr_log(WLR_INFO, "menu exec: %s", item->command);
			if (fork() == 0) {
				setsid();
				execl("/bin/sh", "/bin/sh", "-c",
					item->command, (char *)NULL);
				_exit(1);
			}
		}
		break;

	case WM_MENU_RECONFIG:
		wlr_log(WLR_INFO, "menu: %s", "reconfig");
		/* Will be wired up in integration */
		break;

	case WM_MENU_RESTART:
		wlr_log(WLR_INFO, "menu: %s", "restart");
		/* Will be wired up in integration */
		break;

	case WM_MENU_EXIT:
		wlr_log(WLR_INFO, "menu: %s", "exit");
		wl_display_terminate(server->wl_display);
		break;

	case WM_MENU_STYLE:
		if (item->command && server->style) {
			wlr_log(WLR_INFO, "menu: load style %s",
				item->command);
			style_load(server->style, item->command);
		}
		break;

	case WM_MENU_SHADE:
		if (server->focused_view && server->focused_view->decoration) {
			wlr_log(WLR_INFO, "menu: %s", "toggle shade");
		}
		break;

	case WM_MENU_STICK:
		if (server->focused_view) {
			wm_view_set_sticky(server->focused_view,
				!server->focused_view->sticky);
		}
		break;

	case WM_MENU_MAXIMIZE:
		/* Will be wired up to view maximize toggle */
		break;

	case WM_MENU_ICONIFY:
		/* Will be wired up to view minimize */
		break;

	case WM_MENU_CLOSE:
		if (server->focused_view &&
		    server->focused_view->xdg_toplevel) {
			wlr_xdg_toplevel_send_close(
				server->focused_view->xdg_toplevel);
		}
		break;

	case WM_MENU_KILL:
		if (server->focused_view &&
		    server->focused_view->xdg_toplevel &&
		    server->focused_view->xdg_toplevel->base->resource) {
			struct wl_client *client = wl_resource_get_client(
				server->focused_view->xdg_toplevel->base->resource);
			if (client) {
				wl_client_destroy(client);
			}
		}
		break;

	case WM_MENU_RAISE:
		if (server->focused_view) {
			wm_view_raise(server->focused_view);
		}
		break;

	case WM_MENU_LOWER:
		/* Lower view to bottom of stacking order */
		if (server->focused_view &&
		    server->focused_view->scene_tree) {
			wlr_scene_node_lower_to_bottom(
				&server->focused_view->scene_tree->node);
		}
		break;

	case WM_MENU_COMMAND:
		/* Generic command - will be dispatched by integration */
		if (item->command) {
			wlr_log(WLR_INFO, "menu command: %s", item->command);
		}
		break;

	case WM_MENU_CONFIG:
	case WM_MENU_WORKSPACES:
	case WM_MENU_SENDTO:
	case WM_MENU_LAYER:
	case WM_MENU_SUBMENU:
	case WM_MENU_SEPARATOR:
	case WM_MENU_NOP:
	case WM_MENU_INCLUDE:
	case WM_MENU_STYLESDIR:
		/* These are handled structurally, not as actions */
		break;
	}
}

/* --- Keyboard interaction --- */

bool
wm_menu_handle_key(struct wm_server *server, uint32_t keycode,
	xkb_keysym_t sym, bool pressed)
{
	(void)keycode;

	if (!pressed) {
		return false;
	}

	/* Find the deepest visible menu (the one that has focus) */
	/* We need a root reference - integration will set this */
	/* For now, return false if no menus to handle */
	(void)server;
	(void)sym;

	return false;
}

/*
 * Internal: handle key for a specific visible menu.
 * Called by the integration layer which knows the active root menu.
 */
bool
wm_menu_handle_key_for(struct wm_menu *root_menu, xkb_keysym_t sym)
{
	if (!root_menu || !root_menu->visible) {
		return false;
	}

	struct wm_menu *menu = find_deepest_visible(root_menu);
	if (!menu) {
		return false;
	}

	switch (sym) {
	case XKB_KEY_Up:
	case XKB_KEY_k: {
		/* Move selection up, skip separators */
		int idx = menu->selected_index;
		for (int attempt = 0; attempt < menu->item_count; attempt++) {
			idx--;
			if (idx < 0) {
				idx = menu->item_count - 1;
			}
			struct wm_menu_item *item = menu_get_item(menu, idx);
			if (item && item->type != WM_MENU_SEPARATOR) {
				menu->selected_index = idx;
				menu_update_render(menu);
				return true;
			}
		}
		return true;
	}

	case XKB_KEY_Down:
	case XKB_KEY_j: {
		/* Move selection down, skip separators */
		int idx = menu->selected_index;
		for (int attempt = 0; attempt < menu->item_count; attempt++) {
			idx++;
			if (idx >= menu->item_count) {
				idx = 0;
			}
			struct wm_menu_item *item = menu_get_item(menu, idx);
			if (item && item->type != WM_MENU_SEPARATOR) {
				menu->selected_index = idx;
				menu_update_render(menu);
				return true;
			}
		}
		return true;
	}

	case XKB_KEY_Right:
	case XKB_KEY_l: {
		/* Enter submenu */
		struct wm_menu_item *item = menu_get_item(menu,
			menu->selected_index);
		if (item && item->submenu) {
			int sub_x = menu->x + menu->width;
			int sub_y = menu->y + menu->border_width +
				menu->title_height;
			/* Offset to the selected item */
			int y_off = 0;
			int idx = 0;
			struct wm_menu_item *it;
			wl_list_for_each(it, &menu->items, link) {
				if (idx >= menu->selected_index) {
					break;
				}
				if (it->type == WM_MENU_SEPARATOR) {
					y_off += MENU_SEPARATOR_HEIGHT;
				} else {
					y_off += menu->item_height;
				}
				idx++;
			}
			wm_menu_show(item->submenu, sub_x, sub_y + y_off);
			/* Select first non-separator in submenu */
			int first = 0;
			struct wm_menu_item *si;
			wl_list_for_each(si, &item->submenu->items, link) {
				if (si->type != WM_MENU_SEPARATOR) {
					item->submenu->selected_index = first;
					menu_update_render(item->submenu);
					break;
				}
				first++;
			}
			return true;
		}
		return true;
	}

	case XKB_KEY_Left:
	case XKB_KEY_h: {
		/* Go back to parent menu */
		if (menu->parent && menu->parent->visible) {
			wm_menu_hide(menu);
			return true;
		}
		return true;
	}

	case XKB_KEY_Return:
	case XKB_KEY_KP_Enter: {
		/* Activate selected item */
		struct wm_menu_item *item = menu_get_item(menu,
			menu->selected_index);
		if (!item) {
			return true;
		}

		if (item->submenu) {
			/* Enter submenu (same as Right) */
			return wm_menu_handle_key_for(root_menu,
				XKB_KEY_Right);
		}

		if (item->type != WM_MENU_SEPARATOR &&
		    item->type != WM_MENU_NOP) {
			execute_menu_item(menu, item);
			/* Hide all menus after activation */
			wm_menu_hide(root_menu);
		}
		return true;
	}

	case XKB_KEY_Escape: {
		/* Close current submenu, or close all if at root */
		if (menu->parent && menu->parent->visible) {
			wm_menu_hide(menu);
		} else {
			wm_menu_hide(root_menu);
		}
		return true;
	}

	default:
		return false;
	}
}

/* --- Mouse interaction --- */

bool
wm_menu_handle_motion(struct wm_server *server, double lx, double ly)
{
	(void)server;
	(void)lx;
	(void)ly;
	/* Stub - integration layer will call wm_menu_handle_motion_for */
	return false;
}

/*
 * Internal: handle motion for a specific menu tree.
 */
bool
wm_menu_handle_motion_for(struct wm_menu *root_menu, double lx, double ly)
{
	if (!root_menu || !root_menu->visible) {
		return false;
	}

	struct wm_menu *hit_menu = find_menu_at(root_menu, lx, ly);
	if (!hit_menu) {
		return false;
	}

	int idx = menu_item_at(hit_menu, lx, ly);
	if (idx >= 0 && idx != hit_menu->selected_index) {
		/* Close any open submenu from a different item */
		struct wm_menu_item *old_item = menu_get_item(hit_menu,
			hit_menu->selected_index);
		if (old_item && old_item->submenu &&
		    old_item->submenu->visible) {
			wm_menu_hide(old_item->submenu);
		}

		hit_menu->selected_index = idx;
		menu_update_render(hit_menu);

		/* Auto-open submenu on hover */
		struct wm_menu_item *item = menu_get_item(hit_menu, idx);
		if (item && item->submenu) {
			int sub_x = hit_menu->x + hit_menu->width;
			int sub_y = hit_menu->y + hit_menu->border_width +
				hit_menu->title_height;
			int y_off = 0;
			int i = 0;
			struct wm_menu_item *it;
			wl_list_for_each(it, &hit_menu->items, link) {
				if (i >= idx) {
					break;
				}
				if (it->type == WM_MENU_SEPARATOR) {
					y_off += MENU_SEPARATOR_HEIGHT;
				} else {
					y_off += hit_menu->item_height;
				}
				i++;
			}
			wm_menu_show(item->submenu, sub_x, sub_y + y_off);
		}
	}

	return true;
}

bool
wm_menu_handle_button(struct wm_server *server, double lx, double ly,
	uint32_t button, bool pressed)
{
	(void)server;
	(void)lx;
	(void)ly;
	(void)button;
	(void)pressed;
	/* Stub - integration layer will call wm_menu_handle_button_for */
	return false;
}

/*
 * Internal: handle button click for a specific menu tree.
 */
bool
wm_menu_handle_button_for(struct wm_menu *root_menu, double lx, double ly,
	uint32_t button, bool pressed)
{
	if (!root_menu || !root_menu->visible) {
		return false;
	}

	/* Only handle press events */
	if (!pressed) {
		return find_menu_at(root_menu, lx, ly) != NULL;
	}

	struct wm_menu *hit_menu = find_menu_at(root_menu, lx, ly);
	if (!hit_menu) {
		/* Click outside all menus - close */
		wm_menu_hide(root_menu);
		return false;
	}

	int idx = menu_item_at(hit_menu, lx, ly);
	if (idx < 0) {
		return true; /* clicked on title or border */
	}

	struct wm_menu_item *item = menu_get_item(hit_menu, idx);
	if (!item) {
		return true;
	}

	/* If item has submenu, toggle it */
	if (item->submenu) {
		if (item->submenu->visible) {
			wm_menu_hide(item->submenu);
		} else {
			hit_menu->selected_index = idx;
			menu_update_render(hit_menu);

			int sub_x = hit_menu->x + hit_menu->width;
			int sub_y = hit_menu->y + hit_menu->border_width +
				hit_menu->title_height;
			int y_off = 0;
			int i = 0;
			struct wm_menu_item *it;
			wl_list_for_each(it, &hit_menu->items, link) {
				if (i >= idx) {
					break;
				}
				if (it->type == WM_MENU_SEPARATOR) {
					y_off += MENU_SEPARATOR_HEIGHT;
				} else {
					y_off += hit_menu->item_height;
				}
				i++;
			}
			wm_menu_show(item->submenu, sub_x, sub_y + y_off);
		}
		return true;
	}

	/* Activate the item */
	if (item->type != WM_MENU_SEPARATOR &&
	    item->type != WM_MENU_NOP) {
		execute_menu_item(hit_menu, item);
		wm_menu_hide(root_menu);
	}

	return true;
}

/* --- Lifecycle --- */

void
wm_menu_destroy(struct wm_menu *menu)
{
	if (!menu) {
		return;
	}

	wm_menu_hide(menu);

	struct wm_menu_item *item, *tmp;
	wl_list_for_each_safe(item, tmp, &menu->items, link) {
		wl_list_remove(&item->link);
		menu_item_destroy(item);
	}

	free(menu->title);
	free(menu);
}

bool
wm_menu_is_open(struct wm_server *server)
{
	/* This will be wired up by integration to check root_menu/window_menu */
	(void)server;
	return false;
}

/* --- Action dispatch --- */

void
wm_menu_show_root(struct wm_server *server, int x, int y)
{
	/* Will be wired up by integration - needs root_menu on server */
	(void)server;
	(void)x;
	(void)y;
}

void
wm_menu_show_window(struct wm_server *server, int x, int y)
{
	/* Will be wired up by integration - needs window_menu on server */
	(void)server;
	(void)x;
	(void)y;
}
