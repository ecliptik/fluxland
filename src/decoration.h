/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * decoration.h - Server-side window decoration rendering and management
 *
 * Renders Fluxbox-style window decorations (titlebar with buttons, borders,
 * handle, grips) using the theme engine and integrates them into the scene
 * graph as scene nodes.
 */

#ifndef WM_DECORATION_H
#define WM_DECORATION_H

#include <stdbool.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/util/box.h>

#include "render.h"
#include "style.h"

struct wm_server;
struct wm_view;

/* --- Decoration presets (from Fluxbox apps file [Deco]) --- */

enum wm_decor_preset {
	WM_DECOR_NORMAL,	/* full: title, handle, grips, border, tabs */
	WM_DECOR_NONE,		/* no decorations */
	WM_DECOR_BORDER,	/* border only */
	WM_DECOR_TAB,		/* titlebar and tabs only */
	WM_DECOR_TINY,		/* titlebar only, no resize handles */
	WM_DECOR_TOOL,		/* similar to TINY (tool window style) */
};

/* --- Decoration click region --- */

enum wm_decor_region {
	WM_DECOR_REGION_NONE,
	WM_DECOR_REGION_TITLEBAR,
	WM_DECOR_REGION_LABEL,
	WM_DECOR_REGION_BUTTON,
	WM_DECOR_REGION_HANDLE,
	WM_DECOR_REGION_GRIP_LEFT,
	WM_DECOR_REGION_GRIP_RIGHT,
	WM_DECOR_REGION_BORDER_TOP,
	WM_DECOR_REGION_BORDER_BOTTOM,
	WM_DECOR_REGION_BORDER_LEFT,
	WM_DECOR_REGION_BORDER_RIGHT,
	WM_DECOR_REGION_CLIENT,
};

/* --- External tab bar placement --- */

enum wm_tab_bar_placement {
	WM_TAB_BAR_TOP,
	WM_TAB_BAR_BOTTOM,
	WM_TAB_BAR_LEFT,
	WM_TAB_BAR_RIGHT,
};

/* --- Titlebar button --- */

struct wm_decor_button {
	enum wm_button_type type;
	struct wlr_scene_buffer *node;
	struct wlr_box box;	/* hit-test area, relative to decoration tree */
	bool pressed;
};

/* --- Window decoration --- */

struct wm_decoration {
	struct wm_view *view;
	struct wm_server *server;

	/* Scene tree structure (children of view->scene_tree) */
	struct wlr_scene_tree *tree;		/* decoration root */
	struct wlr_scene_tree *titlebar_tree;	/* titlebar container */
	struct wlr_scene_buffer *title_bg;	/* titlebar background texture */
	struct wlr_scene_buffer *label_buf;	/* label (text area) background + text */
	struct wlr_scene_buffer *handle_buf;	/* bottom handle */
	struct wlr_scene_buffer *grip_left;	/* left resize grip */
	struct wlr_scene_buffer *grip_right;	/* right resize grip */

	/* Border rects (used when no round corners) */
	struct wlr_scene_rect *border_top;
	struct wlr_scene_rect *border_bottom;
	struct wlr_scene_rect *border_left;
	struct wlr_scene_rect *border_right;

	/* Rounded border frame buffer (used when round corners active) */
	struct wlr_scene_buffer *border_frame;

	/* Focus border overlay (accessibility highlight for focused windows) */
	struct wlr_scene_buffer *focus_border_buf;

	/* Titlebar buttons */
	struct wm_decor_button *buttons_left;
	int buttons_left_count;
	struct wm_decor_button *buttons_right;
	int buttons_right_count;

	/* Decoration state */
	bool focused;
	bool shaded;
	enum wm_decor_preset preset;

	/* Saved geometry for shade/unshade */
	struct wlr_box shaded_saved_geometry;

	/* Computed geometry */
	int titlebar_height;
	int handle_height;
	int border_width;
	int grip_width;
	int content_width;	/* client surface width */
	int content_height;	/* client surface height */
	struct wlr_box content_area; /* where the client surface goes, relative to scene_tree */

	/* External tab bar (when tabs are outside the titlebar) */
	struct wlr_scene_buffer *tab_bar_buf;
	int tab_bar_size;
	enum wm_tab_bar_placement tab_bar_placement;
	struct wlr_box tab_bar_box; /* hit-test area, relative to decoration tree */
};

/* --- xdg-decoration protocol state --- */

struct wm_xdg_decoration {
	struct wlr_xdg_toplevel_decoration_v1 *wlr_decoration;
	struct wm_server *server;
	struct wl_listener destroy;
	struct wl_listener request_mode;
};

/* --- API --- */

/*
 * Create decorations for a view using the current style.
 * Returns NULL on error.
 */
struct wm_decoration *wm_decoration_create(struct wm_view *view,
	struct wm_style *style);

/* Remove and free decorations */
void wm_decoration_destroy(struct wm_decoration *decoration);

/*
 * Full re-render of decorations (on theme change, focus change, title change).
 * Uses the given style for rendering.
 */
void wm_decoration_update(struct wm_decoration *decoration,
	struct wm_style *style);

/* Switch focused/unfocused textures */
void wm_decoration_set_focused(struct wm_decoration *decoration,
	bool focused, struct wm_style *style);

/*
 * Parse button layout strings from config.
 * left_str: e.g. "Stick"
 * right_str: e.g. "Shade Minimize Maximize Close"
 */
void wm_decoration_configure_buttons(struct wm_decoration *decoration,
	const char *left_str, const char *right_str);

/*
 * Hit-test for button clicks. Returns the button at (x, y) relative to
 * the decoration tree, or NULL if no button is hit.
 */
struct wm_decor_button *wm_decoration_button_at(
	struct wm_decoration *decoration, double x, double y);

/*
 * Determine what region was clicked at (x, y) relative to the
 * decoration tree.
 */
enum wm_decor_region wm_decoration_region_at(
	struct wm_decoration *decoration, double x, double y);

/*
 * Determine which tab index (0-based) is at (x, y) relative to the
 * decoration tree. Returns -1 if not over a tab label area.
 */
int wm_decoration_tab_at(struct wm_decoration *decoration,
	double x, double y);

/* Toggle shade state (hide/show client surface) */
void wm_decoration_set_shaded(struct wm_decoration *decoration,
	bool shaded, struct wm_style *style);

/* Apply decoration preset */
void wm_decoration_set_preset(struct wm_decoration *decoration,
	enum wm_decor_preset preset, struct wm_style *style);

/*
 * Get the total extents of the decoration (how much larger the decorated
 * window is than the client surface).
 */
void wm_decoration_get_extents(struct wm_decoration *decoration,
	int *top, int *bottom, int *left, int *right);

/*
 * Refresh decoration geometry after the client commits a new size.
 * Updates content_width/content_height and re-renders all elements.
 */
void wm_decoration_refresh_geometry(struct wm_decoration *decoration,
	struct wm_style *style);

/* --- xdg-decoration protocol init --- */

/*
 * Initialize the xdg-decoration-unstable-v1 protocol on the server.
 * Negotiates server-side decorations with clients.
 */
void wm_xdg_decoration_init(struct wm_server *server);

#endif /* WM_DECORATION_H */
