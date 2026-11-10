/*
 * test_systray_menu.c - Unit tests for a tray icon's menu (dbusmenu)
 *
 * systray_menu.c is #included with stand-ins for the menu and server
 * (what it builds and shows is what is looked at) and for the one sd-bus
 * call that sends an entry's click.  libsystemd itself is linked, but no
 * bus is needed: the layout is built by hand.
 */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef WM_HAS_SYSTRAY
#define WM_HAS_SYSTRAY
#endif

#include <systemd/sd-bus.h>
#include <wayland-server-core.h>

/* --- Stand-ins for what systray_menu.c uses --- */

#define WLR_UTIL_LOG_H
#define WM_SERVER_H
#define WM_MENU_H
#define WM_MENU_PARSE_H
#define WM_SYSTRAY_H
#define WM_SYSTRAY_MENU_H
#define WM_UTIL_H
#define WLR_DEBUG 7
#define wlr_log(verbosity, ...) ((void)0)

enum wm_menu_item_type {
	WM_MENU_EXEC,
	WM_MENU_SUBMENU,
	WM_MENU_SEPARATOR,
	WM_MENU_NOP,
	WM_MENU_DBUSMENU,
};

struct wm_menu_item {
	enum wm_menu_item_type type;
	char *label;
	char *command;
	struct wm_menu *submenu;
	bool checked;
	bool toggle;
	struct wl_list link;
};

struct wm_menu {
	char *title;
	struct wl_list items;
	struct wm_menu *parent;
	struct wm_server *server;
};

struct wm_systray {
	struct wm_server *server;
	sd_bus *bus;
	struct wl_list menu_requests;
};

struct wm_server {
	struct wm_systray *systray;
	struct wm_menu *custom_menu;
};

static struct wm_menu *
menu_create(struct wm_server *server, const char *title)
{
	struct wm_menu *menu = calloc(1, sizeof(*menu));
	assert(menu);
	menu->title = title ? strdup(title) : NULL;
	menu->server = server;
	wl_list_init(&menu->items);
	return menu;
}

static struct wm_menu_item *
menu_item_create(enum wm_menu_item_type type, const char *label)
{
	struct wm_menu_item *item = calloc(1, sizeof(*item));
	assert(item);
	item->type = type;
	item->label = label ? strdup(label) : NULL;
	wl_list_init(&item->link);
	return item;
}

static void
wm_menu_destroy(struct wm_menu *menu)
{
	if (!menu) {
		return;
	}
	struct wm_menu_item *item, *tmp;
	wl_list_for_each_safe(item, tmp, &menu->items, link) {
		wl_list_remove(&item->link);
		wm_menu_destroy(item->submenu);
		free(item->label);
		free(item->command);
		free(item);
	}
	free(menu->title);
	free(menu);
}

static void wm_menu_hide_all(struct wm_server *server) { (void)server; }
static void wm_menu_show(struct wm_menu *menu, int x, int y)
	{ (void)menu; (void)x; (void)y; }
static void
wm_systray_call_item_method(struct wm_systray *systray, const char *bus_name,
	const char *object_path, const char *method, int x, int y)
{
	(void)systray; (void)bus_name; (void)object_path; (void)method;
	(void)x; (void)y;
}
static bool
safe_atoi(const char *str, int *out)
{
	char *end = NULL;
	long v = strtol(str, &end, 10);
	if (!str[0] || *end) {
		return false;
	}
	*out = (int)v;
	return true;
}

/* The click of an entry is the one sd-bus call these tests watch */
static int g_event_calls;
static char g_event_dest[128];
static char g_event_path[128];
static char g_event_member[32];
static int32_t g_event_id;

int
sd_bus_call_method_async(sd_bus *bus, sd_bus_slot **slot,
	const char *destination, const char *path, const char *interface,
	const char *member, sd_bus_message_handler_t callback, void *userdata,
	const char *types, ...)
{
	(void)bus; (void)slot; (void)interface; (void)callback; (void)userdata;
	snprintf(g_event_dest, sizeof(g_event_dest), "%s", destination);
	snprintf(g_event_path, sizeof(g_event_path), "%s", path);
	snprintf(g_event_member, sizeof(g_event_member), "%s", member);
	if (strcmp(member, "Event") == 0 && strncmp(types, "is", 2) == 0) {
		va_list ap;
		va_start(ap, types);
		g_event_id = va_arg(ap, int32_t);
		va_end(ap);
	}
	g_event_calls++;
	return 0;
}

void wm_systray_menu_open(struct wm_systray *systray, const char *bus_name,
	const char *item_path, const char *menu_path, const char *title,
	int x, int y);
void wm_systray_menu_activate(struct wm_server *server, const char *spec);
void wm_systray_menu_cancel_all(struct wm_systray *systray);
uint64_t wm_systray_grant_begin(struct wm_systray *systray,
	const char *bus_name);
int wm_systray_grant_reply(sd_bus_message *reply, void *userdata,
	sd_bus_error *ret_error);
uint64_t wm_systray_grant_begin(struct wm_systray *systray,
	const char *bus_name)
{ (void)systray; (void)bus_name; return 0; }
int wm_systray_grant_reply(sd_bus_message *reply, void *userdata,
	sd_bus_error *ret_error)
{ (void)reply; (void)userdata; (void)ret_error; return 0; }
static int g_menu_tokens;
void wm_systray_provide_menu_token(struct wm_systray *systray,
	const char *bus_name, const char *menu_path)
{ (void)systray; (void)bus_name; (void)menu_path; g_menu_tokens++; }
static int g_bus_kicks;
void wm_systray_bus_kick(struct wm_systray *systray)
{ (void)systray; g_bus_kicks++; }

#include "../src/systray_menu.c"

/* --- Helpers --- */

static struct dm_node *
node(int id, const char *label)
{
	struct dm_node *n = calloc(1, sizeof(*n));
	assert(n);
	n->id = id;
	n->label = label ? strdup(label) : NULL;
	n->enabled = true;
	n->visible = true;
	return n;
}

static void
add_child(struct dm_node *parent, struct dm_node *child)
{
	parent->children = realloc((void *)parent->children,
		(parent->child_count + 1) * sizeof(*parent->children));
	assert(parent->children);
	parent->children[parent->child_count++] = child;
}

static struct wm_menu_item *
item_at(struct wm_menu *menu, int index)
{
	int i = 0;
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		if (i++ == index) {
			return item;
		}
	}
	return NULL;
}

static int
item_count(struct wm_menu *menu)
{
	int n = 0;
	struct wm_menu_item *item;
	wl_list_for_each(item, &menu->items, link) {
		n++;
	}
	return n;
}

/* --- Tests --- */

static void
test_plain_label(void)
{
	char *s;

	s = plain_label("_File");
	assert(strcmp(s, "File") == 0);
	free(s);

	s = plain_label("Save _As...");
	assert(strcmp(s, "Save As...") == 0);
	free(s);

	/* A doubled underscore is one underscore of the label */
	s = plain_label("snake__case");
	assert(strcmp(s, "snake_case") == 0);
	free(s);

	s = plain_label("");
	assert(strcmp(s, "") == 0);
	free(s);

	s = plain_label(NULL);
	assert(strcmp(s, "") == 0);
	free(s);

	printf("  PASS: test_plain_label\n");
}

static void
test_build_menu_entries(void)
{
	struct wm_server server = { 0 };
	struct dm_node *root = node(0, NULL);

	struct dm_node *open = node(1, "_Open");
	add_child(root, open);

	struct dm_node *sep = node(2, NULL);
	sep->separator = true;
	add_child(root, sep);

	struct dm_node *check = node(3, "Checked");
	check->toggle_type = 1;
	check->toggle_state = 1;
	add_child(root, check);

	struct dm_node *unchecked = node(4, "Unchecked");
	unchecked->toggle_type = 2;
	unchecked->toggle_state = 0;
	add_child(root, unchecked);

	struct dm_node *disabled = node(5, "Greyed");
	disabled->enabled = false;
	add_child(root, disabled);

	struct dm_node *hidden = node(6, "Hidden");
	hidden->visible = false;
	add_child(root, hidden);

	struct dm_node *sub = node(7, "More");
	add_child(sub, node(8, "Inner"));
	add_child(root, sub);

	/* A submenu with nothing visible in it is no submenu */
	struct dm_node *empty = node(9, "Empty");
	struct dm_node *gone = node(10, "Gone");
	gone->visible = false;
	add_child(empty, gone);
	add_child(root, empty);

	struct wm_menu *menu = build_menu(&server, root, "Title", ":1.5",
		"/MenuBar");
	assert(menu);
	assert(strcmp(menu->title, "Title") == 0);
	assert(item_count(menu) == 6);

	struct wm_menu_item *it = item_at(menu, 0);
	assert(it->type == WM_MENU_DBUSMENU);
	assert(strcmp(it->label, "Open") == 0);	/* mnemonic gone */
	assert(strcmp(it->command, ":1.5 /MenuBar 1") == 0);
	assert(!it->toggle);

	assert(item_at(menu, 1)->type == WM_MENU_SEPARATOR);

	it = item_at(menu, 2);
	assert(it->type == WM_MENU_DBUSMENU && it->toggle && it->checked);
	it = item_at(menu, 3);
	assert(it->type == WM_MENU_DBUSMENU && it->toggle && !it->checked);

	/* Greyed out: inert, and nothing to send */
	it = item_at(menu, 4);
	assert(it->type == WM_MENU_NOP);
	assert(strcmp(it->label, "Greyed") == 0);
	assert(it->command == NULL);

	it = item_at(menu, 5);
	assert(it->type == WM_MENU_SUBMENU);
	assert(it->submenu && it->submenu->parent == menu);
	assert(strcmp(it->submenu->title, "More") == 0);
	assert(item_count(it->submenu) == 1);
	assert(strcmp(item_at(it->submenu, 0)->command, ":1.5 /MenuBar 8") == 0);

	wm_menu_destroy(menu);
	node_free(root);
	printf("  PASS: test_build_menu_entries\n");
}

static void
test_build_menu_empty(void)
{
	struct wm_server server = { 0 };
	struct dm_node *root = node(0, NULL);

	struct wm_menu *menu = build_menu(&server, root, "T", ":1.5", "/m");
	assert(menu);
	assert(item_count(menu) == 0);

	wm_menu_destroy(menu);
	node_free(root);
	printf("  PASS: test_build_menu_empty\n");
}

static void
test_activate_sends_event(void)
{
	struct wm_systray systray = { 0 };
	struct wm_server server = { .systray = &systray };

	g_event_calls = 0;
	g_menu_tokens = 0;
	wm_systray_menu_activate(&server, ":1.77 /com/canonical/dbusmenu 42");
	assert(g_event_calls == 1);
	/* the item is given the user's token for the entry chosen */
	assert(g_menu_tokens == 1);
	assert(strcmp(g_event_dest, ":1.77") == 0);
	assert(strcmp(g_event_path, "/com/canonical/dbusmenu") == 0);
	assert(strcmp(g_event_member, "Event") == 0);
	assert(g_event_id == 42);

	/* Not a spec we made: nothing is sent */
	wm_systray_menu_activate(&server, "garbage");
	wm_systray_menu_activate(&server, ":1.77 /path notanumber");
	wm_systray_menu_activate(&server, ":1.77 /path");
	wm_systray_menu_activate(&server, NULL);
	wm_systray_menu_activate(NULL, ":1.77 /path 1");
	assert(g_event_calls == 1);
	assert(g_menu_tokens == 1);

	/* No tray, no click */
	struct wm_server no_tray = { 0 };
	wm_systray_menu_activate(&no_tray, ":1.77 /path 1");
	assert(g_event_calls == 1);

	printf("  PASS: test_activate_sends_event\n");
}

int
main(void)
{
	printf("test_systray_menu:\n");

	test_plain_label();
	test_build_menu_entries();
	test_build_menu_empty();
	test_activate_sends_event();

	printf("All systray menu tests passed.\n");
	return 0;
}
