/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 *
 * toolbar.h - Built-in toolbar with configurable tool layout
 *
 * The toolbar is a compositor-rendered panel (not a layer-shell client)
 * that sits at the top or bottom of the primary output. The tools shown
 * and their order are controlled by session.screen0.toolbar.tools.
 * Default: "prevworkspace workspacename nextworkspace iconbar clock"
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
#define WM_TOOLBAR_MAX_TOOLS 16

/* Toolbar tool types */
enum wm_toolbar_tool_type {
	WM_TOOL_PREV_WORKSPACE,
	WM_TOOL_NEXT_WORKSPACE,
	WM_TOOL_WORKSPACE_NAME,
	WM_TOOL_ICONBAR,
	WM_TOOL_CLOCK,
	WM_TOOL_PREV_WINDOW,
	WM_TOOL_NEXT_WINDOW,
};

/* A single tool instance in the toolbar */
struct wm_toolbar_tool {
	enum wm_toolbar_tool_type type;
	struct wlr_scene_buffer *buf;
	int x, width;
	/* Per-tool hit boxes (workspace buttons within workspace_name, etc.) */
	struct wlr_box *hit_boxes;
	int hit_box_count;
};

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

	/* Configurable tool array */
	struct wm_toolbar_tool *tools;
	int tool_count;

	/* Quick-access pointers into tools array (NULL if not configured) */
	struct wm_toolbar_tool *iconbar_tool;
	struct wm_toolbar_tool *clock_tool;
	struct wm_toolbar_tool *ws_name_tool;

	/* Geometry */
	int x, y;
	int width, height;
	bool visible;
	bool on_top; /* true if placed at top of screen */

	/* Auto-hide state */
	bool auto_hide;
	bool shown; /* true when visible during auto-hide mode */
	struct wl_event_source *hide_timer;

	/* Icon bar hit regions and entries */
	struct wlr_box *ib_boxes;
	struct wm_iconbar_entry *ib_entries;
	int ib_count;

	/* Clock timer */
	struct wl_event_source *clock_timer;

	/* Cached state to avoid redundant redraws */
	int cached_ws_index;
	char *cached_title;
	char cached_clock[64];

	/* Keyboard focus indicator overlay */
	struct wlr_scene_buffer *focus_indicator;
};

/* Create the toolbar and add it to the scene graph */
struct wm_toolbar *wm_toolbar_create(struct wm_server *server);

/* Destroy the toolbar and free resources */
void wm_toolbar_destroy(struct wm_toolbar *toolbar);

/* Update the workspace display (call on workspace switch) */
void wm_toolbar_update_workspace(struct wm_toolbar *toolbar);

/* Update the icon bar window list (call on map/unmap/focus/title change) */
void wm_toolbar_update_iconbar(struct wm_toolbar *toolbar);

/* Recalculate toolbar geometry (call on output resize) */
void wm_toolbar_relayout(struct wm_toolbar *toolbar);

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
