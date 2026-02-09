/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
 *
 * style.c - Fluxbox style/theme file parser
 *
 * Parses Fluxbox-compatible style files using the rcparser to read
 * key-value pairs and maps them into the wm_style structure.
 */

#define _GNU_SOURCE

#include "style.h"
#include "rcparser.h"
#include <ctype.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* --- Default values --- */

#define DEFAULT_BORDER_WIDTH   1
#define DEFAULT_BEVEL_WIDTH    1
#define DEFAULT_HANDLE_WIDTH   6
#define DEFAULT_TITLE_HEIGHT   0	/* auto */
#define DEFAULT_FONT_SIZE      10
#define DEFAULT_FONT_FAMILY    "sans"

/* Default Fluxbox-like colors */
#define DEFAULT_FOCUS_COLOR       0xFF333333
#define DEFAULT_UNFOCUS_COLOR     0xFF222222
#define DEFAULT_TEXT_COLOR         0xFFE0E0E0
#define DEFAULT_UNFOCUS_TEXT_COLOR 0xFF999999
#define DEFAULT_BORDER_COLOR      0xFF000000
#define DEFAULT_FRAME_COLOR       0xFF555555

/* --- Static helpers --- */

static void
safe_free(char **p)
{
	free(*p);
	*p = NULL;
}

static struct wm_color
make_color(uint8_t r, uint8_t g, uint8_t b)
{
	return (struct wm_color){ .r = r, .g = g, .b = b, .a = 0xFF };
}

static struct wm_color
color_from_argb(uint32_t argb)
{
	return (struct wm_color){
		.r = (argb >> 16) & 0xFF,
		.g = (argb >> 8) & 0xFF,
		.b = argb & 0xFF,
		.a = (argb >> 24) & 0xFF,
	};
}

static uint32_t
argb_from_color(struct wm_color c)
{
	return ((uint32_t)c.a << 24) | ((uint32_t)c.r << 16) |
		((uint32_t)c.g << 8) | (uint32_t)c.b;
}

/* --- Color parsing --- */

struct wm_color
style_parse_color(const char *value)
{
	if (!value || *value == '\0')
		return make_color(0, 0, 0);

	/* Skip leading whitespace */
	while (isspace((unsigned char)*value))
		value++;

	/* #RRGGBB format */
	if (*value == '#') {
		unsigned int r = 0, g = 0, b = 0;
		if (sscanf(value, "#%02x%02x%02x", &r, &g, &b) == 3)
			return make_color(r, g, b);
		/* Try short #RGB form */
		if (sscanf(value, "#%1x%1x%1x", &r, &g, &b) == 3)
			return make_color(r * 17, g * 17, b * 17);
		return make_color(0, 0, 0);
	}

	/* rgb:R/G/B format (values 0-255) */
	if (strncasecmp(value, "rgb:", 4) == 0) {
		unsigned int r = 0, g = 0, b = 0;
		if (sscanf(value + 4, "%u/%u/%u", &r, &g, &b) == 3) {
			if (r > 255) r = 255;
			if (g > 255) g = 255;
			if (b > 255) b = 255;
			return make_color(r, g, b);
		}
		/* Try hex components: rgb:RR/GG/BB */
		if (sscanf(value + 4, "%x/%x/%x", &r, &g, &b) == 3) {
			if (r > 255) r = 255;
			if (g > 255) g = 255;
			if (b > 255) b = 255;
			return make_color(r, g, b);
		}
		return make_color(0, 0, 0);
	}

	/* Named colors - support a few basics */
	if (strcasecmp(value, "white") == 0)
		return make_color(255, 255, 255);
	if (strcasecmp(value, "black") == 0)
		return make_color(0, 0, 0);
	if (strcasecmp(value, "red") == 0)
		return make_color(255, 0, 0);
	if (strcasecmp(value, "green") == 0)
		return make_color(0, 255, 0);
	if (strcasecmp(value, "blue") == 0)
		return make_color(0, 0, 255);
	if (strcasecmp(value, "grey") == 0 || strcasecmp(value, "gray") == 0)
		return make_color(190, 190, 190);

	return make_color(0, 0, 0);
}

/* --- Font parsing --- */

struct wm_font
style_parse_font(const char *value)
{
	struct wm_font font = {
		.family = strdup(DEFAULT_FONT_FAMILY),
		.size = DEFAULT_FONT_SIZE,
		.bold = false,
		.italic = false,
		.shadow_x = 0,
		.shadow_y = 0,
		.shadow_color = make_color(0, 0, 0),
	};

	if (!value || *value == '\0')
		return font;

	/* Skip leading whitespace */
	while (isspace((unsigned char)*value))
		value++;

	/* Format: "family-size:style:style" or "family-size" */
	char *buf = strdup(value);
	if (!buf)
		return font;

	/* Split at first colon to separate family-size from styles */
	char *colon = strchr(buf, ':');
	char *styles = NULL;
	if (colon) {
		*colon = '\0';
		styles = colon + 1;
	}

	/* Parse family-size: find last dash followed by digits */
	char *dash = strrchr(buf, '-');
	if (dash && dash != buf) {
		/* Check that everything after the dash is a digit */
		bool all_digits = true;
		for (char *p = dash + 1; *p; p++) {
			if (!isdigit((unsigned char)*p)) {
				all_digits = false;
				break;
			}
		}
		if (all_digits && *(dash + 1) != '\0') {
			*dash = '\0';
			font.size = atoi(dash + 1);
			if (font.size <= 0)
				font.size = DEFAULT_FONT_SIZE;
		}
	}

	/* Set family */
	if (*buf != '\0') {
		free(font.family);
		font.family = strdup(buf);
	}

	/* Parse style flags from colon-separated parts */
	if (styles) {
		char *saveptr = NULL;
		char *token = strtok_r(styles, ":", &saveptr);
		while (token) {
			while (isspace((unsigned char)*token))
				token++;
			if (strcasecmp(token, "bold") == 0)
				font.bold = true;
			else if (strcasecmp(token, "italic") == 0)
				font.italic = true;
			token = strtok_r(NULL, ":", &saveptr);
		}
	}

	free(buf);
	return font;
}

/* --- Texture parsing --- */

struct wm_texture
style_parse_texture(const char *value)
{
	struct wm_texture tex = {
		.appearance = WM_TEX_RAISED,
		.fill = WM_TEX_SOLID,
		.gradient = WM_GRAD_VERTICAL,
		.bevel = WM_BEVEL1,
		.interlaced = false,
		.color = DEFAULT_FOCUS_COLOR,
		.color_to = DEFAULT_UNFOCUS_COLOR,
		.pixmap_path = NULL,
	};

	if (!value || *value == '\0')
		return tex;

	/* Check for special types first */
	if (strcasecmp(value, "parentrelative") == 0 ||
	    strcasecmp(value, "parent_relative") == 0) {
		tex.fill = WM_TEX_PARENT_RELATIVE;
		tex.appearance = WM_TEX_FLAT;
		tex.bevel = WM_BEVEL_NONE;
		return tex;
	}

	char *buf = strdup(value);
	if (!buf)
		return tex;

	/* Default bevel depends on whether user specifies one explicitly */
	bool bevel_set = false;

	/* Tokenize and match keywords case-insensitively */
	char *saveptr = NULL;
	char *token = strtok_r(buf, " \t", &saveptr);
	while (token) {
		/* Appearance */
		if (strcasecmp(token, "flat") == 0) {
			tex.appearance = WM_TEX_FLAT;
		} else if (strcasecmp(token, "raised") == 0) {
			tex.appearance = WM_TEX_RAISED;
		} else if (strcasecmp(token, "sunken") == 0) {
			tex.appearance = WM_TEX_SUNKEN;
		}
		/* Fill */
		else if (strcasecmp(token, "gradient") == 0) {
			tex.fill = WM_TEX_GRADIENT;
		} else if (strcasecmp(token, "solid") == 0) {
			tex.fill = WM_TEX_SOLID;
		} else if (strcasecmp(token, "pixmap") == 0) {
			tex.fill = WM_TEX_PIXMAP;
		}
		/* Gradient type */
		else if (strcasecmp(token, "horizontal") == 0) {
			tex.gradient = WM_GRAD_HORIZONTAL;
		} else if (strcasecmp(token, "vertical") == 0) {
			tex.gradient = WM_GRAD_VERTICAL;
		} else if (strcasecmp(token, "diagonal") == 0) {
			tex.gradient = WM_GRAD_DIAGONAL;
		} else if (strcasecmp(token, "crossdiagonal") == 0) {
			tex.gradient = WM_GRAD_CROSSDIAGONAL;
		} else if (strcasecmp(token, "pipecross") == 0) {
			tex.gradient = WM_GRAD_PIPECROSS;
		} else if (strcasecmp(token, "elliptic") == 0) {
			tex.gradient = WM_GRAD_ELLIPTIC;
		} else if (strcasecmp(token, "rectangle") == 0) {
			tex.gradient = WM_GRAD_RECTANGLE;
		} else if (strcasecmp(token, "pyramid") == 0) {
			tex.gradient = WM_GRAD_PYRAMID;
		}
		/* Effects */
		else if (strcasecmp(token, "interlaced") == 0) {
			tex.interlaced = true;
		}
		/* Bevel */
		else if (strcasecmp(token, "bevel1") == 0) {
			tex.bevel = WM_BEVEL1;
			bevel_set = true;
		} else if (strcasecmp(token, "bevel2") == 0) {
			tex.bevel = WM_BEVEL2;
			bevel_set = true;
		}
		token = strtok_r(NULL, " \t", &saveptr);
	}

	/* Flat textures default to no bevel unless explicitly set */
	if (tex.appearance == WM_TEX_FLAT && !bevel_set)
		tex.bevel = WM_BEVEL_NONE;

	/* Non-flat textures default to Bevel1 unless explicitly set */
	if (tex.appearance != WM_TEX_FLAT && !bevel_set)
		tex.bevel = WM_BEVEL1;

	free(buf);
	return tex;
}

/* --- Style resource mapping --- */

/*
 * Get a value from the database, supporting wildcard fallback.
 * First tries the exact key, then tries replacing the specific prefix
 * with '*' to match wildcard entries.
 */
static const char *
style_get(struct rc_database *db, const char *key)
{
	const char *val = rc_get_string(db, key);
	if (val)
		return val;

	/* Try wildcard: extract the last component after the last '.' */
	const char *last_dot = strrchr(key, '.');
	if (!last_dot)
		return NULL;

	char wildcard[256];
	int written = snprintf(wildcard, sizeof(wildcard), "*%s", last_dot);
	if (written < 0 || (size_t)written >= sizeof(wildcard))
		return NULL;

	return rc_get_string(db, wildcard);
}

static int
style_get_int(struct rc_database *db, const char *key, int default_val)
{
	const char *val = style_get(db, key);
	if (!val)
		return default_val;
	char *end;
	long result = strtol(val, &end, 10);
	if (end == val || *end != '\0')
		return default_val;
	return (int)result;
}

static void
load_texture(struct rc_database *db, const char *base_key,
	struct wm_texture *tex, const char *style_dir)
{
	const char *tex_str = style_get(db, base_key);
	if (tex_str)
		*tex = style_parse_texture(tex_str);

	/* Load color */
	char key[256];

	snprintf(key, sizeof(key), "%s.color", base_key);
	const char *color_str = style_get(db, key);
	if (color_str) {
		struct wm_color c = style_parse_color(color_str);
		tex->color = argb_from_color(c);
	}

	snprintf(key, sizeof(key), "%s.colorTo", base_key);
	const char *color_to_str = style_get(db, key);
	if (color_to_str) {
		struct wm_color c = style_parse_color(color_to_str);
		tex->color_to = argb_from_color(c);
	}

	/* Load pixmap path */
	snprintf(key, sizeof(key), "%s.pixmap", base_key);
	const char *pixmap_str = style_get(db, key);
	if (pixmap_str && *pixmap_str != '\0') {
		safe_free(&tex->pixmap_path);
		if (pixmap_str[0] == '/' || !style_dir) {
			tex->pixmap_path = strdup(pixmap_str);
		} else {
			/* Resolve relative to style directory */
			size_t len = strlen(style_dir) + 1 + strlen(pixmap_str) + 1;
			tex->pixmap_path = malloc(len);
			if (tex->pixmap_path)
				snprintf(tex->pixmap_path, len, "%s/%s",
					style_dir, pixmap_str);
		}
	}
}

static void
load_color(struct rc_database *db, const char *key, struct wm_color *color)
{
	const char *val = style_get(db, key);
	if (val)
		*color = style_parse_color(val);
}

static void
load_font(struct rc_database *db, const char *key, struct wm_font *font)
{
	const char *val = style_get(db, key);
	if (!val)
		return;

	/* Free old family */
	free(font->family);

	*font = style_parse_font(val);

	/* Load shadow properties */
	char shadow_key[256];
	snprintf(shadow_key, sizeof(shadow_key), "%s.shadow.x", key);
	font->shadow_x = style_get_int(db, shadow_key, 0);

	snprintf(shadow_key, sizeof(shadow_key), "%s.shadow.y", key);
	font->shadow_y = style_get_int(db, shadow_key, 0);

	snprintf(shadow_key, sizeof(shadow_key), "%s.shadow.color", key);
	const char *sc = style_get(db, shadow_key);
	if (sc)
		font->shadow_color = style_parse_color(sc);
}

static char *
load_pixmap_path(struct rc_database *db, const char *key,
	const char *style_dir)
{
	const char *val = style_get(db, key);
	if (!val || *val == '\0')
		return NULL;
	if (val[0] == '/' || !style_dir)
		return strdup(val);
	size_t len = strlen(style_dir) + 1 + strlen(val) + 1;
	char *path = malloc(len);
	if (path)
		snprintf(path, len, "%s/%s", style_dir, val);
	return path;
}

static enum wm_justify
parse_justify(const char *value)
{
	if (!value)
		return WM_JUSTIFY_LEFT;
	if (strcasecmp(value, "center") == 0 || strcasecmp(value, "centre") == 0)
		return WM_JUSTIFY_CENTER;
	if (strcasecmp(value, "right") == 0)
		return WM_JUSTIFY_RIGHT;
	return WM_JUSTIFY_LEFT;
}

static uint8_t
parse_round_corners(const char *value)
{
	if (!value)
		return 0;
	uint8_t mask = 0;
	if (strcasestr(value, "topleft"))
		mask |= WM_CORNER_TOP_LEFT;
	if (strcasestr(value, "topright"))
		mask |= WM_CORNER_TOP_RIGHT;
	if (strcasestr(value, "bottomleft"))
		mask |= WM_CORNER_BOTTOM_LEFT;
	if (strcasestr(value, "bottomright"))
		mask |= WM_CORNER_BOTTOM_RIGHT;
	return mask;
}

static struct wm_font
default_font(void)
{
	return (struct wm_font){
		.family = strdup(DEFAULT_FONT_FAMILY),
		.size = DEFAULT_FONT_SIZE,
		.bold = false,
		.italic = false,
		.shadow_x = 0,
		.shadow_y = 0,
		.shadow_color = make_color(0, 0, 0),
	};
}

static struct wm_texture
default_texture(uint32_t color, uint32_t color_to)
{
	return (struct wm_texture){
		.appearance = WM_TEX_RAISED,
		.fill = WM_TEX_GRADIENT,
		.gradient = WM_GRAD_VERTICAL,
		.bevel = WM_BEVEL1,
		.interlaced = false,
		.color = color,
		.color_to = color_to,
		.pixmap_path = NULL,
	};
}

/* --- Font cleanup helper --- */

static void
font_finish(struct wm_font *font)
{
	free(font->family);
	font->family = NULL;
}

/* --- Texture cleanup helper --- */

static void
texture_finish(struct wm_texture *tex)
{
	free(tex->pixmap_path);
	tex->pixmap_path = NULL;
}

/* --- Public API --- */

struct wm_style *
style_create(void)
{
	struct wm_style *s = calloc(1, sizeof(*s));
	if (!s)
		return NULL;

	/* Set default textures */
	s->window_title_focus = default_texture(DEFAULT_FOCUS_COLOR,
		DEFAULT_UNFOCUS_COLOR);
	s->window_title_unfocus = default_texture(DEFAULT_UNFOCUS_COLOR,
		DEFAULT_UNFOCUS_COLOR);
	s->window_label_focus = default_texture(DEFAULT_FOCUS_COLOR,
		DEFAULT_UNFOCUS_COLOR);
	s->window_label_unfocus = default_texture(DEFAULT_UNFOCUS_COLOR,
		DEFAULT_UNFOCUS_COLOR);
	s->window_label_active = default_texture(DEFAULT_FOCUS_COLOR,
		DEFAULT_UNFOCUS_COLOR);
	s->window_button_focus = default_texture(DEFAULT_FOCUS_COLOR,
		DEFAULT_UNFOCUS_COLOR);
	s->window_button_unfocus = default_texture(DEFAULT_UNFOCUS_COLOR,
		DEFAULT_UNFOCUS_COLOR);
	s->window_button_pressed = default_texture(DEFAULT_UNFOCUS_COLOR,
		DEFAULT_FOCUS_COLOR);
	s->window_handle_focus = default_texture(DEFAULT_FOCUS_COLOR,
		DEFAULT_UNFOCUS_COLOR);
	s->window_handle_unfocus = default_texture(DEFAULT_UNFOCUS_COLOR,
		DEFAULT_UNFOCUS_COLOR);
	s->window_grip_focus = default_texture(DEFAULT_FOCUS_COLOR,
		DEFAULT_UNFOCUS_COLOR);
	s->window_grip_unfocus = default_texture(DEFAULT_UNFOCUS_COLOR,
		DEFAULT_UNFOCUS_COLOR);
	s->menu_title = default_texture(DEFAULT_FOCUS_COLOR,
		DEFAULT_UNFOCUS_COLOR);
	s->menu_frame = default_texture(DEFAULT_UNFOCUS_COLOR,
		DEFAULT_UNFOCUS_COLOR);
	s->menu_hilite = default_texture(DEFAULT_FOCUS_COLOR,
		DEFAULT_UNFOCUS_COLOR);
	s->toolbar_texture = default_texture(DEFAULT_FOCUS_COLOR,
		DEFAULT_UNFOCUS_COLOR);

	/* Default colors */
	s->window_label_focus_text_color = color_from_argb(DEFAULT_TEXT_COLOR);
	s->window_label_unfocus_text_color = color_from_argb(DEFAULT_UNFOCUS_TEXT_COLOR);
	s->window_label_active_text_color = color_from_argb(DEFAULT_TEXT_COLOR);
	s->window_button_focus_pic_color = color_from_argb(DEFAULT_TEXT_COLOR);
	s->window_button_unfocus_pic_color = color_from_argb(DEFAULT_UNFOCUS_TEXT_COLOR);
	s->window_button_pressed_pic_color = color_from_argb(DEFAULT_TEXT_COLOR);
	s->window_frame_focus_color = color_from_argb(DEFAULT_FRAME_COLOR);
	s->window_frame_unfocus_color = color_from_argb(DEFAULT_UNFOCUS_COLOR);
	s->window_border_color = color_from_argb(DEFAULT_BORDER_COLOR);
	s->menu_title_text_color = color_from_argb(DEFAULT_TEXT_COLOR);
	s->menu_frame_text_color = color_from_argb(DEFAULT_TEXT_COLOR);
	s->menu_hilite_text_color = color_from_argb(DEFAULT_TEXT_COLOR);
	s->menu_border_color = color_from_argb(DEFAULT_BORDER_COLOR);
	s->toolbar_text_color = color_from_argb(DEFAULT_TEXT_COLOR);

	/* Default dimensions */
	s->window_border_width = DEFAULT_BORDER_WIDTH;
	s->window_bevel_width = DEFAULT_BEVEL_WIDTH;
	s->window_handle_width = DEFAULT_HANDLE_WIDTH;
	s->window_title_height = DEFAULT_TITLE_HEIGHT;
	s->menu_border_width = DEFAULT_BORDER_WIDTH;

	/* Default fonts */
	s->window_label_focus_font = default_font();
	s->window_label_unfocus_font = default_font();
	s->menu_title_font = default_font();
	s->menu_frame_font = default_font();
	s->toolbar_font = default_font();

	/* Default justify */
	s->window_justify = WM_JUSTIFY_LEFT;
	s->menu_title_justify = WM_JUSTIFY_CENTER;

	return s;
}

static void
style_apply_db(struct wm_style *s, struct rc_database *db)
{
	const char *sd = s->style_dir;

	/* --- Window title --- */
	load_texture(db, "window.title.focus", &s->window_title_focus, sd);
	load_texture(db, "window.title.unfocus", &s->window_title_unfocus, sd);
	s->window_title_height = style_get_int(db, "window.title.height",
		s->window_title_height);

	/* --- Window label --- */
	load_texture(db, "window.label.focus", &s->window_label_focus, sd);
	load_texture(db, "window.label.unfocus", &s->window_label_unfocus, sd);
	load_texture(db, "window.label.active", &s->window_label_active, sd);
	load_color(db, "window.label.focus.textColor",
		&s->window_label_focus_text_color);
	load_color(db, "window.label.unfocus.textColor",
		&s->window_label_unfocus_text_color);
	load_color(db, "window.label.active.textColor",
		&s->window_label_active_text_color);
	load_font(db, "window.label.focus.font",
		&s->window_label_focus_font);
	load_font(db, "window.label.unfocus.font",
		&s->window_label_unfocus_font);
	s->window_justify = parse_justify(style_get(db, "window.justify"));

	/* --- Window buttons --- */
	load_texture(db, "window.button.focus", &s->window_button_focus, sd);
	load_texture(db, "window.button.unfocus", &s->window_button_unfocus, sd);
	load_texture(db, "window.button.pressed", &s->window_button_pressed, sd);
	load_color(db, "window.button.focus.picColor",
		&s->window_button_focus_pic_color);
	load_color(db, "window.button.unfocus.picColor",
		&s->window_button_unfocus_pic_color);
	load_color(db, "window.button.pressed.picColor",
		&s->window_button_pressed_pic_color);

	/* Button pixmaps */
	safe_free(&s->window_close_pixmap);
	s->window_close_pixmap = load_pixmap_path(db, "window.close.pixmap", sd);
	safe_free(&s->window_close_unfocus_pixmap);
	s->window_close_unfocus_pixmap = load_pixmap_path(db,
		"window.close.unfocus.pixmap", sd);
	safe_free(&s->window_close_pressed_pixmap);
	s->window_close_pressed_pixmap = load_pixmap_path(db,
		"window.close.pressed.pixmap", sd);

	safe_free(&s->window_maximize_pixmap);
	s->window_maximize_pixmap = load_pixmap_path(db,
		"window.maximize.pixmap", sd);
	safe_free(&s->window_maximize_unfocus_pixmap);
	s->window_maximize_unfocus_pixmap = load_pixmap_path(db,
		"window.maximize.unfocus.pixmap", sd);
	safe_free(&s->window_maximize_pressed_pixmap);
	s->window_maximize_pressed_pixmap = load_pixmap_path(db,
		"window.maximize.pressed.pixmap", sd);

	safe_free(&s->window_iconify_pixmap);
	s->window_iconify_pixmap = load_pixmap_path(db,
		"window.iconify.pixmap", sd);
	safe_free(&s->window_iconify_unfocus_pixmap);
	s->window_iconify_unfocus_pixmap = load_pixmap_path(db,
		"window.iconify.unfocus.pixmap", sd);
	safe_free(&s->window_iconify_pressed_pixmap);
	s->window_iconify_pressed_pixmap = load_pixmap_path(db,
		"window.iconify.pressed.pixmap", sd);

	safe_free(&s->window_shade_pixmap);
	s->window_shade_pixmap = load_pixmap_path(db,
		"window.shade.pixmap", sd);
	safe_free(&s->window_shade_unfocus_pixmap);
	s->window_shade_unfocus_pixmap = load_pixmap_path(db,
		"window.shade.unfocus.pixmap", sd);
	safe_free(&s->window_shade_pressed_pixmap);
	s->window_shade_pressed_pixmap = load_pixmap_path(db,
		"window.shade.pressed.pixmap", sd);

	safe_free(&s->window_stick_pixmap);
	s->window_stick_pixmap = load_pixmap_path(db,
		"window.stick.pixmap", sd);
	safe_free(&s->window_stick_unfocus_pixmap);
	s->window_stick_unfocus_pixmap = load_pixmap_path(db,
		"window.stick.unfocus.pixmap", sd);
	safe_free(&s->window_stick_pressed_pixmap);
	s->window_stick_pressed_pixmap = load_pixmap_path(db,
		"window.stick.pressed.pixmap", sd);

	/* --- Window handle and grips --- */
	load_texture(db, "window.handle.focus", &s->window_handle_focus, sd);
	load_texture(db, "window.handle.unfocus", &s->window_handle_unfocus, sd);
	s->window_handle_width = style_get_int(db, "window.handleWidth",
		s->window_handle_width);
	load_texture(db, "window.grip.focus", &s->window_grip_focus, sd);
	load_texture(db, "window.grip.unfocus", &s->window_grip_unfocus, sd);

	/* --- Window frame --- */
	load_color(db, "window.frame.focusColor",
		&s->window_frame_focus_color);
	load_color(db, "window.frame.unfocusColor",
		&s->window_frame_unfocus_color);

	/* --- Window borders and geometry --- */
	load_color(db, "window.borderColor", &s->window_border_color);
	s->window_border_width = style_get_int(db, "window.borderWidth",
		s->window_border_width);
	s->window_bevel_width = style_get_int(db, "window.bevelWidth",
		s->window_bevel_width);
	const char *rc = style_get(db, "window.roundCorners");
	if (rc)
		s->window_round_corners = parse_round_corners(rc);

	/* --- Menu --- */
	load_texture(db, "menu.title", &s->menu_title, sd);
	load_color(db, "menu.title.textColor", &s->menu_title_text_color);
	load_font(db, "menu.title.font", &s->menu_title_font);
	s->menu_title_justify = parse_justify(style_get(db, "menu.title.justify"));
	load_texture(db, "menu.frame", &s->menu_frame, sd);
	load_color(db, "menu.frame.textColor", &s->menu_frame_text_color);
	load_font(db, "menu.frame.font", &s->menu_frame_font);
	load_texture(db, "menu.hilite", &s->menu_hilite, sd);
	load_color(db, "menu.hilite.textColor", &s->menu_hilite_text_color);
	load_color(db, "menu.borderColor", &s->menu_border_color);
	s->menu_border_width = style_get_int(db, "menu.borderWidth",
		s->menu_border_width);

	/* --- Toolbar --- */
	load_texture(db, "toolbar", &s->toolbar_texture, sd);
	load_color(db, "toolbar.textColor", &s->toolbar_text_color);
	load_font(db, "toolbar.font", &s->toolbar_font);
}

/*
 * Determine the style directory for resolving relative pixmap paths.
 * A Fluxbox style can be a file or a directory containing theme.cfg.
 */
static char *
resolve_style_dir(const char *path)
{
	char *copy = strdup(path);
	if (!copy)
		return NULL;
	char *dir = dirname(copy);
	char *result = strdup(dir);
	free(copy);
	return result;
}

/*
 * Check if path is a directory containing a theme.cfg or style.cfg.
 * Returns allocated path to the actual style file, or NULL if not a directory.
 */
static char *
resolve_style_path(const char *path)
{
	/* Try path/theme.cfg first, then path/style.cfg */
	size_t len = strlen(path);
	char *try_path = malloc(len + 12);	/* /theme.cfg\0 */
	if (!try_path)
		return NULL;

	snprintf(try_path, len + 12, "%s/theme.cfg", path);
	FILE *f = fopen(try_path, "r");
	if (f) {
		fclose(f);
		return try_path;
	}

	snprintf(try_path, len + 12, "%s/style.cfg", path);
	f = fopen(try_path, "r");
	if (f) {
		fclose(f);
		return try_path;
	}

	free(try_path);
	return NULL;
}

int
style_load(struct wm_style *s, const char *path)
{
	if (!s || !path)
		return -1;

	/* Determine actual file path and style directory */
	char *actual_path = resolve_style_path(path);
	bool is_dir_style = (actual_path != NULL);

	if (!actual_path)
		actual_path = strdup(path);
	if (!actual_path)
		return -1;

	/* Set style_dir: the directory of the style file, or the style
	 * directory itself if it was a directory-based style */
	safe_free(&s->style_dir);
	if (is_dir_style) {
		s->style_dir = strdup(path);
	} else {
		s->style_dir = resolve_style_dir(actual_path);
	}

	safe_free(&s->style_path);
	s->style_path = strdup(actual_path);

	struct rc_database *db = rc_create();
	if (!db) {
		free(actual_path);
		return -1;
	}

	int ret = rc_load(db, actual_path);
	free(actual_path);

	if (ret != 0) {
		rc_destroy(db);
		return -1;
	}

	style_apply_db(s, db);
	rc_destroy(db);
	return 0;
}

int
style_load_overlay(struct wm_style *s, const char *path)
{
	if (!s || !path)
		return -1;

	struct rc_database *db = rc_create();
	if (!db)
		return -1;

	if (rc_load(db, path) != 0) {
		rc_destroy(db);
		return -1;
	}

	style_apply_db(s, db);
	rc_destroy(db);
	return 0;
}

int
style_reload(struct wm_style *s)
{
	if (!s || !s->style_path)
		return -1;

	char *path = strdup(s->style_path);
	if (!path)
		return -1;

	/* Re-initialize to defaults, then reload */
	struct wm_style *fresh = style_create();
	if (!fresh) {
		free(path);
		return -1;
	}

	/* Preserve style_path and style_dir temporarily */
	free(fresh->style_dir);
	free(fresh->style_path);
	fresh->style_dir = s->style_dir;
	fresh->style_path = s->style_path;
	s->style_dir = NULL;
	s->style_path = NULL;

	/* Destroy the old style contents */
	style_destroy(s);

	/* Copy fresh defaults into s (caller still owns the pointer) */
	/* Actually we need to load into fresh and copy back */
	int ret = style_load(fresh, path);
	free(path);

	if (ret != 0) {
		style_destroy(fresh);
		return -1;
	}

	/* Copy everything from fresh to s */
	memcpy(s, fresh, sizeof(*s));
	/* Free the shell without freeing contents (now owned by s) */
	free(fresh);

	return 0;
}

void
style_destroy(struct wm_style *s)
{
	if (!s)
		return;

	/* Textures */
	texture_finish(&s->window_title_focus);
	texture_finish(&s->window_title_unfocus);
	texture_finish(&s->window_label_focus);
	texture_finish(&s->window_label_unfocus);
	texture_finish(&s->window_label_active);
	texture_finish(&s->window_button_focus);
	texture_finish(&s->window_button_unfocus);
	texture_finish(&s->window_button_pressed);
	texture_finish(&s->window_handle_focus);
	texture_finish(&s->window_handle_unfocus);
	texture_finish(&s->window_grip_focus);
	texture_finish(&s->window_grip_unfocus);
	texture_finish(&s->menu_title);
	texture_finish(&s->menu_frame);
	texture_finish(&s->menu_hilite);
	texture_finish(&s->toolbar_texture);

	/* Fonts */
	font_finish(&s->window_label_focus_font);
	font_finish(&s->window_label_unfocus_font);
	font_finish(&s->menu_title_font);
	font_finish(&s->menu_frame_font);
	font_finish(&s->toolbar_font);

	/* Button pixmaps */
	free(s->window_close_pixmap);
	free(s->window_close_unfocus_pixmap);
	free(s->window_close_pressed_pixmap);
	free(s->window_maximize_pixmap);
	free(s->window_maximize_unfocus_pixmap);
	free(s->window_maximize_pressed_pixmap);
	free(s->window_iconify_pixmap);
	free(s->window_iconify_unfocus_pixmap);
	free(s->window_iconify_pressed_pixmap);
	free(s->window_shade_pixmap);
	free(s->window_shade_unfocus_pixmap);
	free(s->window_shade_pressed_pixmap);
	free(s->window_stick_pixmap);
	free(s->window_stick_unfocus_pixmap);
	free(s->window_stick_pressed_pixmap);

	/* Metadata */
	free(s->style_dir);
	free(s->style_path);

	free(s);
}
