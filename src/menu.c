/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * menu.c - Menu state management, input handling, and interaction
 */

#define _GNU_SOURCE
#include <cairo.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

#include "config.h"
#include "decoration.h"
#include "foreign_toplevel.h"
#include "i18n.h"
#include "ipc.h"
#include "keyboard_actions.h"
#include "menu.h"
#include "menu_parse.h"
#include "menu_render.h"
#include "output.h"
#include "rules.h"
#include "server.h"
#include "style.h"
#include "toolbar.h"
#include "util.h"
#include "view.h"
#include "workspace.h"

#define MENU_SEARCH_TIMEOUT_MS 500

static void cancel_pending_submenu(void);

/* --- Window menu --- */

struct wm_menu *
wm_menu_create_window_menu(struct wm_server *server)
{
	struct wm_menu *menu = menu_create(server, _("Window"));

	struct {
		enum wm_menu_item_type type;
		const char *label;
	} entries[] = {
		{WM_MENU_SHADE,    N_("Shade")},
		{WM_MENU_STICK,    N_("Stick")},
		{WM_MENU_MAXIMIZE, N_("Maximize")},
		{WM_MENU_ICONIFY,  N_("Iconify")},
		{WM_MENU_SEPARATOR, NULL},
		{WM_MENU_SENDTO,   N_("Send To...")},
		{WM_MENU_LAYER,    N_("Layer")},
		{WM_MENU_SEPARATOR, NULL},
		{WM_MENU_RAISE,    N_("Raise")},
		{WM_MENU_LOWER,    N_("Lower")},
		{WM_MENU_SEPARATOR, NULL},
		{WM_MENU_CLOSE,    N_("Close")},
		{WM_MENU_KILL,     N_("Kill")},
		{WM_MENU_SEPARATOR, NULL},
		{WM_MENU_REMEMBER, N_("Remember")},
	};

	for (size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); i++) {
		struct wm_menu_item *item = menu_item_create(
			entries[i].type,
			entries[i].label ? _(entries[i].label)
					 : NULL);
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
				item->submenu = menu_create(server, _("Layer"));
				if (item->submenu) {
					item->submenu->parent = menu;
					const char *layers[] = {
						_("Above Dock"), _("Dock"),
						_("Normal"), _("Bottom"), _("Desktop")
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
	struct wm_menu *menu = menu_create(server, _("Workspaces"));

	struct wm_workspace *ws;
	wl_list_for_each(ws, &server->workspaces, link) {
		char label[128];
		snprintf(label, sizeof(label), "%d: %s", ws->index + 1,
			ws->name ? ws->name : _("Workspace"));
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

/* --- Configuration submenu --- */

/*
 * Handle a "config:key value" command from the config submenu.
 * Modifies the in-memory config and triggers a reconfigure.
 */
static void
handle_config_command(struct wm_server *server, const char *cmd)
{
	if (!server->config || !cmd)
		return;

	/* Parse "key value" */
	char key[64];
	int value = 0;
	const char *space = strchr(cmd, ' ');
	if (!space)
		return;
	size_t klen = (size_t)(space - cmd);
	if (klen >= sizeof(key))
		klen = sizeof(key) - 1;
	memcpy(key, cmd, klen);
	key[klen] = '\0';
	if (!safe_atoi(space + 1, &value))
		return;

	struct wm_config *cfg = server->config;

	if (strcmp(key, "focus_policy") == 0) {
		cfg->focus_policy = value;
	} else if (strcmp(key, "focus_new_windows") == 0) {
		cfg->focus_new_windows = !cfg->focus_new_windows;
	} else if (strcmp(key, "opaque_move") == 0) {
		cfg->opaque_move = !cfg->opaque_move;
	} else if (strcmp(key, "opaque_resize") == 0) {
		cfg->opaque_resize = !cfg->opaque_resize;
	} else if (strcmp(key, "workspace_warping") == 0) {
		cfg->workspace_warping = !cfg->workspace_warping;
	} else if (strcmp(key, "full_maximization") == 0) {
		cfg->full_maximization = !cfg->full_maximization;
	} else if (strcmp(key, "tab_placement") == 0) {
		cfg->tab_placement = value;
	} else {
		wlr_log(WLR_ERROR, "config menu: unknown key '%s'", key);
		return;
	}

	/* Apply changes directly without full reconfigure.
	 * A full wm_server_reconfigure() would re-read the config file from
	 * disk, undoing the in-memory toggle, and would destroy/recreate all
	 * menus while we're still inside a menu click handler (UAF). */
	server->focus_policy = cfg->focus_policy;
	if (server->toolbar)
		wm_toolbar_relayout(server->toolbar);
}

static struct wm_menu *
create_focus_submenu(struct wm_server *server)
{
	struct wm_menu *menu = menu_create(server, _("Focus Model"));
	if (!menu)
		return NULL;

	struct { const char *label; const char *cmd; } items[] = {
		{N_("Click To Focus"),    "config:focus_policy 0"},
		{N_("Sloppy Focus"),      "config:focus_policy 1"},
		{N_("Strict Mouse Focus"),"config:focus_policy 2"},
	};

	for (size_t i = 0; i < sizeof(items) / sizeof(items[0]); i++) {
		struct wm_menu_item *item = menu_item_create(
			WM_MENU_COMMAND, _(items[i].label));
		if (item) {
			item->command = strdup(items[i].cmd);
			wl_list_insert(menu->items.prev, &item->link);
		}
	}

	return menu;
}

static struct wm_menu *
create_tab_placement_submenu(struct wm_server *server)
{
	struct wm_menu *menu = menu_create(server, _("Tab Placement"));
	if (!menu)
		return NULL;

	struct { const char *label; const char *cmd; } items[] = {
		{N_("Top"),    "config:tab_placement 0"},
		{N_("Bottom"), "config:tab_placement 1"},
		{N_("Left"),   "config:tab_placement 2"},
		{N_("Right"),  "config:tab_placement 3"},
	};

	for (size_t i = 0; i < sizeof(items) / sizeof(items[0]); i++) {
		struct wm_menu_item *item = menu_item_create(
			WM_MENU_COMMAND, _(items[i].label));
		if (item) {
			item->command = strdup(items[i].cmd);
			wl_list_insert(menu->items.prev, &item->link);
		}
	}

	return menu;
}

struct wm_menu *
wm_menu_create_config_menu(struct wm_server *server)
{
	struct wm_menu *menu = menu_create(server, _("Configuration"));
	if (!menu)
		return NULL;

	/* Focus Model submenu */
	struct wm_menu_item *focus_item = menu_item_create(
		WM_MENU_SUBMENU, _("Focus Model"));
	if (focus_item) {
		focus_item->submenu = create_focus_submenu(server);
		if (focus_item->submenu)
			focus_item->submenu->parent = menu;
		wl_list_insert(menu->items.prev, &focus_item->link);
	}

	/* Boolean toggles */
	struct { const char *label; const char *cmd; } toggles[] = {
		{N_("Focus New Windows"),  "config:focus_new_windows 0"},
		{N_("Opaque Move"),        "config:opaque_move 0"},
		{N_("Opaque Resize"),      "config:opaque_resize 0"},
		{N_("Workspace Warping"),  "config:workspace_warping 0"},
		{N_("Full Maximization"),  "config:full_maximization 0"},
	};

	for (size_t i = 0; i < sizeof(toggles) / sizeof(toggles[0]); i++) {
		struct wm_menu_item *item = menu_item_create(
			WM_MENU_COMMAND, _(toggles[i].label));
		if (item) {
			item->command = strdup(toggles[i].cmd);
			wl_list_insert(menu->items.prev, &item->link);
		}
	}

	/* Separator */
	struct wm_menu_item *sep = menu_item_create(
		WM_MENU_SEPARATOR, NULL);
	if (sep)
		wl_list_insert(menu->items.prev, &sep->link);

	/* Tab Placement submenu */
	struct wm_menu_item *tab_item = menu_item_create(
		WM_MENU_SUBMENU, _("Tab Placement"));
	if (tab_item) {
		tab_item->submenu = create_tab_placement_submenu(server);
		if (tab_item->submenu)
			tab_item->submenu->parent = menu;
		wl_list_insert(menu->items.prev, &tab_item->link);
	}

	return menu;
}

/* --- Workspace switching menu (for [workspaces] directive) --- */

struct wm_menu *
wm_menu_create_ws_switch_menu(struct wm_server *server)
{
	struct wm_menu *menu = menu_create(server, _("Workspaces"));
	if (!menu) {
		return NULL;
	}

	struct wm_workspace *ws;
	wl_list_for_each(ws, &server->workspaces, link) {
		char label[128];
		snprintf(label, sizeof(label), "%d: %s", ws->index + 1,
			ws->name ? ws->name : _("Workspace"));
		struct wm_menu_item *item = menu_item_create(
			WM_MENU_COMMAND, label);
		if (item) {
			char cmd[64];
			snprintf(cmd, sizeof(cmd), "Workspace %d",
				ws->index + 1);
			item->command = strdup(cmd);
			wl_list_insert(menu->items.prev, &item->link);
		}
	}

	return menu;
}

/* --- Window list menu --- */

struct wm_menu *
wm_menu_create_window_list(struct wm_server *server)
{
	struct wm_menu *menu = menu_create(server, _("Windows"));
	if (!menu) {
		return NULL;
	}

	struct wm_workspace *ws;
	wl_list_for_each(ws, &server->workspaces, link) {
		/* Collect views on this workspace */
		bool has_views = false;
		struct wm_view *view;
		wl_list_for_each(view, &server->views, link) {
			if (view->workspace == ws || view->sticky) {
				has_views = true;
				break;
			}
		}
		if (!has_views) {
			continue;
		}

		/* Add workspace header as a nop/separator label */
		char ws_label[128];
		snprintf(ws_label, sizeof(ws_label), "%d: %s",
			ws->index + 1,
			ws->name ? ws->name : "Workspace");
		struct wm_menu_item *header = menu_item_create(
			WM_MENU_NOP, ws_label);
		if (header) {
			wl_list_insert(menu->items.prev, &header->link);
		}

		/* Add each view on this workspace */
		wl_list_for_each(view, &server->views, link) {
			if (view->workspace != ws && !view->sticky) {
				continue;
			}
			const char *title = view->title;
			if (!title || !*title) {
				title = view->app_id ? view->app_id :
					"(untitled)";
			}

			bool is_iconified =
				!view->scene_tree->node.enabled;
			bool is_focused =
				(view == server->focused_view);
			bool is_shaded = view->decoration &&
				view->decoration->content_height == 0;

			/* Build state indicator suffix */
			char suffix[16] = "";
			size_t spos = 0;
			if (view->sticky)
				spos += snprintf(suffix + spos,
					sizeof(suffix) - spos, " [s]");
			if (view->maximized)
				spos += snprintf(suffix + spos,
					sizeof(suffix) - spos, " [M]");
			if (is_shaded)
				snprintf(suffix + spos,
					sizeof(suffix) - spos, " [S]");

			char entry_label[256];
			if (is_iconified) {
				snprintf(entry_label, sizeof(entry_label),
					"[%s]%s", title, suffix);
			} else if (is_focused) {
				snprintf(entry_label, sizeof(entry_label),
					"* %s%s", title, suffix);
			} else {
				snprintf(entry_label, sizeof(entry_label),
					"  %s%s", title, suffix);
			}

			struct wm_menu_item *item = menu_item_create(
				WM_MENU_WINDOW_ENTRY, entry_label);
			if (item) {
				item->data = view;
				/* Store workspace index for switching */
				char cmd[32];
				snprintf(cmd, sizeof(cmd), "%d",
					ws->index);
				item->command = strdup(cmd);
				wl_list_insert(menu->items.prev,
					&item->link);
			}
		}
	}

	return menu;
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

	/* Clamp menu position to the output containing the cursor */
	int out_x = 0, out_y = 0;
	int out_w = 0, out_h = 0;
	struct wlr_output *wlr_output =
		wlr_output_layout_output_at(server->output_layout, x, y);
	if (wlr_output) {
		/* Find our wm_output wrapper for usable area */
		struct wm_output *wm_out;
		wl_list_for_each(wm_out, &server->outputs, link) {
			if (wm_out->wlr_output == wlr_output) {
				/* Get output position in layout */
				struct wlr_box out_box;
				wlr_output_layout_get_box(
					server->output_layout,
					wlr_output, &out_box);
				out_x = out_box.x + wm_out->usable_area.x;
				out_y = out_box.y + wm_out->usable_area.y;
				out_w = wm_out->usable_area.width;
				out_h = wm_out->usable_area.height;
				break;
			}
		}
	}
	if (out_w > 0 && out_h > 0) {
		if (x + menu->width > out_x + out_w) {
			x = out_x + out_w - menu->width;
		}
		if (y + menu->height > out_y + out_h) {
			y = out_y + out_h - menu->height;
		}
		if (x < out_x) {
			x = out_x;
		}
		if (y < out_y) {
			y = out_y;
		}
	}

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

	/* Broadcast menu_opened IPC event for screen readers */
	{
		char esc_title[256], buf[512];
		const char *t = menu->title ? menu->title : "";
		size_t j = 0;
		for (size_t i = 0; t[i] && j + 1 < sizeof(esc_title); i++) {
			unsigned char c = (unsigned char)t[i];
			if (c == '"' || c == '\\') {
				if (j + 2 >= sizeof(esc_title)) break;
				esc_title[j++] = '\\';
			}
			if (c >= 0x20) esc_title[j++] = c;
		}
		esc_title[j] = '\0';
		snprintf(buf, sizeof(buf),
			"{\"event\":\"menu_opened\","
			"\"title\":\"%s\","
			"\"items\":%d}",
			esc_title, menu->item_count);
		wm_ipc_broadcast_event(&server->ipc,
			WM_IPC_EVENT_MENU, buf);
	}
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

	/* Clear search buffer */
	menu->search_len = 0;
	menu->search_buf[0] = '\0';

	cancel_pending_submenu();

	/* Broadcast menu_closed IPC event for screen readers */
	if (menu->server) {
		char esc_title[256], buf[512];
		const char *t = menu->title ? menu->title : "";
		size_t j = 0;
		for (size_t i = 0; t[i] && j + 1 < sizeof(esc_title); i++) {
			unsigned char c = (unsigned char)t[i];
			if (c == '"' || c == '\\') {
				if (j + 2 >= sizeof(esc_title)) break;
				esc_title[j++] = '\\';
			}
			if (c >= 0x20) esc_title[j++] = c;
		}
		esc_title[j] = '\0';
		snprintf(buf, sizeof(buf),
			"{\"event\":\"menu_closed\","
			"\"title\":\"%s\"}",
			esc_title);
		wm_ipc_broadcast_event(&menu->server->ipc,
			WM_IPC_EVENT_MENU, buf);
	}
}

void
wm_menu_hide_all(struct wm_server *server)
{
	if (!server) {
		return;
	}

	if (server->root_menu && server->root_menu->visible) {
		wm_menu_hide(server->root_menu);
	}
	if (server->window_menu && server->window_menu->visible) {
		wm_menu_hide(server->window_menu);
	}
	if (server->window_list_menu) {
		wm_menu_destroy(server->window_list_menu);
		server->window_list_menu = NULL;
	}
	if (server->workspace_menu) {
		wm_menu_destroy(server->workspace_menu);
		server->workspace_menu = NULL;
	}
	if (server->client_menu) {
		wm_menu_destroy(server->client_menu);
		server->client_menu = NULL;
	}
	if (server->custom_menu) {
		wm_menu_destroy(server->custom_menu);
		server->custom_menu = NULL;
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
			wm_spawn_command(item->command);
		}
		break;

	case WM_MENU_RECONFIG:
		wlr_log(WLR_INFO, "menu: %s", "reconfig");
		wm_execute_action(server, WM_ACTION_RECONFIGURE, NULL);
		break;

	case WM_MENU_RESTART:
		wlr_log(WLR_INFO, "menu: %s", "restart");
		if (item->command) {
			/* Restart with different window manager */
			wm_spawn_command(item->command);
			wl_display_terminate(server->wl_display);
		} else {
			/* Restart: reload configuration */
			wm_execute_action(server, WM_ACTION_RECONFIGURE,
				NULL);
		}
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
		wm_execute_action(server, WM_ACTION_SHADE, NULL);
		break;

	case WM_MENU_STICK:
		if (server->focused_view) {
			wm_view_set_sticky(server->focused_view,
				!server->focused_view->sticky);
		}
		break;

	case WM_MENU_MAXIMIZE:
		wm_execute_action(server, WM_ACTION_MAXIMIZE, NULL);
		break;

	case WM_MENU_ICONIFY:
		wm_execute_action(server, WM_ACTION_MINIMIZE, NULL);
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
		if (item->command) {
			wlr_log(WLR_INFO, "menu command: %s", item->command);
			/* Config submenu commands use "config:" prefix */
			if (strncmp(item->command, "config:", 7) == 0) {
				handle_config_command(server,
					item->command + 7);
				break;
			}
			/* Parse "ActionName argument" and dispatch */
			char action_name[64];
			const char *arg = NULL;
			const char *space = strchr(item->command, ' ');
			if (space) {
				size_t len = (size_t)(space - item->command);
				if (len >= sizeof(action_name))
					len = sizeof(action_name) - 1;
				memcpy(action_name, item->command, len);
				action_name[len] = '\0';
				arg = space + 1;
			} else {
				snprintf(action_name, sizeof(action_name),
					"%s", item->command);
			}
			enum wm_action act =
				wm_action_from_name(action_name);
			if (act != WM_ACTION_NOP) {
				wm_execute_action(server, act, arg);
			} else {
				wlr_log(WLR_ERROR,
					"menu: unknown action '%s'",
					action_name);
			}
		}
		break;

	case WM_MENU_WINDOW_ENTRY: {
		/* Switch to workspace and focus the target view */
		struct wm_view *target = item->data;
		if (!target) {
			break;
		}
		/* Validate the view still exists (may have been destroyed
		 * while the menu was open) */
		bool found = false;
		struct wm_view *v;
		wl_list_for_each(v, &server->views, link) {
			if (v == target) {
				found = true;
				break;
			}
		}
		if (!found) {
			wlr_log(WLR_DEBUG, "%s", "menu: target view no longer exists");
			break;
		}
		/* Switch to the view's workspace */
		if (item->command) {
			int ws_idx;
			if (safe_atoi(item->command, &ws_idx))
				wm_workspace_switch(server, ws_idx);
		}
		/* De-iconify if minimized */
		if (!target->scene_tree->node.enabled) {
			wlr_scene_node_set_enabled(
				&target->scene_tree->node, true);
			wm_foreign_toplevel_set_minimized(target, false);
		}
		/* Focus and raise */
		wm_focus_view(target,
			target->xdg_toplevel->base->surface);
		wm_view_raise(target);
		break;
	}

	case WM_MENU_REMEMBER:
		if (server->focused_view && server->config &&
		    server->config->apps_file) {
			wm_rules_remember_window(server->focused_view,
				server->config->apps_file);
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

/* --- Submenu positioning helper --- */

/*
 * Compute the position for a submenu, flipping to the left if it would
 * go off the right edge of the output.
 */
static void
submenu_position(struct wm_menu *parent, int item_y_offset,
	int *out_x, int *out_y)
{
	struct wm_server *server = parent->server;
	int sub_x = parent->x + parent->width;
	int sub_y = parent->y + parent->border_width +
		parent->title_height + item_y_offset;

	/* Get output bounds at the parent menu position */
	struct wlr_output *wlr_output = wlr_output_layout_output_at(
		server->output_layout, parent->x, parent->y);
	if (wlr_output) {
		struct wm_output *wm_out;
		wl_list_for_each(wm_out, &server->outputs, link) {
			if (wm_out->wlr_output == wlr_output) {
				struct wlr_box out_box;
				wlr_output_layout_get_box(
					server->output_layout,
					wlr_output, &out_box);
				int out_right = out_box.x +
					wm_out->usable_area.x +
					wm_out->usable_area.width;
				int out_left = out_box.x +
					wm_out->usable_area.x;
				/* Flip to left side if overflowing right */
				if (sub_x + parent->width > out_right) {
					sub_x = parent->x - parent->width;
					if (sub_x < out_left) {
						sub_x = out_left;
					}
				}
				break;
			}
		}
	}

	*out_x = sub_x;
	*out_y = sub_y;
}

/* --- Type-ahead search --- */

static int
search_timer_cb(void *data)
{
	struct wm_menu *menu = data;
	menu->search_len = 0;
	menu->search_buf[0] = '\0';
	return 0;
}

static bool
menu_type_ahead(struct wm_menu *menu, char ch)
{
	if (!menu || !menu->server || !menu->server->config)
		return false;

	enum wm_menu_search mode = menu->server->config->menu_search;
	if (mode == WM_MENU_SEARCH_NOWHERE)
		return false;

	/* Append character to search buffer */
	if (menu->search_len >= (int)sizeof(menu->search_buf) - 1)
		return false;

	menu->search_buf[menu->search_len++] = tolower((unsigned char)ch);
	menu->search_buf[menu->search_len] = '\0';

	/* Reset search timer */
	if (!menu->search_timer) {
		menu->search_timer = wl_event_loop_add_timer(
			menu->server->wl_event_loop, search_timer_cb, menu);
	}
	if (menu->search_timer) {
		wl_event_source_timer_update(menu->search_timer,
			MENU_SEARCH_TIMEOUT_MS);
	}

	/* Search for matching item */
	int idx = 0;
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		if (item->label && item->type != WM_MENU_SEPARATOR &&
		    item->type != WM_MENU_NOP) {
			bool match = false;
			if (mode == WM_MENU_SEARCH_ITEMSTART) {
				match = (strncasecmp(item->label,
					menu->search_buf,
					menu->search_len) == 0);
			} else {
				match = (strcasestr(item->label,
					menu->search_buf) != NULL);
			}
			if (match) {
				menu->selected_index = idx;
				menu_update_render(menu);
				return true;
			}
		}
		idx++;
	}

	return true; /* consumed even if no match */
}

/* --- Keyboard interaction --- */

bool
wm_menu_handle_key(struct wm_server *server, uint32_t keycode,
	xkb_keysym_t sym, bool pressed)
{
	(void)keycode;

	if (!pressed || !server) {
		return false;
	}

	/* Dispatch to whichever menu is currently visible */
	if (server->window_list_menu && server->window_list_menu->visible) {
		return wm_menu_handle_key_for(server->window_list_menu, sym);
	}

	if (server->window_menu && server->window_menu->visible) {
		return wm_menu_handle_key_for(server->window_menu, sym);
	}

	if (server->root_menu && server->root_menu->visible) {
		return wm_menu_handle_key_for(server->root_menu, sym);
	}

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
				/* Broadcast focus change for accessibility */
				if (menu->server && item->label) {
					char buf[512];
					snprintf(buf, sizeof(buf),
						"{\"event\":\"focus_changed\",\"data\":{"
						"\"target\":\"menu\",\"label\":\"%s\","
						"\"index\":%d,\"has_submenu\":%s}}",
						item->label,
						menu->selected_index,
						item->submenu ? "true" : "false");
					wm_ipc_broadcast_event(&menu->server->ipc,
						WM_IPC_EVENT_FOCUS_CHANGED, buf);
				}
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
				/* Broadcast focus change for accessibility */
				if (menu->server && item->label) {
					char buf[512];
					snprintf(buf, sizeof(buf),
						"{\"event\":\"focus_changed\",\"data\":{"
						"\"target\":\"menu\",\"label\":\"%s\","
						"\"index\":%d,\"has_submenu\":%s}}",
						item->label,
						menu->selected_index,
						item->submenu ? "true" : "false");
					wm_ipc_broadcast_event(&menu->server->ipc,
						WM_IPC_EVENT_FOCUS_CHANGED, buf);
				}
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
			int sub_x, sub_y;
			submenu_position(menu, y_off, &sub_x, &sub_y);
			wm_menu_show(item->submenu, sub_x, sub_y);
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
			/* Hide menus BEFORE executing to avoid UAF —
			 * see button handler comment for details. */
			wm_menu_hide(root_menu);
			execute_menu_item(menu, item);
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

	default: {
		/* Type-ahead search for printable characters */
		char ch = 0;
		if (sym >= XKB_KEY_a && sym <= XKB_KEY_z)
			ch = 'a' + (sym - XKB_KEY_a);
		else if (sym >= XKB_KEY_A && sym <= XKB_KEY_Z)
			ch = 'a' + (sym - XKB_KEY_A);
		else if (sym >= XKB_KEY_0 && sym <= XKB_KEY_9)
			ch = '0' + (sym - XKB_KEY_0);
		else if (sym == XKB_KEY_space)
			ch = ' ';

		if (ch && menu_type_ahead(menu, ch))
			return true;

		return false;
	}
	}
}

/* --- Mouse interaction --- */

bool
wm_menu_handle_motion(struct wm_server *server, double lx, double ly)
{
	if (!server) {
		return false;
	}

	/* Check window list menu first (it's typically on top) */
	if (server->window_list_menu && server->window_list_menu->visible) {
		if (wm_menu_handle_motion_for(server->window_list_menu,
			lx, ly)) {
			return true;
		}
	}

	/* Check window menu next */
	if (server->window_menu && server->window_menu->visible) {
		if (wm_menu_handle_motion_for(server->window_menu, lx, ly)) {
			return true;
		}
	}

	if (server->root_menu && server->root_menu->visible) {
		if (wm_menu_handle_motion_for(server->root_menu, lx, ly)) {
			return true;
		}
	}

	return false;
}

/* Pending submenu open state (for menu_delay) */
static struct {
	struct wm_menu_item *item;
	int sub_x, sub_y;
	struct wl_event_source *timer;
} pending_submenu;

static int
submenu_delay_cb(void *data)
{
	(void)data;
	if (pending_submenu.item && pending_submenu.item->submenu) {
		wm_menu_show(pending_submenu.item->submenu,
			pending_submenu.sub_x, pending_submenu.sub_y);
	}
	pending_submenu.item = NULL;
	return 0;
}

static void
cancel_pending_submenu(void)
{
	if (pending_submenu.timer) {
		wl_event_source_timer_update(pending_submenu.timer, 0);
	}
	pending_submenu.item = NULL;
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

		cancel_pending_submenu();

		hit_menu->selected_index = idx;
		menu_update_render(hit_menu);

		/* Auto-open submenu on hover (with optional delay) */
		struct wm_menu_item *item = menu_get_item(hit_menu, idx);
		if (item && item->submenu) {
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
			int sub_x, sub_y;
			submenu_position(hit_menu, y_off,
				&sub_x, &sub_y);

			int delay = 0;
			if (hit_menu->server && hit_menu->server->config)
				delay = hit_menu->server->config->menu_delay;

			if (delay > 0) {
				pending_submenu.item = item;
				pending_submenu.sub_x = sub_x;
				pending_submenu.sub_y = sub_y;
				if (!pending_submenu.timer &&
				    hit_menu->server) {
					pending_submenu.timer =
						wl_event_loop_add_timer(
						hit_menu->server->wl_event_loop,
						submenu_delay_cb, NULL);
				}
				if (pending_submenu.timer) {
					wl_event_source_timer_update(
						pending_submenu.timer,
						delay);
				}
			} else {
				wm_menu_show(item->submenu, sub_x, sub_y);
			}
		}
	}

	return true;
}

bool
wm_menu_handle_button(struct wm_server *server, double lx, double ly,
	uint32_t button, bool pressed)
{
	if (!server) {
		return false;
	}

	/* Check window list menu first (it's typically on top) */
	if (server->window_list_menu && server->window_list_menu->visible) {
		if (wm_menu_handle_button_for(server->window_list_menu,
			lx, ly, button, pressed)) {
			return true;
		}
	}

	/* Check window menu next */
	if (server->window_menu && server->window_menu->visible) {
		if (wm_menu_handle_button_for(server->window_menu, lx, ly,
			button, pressed)) {
			return true;
		}
	}

	if (server->root_menu && server->root_menu->visible) {
		if (wm_menu_handle_button_for(server->root_menu, lx, ly,
			button, pressed)) {
			return true;
		}
	}

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
			int sub_x, sub_y;
			submenu_position(hit_menu, y_off,
				&sub_x, &sub_y);
			wm_menu_show(item->submenu, sub_x, sub_y);
		}
		return true;
	}

	/* Activate the item */
	if (item->type != WM_MENU_SEPARATOR &&
	    item->type != WM_MENU_NOP) {
		/* Hide menus BEFORE executing the action to avoid UAF:
		 * execute_menu_item may trigger wm_server_reconfigure()
		 * which destroys and recreates all menus. If we hid
		 * after, root_menu would be a dangling pointer. */
		wm_menu_hide(root_menu);
		execute_menu_item(hit_menu, item);
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

	if (menu->search_timer) {
		wl_event_source_remove(menu->search_timer);
	}

	free(menu->title);
	free(menu);
}

/* --- Action dispatch --- */

void
wm_menu_show_root(struct wm_server *server, int x, int y)
{
	if (!server || !server->root_menu) {
		return;
	}

	/* Toggle: if root menu is already visible, hide it */
	if (server->root_menu->visible) {
		wm_menu_hide_all(server);
		return;
	}

	/* Hide any other menus first, then show root */
	wm_menu_hide_all(server);

	wm_menu_show(server->root_menu, x, y);
}

void
wm_menu_show_window(struct wm_server *server, int x, int y)
{
	if (!server) {
		return;
	}

	/* Hide any existing menus first */
	wm_menu_hide_all(server);

	/* If a custom window menu file is configured, reload from it */
	if (server->config && server->config->window_menu_file) {
		if (server->window_menu) {
			wm_menu_destroy(server->window_menu);
		}
		server->window_menu = wm_menu_load(server,
			server->config->window_menu_file);
	}

	if (!server->window_menu) {
		return;
	}

	wm_menu_show(server->window_menu, x, y);
}

void
wm_menu_show_window_list(struct wm_server *server, int x, int y)
{
	if (!server) {
		return;
	}

	/* Hide any existing menus first */
	wm_menu_hide_all(server);

	/* Build a fresh window list menu (it's dynamic) */
	server->window_list_menu = wm_menu_create_window_list(server);
	if (!server->window_list_menu) {
		return;
	}

	wm_menu_show(server->window_list_menu, x, y);
}

void
wm_menu_show_workspace_menu(struct wm_server *server, int x, int y)
{
	if (!server) {
		return;
	}

	wm_menu_hide_all(server);

	/* Build a dynamic workspace menu for switching (not "Send To") */
	struct wm_menu *menu = menu_create(server, _("Workspaces"));
	if (!menu) {
		return;
	}

	struct wm_workspace *ws;
	wl_list_for_each(ws, &server->workspaces, link) {
		char label[128];
		bool is_current = (ws == wm_workspace_get_active(server));
		snprintf(label, sizeof(label), "%s%d: %s",
			is_current ? "* " : "  ",
			ws->index + 1,
			ws->name ? ws->name : _("Workspace"));
		struct wm_menu_item *item = menu_item_create(
			WM_MENU_COMMAND, label);
		if (item) {
			char cmd[64];
			snprintf(cmd, sizeof(cmd), "Workspace %d",
				ws->index + 1);
			item->command = strdup(cmd);
			wl_list_insert(menu->items.prev, &item->link);
		}
	}

	server->workspace_menu = menu;
	wm_menu_show(server->workspace_menu, x, y);
}

void
wm_menu_show_client_menu(struct wm_server *server, const char *pattern,
	int x, int y)
{
	if (!server) {
		return;
	}

	wm_menu_hide_all(server);

	struct wm_menu *menu = menu_create(server, _("Clients"));
	if (!menu) {
		return;
	}

	struct wm_view *view;
	wl_list_for_each(view, &server->views, link) {
		/* Apply optional pattern filter on app_id */
		if (pattern && *pattern) {
			const char *app_id = view->app_id ? view->app_id : "";
			if (strcasestr(app_id, pattern) == NULL) {
				continue;
			}
		}

		const char *title = view->title;
		if (!title || !*title) {
			title = view->app_id ? view->app_id : "(untitled)";
		}

		bool is_iconified = !view->scene_tree->node.enabled;
		bool is_focused = (view == server->focused_view);
		bool is_shaded = view->decoration &&
			view->decoration->content_height == 0;

		/* Build state indicator suffix */
		char suffix[16] = "";
		size_t spos = 0;
		if (view->sticky)
			spos += snprintf(suffix + spos,
				sizeof(suffix) - spos, " [s]");
		if (view->maximized)
			spos += snprintf(suffix + spos,
				sizeof(suffix) - spos, " [M]");
		if (is_shaded)
			snprintf(suffix + spos,
				sizeof(suffix) - spos, " [S]");

		char entry_label[256];
		if (is_iconified) {
			snprintf(entry_label, sizeof(entry_label),
				"[%s]%s", title, suffix);
		} else if (is_focused) {
			snprintf(entry_label, sizeof(entry_label),
				"* %s%s", title, suffix);
		} else {
			snprintf(entry_label, sizeof(entry_label),
				"  %s%s", title, suffix);
		}

		struct wm_menu_item *item = menu_item_create(
			WM_MENU_WINDOW_ENTRY, entry_label);
		if (item) {
			item->data = view;
			int ws_idx = view->workspace ?
				view->workspace->index : 0;
			char cmd[32];
			snprintf(cmd, sizeof(cmd), "%d", ws_idx);
			item->command = strdup(cmd);
			wl_list_insert(menu->items.prev, &item->link);
		}
	}

	server->client_menu = menu;
	wm_menu_show(server->client_menu, x, y);
}

void
wm_menu_show_custom(struct wm_server *server, const char *path,
	int x, int y)
{
	if (!server || !path || !*path) {
		return;
	}

	wm_menu_hide_all(server);

	server->custom_menu = wm_menu_load(server, path);
	if (!server->custom_menu) {
		return;
	}

	wm_menu_show(server->custom_menu, x, y);
}
