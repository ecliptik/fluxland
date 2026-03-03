/*
 * test_focus_nav.c - Unit tests for keyboard focus navigation state machine
 *
 * Tests the focus_nav.c module which manages a state machine for
 * navigating between windows and toolbar elements via keyboard.
 */

#define _GNU_SOURCE

#include "focus_nav.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ipc.h"
#include "server.h"
#include "slit.h"
#include "toolbar.h"
#include "view.h"
#include "workspace.h"

/* --- Stub call counters for verification --- */

static int stub_update_iconbar_count;
static int stub_broadcast_count;
static int stub_switch_next_count;
static int stub_switch_prev_count;
static int stub_focus_next_count;
static int stub_focus_prev_count;
static int stub_focus_view_count;
static char last_broadcast_event[1024];

/* --- Stubs for util.h functions --- */

void wm_spawn_command(const char *cmd) { (void)cmd; }
void wm_json_escape(char *dst, size_t dst_size, const char *src)
{
	if (!src) { if (dst_size > 0) dst[0] = '\0'; return; }
	size_t j = 0;
	for (size_t i = 0; src[i] && j + 6 < dst_size; i++) {
		unsigned char c = (unsigned char)src[i];
		if (c == '"' || c == '\\') { dst[j++] = '\\'; dst[j++] = c; }
		else if (c < 0x20) { continue; }
		else { dst[j++] = c; }
	}
	dst[j] = '\0';
}

/* --- Stubs for functions called by focus_nav.c --- */

void wm_toolbar_update_iconbar(struct wm_toolbar *t)
	{ (void)t; stub_update_iconbar_count++; }
void wm_ipc_broadcast_event(struct wm_ipc_server *ipc,
	enum wm_ipc_event event, const char *json)
	{ (void)ipc; (void)event; stub_broadcast_count++;
	  if (json) snprintf(last_broadcast_event,
		sizeof(last_broadcast_event), "%s", json); }
void wm_workspace_switch_next(struct wm_server *s)
	{ (void)s; stub_switch_next_count++; }
void wm_workspace_switch_prev(struct wm_server *s)
	{ (void)s; stub_switch_prev_count++; }
void wm_focus_next_view(struct wm_server *s)
	{ (void)s; stub_focus_next_count++; }
void wm_focus_prev_view(struct wm_server *s)
	{ (void)s; stub_focus_prev_count++; }
void wm_focus_view(struct wm_view *v, struct wlr_surface *s)
	{ (void)v; (void)s; stub_focus_view_count++; }

/* --- Test helpers --- */

static struct wm_server test_server;
static struct wm_toolbar test_toolbar;
static struct wm_toolbar_tool test_tools[5];

static void
reset_counters(void)
{
	stub_update_iconbar_count = 0;
	stub_broadcast_count = 0;
	stub_switch_next_count = 0;
	stub_switch_prev_count = 0;
	stub_focus_next_count = 0;
	stub_focus_prev_count = 0;
	stub_focus_view_count = 0;
	memset(last_broadcast_event, 0, sizeof(last_broadcast_event));
}

static void
setup(void)
{
	memset(&test_server, 0, sizeof(test_server));
	memset(&test_toolbar, 0, sizeof(test_toolbar));
	memset(test_tools, 0, sizeof(test_tools));

	wl_list_init(&test_server.views);
	wl_list_init(&test_server.workspaces);
	wl_list_init(&test_server.keybindings);
	wl_list_init(&test_server.keymodes);

	/* Default toolbar: 3 tools */
	test_tools[0].type = WM_TOOL_PREV_WORKSPACE;
	test_tools[1].type = WM_TOOL_WORKSPACE_NAME;
	test_tools[2].type = WM_TOOL_NEXT_WORKSPACE;

	test_toolbar.tools = test_tools;
	test_toolbar.tool_count = 3;
	test_toolbar.visible = true;
	test_toolbar.server = &test_server;

	test_server.toolbar = &test_toolbar;

	wm_focus_nav_init(&test_server.focus_nav);
	reset_counters();
}

/* --- Tests --- */

/* Test: init sets zone=WINDOWS, index=-1 */
static void
test_init(void)
{
	struct wm_focus_nav nav;
	memset(&nav, 0xFF, sizeof(nav)); /* poison */
	wm_focus_nav_init(&nav);
	assert(nav.zone == WM_FOCUS_ZONE_WINDOWS);
	assert(nav.toolbar_index == -1);
	assert(nav.slit_index == -1);
	printf("  PASS: test_init\n");
}

/* Test: enter_toolbar switches zone and broadcasts event */
static void
test_enter_toolbar(void)
{
	setup();
	wm_focus_nav_enter_toolbar(&test_server);

	assert(test_server.focus_nav.zone == WM_FOCUS_ZONE_TOOLBAR);
	assert(test_server.focus_nav.toolbar_index == 0);
	assert(stub_update_iconbar_count == 1);
	assert(stub_broadcast_count == 1);
	assert(strstr(last_broadcast_event,
		"\"target\":\"toolbar\"") != NULL);
	assert(strstr(last_broadcast_event,
		"\"element\":\"Previous Workspace\"") != NULL);
	assert(strstr(last_broadcast_event,
		"\"index\":0") != NULL);
	printf("  PASS: test_enter_toolbar\n");
}

/* Test: enter_toolbar guards (NULL, invisible, empty) */
static void
test_enter_toolbar_guards(void)
{
	/* NULL toolbar */
	setup();
	test_server.toolbar = NULL;
	wm_focus_nav_enter_toolbar(&test_server);
	assert(test_server.focus_nav.zone == WM_FOCUS_ZONE_WINDOWS);
	assert(stub_update_iconbar_count == 0);

	/* Invisible toolbar */
	setup();
	test_toolbar.visible = false;
	wm_focus_nav_enter_toolbar(&test_server);
	assert(test_server.focus_nav.zone == WM_FOCUS_ZONE_WINDOWS);

	/* Empty toolbar (0 tools) */
	setup();
	test_toolbar.tool_count = 0;
	wm_focus_nav_enter_toolbar(&test_server);
	assert(test_server.focus_nav.zone == WM_FOCUS_ZONE_WINDOWS);
	printf("  PASS: test_enter_toolbar_guards\n");
}

/* Test: return_to_windows resets zone and index */
static void
test_return_to_windows(void)
{
	setup();
	/* Enter toolbar first */
	test_server.focus_nav.zone = WM_FOCUS_ZONE_TOOLBAR;
	test_server.focus_nav.toolbar_index = 2;

	wm_focus_nav_return_to_windows(&test_server);

	assert(test_server.focus_nav.zone == WM_FOCUS_ZONE_WINDOWS);
	assert(test_server.focus_nav.toolbar_index == -1);
	assert(stub_update_iconbar_count == 1);
	printf("  PASS: test_return_to_windows\n");
}

/* Test: return_to_windows is no-op when already in WINDOWS zone */
static void
test_return_to_windows_noop(void)
{
	setup();
	/* Already in WINDOWS zone */
	wm_focus_nav_return_to_windows(&test_server);
	assert(stub_update_iconbar_count == 0);
	assert(stub_broadcast_count == 0);
	printf("  PASS: test_return_to_windows_noop\n");
}

/* Test: return_to_windows broadcasts focused view info */
static void
test_return_to_windows_with_view(void)
{
	setup();
	test_server.focus_nav.zone = WM_FOCUS_ZONE_TOOLBAR;

	struct wm_view mock_view;
	memset(&mock_view, 0, sizeof(mock_view));
	mock_view.app_id = "test-app";
	mock_view.title = "Test Window";
	mock_view.id = 42;
	test_server.focused_view = &mock_view;

	wm_focus_nav_return_to_windows(&test_server);

	assert(stub_broadcast_count == 1);
	assert(strstr(last_broadcast_event,
		"\"target\":\"window\"") != NULL);
	assert(strstr(last_broadcast_event, "test-app") != NULL);
	assert(strstr(last_broadcast_event, "Test Window") != NULL);
	assert(strstr(last_broadcast_event, "\"id\":42") != NULL);
	printf("  PASS: test_return_to_windows_with_view\n");
}

/* Test: next_element increments index and wraps */
static void
test_next_element(void)
{
	setup();
	wm_focus_nav_enter_toolbar(&test_server);
	reset_counters();

	/* Move from 0 to 1 */
	wm_focus_nav_next_element(&test_server);
	assert(test_server.focus_nav.toolbar_index == 1);
	assert(stub_update_iconbar_count == 1);
	assert(stub_broadcast_count == 1);
	assert(strstr(last_broadcast_event,
		"\"element\":\"Workspace Name\"") != NULL);

	/* Move from 1 to 2 */
	wm_focus_nav_next_element(&test_server);
	assert(test_server.focus_nav.toolbar_index == 2);
	assert(strstr(last_broadcast_event,
		"\"element\":\"Next Workspace\"") != NULL);

	/* Wrap from 2 to 0 */
	wm_focus_nav_next_element(&test_server);
	assert(test_server.focus_nav.toolbar_index == 0);
	printf("  PASS: test_next_element\n");
}

/* Test: prev_element decrements index and wraps */
static void
test_prev_element(void)
{
	setup();
	wm_focus_nav_enter_toolbar(&test_server);
	reset_counters();

	/* Wrap from 0 to 2 (last) */
	wm_focus_nav_prev_element(&test_server);
	assert(test_server.focus_nav.toolbar_index == 2);
	assert(stub_update_iconbar_count == 1);

	/* Move from 2 to 1 */
	wm_focus_nav_prev_element(&test_server);
	assert(test_server.focus_nav.toolbar_index == 1);

	/* Move from 1 to 0 */
	wm_focus_nav_prev_element(&test_server);
	assert(test_server.focus_nav.toolbar_index == 0);
	printf("  PASS: test_prev_element\n");
}

/* Test: next/prev element no-op when not in toolbar zone */
static void
test_next_prev_not_in_toolbar(void)
{
	setup();
	/* Zone is WINDOWS */
	wm_focus_nav_next_element(&test_server);
	assert(test_server.focus_nav.toolbar_index == -1);
	assert(stub_update_iconbar_count == 0);

	wm_focus_nav_prev_element(&test_server);
	assert(test_server.focus_nav.toolbar_index == -1);
	assert(stub_update_iconbar_count == 0);
	printf("  PASS: test_next_prev_not_in_toolbar\n");
}

/* Test: activate dispatches correct action for workspace tools */
static void
test_activate_workspace_tools(void)
{
	setup();
	wm_focus_nav_enter_toolbar(&test_server);
	reset_counters();

	/* Index 0 = PREV_WORKSPACE -> calls switch_prev */
	wm_focus_nav_activate(&test_server);
	assert(stub_switch_prev_count == 1);
	assert(stub_switch_next_count == 0);

	/* Move to index 2 = NEXT_WORKSPACE -> calls switch_next */
	test_server.focus_nav.toolbar_index = 2;
	wm_focus_nav_activate(&test_server);
	assert(stub_switch_next_count == 1);

	/* Move to index 1 = WORKSPACE_NAME -> calls switch_next */
	test_server.focus_nav.toolbar_index = 1;
	reset_counters();
	wm_focus_nav_activate(&test_server);
	assert(stub_switch_next_count == 1);
	printf("  PASS: test_activate_workspace_tools\n");
}

/* Test: activate CLOCK tool is a no-op */
static void
test_activate_clock_noop(void)
{
	setup();
	test_tools[0].type = WM_TOOL_CLOCK;
	wm_focus_nav_enter_toolbar(&test_server);
	reset_counters();

	wm_focus_nav_activate(&test_server);
	assert(stub_switch_next_count == 0);
	assert(stub_switch_prev_count == 0);
	assert(stub_focus_next_count == 0);
	assert(stub_focus_prev_count == 0);
	assert(stub_focus_view_count == 0);
	printf("  PASS: test_activate_clock_noop\n");
}

/* Test: activate PREV/NEXT_WINDOW and return to windows */
static void
test_activate_window_tools(void)
{
	setup();
	test_tools[0].type = WM_TOOL_PREV_WINDOW;
	wm_focus_nav_enter_toolbar(&test_server);
	reset_counters();

	wm_focus_nav_activate(&test_server);
	assert(stub_focus_prev_count == 1);
	/* PREV_WINDOW returns to windows zone */
	assert(test_server.focus_nav.zone == WM_FOCUS_ZONE_WINDOWS);

	/* Test NEXT_WINDOW */
	setup();
	test_tools[0].type = WM_TOOL_NEXT_WINDOW;
	wm_focus_nav_enter_toolbar(&test_server);
	reset_counters();

	wm_focus_nav_activate(&test_server);
	assert(stub_focus_next_count == 1);
	assert(test_server.focus_nav.zone == WM_FOCUS_ZONE_WINDOWS);
	printf("  PASS: test_activate_window_tools\n");
}

/* Test: activate is no-op when not in toolbar zone */
static void
test_activate_not_in_toolbar(void)
{
	setup();
	/* Zone is WINDOWS */
	wm_focus_nav_activate(&test_server);
	assert(stub_switch_prev_count == 0);
	assert(stub_switch_next_count == 0);
	assert(stub_focus_next_count == 0);
	assert(stub_focus_prev_count == 0);
	printf("  PASS: test_activate_not_in_toolbar\n");
}

/* Test: get_toolbar_index returns correct values */
static void
test_get_toolbar_index(void)
{
	setup();
	/* In WINDOWS zone -> -1 */
	int idx = wm_focus_nav_get_toolbar_index(&test_server);
	assert(idx == -1);

	/* Enter toolbar -> 0 */
	wm_focus_nav_enter_toolbar(&test_server);
	idx = wm_focus_nav_get_toolbar_index(&test_server);
	assert(idx == 0);

	/* Move next -> 1 */
	wm_focus_nav_next_element(&test_server);
	idx = wm_focus_nav_get_toolbar_index(&test_server);
	assert(idx == 1);

	/* Move next -> 2 */
	wm_focus_nav_next_element(&test_server);
	idx = wm_focus_nav_get_toolbar_index(&test_server);
	assert(idx == 2);
	printf("  PASS: test_get_toolbar_index\n");
}

/* --- Slit test helpers --- */

static struct wm_slit test_slit;
static struct wm_slit_client test_slit_clients[3];

static void
setup_slit(void)
{
	setup();
	memset(&test_slit, 0, sizeof(test_slit));
	memset(test_slit_clients, 0, sizeof(test_slit_clients));

	test_slit.server = &test_server;
	test_slit.client_count = 3;
	wl_list_init(&test_slit.clients);

	for (int i = 0; i < 3; i++) {
		test_slit_clients[i].slit = &test_slit;
		wl_list_insert(test_slit.clients.prev,
			&test_slit_clients[i].link);
	}

	test_server.slit = &test_slit;
}

/* Test: enter_slit switches zone and sets index=0 */
static void
test_enter_slit(void)
{
	setup_slit();
	wm_focus_nav_enter_slit(&test_server);

	assert(test_server.focus_nav.zone == WM_FOCUS_ZONE_SLIT);
	assert(test_server.focus_nav.slit_index == 0);
	assert(stub_broadcast_count == 1);
	assert(strstr(last_broadcast_event,
		"\"target\":\"slit\"") != NULL);
	assert(strstr(last_broadcast_event,
		"\"index\":0") != NULL);
	printf("  PASS: test_enter_slit\n");
}

/* Test: enter_slit guards (NULL slit, empty slit) */
static void
test_enter_slit_guards(void)
{
	/* NULL slit */
	setup();
	test_server.slit = NULL;
	wm_focus_nav_enter_slit(&test_server);
	assert(test_server.focus_nav.zone == WM_FOCUS_ZONE_WINDOWS);
	assert(stub_broadcast_count == 0);

	/* Empty slit (0 clients) */
	setup();
	memset(&test_slit, 0, sizeof(test_slit));
	test_slit.client_count = 0;
	test_server.slit = &test_slit;
	wm_focus_nav_enter_slit(&test_server);
	assert(test_server.focus_nav.zone == WM_FOCUS_ZONE_WINDOWS);
	assert(stub_broadcast_count == 0);
	printf("  PASS: test_enter_slit_guards\n");
}

/* Test: return_from_slit resets zone to WINDOWS */
static void
test_return_from_slit(void)
{
	setup_slit();
	wm_focus_nav_enter_slit(&test_server);
	reset_counters();

	wm_focus_nav_return_from_slit(&test_server);

	assert(test_server.focus_nav.zone == WM_FOCUS_ZONE_WINDOWS);
	assert(test_server.focus_nav.slit_index == -1);
	assert(stub_broadcast_count == 1);
	assert(strstr(last_broadcast_event,
		"\"target\":\"windows\"") != NULL);
	printf("  PASS: test_return_from_slit\n");
}

/* Test: slit_next cycles through clients and wraps */
static void
test_slit_next(void)
{
	setup_slit();
	wm_focus_nav_enter_slit(&test_server);
	reset_counters();

	/* 0 -> 1 */
	wm_focus_nav_slit_next(&test_server);
	assert(test_server.focus_nav.slit_index == 1);
	assert(stub_broadcast_count == 1);

	/* 1 -> 2 */
	wm_focus_nav_slit_next(&test_server);
	assert(test_server.focus_nav.slit_index == 2);

	/* 2 -> 0 (wrap) */
	wm_focus_nav_slit_next(&test_server);
	assert(test_server.focus_nav.slit_index == 0);
	printf("  PASS: test_slit_next\n");
}

/* Test: slit_prev cycles backward and wraps */
static void
test_slit_prev(void)
{
	setup_slit();
	wm_focus_nav_enter_slit(&test_server);
	reset_counters();

	/* 0 -> 2 (wrap backward) */
	wm_focus_nav_slit_prev(&test_server);
	assert(test_server.focus_nav.slit_index == 2);
	assert(stub_broadcast_count == 1);

	/* 2 -> 1 */
	wm_focus_nav_slit_prev(&test_server);
	assert(test_server.focus_nav.slit_index == 1);

	/* 1 -> 0 */
	wm_focus_nav_slit_prev(&test_server);
	assert(test_server.focus_nav.slit_index == 0);
	printf("  PASS: test_slit_prev\n");
}

/* Test: slit_next/prev no-op when not in slit zone */
static void
test_slit_nav_not_in_slit(void)
{
	setup_slit();
	/* Zone is WINDOWS, not SLIT */
	wm_focus_nav_slit_next(&test_server);
	assert(test_server.focus_nav.slit_index == -1);
	assert(stub_broadcast_count == 0);

	wm_focus_nav_slit_prev(&test_server);
	assert(test_server.focus_nav.slit_index == -1);
	assert(stub_broadcast_count == 0);
	printf("  PASS: test_slit_nav_not_in_slit\n");
}

int
main(void)
{
	printf("test_focus_nav:\n");

	test_init();
	test_enter_toolbar();
	test_enter_toolbar_guards();
	test_return_to_windows();
	test_return_to_windows_noop();
	test_return_to_windows_with_view();
	test_next_element();
	test_prev_element();
	test_next_prev_not_in_toolbar();
	test_activate_workspace_tools();
	test_activate_clock_noop();
	test_activate_window_tools();
	test_activate_not_in_toolbar();
	test_get_toolbar_index();

	/* Slit navigation tests */
	test_enter_slit();
	test_enter_slit_guards();
	test_return_from_slit();
	test_slit_next();
	test_slit_prev();
	test_slit_nav_not_in_slit();

	printf("All focus_nav tests passed.\n");
	return 0;
}
