/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 *
 * style.h - Fluxbox style/theme file parser and data structures
 *
 * Parses Fluxbox-compatible style files that use X resource database syntax
 * to define textures, colors, fonts, and layout for window decorations,
 * toolbar, menus, and slit.
 */

#ifndef WM_STYLE_H
#define WM_STYLE_H

#include <stdbool.h>
#include <stdint.h>

/* --- Texture specification --- */

enum wm_texture_appearance {
	WM_TEX_FLAT,
	WM_TEX_RAISED,
	WM_TEX_SUNKEN,
};

enum wm_texture_fill {
	WM_TEX_SOLID,
	WM_TEX_GRADIENT,
	WM_TEX_PIXMAP,
	WM_TEX_PARENT_RELATIVE,
};

enum wm_gradient_type {
	WM_GRAD_HORIZONTAL,
	WM_GRAD_VERTICAL,
	WM_GRAD_DIAGONAL,
	WM_GRAD_CROSSDIAGONAL,
	WM_GRAD_PIPECROSS,
	WM_GRAD_ELLIPTIC,
	WM_GRAD_RECTANGLE,
	WM_GRAD_PYRAMID,
};

enum wm_bevel_type {
	WM_BEVEL_NONE,
	WM_BEVEL1,
	WM_BEVEL2,
};

struct wm_texture {
	enum wm_texture_appearance appearance;
	enum wm_texture_fill fill;
	enum wm_gradient_type gradient;
	enum wm_bevel_type bevel;
	bool interlaced;
	uint32_t color;		/* ARGB format */
	uint32_t color_to;	/* gradient end color, ARGB */
	char *pixmap_path;	/* for pixmap textures */
};

/* --- Color --- */

struct wm_color {
	uint8_t r, g, b, a;
};

/* --- Font --- */

struct wm_font {
	char *family;
	int size;
	bool bold;
	bool italic;
	int shadow_x;
	int shadow_y;
	struct wm_color shadow_color;
	bool halo;
	struct wm_color halo_color;
};

/* --- Text justification --- */

enum wm_justify {
	WM_JUSTIFY_LEFT,
	WM_JUSTIFY_CENTER,
	WM_JUSTIFY_RIGHT,
};

/* --- Round corners bitmask --- */

#define WM_CORNER_TOP_LEFT     (1 << 0)
#define WM_CORNER_TOP_RIGHT    (1 << 1)
#define WM_CORNER_BOTTOM_LEFT  (1 << 2)
#define WM_CORNER_BOTTOM_RIGHT (1 << 3)

/* --- Full style structure --- */

struct wm_style {
	/* Window title bar */
	struct wm_texture window_title_focus;
	struct wm_texture window_title_unfocus;
	int window_title_height;	/* 0 = auto from font */

	/* Window label (title text area) */
	struct wm_texture window_label_focus;
	struct wm_texture window_label_unfocus;
	struct wm_texture window_label_active;	/* active tab */
	struct wm_color window_label_focus_text_color;
	struct wm_color window_label_unfocus_text_color;
	struct wm_color window_label_active_text_color;
	struct wm_font window_label_focus_font;
	struct wm_font window_label_unfocus_font;
	enum wm_justify window_justify;

	/* Window buttons */
	struct wm_texture window_button_focus;
	struct wm_texture window_button_unfocus;
	struct wm_texture window_button_pressed;
	struct wm_color window_button_focus_pic_color;
	struct wm_color window_button_unfocus_pic_color;
	struct wm_color window_button_pressed_pic_color;

	/* Per-button pixmaps: close */
	char *window_close_pixmap;
	char *window_close_unfocus_pixmap;
	char *window_close_pressed_pixmap;
	/* maximize */
	char *window_maximize_pixmap;
	char *window_maximize_unfocus_pixmap;
	char *window_maximize_pressed_pixmap;
	/* iconify */
	char *window_iconify_pixmap;
	char *window_iconify_unfocus_pixmap;
	char *window_iconify_pressed_pixmap;
	/* shade */
	char *window_shade_pixmap;
	char *window_shade_unfocus_pixmap;
	char *window_shade_pressed_pixmap;
	/* stick */
	char *window_stick_pixmap;
	char *window_stick_unfocus_pixmap;
	char *window_stick_pressed_pixmap;

	/* Window handle (bottom resize bar) and grips */
	struct wm_texture window_handle_focus;
	struct wm_texture window_handle_unfocus;
	int window_handle_width;
	struct wm_texture window_grip_focus;
	struct wm_texture window_grip_unfocus;

	/* Window frame */
	struct wm_color window_frame_focus_color;
	struct wm_color window_frame_unfocus_color;

	/* Window borders */
	struct wm_color window_border_color;
	int window_border_width;
	int window_bevel_width;
	uint8_t window_round_corners;	/* bitmask of WM_CORNER_* */

	/* Focus border (accessibility highlight around focused windows) */
	int window_focus_border_width;
	struct wm_color window_focus_border_color;

	/* Menu title */
	struct wm_texture menu_title;
	struct wm_color menu_title_text_color;
	struct wm_font menu_title_font;
	enum wm_justify menu_title_justify;

	/* Menu frame (items area) */
	struct wm_texture menu_frame;
	struct wm_color menu_frame_text_color;
	struct wm_font menu_frame_font;

	/* Menu selection highlight */
	struct wm_texture menu_hilite;
	struct wm_color menu_hilite_text_color;

	/* Menu borders */
	struct wm_color menu_border_color;
	int menu_border_width;

	/* Menu bullet (submenu indicator) */
	char *menu_bullet;		/* "triangle", "square", "diamond", "empty" */
	enum wm_justify menu_bullet_position;	/* LEFT or RIGHT (default) */

	/* Menu item/title heights (0 = auto from font) */
	int menu_item_height;
	int menu_title_height;

	/* Slit */
	struct wm_texture slit_texture;
	struct wm_color slit_border_color;
	int slit_border_width;

	/* Toolbar */
	struct wm_texture toolbar_texture;
	struct wm_color toolbar_text_color;
	struct wm_font toolbar_font;

	/* Toolbar icon bar (window list) */
	struct wm_color toolbar_iconbar_focused_color;
	struct wm_color toolbar_iconbar_focused_text_color;
	struct wm_color toolbar_iconbar_unfocused_color;
	struct wm_color toolbar_iconbar_unfocused_text_color;
	bool toolbar_iconbar_has_focused_color;
	bool toolbar_iconbar_has_unfocused_color;

	/* Style metadata */
	char *style_dir;	/* directory containing style file */
	char *style_path;	/* full path to loaded style file */
};

/* --- API --- */

/* Create a style with sensible defaults (Fluxbox-like dark theme) */
struct wm_style *style_create(void);

/* Load style from a file, returns 0 on success, -1 on error */
int style_load(struct wm_style *style, const char *path);

/* Apply an overlay file on top of already-loaded style */
int style_load_overlay(struct wm_style *style, const char *path);

/* Reload the style from the previously loaded path */
int style_reload(struct wm_style *style);

/* Destroy a style and free all memory */
void style_destroy(struct wm_style *style);

/* --- Parsing helpers (exposed for render.c) --- */

/* Parse a texture description string like "Raised Gradient Vertical" */
struct wm_texture style_parse_texture(const char *value);

/* Parse a color string like "#RRGGBB" or "rgb:R/G/B" */
struct wm_color style_parse_color(const char *value);

/* Parse a font string like "sans-10:bold" */
struct wm_font style_parse_font(const char *value);

/* Convert wm_color to ARGB uint32_t */
static inline uint32_t wm_color_to_argb(struct wm_color c)
{
	return ((uint32_t)c.a << 24) | ((uint32_t)c.r << 16) |
		((uint32_t)c.g << 8) | (uint32_t)c.b;
}

/* Convert ARGB uint32_t to wm_color */
static inline struct wm_color wm_argb_to_color(uint32_t argb)
{
	return (struct wm_color){
		.r = (argb >> 16) & 0xFF,
		.g = (argb >> 8) & 0xFF,
		.b = argb & 0xFF,
		.a = (argb >> 24) & 0xFF,
	};
}

#endif /* WM_STYLE_H */
