/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * systray_menu.c - A tray icon's menu (com.canonical.dbusmenu)
 *
 * GetLayout answers with the whole tree, each entry with its properties
 * (label, enabled, visible, type, toggle-type, toggle-state) and its
 * children.  The tree becomes a wm_menu: submenus for entries with
 * children, a separator for each "separator", the disabled ones as
 * inert items, check and radio marks as the menu's state dots.
 */

#ifdef WM_HAS_SYSTRAY

#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <systemd/sd-bus.h>
#include <time.h>
#include <wayland-server-core.h>
#include <wlr/util/log.h>

#include "menu.h"
#include "menu_parse.h"
#include "server.h"
#include "systray.h"
#include "systray_menu.h"
#include "util.h"

#define DBUSMENU_IFACE "com.canonical.dbusmenu"

/* What a hostile or broken application can make us build */
#define DBUSMENU_MAX_DEPTH 8
#define DBUSMENU_MAX_NODES 512

/* --- The layout, as received --- */

struct dm_node {
	int32_t id;
	char *label;
	bool enabled;
	bool visible;
	bool separator;
	int toggle_type;	/* 0 none, 1 checkmark, 2 radio */
	int toggle_state;	/* 1 on */
	struct dm_node **children;
	size_t child_count;
};

struct dm_parse {
	int nodes;
};

static void
node_free(struct dm_node *node)
{
	if (!node) {
		return;
	}
	for (size_t i = 0; i < node->child_count; i++) {
		node_free(node->children[i]);
	}
	free((void *)node->children);
	free(node->label);
	free(node);
}

/* The properties of an entry (a{sv}); the ones not used are skipped */
static int
read_properties(sd_bus_message *m, struct dm_node *node)
{
	int r = sd_bus_message_enter_container(m, 'a', "{sv}");
	if (r < 0) {
		return r;
	}

	while ((r = sd_bus_message_enter_container(m, 'e', "sv")) > 0) {
		const char *key = NULL;
		r = sd_bus_message_read(m, "s", &key);
		if (r < 0) {
			return r;
		}

		char type = 0;
		const char *contents = NULL;
		r = sd_bus_message_peek_type(m, &type, &contents);
		if (r < 0) {
			return r;
		}

		if (strcmp(key, "label") == 0 && strcmp(contents, "s") == 0) {
			const char *val = NULL;
			r = sd_bus_message_read(m, "v", "s", &val);
			if (r >= 0 && val) {
				free(node->label);
				node->label = strdup(val);
			}
		} else if (strcmp(key, "enabled") == 0 &&
			   strcmp(contents, "b") == 0) {
			int val = 1;
			r = sd_bus_message_read(m, "v", "b", &val);
			node->enabled = val;
		} else if (strcmp(key, "visible") == 0 &&
			   strcmp(contents, "b") == 0) {
			int val = 1;
			r = sd_bus_message_read(m, "v", "b", &val);
			node->visible = val;
		} else if (strcmp(key, "type") == 0 &&
			   strcmp(contents, "s") == 0) {
			const char *val = NULL;
			r = sd_bus_message_read(m, "v", "s", &val);
			node->separator = r >= 0 && val &&
				strcmp(val, "separator") == 0;
		} else if (strcmp(key, "toggle-type") == 0 &&
			   strcmp(contents, "s") == 0) {
			const char *val = NULL;
			r = sd_bus_message_read(m, "v", "s", &val);
			if (r >= 0 && val) {
				node->toggle_type =
					strcmp(val, "checkmark") == 0 ? 1 :
					strcmp(val, "radio") == 0 ? 2 : 0;
			}
		} else if (strcmp(key, "toggle-state") == 0 &&
			   strcmp(contents, "i") == 0) {
			int32_t val = 0;
			r = sd_bus_message_read(m, "v", "i", &val);
			node->toggle_state = val;
		} else {
			r = sd_bus_message_skip(m, "v");
		}
		if (r < 0) {
			return r;
		}

		r = sd_bus_message_exit_container(m);
		if (r < 0) {
			return r;
		}
	}
	if (r < 0) {
		return r;
	}
	return sd_bus_message_exit_container(m);
}

/* An entry and below: (ia{sv}av) */
static int
read_node(sd_bus_message *m, struct dm_parse *state, int depth,
	struct dm_node **out)
{
	if (depth > DBUSMENU_MAX_DEPTH || ++state->nodes > DBUSMENU_MAX_NODES) {
		return -E2BIG;
	}

	struct dm_node *node = calloc(1, sizeof(*node));
	if (!node) {
		return -ENOMEM;
	}
	node->enabled = true;
	node->visible = true;

	int r = sd_bus_message_enter_container(m, 'r', "ia{sv}av");
	if (r < 0) {
		goto fail;
	}
	r = sd_bus_message_read(m, "i", &node->id);
	if (r < 0) {
		goto fail;
	}
	r = read_properties(m, node);
	if (r < 0) {
		goto fail;
	}

	r = sd_bus_message_enter_container(m, 'a', "v");
	if (r < 0) {
		goto fail;
	}
	while ((r = sd_bus_message_enter_container(m, 'v',
			"(ia{sv}av)")) > 0) {
		struct dm_node *child = NULL;
		r = read_node(m, state, depth + 1, &child);
		if (r < 0) {
			goto fail;
		}
		struct dm_node **grown = (struct dm_node **)realloc(
			(void *)node->children,
			(node->child_count + 1) * sizeof(*grown));
		if (!grown) {
			node_free(child);
			r = -ENOMEM;
			goto fail;
		}
		node->children = grown;
		node->children[node->child_count++] = child;

		r = sd_bus_message_exit_container(m);
		if (r < 0) {
			goto fail;
		}
	}
	if (r < 0) {
		goto fail;
	}
	r = sd_bus_message_exit_container(m);
	if (r < 0) {
		goto fail;
	}
	r = sd_bus_message_exit_container(m);
	if (r < 0) {
		goto fail;
	}

	*out = node;
	return 0;

fail:
	node_free(node);
	return r;
}

/* --- The layout, as a menu --- */

/*
 * A label's mnemonic: "_File" is "File" (the underscore marks the key
 * to press), "__" is one underscore
 */
static char *
plain_label(const char *label)
{
	char *out = strdup(label ? label : "");
	if (!out) {
		return NULL;
	}
	char *w = out;
	for (const char *c = label ? label : ""; *c; c++) {
		if (*c == '_') {
			if (c[1] == '_') {
				*w++ = '_';
				c++;
			}
			continue;
		}
		*w++ = *c;
	}
	*w = '\0';
	return out;
}

static struct wm_menu *
build_menu(struct wm_server *server, const struct dm_node *node,
	const char *title, const char *bus_name, const char *menu_path)
{
	struct wm_menu *menu = menu_create(server, title);
	if (!menu) {
		return NULL;
	}

	for (size_t i = 0; i < node->child_count; i++) {
		const struct dm_node *child = node->children[i];
		if (!child->visible) {
			continue;
		}

		if (child->separator) {
			struct wm_menu_item *sep = menu_item_create(
				WM_MENU_SEPARATOR, NULL);
			if (sep) {
				wl_list_insert(menu->items.prev, &sep->link);
			}
			continue;
		}

		char *label = plain_label(child->label);
		if (!label) {
			continue;
		}

		struct wm_menu_item *item = NULL;
		if (child->child_count > 0) {
			struct wm_menu *sub = build_menu(server, child, label,
				bus_name, menu_path);
			if (sub && wl_list_empty(&sub->items)) {
				wm_menu_destroy(sub);
				sub = NULL;
			}
			if (sub) {
				item = menu_item_create(WM_MENU_SUBMENU,
					label);
				if (item) {
					item->submenu = sub;
					sub->parent = menu;
				} else {
					wm_menu_destroy(sub);
				}
			}
		} else if (child->enabled) {
			item = menu_item_create(WM_MENU_DBUSMENU, label);
			if (item) {
				char spec[512];
				snprintf(spec, sizeof(spec), "%s %s %d",
					bus_name, menu_path, child->id);
				item->command = strdup(spec);
			}
		} else {
			/* Greyed out, and not to be chosen */
			item = menu_item_create(WM_MENU_NOP, label);
		}
		free(label);

		if (!item) {
			continue;
		}
		if (child->toggle_type != 0) {
			item->toggle = true;
			item->checked = child->toggle_state == 1;
		}
		wl_list_insert(menu->items.prev, &item->link);
	}

	return menu;
}

/* --- Fetching it --- */

struct menu_request {
	struct wm_systray *systray;
	sd_bus_slot *slot;
	char *bus_name;
	char *item_path;
	char *menu_path;
	char *title;
	int x, y;
	struct wl_list link;	/* the systray's menu_requests */
};

static void
request_free(struct menu_request *req)
{
	wl_list_remove(&req->link);
	sd_bus_slot_unref(req->slot);
	free(req->bus_name);
	free(req->item_path);
	free(req->menu_path);
	free(req->title);
	free(req);
}

/* The application has no menu to show: let it show its own, if it does */
static void
fall_back_to_context_menu(const struct menu_request *req)
{
	wm_systray_call_item_method(req->systray, req->bus_name,
		req->item_path, "ContextMenu", req->x, req->y);
}

static int
handle_layout(sd_bus_message *reply, void *userdata, sd_bus_error *ret_error)
{
	struct menu_request *req = userdata;
	struct wm_systray *systray = req->systray;
	struct wm_server *server = systray->server;

	struct dm_node *root = NULL;
	struct dm_parse state = { 0 };
	if (!sd_bus_message_is_method_error(reply, NULL)) {
		uint32_t revision = 0;
		if (sd_bus_message_read(reply, "u", &revision) >= 0 &&
		    read_node(reply, &state, 0, &root) < 0) {
			root = NULL;
		}
	}

	struct wm_menu *menu = root ? build_menu(server, root, req->title,
		req->bus_name, req->menu_path) : NULL;
	node_free(root);

	if (!menu || wl_list_empty(&menu->items)) {
		wlr_log(WLR_DEBUG, "systray: no menu from %s at %s",
			req->bus_name, req->menu_path);
		if (menu) {
			wm_menu_destroy(menu);
		}
		fall_back_to_context_menu(req);
		request_free(req);
		return 0;
	}

	/* A tray menu takes the place of whichever other menu is shown */
	wm_menu_hide_all(server);
	server->custom_menu = menu;
	wm_menu_show(menu, req->x, req->y);

	request_free(req);
	return 0;
}

void
wm_systray_menu_open(struct wm_systray *systray, const char *bus_name,
	const char *item_path, const char *menu_path, const char *title,
	int x, int y)
{
	if (!systray || !bus_name || !menu_path || !*menu_path) {
		return;
	}

	struct menu_request *req = calloc(1, sizeof(*req));
	if (!req) {
		return;
	}
	req->systray = systray;
	req->bus_name = strdup(bus_name);
	req->item_path = strdup(item_path ? item_path : "/StatusNotifierItem");
	req->menu_path = strdup(menu_path);
	req->title = strdup(title && *title ? title : "Menu");
	req->x = x;
	req->y = y;
	wl_list_insert(&systray->menu_requests, &req->link);
	if (!req->bus_name || !req->item_path || !req->menu_path ||
	    !req->title) {
		request_free(req);
		return;
	}

	/* The application may fill its menu when it is about to be shown;
	 * messages reach it in order, so it has by the time of GetLayout */
	sd_bus_call_method_async(systray->bus, NULL, bus_name, menu_path,
		DBUSMENU_IFACE, "AboutToShow", NULL, NULL, "i", 0);

	/* Every entry, all the way down, every property */
	sd_bus_message *call = NULL;
	int r = sd_bus_message_new_method_call(systray->bus, &call, bus_name,
		menu_path, DBUSMENU_IFACE, "GetLayout");
	if (r >= 0) {
		r = sd_bus_message_append(call, "ii", 0, -1);
	}
	if (r >= 0) {
		r = sd_bus_message_append_strv(call, (char *[]){ NULL });
	}
	if (r >= 0) {
		r = sd_bus_call_async(systray->bus, &req->slot, call,
			handle_layout, req, 0);
	}
	sd_bus_message_unref(call);
	wm_systray_bus_kick(systray);
	if (r < 0) {
		wlr_log(WLR_DEBUG, "systray: GetLayout to %s failed: %s",
			bus_name, strerror(-r));
		fall_back_to_context_menu(req);
		request_free(req);
	}
}

void
wm_systray_menu_cancel_all(struct wm_systray *systray)
{
	struct menu_request *req, *tmp;
	wl_list_for_each_safe(req, tmp, &systray->menu_requests, link) {
		request_free(req);
	}
}

/* --- Choosing an entry --- */

void
wm_systray_menu_activate(struct wm_server *server, const char *spec)
{
	if (!server || !server->systray || !spec) {
		return;
	}

	/* "bus path id", as build_menu made it */
	char *copy = strdup(spec);
	if (!copy) {
		return;
	}
	char *save = NULL;
	const char *bus_name = strtok_r(copy, " ", &save);
	const char *path = strtok_r(NULL, " ", &save);
	const char *id_str = strtok_r(NULL, " ", &save);
	int id = 0;
	if (!bus_name || !path || !id_str || !safe_atoi(id_str, &id)) {
		free(copy);
		return;
	}

	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	uint32_t time_ms = (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);

	/* The entry is the user's doing: its item may focus its window for it */
	wm_systray_provide_menu_token(server->systray, bus_name, path);

	uint64_t grant = wm_systray_grant_begin(server->systray, bus_name);
	sd_bus_call_method_async(server->systray->bus, NULL, bus_name, path,
		DBUSMENU_IFACE, "Event",
		grant ? wm_systray_grant_reply : NULL,
		grant ? (void *)(uintptr_t)grant : NULL, "isvu",
		(int32_t)id, "clicked", "i", (int32_t)0, time_ms);
	wm_systray_bus_kick(server->systray);
	wlr_log(WLR_DEBUG, "systray: clicked entry %d of %s", id, bus_name);
	free(copy);
}

#endif /* WM_HAS_SYSTRAY */
