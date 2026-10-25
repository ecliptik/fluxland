/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * style.c - Fluxbox style file parser
 *
 * Parses Fluxbox-compatible style files using the rcparser to read
 * key-value pairs and maps them into the wm_style structure.
 */

#define _GNU_SOURCE

#include "style.h"
#include "x11_colors.h"
#include "rcparser.h"
#include "util.h"
#include <ctype.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define MAX_FONT_SIZE 200

/* Forward declaration */
static void style_clear(struct wm_style *s);
static void texture_finish(struct wm_texture *tex);
static void font_finish(struct wm_font *font);
static char *load_pixmap_path(struct rc_database *db, const char *key,
	const char *style_dir);
static uint8_t parse_round_corners(const char *value);

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
/* Fluxbox's, for a texture without .color or .colorTo */
#define DEFAULT_TEXTURE_COLOR     0xFFA9A9A9	/* darkgray */
#define DEFAULT_TEXTURE_COLOR_TO  0xFFFFFFFF	/* white */
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

/*
 * Scale an rgb: component value based on its hex digit count per X11 spec.
 * X11's XParseColor defines rgb:R/G/B where each component is 1-4 hex digits,
 * scaled to 8-bit by repeating (1 digit), identity (2), or shifting (3-4).
 */
static unsigned int
scale_rgb_component(unsigned int val, size_t digits)
{
	switch (digits) {
	case 1: return val * 17;	/* 0xC -> 0xCC */
	case 2: return val;		/* 0xCC -> 0xCC */
	case 3: return val >> 4;	/* 0xCCC -> 0xCC */
	case 4: return val >> 8;	/* 0xCCCC -> 0xCC */
	default: return val > 255 ? 255 : val;
	}
}

static int
compare_x11_color(const void *key, const void *elem)
{
	return strcmp(key, ((const struct wm_x11_color *)elem)->name);
}

/*
 * A colour as XParseColor reads one (Fluxbox's FbTk::Color): #RGB to
 * #RRRRGGGGBBBB with the digits as the high bits, rgb:R/G/B with each
 * scaled to its digits, or an X11 colour name in any case and with any
 * spaces ("Light Steel Blue").  False for anything else.
 */
bool
style_try_parse_color(const char *value, struct wm_color *out)
{
	if (!value)
		return false;
	while (isspace((unsigned char)*value))
		value++;
	size_t len = strlen(value);
	while (len > 0 && isspace((unsigned char)value[len - 1]))
		len--;
	if (len == 0)
		return false;

	if (*value == '#') {
		size_t digits = len - 1;
		if (digits < 3 || digits > 12 || digits % 3 != 0)
			return false;
		size_t n = digits / 3;
		unsigned int c[3];
		for (int i = 0; i < 3; i++) {
			c[i] = 0;
			for (size_t j = 0; j < n; j++) {
				char ch = value[1 + i * n + j];
				if (!isxdigit((unsigned char)ch))
					return false;
				c[i] = c[i] * 16 + (unsigned int)(isdigit(
					(unsigned char)ch) ? ch - '0' :
					tolower((unsigned char)ch) - 'a' + 10);
			}
			/* The digits are the top bits of 16 */
			c[i] <<= 16 - 4 * n;
			c[i] >>= 8;
		}
		*out = make_color(c[0], c[1], c[2]);
		return true;
	}

	if (strncasecmp(value, "rgb:", 4) == 0) {
		char rs[8], gs[8], bs[8];
		if (sscanf(value + 4, "%7[^/]/%7[^/]/%7s", rs, gs, bs) != 3)
			return false;
		const char *parts[3] = { rs, gs, bs };
		unsigned int c[3];
		for (int i = 0; i < 3; i++) {
			size_t n = strspn(parts[i], "0123456789abcdefABCDEF");
			if (n == 0 || n > 4 || parts[i][n] != '\0')
				return false;
			c[i] = scale_rgb_component(
				(unsigned int)strtoul(parts[i], NULL, 16), n);
			if (c[i] > 255)
				c[i] = 255;
		}
		*out = make_color(c[0], c[1], c[2]);
		return true;
	}

	char name[64];
	size_t k = 0;
	for (size_t i = 0; i < len && k < sizeof(name) - 1; i++) {
		if (!isspace((unsigned char)value[i]))
			name[k++] = (char)tolower((unsigned char)value[i]);
	}
	name[k] = '\0';
	const struct wm_x11_color *x = bsearch(name, wm_x11_colors,
		sizeof(wm_x11_colors) / sizeof(wm_x11_colors[0]),
		sizeof(wm_x11_colors[0]), compare_x11_color);
	if (!x)
		return false;
	*out = make_color(x->r, x->g, x->b);
	return true;
}

struct wm_color
style_parse_color(const char *value)
{
	struct wm_color c;
	return style_try_parse_color(value, &c) ? c : make_color(0, 0, 0);
}

/* --- Font parsing --- */

/*
 * An X font name (XLFD): -foundry-family-weight-slant-setwidth-style-
 * pixels-points-...; the size in pixels, as X drew it.
 */
static bool
parse_xlfd(const char *name, struct wm_font *font)
{
	char buf[256];
	snprintf(buf, sizeof(buf), "%s", name);
	char *field[14] = { 0 };
	int n = 0;
	char *p = buf + 1;
	while (n < 14) {
		field[n++] = p;
		char *dash = strchr(p, '-');
		if (!dash)
			break;
		*dash = '\0';
		p = dash + 1;
	}
	if (n < 2)
		return false;
	if (field[1][0] && strcmp(field[1], "*") != 0) {
		free(font->family);
		font->family = strdup(field[1]);
	}
	if (n > 2 && (strcasestr(field[2], "bold") ||
	    strcasecmp(field[2], "black") == 0 ||
	    strcasecmp(field[2], "heavy") == 0))
		font->bold = true;
	if (n > 3 && (strcasecmp(field[3], "i") == 0 ||
	    strcasecmp(field[3], "o") == 0))
		font->italic = true;
	int v;
	if (n > 6 && safe_atoi(field[6], &v) && v > 0 && v <= MAX_FONT_SIZE) {
		font->size = v;
		font->pixel_size = true;
	} else if (n > 7 && safe_atoi(field[7], &v) && v >= 10 &&
		   v / 10 <= MAX_FONT_SIZE) {
		font->size = v / 10;	/* decipoints */
		font->pixel_size = false;
	}
	return true;
}

/*
 * A font as Fluxbox's FbTk::Font loads one: of "a|b|c" the first
 * (fontconfig always finds one); an X font name (-*-helvetica-...); or
 * a fontconfig pattern, "Sans-10:bold", "sans:pixelsize=11:weight=bold"
 * (fluxland 1.x's ":halo" too).  The default is a sans 10 point font.
 */
struct wm_font
style_parse_font(const char *value)
{
	struct wm_font font = {
		.family = strdup(DEFAULT_FONT_FAMILY),
		.size = DEFAULT_FONT_SIZE,
		.shadow_color = make_color(0, 0, 0),
		.halo_color = make_color(0, 0, 0),
	};
	if (!value)
		return font;

	/* The first of a list, without its spaces around */
	char buf[256];
	while (isspace((unsigned char)*value))
		value++;
	size_t len = strcspn(value, "|");
	if (len >= sizeof(buf))
		len = sizeof(buf) - 1;
	memcpy(buf, value, len);
	buf[len] = '\0';
	while (len > 0 && isspace((unsigned char)buf[len - 1]))
		buf[--len] = '\0';
	if (len == 0)
		return font;

	if (buf[0] == '-') {
		parse_xlfd(buf, &font);
		return font;
	}
	/* Fluxbox's names for its default font */
	if (strcmp(buf, "fixed") == 0 || strcmp(buf, "__DEFAULT__") == 0)
		return font;

	/* family[-size][:option...]; the family may have spaces and the
	 * size follows its last dash */
	char *styles = strchr(buf, ':');
	if (styles)
		*styles++ = '\0';
	char *dash = strrchr(buf, '-');
	if (dash && dash != buf && dash[1] &&
	    strspn(dash + 1, "0123456789.") == strlen(dash + 1)) {
		*dash = '\0';
		int size = (int)(strtod(dash + 1, NULL) + 0.5);
		if (size > 0 && size <= MAX_FONT_SIZE)
			font.size = size;
	}
	if (buf[0]) {
		free(font.family);
		font.family = strdup(buf);
	}

	for (char *save = NULL, *opt = styles ?
	     strtok_r(styles, ":", &save) : NULL; opt;
	     opt = strtok_r(NULL, ":", &save)) {
		while (isspace((unsigned char)*opt))
			opt++;
		char *eq = strchr(opt, '=');
		const char *val = eq ? eq + 1 : NULL;
		if (eq)
			*eq = '\0';
		int v;
		if (strcasecmp(opt, "bold") == 0) {
			font.bold = true;
		} else if (strcasecmp(opt, "italic") == 0 ||
			   strcasecmp(opt, "oblique") == 0) {
			font.italic = true;
		} else if (strcasecmp(opt, "halo") == 0) {
			font.halo = true;
		} else if (val && strcasecmp(opt, "size") == 0) {
			v = (int)(strtod(val, NULL) + 0.5);
			if (v > 0 && v <= MAX_FONT_SIZE) {
				font.size = v;
				font.pixel_size = false;
			}
		} else if (val && strcasecmp(opt, "pixelsize") == 0) {
			v = (int)(strtod(val, NULL) + 0.5);
			if (v > 0 && v <= MAX_FONT_SIZE) {
				font.size = v;
				font.pixel_size = true;
			}
		} else if (val && (strcasecmp(opt, "weight") == 0 ||
			   strcasecmp(opt, "style") == 0)) {
			if (strcasestr(val, "bold") || strcasestr(val, "black") ||
			    strcasestr(val, "heavy") || (safe_atoi(val, &v) &&
			    v >= 150))
				font.bold = true;
			if (strcasestr(val, "italic") || strcasestr(val, "oblique"))
				font.italic = true;
		} else if (val && strcasecmp(opt, "slant") == 0) {
			if (strcasestr(val, "italic") || strcasestr(val, "oblique") ||
			    (safe_atoi(val, &v) && v > 0))
				font.italic = true;
		}
	}
	return font;
}

/* --- Texture parsing --- */

/*
 * A texture as Fluxbox reads one (FbTk::Texture::setFromString): words
 * are found anywhere and in any case ("FlatCrossDiagonalGradient"),
 * with Flat, Solid and Diagonal when none is given.  fluxland 1.x's
 * Pixmap keyword is kept; Fluxbox draws a texture's .pixmap whenever
 * it has one.
 */
struct wm_texture
style_parse_texture(const char *value)
{
	struct wm_texture tex = {
		.appearance = WM_TEX_FLAT,
		.fill = WM_TEX_SOLID,
		.gradient = WM_GRAD_DIAGONAL,
		.bevel = WM_BEVEL_NONE,
		.color = DEFAULT_TEXTURE_COLOR,
		.color_to = DEFAULT_TEXTURE_COLOR_TO,
	};

	if (!value)
		return tex;
	if (strcasestr(value, "parentrelative") ||
	    strcasestr(value, "parent_relative")) {
		tex.fill = WM_TEX_PARENT_RELATIVE;
		return tex;
	}

	if (strcasestr(value, "gradient")) {
		static const struct {
			const char *word;
			enum wm_gradient_type type;
		} types[] = {
			/* Fluxbox's order: crossdiagonal before diagonal */
			{ "crossdiagonal", WM_GRAD_CROSSDIAGONAL },
			{ "rectangle", WM_GRAD_RECTANGLE },
			{ "pyramid", WM_GRAD_PYRAMID },
			{ "pipecross", WM_GRAD_PIPECROSS },
			{ "elliptic", WM_GRAD_ELLIPTIC },
			{ "diagonal", WM_GRAD_DIAGONAL },
			{ "horizontal", WM_GRAD_HORIZONTAL },
			{ "vertical", WM_GRAD_VERTICAL },
		};
		tex.fill = WM_TEX_GRADIENT;
		for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
			if (strcasestr(value, types[i].word)) {
				tex.gradient = types[i].type;
				break;
			}
		}
	} else if (!strcasestr(value, "solid") &&
		   strcasestr(value, "pixmap")) {
		tex.fill = WM_TEX_PIXMAP;
	}

	if (strcasestr(value, "raised"))
		tex.appearance = WM_TEX_RAISED;
	else if (strcasestr(value, "sunken"))
		tex.appearance = WM_TEX_SUNKEN;

	if (tex.appearance != WM_TEX_FLAT)
		tex.bevel = strcasestr(value, "bevel2") ? WM_BEVEL2 : WM_BEVEL1;
	tex.invert = strcasestr(value, "invert") != NULL;
	tex.interlaced = strcasestr(value, "interlaced") != NULL;
	tex.tiled = strcasestr(value, "tiled") != NULL;
	return tex;
}

/* --- Style resource mapping --- */

/*
 * X resource matching, as Fluxbox reads styles with XrmGetResource: an
 * entry such as "*unfocus.color" or "menu*Color" matches a key when its
 * components match the key's in order, '.' binding to the next one,
 * '*' skipping any number, '?' standing for one.  Names and class
 * names (Color) are told apart by case only, so matching without case
 * covers both.  The most specific entry wins, compared level by level
 * from the left: a component over a skipped level, a name over '?',
 * '.' over '*'.
 */
#define XRM_MAX_LEVELS 12

struct xrm_entry {
	int count;
	const char *name[XRM_MAX_LEVELS];
	size_t len[XRM_MAX_LEVELS];
	bool loose[XRM_MAX_LEVELS];   /* '*' before the component */
};

static bool
xrm_split_entry(const char *key, struct xrm_entry *e)
{
	e->count = 0;
	bool loose = false;
	const char *p = key;
	while (*p) {
		if (*p == '*' || *p == '.') {
			loose |= *p == '*';
			p++;
			continue;
		}
		if (e->count == XRM_MAX_LEVELS)
			return false;
		size_t n = strcspn(p, ".*");
		e->name[e->count] = p;
		e->len[e->count] = n;
		e->loose[e->count] = loose;
		e->count++;
		loose = false;
		p += n;
	}
	/* "a.b*" can't match: the last component must be named */
	return e->count > 0 && p[-1] != '*' && p[-1] != '.';
}

/* Best score of entry components j.. against key levels i.. */
static bool
xrm_match(const struct xrm_entry *e, int j, const struct xrm_entry *k,
	int i, int *score, int *best)
{
	if (j == e->count || i == k->count) {
		if (j != e->count || i != k->count)
			return false;
		for (int l = 0; l < k->count; l++) {
			if (score[l] != best[l]) {
				if (score[l] > best[l])
					memcpy(best, score, sizeof(int) * k->count);
				break;
			}
		}
		return true;
	}
	bool any = false;
	bool any_one = e->len[j] == 1 && e->name[j][0] == '?';
	if (any_one || (e->len[j] == k->len[i] &&
	    strncasecmp(e->name[j], k->name[i], k->len[i]) == 0)) {
		score[i] = (any_one ? 2 : 4) + !e->loose[j];
		any |= xrm_match(e, j + 1, k, i + 1, score, best);
	}
	if (e->loose[j]) {
		score[i] = 0;
		any |= xrm_match(e, j, k, i + 1, score, best);
	}
	return any;
}

static const char *
xrm_get(struct rc_database *db, const char *key)
{
	/* An exact key needs no search */
	const char *val = rc_get_string(db, key);
	if (val)
		return val;

	struct xrm_entry k, e;
	if (!xrm_split_entry(key, &k))
		return NULL;
	int best[XRM_MAX_LEVELS], score[XRM_MAX_LEVELS];
	int best_entry[XRM_MAX_LEVELS];
	for (int l = 0; l < k.count; l++)
		best_entry[l] = -1;
	for (int n = 0; n < db->count; n++) {
		const char *ekey = db->entries[n].key;
		if (!strpbrk(ekey, "*?") || !xrm_split_entry(ekey, &e))
			continue;
		for (int l = 0; l < k.count; l++)
			best[l] = -1;
		if (!xrm_match(&e, 0, &k, 0, score, best))
			continue;
		int l = 0;
		while (l < k.count && best[l] == best_entry[l])
			l++;
		if (l < k.count && best[l] > best_entry[l]) {
			memcpy(best_entry, best, sizeof(best));
			val = db->entries[n].value;
		}
	}
	return val;
}

/*
 * Look a style resource up, with the fallbacks to the top-level keys of
 * old styles that Fluxbox's themes have (their fallback() methods);
 * fallbacks between items are spelled out where the item is loaded.
 */
static const char *
style_get(struct rc_database *db, const char *key)
{
	static const struct {
		const char *key;
		const char *fallback[3];
	} fallbacks[] = {
		{ "window.borderWidth", { "borderWidth" } },
		{ "window.borderColor", { "borderColor" } },
		{ "window.bevelWidth", { "bevelWidth" } },
		{ "window.handleWidth", { "handleWidth" } },
		{ "menu.borderWidth", { "borderWidth" } },
		{ "menu.borderColor", { "borderColor" } },
		{ "menu.bevelWidth", { "bevelWidth" } },
		{ "slit.borderWidth", { "borderWidth" } },
		{ "slit.borderColor", { "borderColor" } },
		{ "toolbar.borderWidth", { "borderWidth" } },
		{ "toolbar.borderColor", { "borderColor" } },
		{ "toolbar.bevelWidth", { "bevelWidth" } },
	};
	const char *val = xrm_get(db, key);
	if (val)
		return val;
	for (size_t i = 0; i < sizeof(fallbacks) / sizeof(fallbacks[0]); i++) {
		if (strcmp(fallbacks[i].key, key) != 0)
			continue;
		for (int j = 0; j < 3 && fallbacks[i].fallback[j]; j++) {
			if ((val = xrm_get(db, fallbacks[i].fallback[j])))
				return val;
		}
	}
	return NULL;
}

/* The first of a NULL-terminated list of keys the style has */
static const char *
first_key(struct rc_database *db, const char *const *keys)
{
	for (int i = 0; keys[i]; i++) {
		if (style_get(db, keys[i]))
			return keys[i];
	}
	return NULL;
}

static int
style_get_int(struct rc_database *db, const char *key, int default_val)
{
	const char *val = style_get(db, key);
	if (!val)
		return default_val;
	char *end;
	errno = 0;
	long result = strtol(val, &end, 10);
	if (end == val || *end != '\0')
		return default_val;
	if (errno == ERANGE || result > INT_MAX || result < INT_MIN)
		return default_val;
	return (int)result;
}

/*
 * Resolve a relative pixmap filename to a full path.
 * Tries style_dir/filename first, then style_dir/pixmaps/filename
 * as a fallback (standard Fluxbox style convention).
 */
static char *
resolve_pixmap_file(const char *style_dir, const char *filename)
{
	if (!style_dir || !filename || *filename == '\0')
		return NULL;
	if (filename[0] == '/')
		return strdup(filename);

	/* Try style_dir/filename */
	size_t dir_len = strlen(style_dir);
	size_t name_len = strlen(filename);
	size_t len = dir_len + 1 + name_len + 1;
	char *path = malloc(len);
	if (!path)
		return NULL;
	snprintf(path, len, "%s/%s", style_dir, filename);
	if (access(path, R_OK) == 0)
		return path;

	/* Try style_dir/pixmaps/filename */
	size_t plen = dir_len + 9 + name_len + 1; /* "/pixmaps/" = 9 */
	char *ppath = malloc(plen);
	if (!ppath) {
		free(path);
		return NULL;
	}
	snprintf(ppath, plen, "%s/pixmaps/%s", style_dir, filename);
	if (access(ppath, R_OK) == 0) {
		free(path);
		return ppath;
	}

	/* Neither exists; return the first path and let loader handle error */
	free(ppath);
	return path;
}

static void
load_texture(struct rc_database *db, const char *base_key,
	struct wm_texture *tex, const char *style_dir)
{
	char key[256];
	snprintf(key, sizeof(key), "%s.color", base_key);
	const char *color_str = style_get(db, key);
	snprintf(key, sizeof(key), "%s.colorTo", base_key);
	const char *color_to_str = style_get(db, key);

	/* A texture the style names starts from Fluxbox's defaults (as
	 * does one it gives only colours: Flat Solid); one it doesn't
	 * name keeps fluxland's */
	const char *tex_str = style_get(db, base_key);
	if (tex_str || color_str || color_to_str) {
		char *old_pixmap = tex->pixmap_path;
		*tex = style_parse_texture(tex_str ? tex_str : "");
		tex->pixmap_path = old_pixmap;
	}
	if (color_str)
		tex->color = argb_from_color(style_parse_color(color_str));
	if (color_to_str)
		tex->color_to = argb_from_color(style_parse_color(color_to_str));

	/* Load pixmap path */
	snprintf(key, sizeof(key), "%s.pixmap", base_key);
	const char *pixmap_str = style_get(db, key);
	if (pixmap_str && *pixmap_str != '\0') {
		safe_free(&tex->pixmap_path);
		/* Drawn instead of the texture when it loads, whatever
		 * the texture says, as in Fluxbox */
		tex->pixmap_path = resolve_pixmap_file(style_dir, pixmap_str);
	}
}

static void
load_color(struct rc_database *db, const char *key, struct wm_color *color)
{
	struct wm_color c;
	if (style_try_parse_color(style_get(db, key), &c))
		*color = c;
}

/*
 * A font and its effect as Fluxbox's ThemeItem<Font> reads them: KEY,
 * KEY.effect (halo or shadow), KEY.halo.color, KEY.shadow.color,
 * KEY.shadow.x and KEY.shadow.y.  fluxland 1.x's ":halo" in the font and
 * its halo colour next to the font key are read too.
 */
static void
load_font(struct rc_database *db, const char *key, struct wm_font *font)
{
	const char *val = style_get(db, key);
	if (!val)
		return;
	free(font->family);
	*font = style_parse_font(val);

	char k[256];
	snprintf(k, sizeof(k), "%s.effect", key);
	const char *effect = style_get(db, k);
	if (effect && strcasecmp(effect, "shadow") == 0) {
		snprintf(k, sizeof(k), "%s.shadow.x", key);
		font->shadow_x = style_get_int(db, k, 0);
		snprintf(k, sizeof(k), "%s.shadow.y", key);
		font->shadow_y = style_get_int(db, k, 0);
		snprintf(k, sizeof(k), "%s.shadow.color", key);
		style_try_parse_color(style_get(db, k), &font->shadow_color);
	} else if (effect && strcasecmp(effect, "halo") == 0) {
		font->halo = true;
	} else {
		/* fluxland 1.x: shadow offsets without an effect */
		snprintf(k, sizeof(k), "%s.shadow.x", key);
		font->shadow_x = style_get_int(db, k, 0);
		snprintf(k, sizeof(k), "%s.shadow.y", key);
		font->shadow_y = style_get_int(db, k, 0);
		snprintf(k, sizeof(k), "%s.shadow.color", key);
		style_try_parse_color(style_get(db, k), &font->shadow_color);
	}
	if (font->halo) {
		snprintf(k, sizeof(k), "%s.halo.color", key);
		if (!style_try_parse_color(style_get(db, k), &font->halo_color)) {
			/* fluxland 1.x: window.label.focus.halo.color */
			size_t klen = strlen(key);
			if (klen > 5 && strcmp(key + klen - 5, ".font") == 0) {
				snprintf(k, sizeof(k), "%.*s.halo.color",
					(int)(klen - 5), key);
				style_try_parse_color(style_get(db, k),
					&font->halo_color);
			}
		}
	}
}

/*
 * A texture as Fluxbox's ThemeManager::loadItem reads one: from the
 * first of the names the style has, with that name's colours (darkgray
 * and white without them) and pixmap; with none of them, Flat Solid in
 * the first name's colours (ThemeItem<Texture>::setDefaultValue).
 */
static void
load_texture_chain(struct rc_database *db, const char *const *names,
	struct wm_texture *tex, const char *style_dir)
{
	const char *name = first_key(db, names);
	if (!name)
		name = names[0];
	texture_finish(tex);
	*tex = style_parse_texture(style_get(db, name));

	/* A colour that doesn't parse leaves darkgray or white */
	char key[256];
	struct wm_color c;
	snprintf(key, sizeof(key), "%s.color", name);
	if (style_try_parse_color(style_get(db, key), &c))
		tex->color = argb_from_color(c);
	snprintf(key, sizeof(key), "%s.colorTo", name);
	if (style_try_parse_color(style_get(db, key), &c))
		tex->color_to = argb_from_color(c);
	const char *v;
	snprintf(key, sizeof(key), "%s.pixmap", name);
	if ((v = style_get(db, key)) && *v)
		tex->pixmap_path = resolve_pixmap_file(style_dir, v);
}

/* The first of the keys the style has, or the default */
static struct wm_color
color_chain(struct rc_database *db, const char *const *keys,
	struct wm_color def)
{
	/* Fluxbox's default too for one that doesn't parse (white) */
	const char *key = first_key(db, keys);
	struct wm_color c;
	return key && style_try_parse_color(style_get(db, key), &c) ? c : def;
}

static int
int_chain(struct rc_database *db, const char *const *keys, int def)
{
	const char *key = first_key(db, keys);
	if (!key)
		return def;
	int v;
	return safe_atoi(style_get(db, key), &v) ? v : def;
}

static void
font_chain(struct rc_database *db, const char *const *keys,
	struct wm_font *font)
{
	const char *key = first_key(db, keys);
	if (key)
		load_font(db, key, font);
}

static enum wm_justify parse_justify(const char *value);

/* Text, justify and border of a tool, each from the first key found */
static void
load_tool_text(struct rc_database *db, struct wm_tool_style *t,
	const char *const *font, const char *const *color,
	const char *const *justify, const char *const *border_width,
	const char *const *border_color)
{
	font_chain(db, font, &t->font);
	t->text_color = color_chain(db, color, make_color(255, 255, 255));
	const char *key = first_key(db, justify);
	t->justify = key ? parse_justify(style_get(db, key)) : WM_JUSTIFY_LEFT;
	t->border_width = int_chain(db, border_width, 0);
	if (t->border_width < 0)
		t->border_width = 0;
	t->border_color = color_chain(db, border_color, make_color(0, 0, 0));
}

#define KEYS(...) ((const char *const []){ __VA_ARGS__, NULL })

/*
 * The window frame as Fluxbox's FbWinFrameTheme reads it, with the
 * window.label.* IconbarThemes for the labels and tabs and the
 * WinButtonTheme pictures.
 */
static void
load_window(struct wm_style *s, struct rc_database *db, const char *sd)
{
	load_texture_chain(db, KEYS("window.title.focus"),
		&s->window_title_focus, sd);
	load_texture_chain(db, KEYS("window.title.unfocus"),
		&s->window_title_unfocus, sd);
	s->window_title_height = int_chain(db, KEYS("window.title.height"), 0);
	if (s->window_title_height < 0)
		s->window_title_height = 0;
	font_chain(db, KEYS("window.font"), &s->window_font);

	/* Labels and tabs: textures fall back to toolbar.windowLabel,
	 * fonts to window.font, borders through window.* */
	load_texture_chain(db,
		KEYS("window.label.focus", "toolbar.windowLabel"),
		&s->window_label_focus, sd);
	load_texture_chain(db,
		KEYS("window.label.unfocus", "toolbar.windowLabel"),
		&s->window_label_unfocus, sd);
	/* Fluxbox 1.0's, for fluxland 1.x styles; 1.3 has none */
	load_texture_chain(db, KEYS("window.label.active",
			"window.label.focus", "toolbar.windowLabel"),
		&s->window_label_active, sd);
	struct wm_tool_style lf = { 0 }, lu = { 0 };
	lf.font = s->window_label_focus_font;
	lu.font = s->window_label_unfocus_font;
	if (style_get(db, "window.font")) {
		/* load_font replaces the family it is given */
		font_chain(db, KEYS("window.font"), &lf.font);
		font_chain(db, KEYS("window.font"), &lu.font);
	}
	load_tool_text(db, &lf,
		KEYS("window.label.focus.font"),
		KEYS("window.label.focus.textColor"),
		KEYS("window.label.focus.justify", "window.label.justify",
			"window.justify"),
		KEYS("window.label.focus.borderWidth",
			"window.label.borderWidth", "window.borderWidth",
			"borderWidth"),
		KEYS("window.label.focus.borderColor",
			"window.label.borderColor", "window.borderColor",
			"borderColor"));
	load_tool_text(db, &lu,
		KEYS("window.label.unfocus.font"),
		KEYS("window.label.unfocus.textColor"),
		KEYS("window.label.unfocus.justify", "window.label.justify",
			"window.justify"),
		KEYS("window.label.unfocus.borderWidth",
			"window.label.borderWidth", "window.borderWidth",
			"borderWidth"),
		KEYS("window.label.unfocus.borderColor",
			"window.label.borderColor", "window.borderColor",
			"borderColor"));
	s->window_label_focus_font = lf.font;
	s->window_label_unfocus_font = lu.font;
	s->window_label_focus_text_color = lf.text_color;
	s->window_label_unfocus_text_color = lu.text_color;
	s->window_label_active_text_color = color_chain(db,
		KEYS("window.label.active.textColor",
			"window.label.focus.textColor"), lf.text_color);
	s->window_justify = lf.justify;
	s->window_label_focus_border_width = lf.border_width;
	s->window_label_unfocus_border_width = lu.border_width;
	s->window_label_focus_border_color = lf.border_color;
	s->window_label_unfocus_border_color = lu.border_color;

	/* Buttons */
	load_texture_chain(db, KEYS("window.button.focus"),
		&s->window_button_focus, sd);
	load_texture_chain(db, KEYS("window.button.unfocus"),
		&s->window_button_unfocus, sd);
	load_texture_chain(db, KEYS("window.button.pressed"),
		&s->window_button_pressed, sd);
	const struct wm_color white = make_color(255, 255, 255);
	s->window_button_focus_pic_color = color_chain(db,
		KEYS("window.button.focus.picColor"), white);
	s->window_button_unfocus_pic_color = color_chain(db,
		KEYS("window.button.unfocus.picColor"), white);
	s->window_button_pressed_pic_color = color_chain(db,
		KEYS("window.button.pressed.picColor",
			"window.button.focus.picColor"), white);

	static const char *const pictures[WM_WINDOW_PIXMAPS] = {
		"close", "maximize", "iconify", "shade", "unshade",
		"stick", "stuck", "menuicon", "lhalf", "rhalf",
	};
	static const char *const states[WM_PIXMAP_STATES] = {
		"", ".unfocus", ".pressed",
	};
	for (int i = 0; i < WM_WINDOW_PIXMAPS; i++) {
		for (int j = 0; j < WM_PIXMAP_STATES; j++) {
			char key[64];
			snprintf(key, sizeof(key), "window.%s%s.pixmap",
				pictures[i], states[j]);
			safe_free(&s->window_pixmaps[i][j]);
			s->window_pixmaps[i][j] = load_pixmap_path(db, key, sd);
		}
	}

	/* Handle and grips */
	load_texture_chain(db, KEYS("window.handle.focus"),
		&s->window_handle_focus, sd);
	load_texture_chain(db, KEYS("window.handle.unfocus"),
		&s->window_handle_unfocus, sd);
	load_texture_chain(db, KEYS("window.grip.focus"),
		&s->window_grip_focus, sd);
	load_texture_chain(db, KEYS("window.grip.unfocus"),
		&s->window_grip_unfocus, sd);
	s->window_handle_width = int_chain(db, KEYS("window.handleWidth"), 0);
	if (s->window_handle_width < 0)
		s->window_handle_width = 1;
	if (s->window_handle_width > 200)
		s->window_handle_width = 200;

	/* fluxland 1.x's frame colours */
	load_color(db, "window.frame.focusColor",
		&s->window_frame_focus_color);
	load_color(db, "window.frame.unfocusColor",
		&s->window_frame_unfocus_color);

	/* Borders and bevel */
	s->window_border_width = int_chain(db,
		KEYS("window.focus.borderWidth", "window.borderWidth"), 0);
	if (s->window_border_width < 0)
		s->window_border_width = 0;
	s->window_border_color = color_chain(db,
		KEYS("window.focus.borderColor", "window.borderColor"),
		make_color(0, 0, 0));
	s->window_border_unfocus_color = color_chain(db,
		KEYS("window.unfocus.borderColor", "window.borderColor"),
		make_color(0, 0, 0));
	s->window_bevel_width = int_chain(db, KEYS("window.bevelWidth"), 0);
	if (s->window_bevel_width < 0)
		s->window_bevel_width = 0;
	if (s->window_bevel_width > 20)
		s->window_bevel_width = 20;
	const char *rc = style_get(db, "window.roundCorners");
	if (rc)
		s->window_round_corners = parse_round_corners(rc);
}

/*
 * Menus as Fluxbox's FbTk::MenuTheme reads them: borders and bevel to
 * the top-level keys (as style_get does), the hilite font and justify
 * to the frame's, no bullet unless menu.bullet says, on the left
 * unless menu.bullet.position does.
 */
static void
load_menu(struct wm_style *s, struct rc_database *db, const char *sd)
{
	const struct wm_color white = make_color(255, 255, 255);
	load_texture_chain(db, KEYS("menu.title"), &s->menu_title, sd);
	load_texture_chain(db, KEYS("menu.frame"), &s->menu_frame, sd);
	load_texture_chain(db, KEYS("menu.hilite"), &s->menu_hilite, sd);
	s->menu_title_text_color = color_chain(db,
		KEYS("menu.title.textColor"), white);
	s->menu_frame_text_color = color_chain(db,
		KEYS("menu.frame.textColor"), white);
	s->menu_hilite_text_color = color_chain(db,
		KEYS("menu.hilite.textColor"), white);
	s->menu_disable_text_color = color_chain(db,
		KEYS("menu.frame.disableColor"), white);
	font_chain(db, KEYS("menu.title.font"), &s->menu_title_font);
	font_chain(db, KEYS("menu.frame.font"), &s->menu_frame_font);
	font_finish(&s->menu_hilite_font);
	s->menu_hilite_font = s->menu_frame_font;
	s->menu_hilite_font.family = s->menu_frame_font.family ?
		strdup(s->menu_frame_font.family) : NULL;
	font_chain(db, KEYS("menu.hilite.font"), &s->menu_hilite_font);
	const char *k = first_key(db, KEYS("menu.title.justify"));
	s->menu_title_justify = k ? parse_justify(style_get(db, k)) :
		WM_JUSTIFY_LEFT;
	k = first_key(db, KEYS("menu.frame.justify"));
	s->menu_frame_justify = k ? parse_justify(style_get(db, k)) :
		WM_JUSTIFY_LEFT;
	k = first_key(db, KEYS("menu.hilite.justify", "menu.frame.justify"));
	s->menu_hilite_justify = k ? parse_justify(style_get(db, k)) :
		WM_JUSTIFY_LEFT;

	s->menu_border_width = int_chain(db, KEYS("menu.borderWidth"), 0);
	if (s->menu_border_width < 0)
		s->menu_border_width = 0;
	if (s->menu_border_width > 20)
		s->menu_border_width = 20;
	s->menu_border_color = color_chain(db, KEYS("menu.borderColor"),
		make_color(0, 0, 0));
	s->menu_bevel_width = int_chain(db, KEYS("menu.bevelWidth"), 0);
	if (s->menu_bevel_width < 0)
		s->menu_bevel_width = 0;
	if (s->menu_bevel_width > 20)
		s->menu_bevel_width = 20;
	s->menu_item_height = int_chain(db, KEYS("menu.itemHeight"), 0);
	s->menu_title_height = int_chain(db, KEYS("menu.titleHeight"), 0);

	/* ThemeItem<BulletType>: a word found anywhere, else none */
	const char *bullet = style_get(db, "menu.bullet");
	free(s->menu_bullet);
	s->menu_bullet = strdup(!bullet ? "empty" :
		strcasestr(bullet, "empty") ? "empty" :
		strcasestr(bullet, "square") ? "square" :
		strcasestr(bullet, "triangle") ? "triangle" :
		strcasestr(bullet, "diamond") ? "diamond" : "empty");
	k = first_key(db, KEYS("menu.bullet.position"));
	s->menu_bullet_position = k && parse_justify(style_get(db, k)) ==
		WM_JUSTIFY_RIGHT ? WM_JUSTIFY_RIGHT : WM_JUSTIFY_LEFT;

	char **pixmaps[] = { &s->menu_submenu_pixmap,
		&s->menu_hilite_submenu_pixmap, &s->menu_selected_pixmap,
		&s->menu_hilite_selected_pixmap, &s->menu_unselected_pixmap,
		&s->menu_hilite_unselected_pixmap };
	static const char *const keys[] = { "menu.submenu.pixmap",
		"menu.hilite.submenu.pixmap", "menu.selected.pixmap",
		"menu.hilite.selected.pixmap", "menu.unselected.pixmap",
		"menu.hilite.unselected.pixmap" };
	for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
		safe_free(pixmaps[i]);
		*pixmaps[i] = load_pixmap_path(db, keys[i], sd);
	}
}

/*
 * The toolbar and its tools, with the fallbacks of Fluxbox's
 * ToolbarTheme, ToolTheme, WorkspaceNameTheme, ButtonTheme and
 * IconbarTheme.  fluxland 1.x's toolbar.font and toolbar.textColor
 * come last, before Fluxbox's defaults.
 */
static void
load_toolbar(struct wm_style *s, struct rc_database *db, const char *sd)
{
	load_texture_chain(db, KEYS("toolbar"), &s->toolbar_texture, sd);
	s->toolbar_bevel_width = int_chain(db, KEYS("toolbar.bevelWidth"), 0);
	if (s->toolbar_bevel_width < 0)
		s->toolbar_bevel_width = 0;
	if (s->toolbar_bevel_width > 20)
		s->toolbar_bevel_width = 20;
	s->toolbar_border_width =
		int_chain(db, KEYS("toolbar.borderWidth"), 0);
	if (s->toolbar_border_width < 0)
		s->toolbar_border_width = 0;
	s->toolbar_border_color = color_chain(db, KEYS("toolbar.borderColor"),
		make_color(0, 0, 0));
	s->toolbar_height = int_chain(db, KEYS("toolbar.height"), 0);
	if (s->toolbar_height < 0 || s->toolbar_height > 100)
		s->toolbar_height = 0;
	load_font(db, "toolbar.font", &s->toolbar_font);
	s->toolbar_text_color = color_chain(db, KEYS("toolbar.textColor"),
		make_color(255, 255, 255));

	load_texture_chain(db, KEYS("toolbar.clock"),
		&s->toolbar_clock.texture, sd);
	load_tool_text(db, &s->toolbar_clock,
		KEYS("toolbar.clock.font", "toolbar.font"),
		KEYS("toolbar.clock.textColor", "toolbar.textColor"),
		KEYS("toolbar.clock.justify", "toolbar.justify"),
		KEYS("toolbar.clock.borderWidth"),
		KEYS("toolbar.clock.borderColor"));

	load_texture_chain(db, KEYS("toolbar.workspace", "toolbar.label"),
		&s->toolbar_workspace.texture, sd);
	load_tool_text(db, &s->toolbar_workspace,
		KEYS("toolbar.workspace.font", "toolbar.font"),
		KEYS("toolbar.workspace.textColor", "toolbar.label.textColor",
			"toolbar.textColor"),
		KEYS("toolbar.workspace.justify", "toolbar.justify"),
		KEYS("toolbar.workspace.borderWidth"),
		KEYS("toolbar.workspace.borderColor"));

	load_texture_chain(db, KEYS("toolbar.button", "toolbar.clock"),
		&s->toolbar_button.texture, sd);
	load_tool_text(db, &s->toolbar_button,
		KEYS("toolbar.button.font", "toolbar.font"),
		KEYS("toolbar.button.textColor", "toolbar.textColor"),
		KEYS("toolbar.button.justify", "toolbar.justify"),
		KEYS("toolbar.button.borderWidth"),
		KEYS("toolbar.button.borderColor"));
	s->toolbar_button_pic_color = color_chain(db,
		KEYS("toolbar.button.picColor", "toolbar.clock.picColor",
			"toolbar.clock.textColor", "toolbar.textColor"),
		make_color(255, 255, 255));
	/* ArrowButton: 0 (unset) is 250 */
	s->toolbar_button_scale = int_chain(db, KEYS("toolbar.button.scale"), 0);
	if (s->toolbar_button_scale <= 0)
		s->toolbar_button_scale = 250;
	if (style_get(db, "toolbar.button.pressed")) {
		load_texture_chain(db, KEYS("toolbar.button.pressed"),
			&s->toolbar_button_pressed, sd);
	} else {
		/* The button's texture with its bevel the other way */
		texture_finish(&s->toolbar_button_pressed);
		s->toolbar_button_pressed = s->toolbar_button.texture;
		s->toolbar_button_pressed.pixmap_path =
			s->toolbar_button.texture.pixmap_path ?
			strdup(s->toolbar_button.texture.pixmap_path) : NULL;
		if (s->toolbar_button_pressed.appearance == WM_TEX_RAISED)
			s->toolbar_button_pressed.appearance = WM_TEX_SUNKEN;
		else if (s->toolbar_button_pressed.appearance == WM_TEX_SUNKEN)
			s->toolbar_button_pressed.appearance = WM_TEX_RAISED;
	}

	load_texture_chain(db, KEYS("toolbar.iconbar.empty",
			"toolbar.iconbar", "toolbar.windowLabel", "toolbar"),
		&s->toolbar_iconbar_empty, sd);
	s->toolbar_iconbar_border_width = int_chain(db,
		KEYS("toolbar.iconbar.borderWidth", "toolbar.borderWidth",
			"window.borderWidth", "borderWidth"), 0);
	s->toolbar_iconbar_border_color = color_chain(db,
		KEYS("toolbar.iconbar.borderColor", "toolbar.borderColor",
			"window.borderColor", "borderColor"),
		make_color(0, 0, 0));

	load_texture_chain(db,
		KEYS("toolbar.iconbar.focused", "toolbar.windowLabel"),
		&s->toolbar_iconbar_focused.texture, sd);
	load_tool_text(db, &s->toolbar_iconbar_focused,
		KEYS("toolbar.iconbar.focused.font", "window.font",
			"toolbar.font"),
		KEYS("toolbar.iconbar.focused.textColor", "toolbar.textColor"),
		KEYS("toolbar.iconbar.focused.justify",
			"toolbar.iconbar.justify", "window.justify"),
		KEYS("toolbar.iconbar.focused.borderWidth",
			"toolbar.iconbar.borderWidth", "window.borderWidth",
			"borderWidth"),
		KEYS("toolbar.iconbar.focused.borderColor",
			"toolbar.iconbar.borderColor", "window.borderColor",
			"borderColor"));

	load_texture_chain(db,
		KEYS("toolbar.iconbar.unfocused", "toolbar.windowLabel"),
		&s->toolbar_iconbar_unfocused.texture, sd);
	load_tool_text(db, &s->toolbar_iconbar_unfocused,
		KEYS("toolbar.iconbar.unfocused.font", "window.font",
			"toolbar.font"),
		KEYS("toolbar.iconbar.unfocused.textColor",
			"toolbar.textColor"),
		KEYS("toolbar.iconbar.unfocused.justify",
			"toolbar.iconbar.justify", "window.justify"),
		KEYS("toolbar.iconbar.unfocused.borderWidth",
			"toolbar.iconbar.borderWidth", "window.borderWidth",
			"borderWidth"),
		KEYS("toolbar.iconbar.unfocused.borderColor",
			"toolbar.iconbar.borderColor", "window.borderColor",
			"borderColor"));
}

static char *
load_pixmap_path(struct rc_database *db, const char *key,
	const char *style_dir)
{
	const char *val = style_get(db, key);
	if (!val || *val == '\0')
		return NULL;
	return resolve_pixmap_file(style_dir, val);
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
		.halo = false,
		.halo_color = make_color(0, 0, 0),
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
	/* The toolbar's tools are set by load_toolbar; these are for a
	 * compositor running without a style */
	s->toolbar_clock.texture.fill = WM_TEX_PARENT_RELATIVE;
	s->toolbar_workspace.texture.fill = WM_TEX_PARENT_RELATIVE;
	s->toolbar_button.texture.fill = WM_TEX_PARENT_RELATIVE;
	s->toolbar_button_pressed = default_texture(DEFAULT_UNFOCUS_COLOR,
		DEFAULT_FOCUS_COLOR);
	s->toolbar_iconbar_empty.fill = WM_TEX_PARENT_RELATIVE;
	s->toolbar_iconbar_unfocused.texture.fill = WM_TEX_PARENT_RELATIVE;
	s->toolbar_iconbar_focused.texture = default_texture(0xFF4C566A,
		0xFF4C566A);
	s->toolbar_iconbar_focused.texture.appearance = WM_TEX_FLAT;
	s->toolbar_iconbar_focused.texture.bevel = WM_BEVEL_NONE;

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
	s->window_border_unfocus_color = color_from_argb(DEFAULT_BORDER_COLOR);
	s->menu_title_text_color = color_from_argb(DEFAULT_TEXT_COLOR);
	s->menu_frame_text_color = color_from_argb(DEFAULT_TEXT_COLOR);
	s->menu_hilite_text_color = color_from_argb(DEFAULT_TEXT_COLOR);
	s->menu_border_color = color_from_argb(DEFAULT_BORDER_COLOR);
	s->toolbar_text_color = color_from_argb(DEFAULT_TEXT_COLOR);
	s->toolbar_button_pic_color = color_from_argb(DEFAULT_TEXT_COLOR);
	s->toolbar_button_scale = 250;
	struct wm_tool_style *tools[] = { &s->toolbar_clock,
		&s->toolbar_workspace, &s->toolbar_button,
		&s->toolbar_iconbar_focused, &s->toolbar_iconbar_unfocused };
	for (size_t i = 0; i < sizeof(tools) / sizeof(tools[0]); i++) {
		tools[i]->font = default_font();
		tools[i]->text_color = color_from_argb(DEFAULT_TEXT_COLOR);
		tools[i]->justify = WM_JUSTIFY_CENTER;
	}

	/* Default dimensions */
	s->window_border_width = DEFAULT_BORDER_WIDTH;
	s->window_bevel_width = DEFAULT_BEVEL_WIDTH;
	s->window_handle_width = DEFAULT_HANDLE_WIDTH;
	s->window_title_height = DEFAULT_TITLE_HEIGHT;
	s->menu_border_width = DEFAULT_BORDER_WIDTH;

	/* Focus border defaults (disabled by default, enabled in HC styles) */
	s->window_focus_border_width = 0;
	s->window_focus_border_color = make_color(0x88, 0xC0, 0xD0);

	/* Menu bullet defaults */
	s->menu_bullet = strdup("triangle");
	s->menu_bullet_position = WM_JUSTIFY_RIGHT;
	s->menu_item_height = 0;
	s->menu_title_height = 0;

	/* Slit defaults (fall back to toolbar texture) */
	s->slit_texture = default_texture(DEFAULT_FOCUS_COLOR,
		DEFAULT_UNFOCUS_COLOR);
	s->slit_border_color = color_from_argb(DEFAULT_BORDER_COLOR);
	s->slit_border_width = DEFAULT_BORDER_WIDTH;

	/* Default fonts */
	s->window_label_focus_font = default_font();
	s->window_label_unfocus_font = default_font();
	s->menu_title_font = default_font();
	s->menu_frame_font = default_font();
	s->menu_hilite_font = default_font();
	s->toolbar_font = default_font();
	s->window_font = default_font();

	/* Default justify */
	s->window_justify = WM_JUSTIFY_LEFT;
	s->menu_title_justify = WM_JUSTIFY_CENTER;

	return s;
}

static void
style_apply_db(struct wm_style *s, struct rc_database *db)
{
	const char *sd = s->style_dir;

	/* --- Window frame, as FbWinFrameTheme and its label themes --- */
	load_window(s, db, sd);

	/* --- Focus border (accessibility) --- */
	s->window_focus_border_width = style_get_int(db,
		"window.focus.border.width", s->window_focus_border_width);
	load_color(db, "window.focus.border.color",
		&s->window_focus_border_color);

	/* --- Menu --- */
	load_menu(s, db, sd);

	/* --- Slit --- */
	load_texture(db, "slit", &s->slit_texture, sd);
	load_color(db, "slit.borderColor", &s->slit_border_color);
	s->slit_border_width = style_get_int(db, "slit.borderWidth",
		s->slit_border_width);

	/* --- Toolbar --- */
	load_toolbar(s, db, sd);
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
	FILE *f = fopen_config(try_path, "r");
	if (f) {
		fclose(f);
		return try_path;
	}

	snprintf(try_path, len + 12, "%s/style.cfg", path);
	f = fopen_config(try_path, "r");
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

	struct rc_database *db = rc_create();
	if (!db) {
		free(actual_path);
		return -1;
	}
	if (rc_load(db, actual_path) != 0) {
		rc_destroy(db);
		free(actual_path);
		return -1;
	}
	/* The overlay's resources replace the style's, as Fluxbox reads
	 * both into one database */
	if (s->overlay_path)
		rc_load(db, s->overlay_path);

	struct wm_style *fresh = style_create();
	if (!fresh) {
		rc_destroy(db);
		free(actual_path);
		return -1;
	}
	/* The directory of the style file, or the style directory itself
	 * for a directory style; NULL is non-fatal (no pixmaps) */
	fresh->style_dir = is_dir_style ? strdup(path) :
		resolve_style_dir(actual_path);
	fresh->style_path = actual_path;
	fresh->overlay_path = s->overlay_path;
	s->overlay_path = NULL;
	style_apply_db(fresh, db);
	rc_destroy(db);

	/* Swap the contents in: callers keep their pointer */
	style_clear(s);
	memcpy(s, fresh, sizeof(*s));
	free(fresh);
	return 0;
}

void
style_set_overlay(struct wm_style *s, const char *path)
{
	if (!s)
		return;
	safe_free(&s->overlay_path);
	if (path && *path)
		s->overlay_path = strdup(path);
}

int
style_reload(struct wm_style *s)
{
	if (!s || !s->style_path)
		return -1;
	char *path = strdup(s->style_path);
	if (!path)
		return -1;
	int ret = style_load(s, path);
	free(path);
	return ret;
}

/*
 * Free all internal resources of a style without freeing the struct itself.
 * Used by style_reload() to reinitialize an existing style in-place.
 */
static void
style_clear(struct wm_style *s)
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
	texture_finish(&s->slit_texture);
	texture_finish(&s->toolbar_texture);
	texture_finish(&s->toolbar_button_pressed);
	texture_finish(&s->toolbar_iconbar_empty);
	struct wm_tool_style *tools[] = { &s->toolbar_clock,
		&s->toolbar_workspace, &s->toolbar_button,
		&s->toolbar_iconbar_focused, &s->toolbar_iconbar_unfocused };
	for (size_t i = 0; i < sizeof(tools) / sizeof(tools[0]); i++) {
		texture_finish(&tools[i]->texture);
		font_finish(&tools[i]->font);
	}

	/* Fonts */
	font_finish(&s->window_label_focus_font);
	font_finish(&s->window_label_unfocus_font);
	font_finish(&s->menu_title_font);
	font_finish(&s->menu_frame_font);
	font_finish(&s->menu_hilite_font);
	free(s->menu_submenu_pixmap);
	free(s->menu_hilite_submenu_pixmap);
	free(s->menu_selected_pixmap);
	free(s->menu_hilite_selected_pixmap);
	free(s->menu_unselected_pixmap);
	free(s->menu_hilite_unselected_pixmap);
	font_finish(&s->toolbar_font);
	font_finish(&s->window_font);

	/* Menu bullet */
	free(s->menu_bullet);

	/* Button pixmaps */
	for (int i = 0; i < WM_WINDOW_PIXMAPS; i++)
		for (int j = 0; j < WM_PIXMAP_STATES; j++)
			free(s->window_pixmaps[i][j]);

	/* Metadata */
	free(s->style_dir);
	free(s->style_path);
	free(s->overlay_path);
}

void
style_destroy(struct wm_style *s)
{
	if (!s)
		return;
	style_clear(s);
	free(s);
}
