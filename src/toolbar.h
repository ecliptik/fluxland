/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 *
 * toolbar.h - Built-in toolbar with workspace switcher, icon bar, and clock
 *
 * The toolbar is a compositor-rendered panel (not a layer-shell client)
 * that sits at the top or bottom of the primary output. It shows:
 *   - Workspace buttons (left)
 *   - Icon bar / window list for current workspace (center)
 *   - Clock with configurable strftime format (right)
 */

#ifndef WM_TOOLBAR_H
#define WM_TOOLBAR_H

#include <stdbool.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>

struct wm_server;
struct wm_view;

/* Icon bar display modes (which windows to show) */
enum wm_iconbar_mode {
	WM_ICONBAR_MODE_WORKSPACE,       /* views on current workspace (default) */
	WM_ICONBAR_MODE_ALL_WINDOWS,     /* views from all workspaces */
	WM_ICONBAR_MODE_ICONS,           /* only iconified/minimized views */
	WM_ICONBAR_MODE_NO_ICONS,        /* non-iconified views on current ws */
	WM_ICONBAR_MODE_WORKSPACE_ICONS, /* iconified views on current ws */
};

#define WM_TOOLBAR_HEIGHT 24
#define WM_TOOLBAR_PADDING 4
#define WM_TOOLBAR_CLOCK_FMT "%H:%M"
#define WM_TOOLBAR_ICONBAR_MAX 64

/* An entry in the icon bar representing one window */
struct wm_iconbar_entry {
	struct wm_view *view;
	bool focused;
	bool iconified; /* minimized / scene node disabled */
};

struct wm_toolbar {
	struct wm_server *server;

	/* Icon bar mode */
	enum wm_iconbar_mode iconbar_mode;

	/* Scene graph nodes */
	struct wlr_scene_tree *scene_tree;
	struct wlr_scene_buffer *bg_buf;
	struct wlr_scene_buffer *workspace_buf;
	struct wlr_scene_buffer *iconbar_buf;
	struct wlr_scene_buffer *clock_buf;

	/* Geometry */
	int x, y;
	int width, height;
	bool visible;
	bool on_top; /* true if placed at top of screen */

	/* Auto-hide state */
	bool auto_hide;
	bool shown; /* true when visible during auto-hide mode */
	struct wl_event_source *hide_timer;

	/* Workspace button hit regions (for click handling) */
	struct wlr_box *ws_boxes;
	int ws_box_count;

	/* Icon bar hit regions and entries */
	struct wlr_box *ib_boxes;
	struct wm_iconbar_entry *ib_entries;
	int ib_count;
	int ib_x_offset; /* icon bar x offset within toolbar */

	/* Clock timer */
	struct wl_event_source *clock_timer;

	/* Cached state to avoid redundant redraws */
	int cached_ws_index;
	char *cached_title;
	char cached_clock[64];
};

/* Create the toolbar and add it to the scene graph */
struct wm_toolbar *wm_toolbar_create(struct wm_server *server);

/* Destroy the toolbar and free resources */
void wm_toolbar_destroy(struct wm_toolbar *toolbar);

/* Update the workspace display (call on workspace switch) */
void wm_toolbar_update_workspace(struct wm_toolbar *toolbar);

/* Update the window title display (call on focus change or title change) */
void wm_toolbar_update_title(struct wm_toolbar *toolbar);

/* Update the icon bar window list (call on map/unmap/focus/title change) */
void wm_toolbar_update_iconbar(struct wm_toolbar *toolbar);

/* Recalculate toolbar geometry (call on output resize) */
void wm_toolbar_relayout(struct wm_toolbar *toolbar);

/* Handle pointer click at layout coordinates; returns true if consumed */
bool wm_toolbar_handle_click(struct wm_toolbar *toolbar,
	double lx, double ly);

/* Handle scroll/wheel on toolbar; returns true if consumed */
bool wm_toolbar_handle_scroll(struct wm_toolbar *toolbar,
	double lx, double ly, double delta);

/* Notify toolbar of pointer motion (for auto-hide trigger) */
void wm_toolbar_notify_pointer_motion(struct wm_toolbar *toolbar,
	double lx, double ly);

/* Toggle toolbar layer between above-dock and normal */
void wm_toolbar_toggle_above(struct wm_toolbar *toolbar);

/* Toggle toolbar visibility on/off */
void wm_toolbar_toggle_visible(struct wm_toolbar *toolbar);

#endif /* WM_TOOLBAR_H */
