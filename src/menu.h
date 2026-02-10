/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
 *
 * menu.h - Fluxbox menu file parser, rendering, and interaction
 *
 * Supports the Fluxbox menu file format:
 *   [begin] (title) ... [end]
 *   [exec] (label) {command} <icon>
 *   [submenu] (label) {title} ... [end]
 *   [separator], [nop], [include], [config], [workspaces],
 *   [reconfig], [restart], [exit], [style], [stylesdir],
 *   [command], and window menu items.
 */

#ifndef WM_MENU_H
#define WM_MENU_H

#include <stdbool.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>

#include "keybind.h"
#include "style.h"

struct wm_server;
struct wm_view;

/* --- Menu item types --- */

enum wm_menu_item_type {
	WM_MENU_EXEC,		/* [exec] (label) {command} */
	WM_MENU_SUBMENU,	/* [submenu] (label) */
	WM_MENU_SEPARATOR,	/* [separator] */
	WM_MENU_NOP,		/* [nop] (label) */
	WM_MENU_INCLUDE,	/* [include] (path) */
	WM_MENU_CONFIG,		/* [config] */
	WM_MENU_WORKSPACES,	/* [workspaces] */
	WM_MENU_RECONFIG,	/* [reconfig] */
	WM_MENU_RESTART,	/* [restart] */
	WM_MENU_EXIT,		/* [exit] */
	WM_MENU_STYLE,		/* [style] (label) {filename} */
	WM_MENU_STYLESDIR,	/* [stylesdir] (directory) */
	WM_MENU_COMMAND,	/* [command] (label) - keys-file action */
	/* Window menu items */
	WM_MENU_SHADE,
	WM_MENU_STICK,
	WM_MENU_MAXIMIZE,
	WM_MENU_ICONIFY,
	WM_MENU_CLOSE,
	WM_MENU_KILL,
	WM_MENU_RAISE,
	WM_MENU_LOWER,
	WM_MENU_SENDTO,	/* workspace list submenu */
	WM_MENU_LAYER,		/* layer selection submenu */
	WM_MENU_WINDOW_ENTRY,	/* window list entry (focus/deiconify) */
};

/* --- Menu item --- */

struct wm_menu_item {
	enum wm_menu_item_type type;
	char *label;
	char *command;		/* for exec, style, restart, command */
	char *icon_path;	/* optional icon */
	struct wm_menu *submenu;	/* for submenu type */
	void *data;		/* opaque data (e.g. view ptr for window list) */
	struct wl_list link;	/* wm_menu.items */
};

/* --- Menu --- */

struct wm_menu {
	char *title;
	struct wl_list items;	/* wm_menu_item.link */
	struct wm_menu *parent;
	struct wm_server *server;

	/* Display state */
	bool visible;
	int x, y;
	int width, height;
	int selected_index;

	/* Scene graph nodes */
	struct wlr_scene_tree *scene_tree;
	struct wlr_scene_buffer *title_buf;
	struct wlr_scene_rect *frame_bg;
	struct wlr_scene_buffer *items_buf;
	struct wlr_scene_rect *hilite_rect;

	/* Computed layout */
	int title_height;
	int item_height;
	int item_count;
	int border_width;
};

/* --- Menu loading --- */

/*
 * Load a root menu from a Fluxbox-format menu file.
 * Returns the menu, or NULL on error.
 */
struct wm_menu *wm_menu_load(struct wm_server *server, const char *path);

/*
 * Create the default window context menu.
 */
struct wm_menu *wm_menu_create_window_menu(struct wm_server *server);

/*
 * Create a dynamic workspace list menu.
 */
struct wm_menu *wm_menu_create_workspace_menu(struct wm_server *server);

/*
 * Create a dynamic window list menu grouped by workspace.
 */
struct wm_menu *wm_menu_create_window_list(struct wm_server *server);

/* --- Menu display --- */

/*
 * Show a menu at the given position (layout coordinates).
 * Creates scene graph nodes and renders the menu.
 */
void wm_menu_show(struct wm_menu *menu, int x, int y);

/*
 * Hide a menu and destroy its scene graph nodes.
 */
void wm_menu_hide(struct wm_menu *menu);

/*
 * Hide all visible menus.
 */
void wm_menu_hide_all(struct wm_server *server);

/* --- Menu interaction --- */

/*
 * Handle keyboard input while a menu is visible.
 * Returns true if the key was consumed by the menu.
 */
bool wm_menu_handle_key(struct wm_server *server, uint32_t keycode,
	xkb_keysym_t sym, bool pressed);

/*
 * Handle pointer motion over menus.
 * Returns true if the motion was over a menu.
 */
bool wm_menu_handle_motion(struct wm_server *server, double lx, double ly);

/*
 * Handle pointer button click on menus.
 * Returns true if the click was consumed by a menu.
 */
bool wm_menu_handle_button(struct wm_server *server, double lx, double ly,
	uint32_t button, bool pressed);

/* --- Internal interaction (for integration layer) --- */

/*
 * Handle key for a specific visible menu tree.
 * Called by the integration layer which knows the active root menu.
 */
bool wm_menu_handle_key_for(struct wm_menu *root_menu, xkb_keysym_t sym);

/*
 * Handle motion for a specific menu tree.
 */
bool wm_menu_handle_motion_for(struct wm_menu *root_menu,
	double lx, double ly);

/*
 * Handle button click for a specific menu tree.
 */
bool wm_menu_handle_button_for(struct wm_menu *root_menu,
	double lx, double ly, uint32_t button, bool pressed);

/* --- Menu lifecycle --- */

/*
 * Destroy a menu and all its items (recursive for submenus).
 */
void wm_menu_destroy(struct wm_menu *menu);

/*
 * Check if any menu is currently visible.
 */
bool wm_menu_is_open(struct wm_server *server);

/* --- Action dispatch --- */

/*
 * Show the root menu at the given position.
 */
void wm_menu_show_root(struct wm_server *server, int x, int y);

/*
 * Show the window menu for the focused view.
 */
void wm_menu_show_window(struct wm_server *server, int x, int y);

/*
 * Show the window list menu at the given position.
 * Creates a dynamic menu listing all views grouped by workspace.
 */
void wm_menu_show_window_list(struct wm_server *server, int x, int y);

#endif /* WM_MENU_H */
