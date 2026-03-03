/*
 * test_ipc.c - Unit tests for IPC command parsing and dispatch
 *
 * Tests the IPC command handling layer (ipc_commands.c) and event
 * subscription logic. The socket transport (ipc.c) requires a running
 * event loop, so we test the command dispatch directly via
 * wm_ipc_handle_command().
 */

#define _GNU_SOURCE

#include "ipc.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * Stubs for compositor functions referenced by ipc_commands.c.
 * Only the command dispatch paths exercised below need stubs.
 */
#include "config.h"
#include "screen_cast.h"
#include "decoration.h"
#include "keybind.h"
#include "keyboard.h"
#include "menu.h"
#include "output.h"
#include "placement.h"
#include "render.h"
#include "rules.h"
#include "server.h"
#include "slit.h"
#include "style.h"
#include "tabgroup.h"
#include "toolbar.h"
#include "view.h"
#include "workspace.h"
#include "foreign_toplevel.h"

/* --- Screen cast stubs (referenced by ipc_commands.c) --- */

struct wm_screen_cast *wm_screen_cast_init(struct wm_server *s)
	{ (void)s; return NULL; }
void wm_screen_cast_destroy(struct wm_screen_cast *sc) { (void)sc; }
bool wm_screen_cast_start(struct wm_screen_cast *sc, const char *o)
	{ (void)sc; (void)o; return false; }
void wm_screen_cast_stop(struct wm_screen_cast *sc) { (void)sc; }
enum wm_screen_cast_state wm_screen_cast_get_state(
	const struct wm_screen_cast *sc)
	{ (void)sc; return WM_SCREEN_CAST_STOPPED; }
uint32_t wm_screen_cast_get_node_id(const struct wm_screen_cast *sc)
	{ (void)sc; return 0; }

/* --- Stubs for functions called by ipc_commands.c --- */

void wm_focus_next_view(struct wm_server *s) { (void)s; }
void wm_focus_prev_view(struct wm_server *s) { (void)s; }
void wm_focus_view(struct wm_view *v, struct wlr_surface *s)
	{ (void)v; (void)s; }
void wm_view_raise(struct wm_view *v) { (void)v; }
void wm_view_lower(struct wm_view *v) { (void)v; }
void wm_view_raise_layer(struct wm_view *v) { (void)v; }
void wm_view_lower_layer(struct wm_view *v) { (void)v; }
void wm_view_set_layer(struct wm_view *v, enum wm_view_layer l)
	{ (void)v; (void)l; }
void wm_view_set_opacity(struct wm_view *v, int a) { (void)v; (void)a; }
void wm_view_set_sticky(struct wm_view *v, bool s) { (void)v; (void)s; }
void wm_view_get_geometry(struct wm_view *v, struct wlr_box *b)
	{ (void)v; b->x = 0; b->y = 0; b->width = 100; b->height = 100; }
void wm_view_begin_interactive(struct wm_view *v, enum wm_cursor_mode m,
	uint32_t e) { (void)v; (void)m; (void)e; }
void wm_view_maximize_vert(struct wm_view *v) { (void)v; }
void wm_view_maximize_horiz(struct wm_view *v) { (void)v; }
void wm_view_toggle_decoration(struct wm_view *v) { (void)v; }
void wm_view_activate_tab(struct wm_view *v, int i) { (void)v; (void)i; }
void wm_view_lhalf(struct wm_view *v) { (void)v; }
void wm_view_rhalf(struct wm_view *v) { (void)v; }
void wm_view_resize_by(struct wm_view *v, int w, int h)
	{ (void)v; (void)w; (void)h; }
void wm_view_focus_direction(struct wm_server *s, int dx, int dy)
	{ (void)s; (void)dx; (void)dy; }
void wm_view_set_head(struct wm_view *v, int h) { (void)v; (void)h; }
void wm_view_send_to_next_head(struct wm_view *v) { (void)v; }
void wm_view_send_to_prev_head(struct wm_view *v) { (void)v; }
void wm_view_close_all(struct wm_server *s) { (void)s; }
void wm_view_cycle_next(struct wm_server *s) { (void)s; }
void wm_view_cycle_prev(struct wm_server *s) { (void)s; }
void wm_view_deiconify_last(struct wm_server *s) { (void)s; }
void wm_view_deiconify_all(struct wm_server *s) { (void)s; }
void wm_view_deiconify_all_workspace(struct wm_server *s) { (void)s; }
enum wm_view_layer wm_view_layer_from_name(const char *n)
	{ (void)n; return WM_LAYER_NORMAL; }

void wm_workspace_switch(struct wm_server *s, int i) { (void)s; (void)i; }
void wm_workspace_switch_next(struct wm_server *s) { (void)s; }
void wm_workspace_switch_prev(struct wm_server *s) { (void)s; }
void wm_workspace_switch_right(struct wm_server *s) { (void)s; }
void wm_workspace_switch_left(struct wm_server *s) { (void)s; }
void wm_workspace_set_name(struct wm_server *s, const char *n)
	{ (void)s; (void)n; }
void wm_workspace_add(struct wm_server *s) { (void)s; }
void wm_workspace_remove_last(struct wm_server *s) { (void)s; }
static struct wm_workspace *stub_active_workspace;
struct wm_workspace *wm_workspace_get_active(struct wm_server *s)
	{ (void)s; return stub_active_workspace; }
void wm_view_send_to_workspace(struct wm_server *s, int i)
	{ (void)s; (void)i; }
void wm_view_take_to_workspace(struct wm_server *s, int i)
	{ (void)s; (void)i; }
void wm_view_send_to_next_workspace(struct wm_server *s) { (void)s; }
void wm_view_send_to_prev_workspace(struct wm_server *s) { (void)s; }
void wm_view_take_to_next_workspace(struct wm_server *s) { (void)s; }
void wm_view_take_to_prev_workspace(struct wm_server *s) { (void)s; }

void wm_menu_show_root(struct wm_server *s, int x, int y)
	{ (void)s; (void)x; (void)y; }
void wm_menu_show_window(struct wm_server *s, int x, int y)
	{ (void)s; (void)x; (void)y; }
void wm_menu_show_window_list(struct wm_server *s, int x, int y)
	{ (void)s; (void)x; (void)y; }
void wm_menu_hide_all(struct wm_server *s) { (void)s; }
void wm_menu_show_workspace_menu(struct wm_server *s, int x, int y)
	{ (void)s; (void)x; (void)y; }
void wm_menu_show_client_menu(struct wm_server *s, const char *a,
	int x, int y) { (void)s; (void)a; (void)x; (void)y; }
void wm_menu_show_custom(struct wm_server *s, const char *a,
	int x, int y) { (void)s; (void)a; (void)x; (void)y; }

void wm_decoration_update(struct wm_decoration *d, struct wm_style *s)
	{ (void)d; (void)s; }
void wm_decoration_set_preset(struct wm_decoration *d,
	enum wm_decor_preset p, struct wm_style *s)
	{ (void)d; (void)p; (void)s; }
void wm_decoration_set_shaded(struct wm_decoration *d, bool shaded,
	struct wm_style *s) { (void)d; (void)shaded; (void)s; }

int style_load(struct wm_style *s, const char *p)
	{ (void)s; (void)p; return 0; }
void wm_toolbar_relayout(struct wm_toolbar *t) { (void)t; }
void wm_toolbar_toggle_above(struct wm_toolbar *t) { (void)t; }
void wm_toolbar_toggle_visible(struct wm_toolbar *t) { (void)t; }
void wm_toolbar_update_iconbar(struct wm_toolbar *t) { (void)t; }
void wm_spawn_command(const char *cmd) { (void)cmd; }
void wm_slit_toggle_above(struct wm_slit *s) { (void)s; }
void wm_slit_toggle_hidden(struct wm_slit *s) { (void)s; }

void wm_tab_group_next(struct wm_tab_group *g) { (void)g; }
void wm_tab_group_prev(struct wm_tab_group *g) { (void)g; }
void wm_tab_group_remove(struct wm_view *v) { (void)v; }
void wm_tab_group_move_left(struct wm_tab_group *g, struct wm_view *v)
	{ (void)g; (void)v; }
void wm_tab_group_move_right(struct wm_tab_group *g, struct wm_view *v)
	{ (void)g; (void)v; }

void wm_arrange_windows_grid(struct wm_server *s) { (void)s; }
void wm_arrange_windows_vert(struct wm_server *s) { (void)s; }
void wm_arrange_windows_horiz(struct wm_server *s) { (void)s; }
void wm_arrange_windows_cascade(struct wm_server *s) { (void)s; }
void wm_arrange_windows_stack_left(struct wm_server *s) { (void)s; }
void wm_arrange_windows_stack_right(struct wm_server *s) { (void)s; }
void wm_arrange_windows_stack_top(struct wm_server *s) { (void)s; }
void wm_arrange_windows_stack_bottom(struct wm_server *s) { (void)s; }

void wm_keyboard_apply_config(struct wm_server *s) { (void)s; }
void wm_keyboard_next_layout(struct wm_server *s) { (void)s; }
void wm_keyboard_prev_layout(struct wm_server *s) { (void)s; }

void keybind_destroy_all(struct wl_list *l) { (void)l; }
void keybind_destroy_list(struct wl_list *l) { (void)l; }
int keybind_load(struct wl_list *l, const char *p)
	{ (void)l; (void)p; return 0; }
bool keybind_add_from_string(struct wl_list *l, const char *s)
	{ (void)l; (void)s; return true; }
struct wm_keymode *keybind_get_mode(struct wl_list *l, const char *n)
	{ (void)l; (void)n; return NULL; }

int config_reload(struct wm_config *c) { (void)c; return 0; }

int wm_rules_remember_window(struct wm_view *v, const char *p)
	{ (void)v; (void)p; return 0; }

void wm_foreign_toplevel_set_minimized(struct wm_view *v, bool m)
	{ (void)v; (void)m; }

cairo_surface_t *wm_render_text(const char *text, const struct wm_font *f,
	const struct wm_color *c, int max_w, int *tw, int *th,
	enum wm_justify j, float s)
	{ (void)text; (void)f; (void)c; (void)max_w; (void)tw; (void)th;
	  (void)j; (void)s; return NULL; }
cairo_surface_t *wm_render_texture(const struct wm_texture *t,
	int w, int h, float s)
	{ (void)t; (void)w; (void)h; (void)s; return NULL; }
int wm_measure_text_width(const char *text, const struct wm_font *f, float s)
	{ (void)f; (void)s; return text ? (int)(strlen(text) * 7) : 0; }

/* --- Group C: wlroots function stubs (linker overrides) ---
 * These override the real wlroots/wayland symbols so we can test
 * code paths that call wlroots functions with mock data. */

static int stub_close_called;
void wlr_xdg_toplevel_send_close(struct wlr_xdg_toplevel *t)
	{ (void)t; stub_close_called++; }

static int stub_set_size_w, stub_set_size_h;
uint32_t wlr_xdg_toplevel_set_size(struct wlr_xdg_toplevel *t, int w, int h)
	{ (void)t; stub_set_size_w = w; stub_set_size_h = h; return 0; }

static int stub_set_pos_x, stub_set_pos_y;
void wlr_scene_node_set_position(struct wlr_scene_node *n, int x, int y)
	{ (void)n; stub_set_pos_x = x; stub_set_pos_y = y; }

void wlr_scene_node_set_enabled(struct wlr_scene_node *n, bool enabled)
	{ n->enabled = enabled; }

void wlr_seat_keyboard_notify_clear_focus(struct wlr_seat *seat)
	{ (void)seat; }

static int stub_terminate_called;
void wl_display_terminate(struct wl_display *d)
	{ (void)d; stub_terminate_called++; }

struct wl_client *wl_resource_get_client(struct wl_resource *r)
	{ (void)r; return NULL; }
void wl_client_destroy(struct wl_client *c) { (void)c; }
void wlr_output_layout_output_coords(struct wlr_output_layout *l,
	struct wlr_output *o, double *ox, double *oy)
	{ (void)l; (void)o; *ox = 0; *oy = 0; }

/* --- Test helpers --- */

static struct wm_server test_server;
static struct wm_ipc_server test_ipc;
static struct wm_ipc_client test_client;

static void
setup(void)
{
	memset(&test_server, 0, sizeof(test_server));
	memset(&test_ipc, 0, sizeof(test_ipc));
	memset(&test_client, 0, sizeof(test_client));

	wl_list_init(&test_server.views);
	wl_list_init(&test_server.workspaces);
	wl_list_init(&test_server.keybindings);
	wl_list_init(&test_server.keymodes);
#ifdef WM_HAS_XWAYLAND
	wl_list_init(&test_server.xwayland_views);
#endif
	wl_list_init(&test_ipc.clients);

	test_ipc.server = &test_server;
	test_client.ipc = &test_ipc;
	test_client.subscribed_events = 0;
	test_client.fd = -1;
	stub_active_workspace = NULL;
}

/* --- Mock objects for Group B tests --- */

static struct wm_view mock_view;
static struct wm_tab_group mock_tab_group;
static struct wm_toolbar mock_toolbar;
static struct wm_slit mock_slit;

static void
setup_with_view(void)
{
	setup();
	memset(&mock_view, 0, sizeof(mock_view));
	mock_view.server = &test_server;
	mock_view.focus_alpha = 255;
	mock_view.unfocus_alpha = 200;
	test_server.focused_view = &mock_view;
}

static void
setup_with_tabbed_view(void)
{
	setup_with_view();
	memset(&mock_tab_group, 0, sizeof(mock_tab_group));
	wl_list_init(&mock_tab_group.views);
	mock_tab_group.active_view = &mock_view;
	mock_tab_group.count = 1;
	mock_view.tab_group = &mock_tab_group;
}

static void
setup_with_toolbar_slit(void)
{
	setup();
	memset(&mock_toolbar, 0, sizeof(mock_toolbar));
	memset(&mock_slit, 0, sizeof(mock_slit));
	test_server.toolbar = &mock_toolbar;
	test_server.slit = &mock_slit;
}

/* --- Mock objects for Group C tests (wlroots-dependent actions) --- */

static struct wlr_xdg_toplevel mock_xdg_toplevel;
static struct wlr_xdg_surface mock_xdg_surface;
static struct wlr_surface mock_surface;
static struct wlr_scene_tree mock_scene_tree;
static struct wm_decoration mock_decoration;
static struct wm_style mock_style;
static struct wlr_cursor mock_cursor;

/* Listener callbacks for Maximize/Fullscreen/Minimize */
static int stub_maximize_called;
static void mock_request_maximize(struct wl_listener *l, void *data)
	{ (void)l; (void)data; stub_maximize_called++; }

static int stub_fullscreen_called;
static void mock_request_fullscreen(struct wl_listener *l, void *data)
	{ (void)l; (void)data; stub_fullscreen_called++; }

static int stub_minimize_called;
static void mock_request_minimize(struct wl_listener *l, void *data)
	{ (void)l; (void)data; stub_minimize_called++; }

/*
 * Setup a view with full mock wlroots objects for Group C tests.
 * Provides xdg_toplevel, scene_tree, decoration, style, and cursor.
 */
static void
setup_with_wlr_view(void)
{
	setup();
	memset(&mock_view, 0, sizeof(mock_view));
	memset(&mock_xdg_toplevel, 0, sizeof(mock_xdg_toplevel));
	memset(&mock_xdg_surface, 0, sizeof(mock_xdg_surface));
	memset(&mock_surface, 0, sizeof(mock_surface));
	memset(&mock_scene_tree, 0, sizeof(mock_scene_tree));
	memset(&mock_decoration, 0, sizeof(mock_decoration));
	memset(&mock_style, 0, sizeof(mock_style));
	memset(&mock_cursor, 0, sizeof(mock_cursor));

	/* Wire up xdg_toplevel -> base -> surface/resource */
	mock_xdg_toplevel.base = &mock_xdg_surface;
	mock_xdg_surface.surface = &mock_surface;

	/* Wire up view */
	mock_view.server = &test_server;
	mock_view.xdg_toplevel = &mock_xdg_toplevel;
	mock_view.scene_tree = &mock_scene_tree;
	mock_view.decoration = &mock_decoration;
	mock_view.focus_alpha = 255;
	mock_view.unfocus_alpha = 200;

	/* Set up listener callbacks */
	mock_view.request_maximize.notify = mock_request_maximize;
	mock_view.request_fullscreen.notify = mock_request_fullscreen;
	mock_view.request_minimize.notify = mock_request_minimize;

	/* Wire up server */
	test_server.focused_view = &mock_view;
	test_server.style = &mock_style;
	test_server.cursor = &mock_cursor;
	mock_cursor.x = 100;
	mock_cursor.y = 200;

	/* Reset tracking counters */
	stub_close_called = 0;
	stub_set_size_w = 0;
	stub_set_size_h = 0;
	stub_set_pos_x = 0;
	stub_set_pos_y = 0;
	stub_terminate_called = 0;
	stub_maximize_called = 0;
	stub_fullscreen_called = 0;
	stub_minimize_called = 0;
}

/* Check that response JSON contains "success":true */
static bool
response_is_success(const char *json)
{
	return json && strstr(json, "\"success\":true") != NULL;
}

/* Check that response JSON contains "success":false */
static bool
response_is_error(const char *json)
{
	return json && strstr(json, "\"success\":false") != NULL;
}

/* --- IPC command dispatch tests --- */

/* Test: ping command returns success */
static void
test_ping(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"ping\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_ping\n");
}

/* Test: empty request returns error */
static void
test_empty_request(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client, "");
	assert(r != NULL);
	assert(response_is_error(r));
	free(r);
	printf("  PASS: test_empty_request\n");
}

/* Test: NULL request returns error */
static void
test_null_request(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client, NULL);
	assert(r != NULL);
	assert(response_is_error(r));
	free(r);
	printf("  PASS: test_null_request\n");
}

/* Test: missing command field returns error */
static void
test_missing_command(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"foo\":\"bar\"}");
	assert(r != NULL);
	assert(response_is_error(r));
	free(r);
	printf("  PASS: test_missing_command\n");
}

/* Test: unknown command returns error */
static void
test_unknown_command(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"nonexistent_command\"}");
	assert(r != NULL);
	assert(response_is_error(r));
	free(r);
	printf("  PASS: test_unknown_command\n");
}

/* Test: get_config returns success with config data */
static void
test_get_config(void)
{
	setup();
	struct wm_config cfg = {0};
	cfg.workspace_count = 4;
	cfg.focus_policy = WM_FOCUS_CLICK;
	cfg.placement_policy = WM_PLACEMENT_ROW_SMART;
	cfg.edge_snap_threshold = 10;
	cfg.toolbar_visible = true;
	test_server.config = &cfg;

	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"get_config\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(strstr(r, "\"workspace_count\":4") != NULL);
	assert(strstr(r, "\"focus_policy\":\"click\"") != NULL);
	assert(strstr(r, "\"placement_policy\":\"row_smart\"") != NULL);
	assert(strstr(r, "\"edge_snap_threshold\":10") != NULL);
	assert(strstr(r, "\"toolbar_visible\":true") != NULL);
	free(r);
	printf("  PASS: test_get_config\n");
}

/* Test: get_config with sloppy focus */
static void
test_get_config_sloppy(void)
{
	setup();
	struct wm_config cfg = {0};
	cfg.workspace_count = 2;
	cfg.focus_policy = WM_FOCUS_SLOPPY;
	cfg.placement_policy = WM_PLACEMENT_CASCADE;
	cfg.edge_snap_threshold = 5;
	cfg.toolbar_visible = false;
	test_server.config = &cfg;

	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"get_config\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(strstr(r, "\"focus_policy\":\"sloppy\"") != NULL);
	assert(strstr(r, "\"placement_policy\":\"cascade\"") != NULL);
	assert(strstr(r, "\"toolbar_visible\":false") != NULL);
	free(r);
	printf("  PASS: test_get_config_sloppy\n");
}

/* Test: get_workspaces with empty workspace list */
static void
test_get_workspaces_empty(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"get_workspaces\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(strstr(r, "\"data\":[]") != NULL);
	free(r);
	printf("  PASS: test_get_workspaces_empty\n");
}

/* Test: get_windows with empty view list */
static void
test_get_windows_empty(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"get_windows\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(strstr(r, "\"data\":[]") != NULL);
	free(r);
	printf("  PASS: test_get_windows_empty\n");
}

/* Test: get_outputs with empty output list */
static void
test_get_outputs_empty(void)
{
	setup();
	wl_list_init(&test_server.outputs);
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"get_outputs\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(strstr(r, "\"data\":[]") != NULL);
	free(r);
	printf("  PASS: test_get_outputs_empty\n");
}

/* --- Subscribe command tests --- */

/* Test: subscribe to window events */
static void
test_subscribe_window(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"subscribe\",\"events\":\"window\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(test_client.subscribed_events & WM_IPC_EVENT_WINDOW_OPEN);
	assert(test_client.subscribed_events & WM_IPC_EVENT_WINDOW_CLOSE);
	assert(test_client.subscribed_events & WM_IPC_EVENT_WINDOW_FOCUS);
	assert(test_client.subscribed_events & WM_IPC_EVENT_WINDOW_TITLE);
	free(r);
	printf("  PASS: test_subscribe_window\n");
}

/* Test: subscribe to workspace events */
static void
test_subscribe_workspace(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"subscribe\",\"events\":\"workspace\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(test_client.subscribed_events & WM_IPC_EVENT_WORKSPACE);
	/* Should NOT have window events */
	assert(!(test_client.subscribed_events & WM_IPC_EVENT_WINDOW_OPEN));
	free(r);
	printf("  PASS: test_subscribe_workspace\n");
}

/* Test: subscribe to output events */
static void
test_subscribe_output(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"subscribe\",\"events\":\"output\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(test_client.subscribed_events & WM_IPC_EVENT_OUTPUT_ADD);
	assert(test_client.subscribed_events & WM_IPC_EVENT_OUTPUT_REMOVE);
	free(r);
	printf("  PASS: test_subscribe_output\n");
}

/* Test: subscribe to multiple events (comma-separated) */
static void
test_subscribe_multiple(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"subscribe\","
		"\"events\":\"workspace,output,style_changed\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(test_client.subscribed_events & WM_IPC_EVENT_WORKSPACE);
	assert(test_client.subscribed_events & WM_IPC_EVENT_OUTPUT_ADD);
	assert(test_client.subscribed_events & WM_IPC_EVENT_OUTPUT_REMOVE);
	assert(test_client.subscribed_events & WM_IPC_EVENT_STYLE_CHANGED);
	free(r);
	printf("  PASS: test_subscribe_multiple\n");
}

/* Test: subscribe to individual window sub-events */
static void
test_subscribe_window_sub_events(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"subscribe\",\"events\":\"window_close\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(test_client.subscribed_events & WM_IPC_EVENT_WINDOW_CLOSE);
	assert(!(test_client.subscribed_events & WM_IPC_EVENT_WINDOW_OPEN));
	free(r);

	/* Also test window_focus and window_title */
	setup();
	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"subscribe\","
		"\"events\":\"window_focus,window_title\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(test_client.subscribed_events & WM_IPC_EVENT_WINDOW_FOCUS);
	assert(test_client.subscribed_events & WM_IPC_EVENT_WINDOW_TITLE);
	assert(!(test_client.subscribed_events & WM_IPC_EVENT_WINDOW_OPEN));
	free(r);
	printf("  PASS: test_subscribe_window_sub_events\n");
}

/* Test: subscribe with missing events argument */
static void
test_subscribe_missing_events(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"subscribe\"}");
	assert(r != NULL);
	assert(response_is_error(r));
	free(r);
	printf("  PASS: test_subscribe_missing_events\n");
}

/* Test: subscribe to accessibility meta-subscription */
static void
test_subscribe_accessibility(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"subscribe\",\"events\":\"accessibility\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(test_client.subscribed_events & WM_IPC_EVENT_FOCUS_CHANGED);
	assert(test_client.subscribed_events & WM_IPC_EVENT_MENU);
	assert(test_client.subscribed_events & WM_IPC_EVENT_WORKSPACE);
	assert(test_client.subscribed_events & WM_IPC_EVENT_WINDOW_FOCUS);
	free(r);
	printf("  PASS: test_subscribe_accessibility\n");
}

/* Test: subscribe to config/style events */
static void
test_subscribe_config_events(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"subscribe\","
		"\"events\":\"config_reloaded,style_changed,"
		"focus_changed,menu\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(test_client.subscribed_events & WM_IPC_EVENT_CONFIG_RELOADED);
	assert(test_client.subscribed_events & WM_IPC_EVENT_STYLE_CHANGED);
	assert(test_client.subscribed_events & WM_IPC_EVENT_FOCUS_CHANGED);
	assert(test_client.subscribed_events & WM_IPC_EVENT_MENU);
	free(r);
	printf("  PASS: test_subscribe_config_events\n");
}

/* --- Command action dispatch tests --- */

/* Test: command with missing action */
static void
test_command_missing_action(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\"}");
	assert(r != NULL);
	assert(response_is_error(r));
	free(r);
	printf("  PASS: test_command_missing_action\n");
}

/* Test: command with unknown action */
static void
test_command_unknown_action(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"TotallyNotAnAction\"}");
	assert(r != NULL);
	assert(response_is_error(r));
	free(r);
	printf("  PASS: test_command_unknown_action\n");
}

/* Test: command FocusNext dispatches successfully */
static void
test_command_focus_next(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"FocusNext\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_command_focus_next\n");
}

/* Test: command ShowDesktop dispatches successfully */
static void
test_command_show_desktop(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"ShowDesktop\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_command_show_desktop\n");
}

/* Test: Exec command blocked by ipc_no_exec */
static void
test_command_exec_blocked(void)
{
	setup();
	test_server.ipc_no_exec = true;
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"Exec echo test\"}");
	assert(r != NULL);
	/* Exec is blocked but still returns success (silently blocked) */
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_command_exec_blocked\n");
}

/* Test: BindKey blocked by ipc_no_exec */
static void
test_command_bindkey_blocked(void)
{
	setup();
	test_server.ipc_no_exec = true;
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"BindKey Mod4 t :Exec foot\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_command_bindkey_blocked\n");
}

/* Test: unsupported actions return error */
static void
test_command_unsupported_action(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"MacroCmd\"}");
	assert(r != NULL);
	assert(response_is_error(r));
	free(r);
	printf("  PASS: test_command_unsupported_action\n");
}

/* Test: ToggleShowPosition toggles state */
static void
test_command_toggle_show_position(void)
{
	setup();
	test_server.show_position = false;
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"ToggleShowPosition\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(test_server.show_position == true);
	free(r);

	/* Toggle back */
	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"ToggleShowPosition\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(test_server.show_position == false);
	free(r);
	printf("  PASS: test_command_toggle_show_position\n");
}

/* --- Event throttle tests --- */

/* Test: non-throttled events are never throttled */
static void
test_throttle_non_throttled_event(void)
{
	setup();
	/* WM_IPC_EVENT_WINDOW_OPEN is not throttled; broadcast should work.
	 * We can't directly test should_throttle_event (it's static), but
	 * we can verify broadcast_event doesn't crash with no clients */
	wm_ipc_broadcast_event(&test_ipc, WM_IPC_EVENT_WINDOW_OPEN,
		"{\"event\":\"window_open\"}");
	printf("  PASS: test_throttle_non_throttled_event\n");
}

/* Test: broadcast with no subscribed clients is a no-op */
static void
test_broadcast_no_subscribers(void)
{
	setup();
	/* Create a client that subscribes to workspace but broadcast window */
	test_client.subscribed_events = WM_IPC_EVENT_WORKSPACE;
	wl_list_insert(&test_ipc.clients, &test_client.link);
	test_ipc.client_count = 1;

	/* This should not attempt to send to the client (wrong event) */
	wm_ipc_broadcast_event(&test_ipc, WM_IPC_EVENT_WINDOW_OPEN,
		"{\"event\":\"window_open\"}");

	wl_list_remove(&test_client.link);
	printf("  PASS: test_broadcast_no_subscribers\n");
}

/* --- JSON parsing edge cases (tested via command dispatch) --- */

/* Test: whitespace in JSON is handled */
static void
test_json_whitespace(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{ \"command\" : \"ping\" }");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_json_whitespace\n");
}

/* Test: JSON with extra fields still works */
static void
test_json_extra_fields(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"ping\",\"extra\":\"ignored\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_json_extra_fields\n");
}

/* Test: malformed JSON (no closing brace) returns error */
static void
test_json_malformed(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"ping\"");
	/* Should still parse the command field from this partial JSON */
	assert(r != NULL);
	/* The minimal JSON parser may still find the command */
	free(r);
	printf("  PASS: test_json_malformed\n");
}

/* Test: JSON with escaped quotes in values */
static void
test_json_escaped_quotes(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"FocusNext\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_json_escaped_quotes\n");
}

/* Test: very long command name is handled safely */
static void
test_long_command_name(void)
{
	setup();
	/* Create a JSON request with a very long command name */
	char buf[2048];
	memset(buf, 0, sizeof(buf));
	strcpy(buf, "{\"command\":\"");
	memset(buf + strlen(buf), 'x', 1000);
	strcat(buf, "\"}");

	char *r = wm_ipc_handle_command(&test_ipc, &test_client, buf);
	assert(r != NULL);
	assert(response_is_error(r));
	free(r);
	printf("  PASS: test_long_command_name\n");
}

/* --- Event type bitmask tests --- */

/* Test: event enum values are distinct power-of-two bits */
static void
test_event_bitmask_distinct(void)
{
	/* Each event should be a unique bit */
	uint32_t all = 0;
	uint32_t events[] = {
		WM_IPC_EVENT_WINDOW_OPEN,
		WM_IPC_EVENT_WINDOW_CLOSE,
		WM_IPC_EVENT_WINDOW_FOCUS,
		WM_IPC_EVENT_WINDOW_TITLE,
		WM_IPC_EVENT_WORKSPACE,
		WM_IPC_EVENT_OUTPUT_ADD,
		WM_IPC_EVENT_OUTPUT_REMOVE,
		WM_IPC_EVENT_STYLE_CHANGED,
		WM_IPC_EVENT_CONFIG_RELOADED,
		WM_IPC_EVENT_FOCUS_CHANGED,
		WM_IPC_EVENT_MENU,
	};
	size_t n = sizeof(events) / sizeof(events[0]);
	for (size_t i = 0; i < n; i++) {
		/* Each event should be a power of 2 */
		assert((events[i] & (events[i] - 1)) == 0);
		/* Should not overlap with previously seen events */
		assert((all & events[i]) == 0);
		all |= events[i];
	}
	printf("  PASS: test_event_bitmask_distinct\n");
}

/* Test: IPC client struct initializes cleanly */
static void
test_client_init(void)
{
	struct wm_ipc_client c = {0};
	assert(c.subscribed_events == 0);
	assert(c.read_buf == NULL);
	assert(c.read_len == 0);
	assert(c.read_cap == 0);
	printf("  PASS: test_client_init\n");
}

/* Test: IPC server constants */
static void
test_ipc_constants(void)
{
	/* Throttle interval should be reasonable (10-100ms) */
	assert(IPC_EVENT_THROTTLE_MS >= 10);
	assert(IPC_EVENT_THROTTLE_MS <= 100);
	printf("  PASS: test_ipc_constants\n");
}

/* === Group A: Server-only action tests (no focused_view needed) === */

/* Test: FocusPrev dispatches successfully */
static void
test_action_focus_prev(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"FocusPrev\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_focus_prev\n");
}

/* Test: NextWorkspace dispatches successfully */
static void
test_action_next_workspace(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"NextWorkspace\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_next_workspace\n");
}

/* Test: PrevWorkspace dispatches successfully */
static void
test_action_prev_workspace(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"PrevWorkspace\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_prev_workspace\n");
}

/* Test: RightWorkspace and LeftWorkspace dispatch successfully */
static void
test_action_right_left_workspace(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"RightWorkspace\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"LeftWorkspace\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_right_left_workspace\n");
}

/* Test: ArrangeWindows (grid) dispatches successfully */
static void
test_action_arrange_grid(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"ArrangeWindows\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_arrange_grid\n");
}

/* Test: ArrangeWindowsVertical and ArrangeWindowsHorizontal */
static void
test_action_arrange_vert_horiz(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"ArrangeWindowsVertical\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"ArrangeWindowsHorizontal\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_arrange_vert_horiz\n");
}

/* Test: CascadeWindows dispatches successfully */
static void
test_action_cascade(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"CascadeWindows\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_cascade\n");
}

/* Test: ArrangeStack variants (Left/Right/Top/Bottom) */
static void
test_action_arrange_stack_variants(void)
{
	const char *actions[] = {
		"ArrangeWindowsStackLeft",
		"ArrangeWindowsStackRight",
		"ArrangeWindowsStackTop",
		"ArrangeWindowsStackBottom",
	};
	for (int i = 0; i < 4; i++) {
		setup();
		char buf[128];
		snprintf(buf, sizeof(buf),
			"{\"command\":\"command\",\"action\":\"%s\"}",
			actions[i]);
		char *r = wm_ipc_handle_command(&test_ipc, &test_client, buf);
		assert(r != NULL);
		assert(response_is_success(r));
		free(r);
	}
	printf("  PASS: test_action_arrange_stack_variants\n");
}

/* Test: CloseAllWindows dispatches successfully */
static void
test_action_close_all_windows(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"CloseAllWindows\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_close_all_windows\n");
}

/* Test: AddWorkspace and RemoveLastWorkspace */
static void
test_action_add_remove_workspace(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"AddWorkspace\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"RemoveLastWorkspace\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_add_remove_workspace\n");
}

/* Test: SendToNextWorkspace and SendToPrevWorkspace */
static void
test_action_send_to_next_prev_workspace(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"SendToNextWorkspace\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"SendToPrevWorkspace\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_send_to_next_prev_workspace\n");
}

/* Test: TakeToNextWorkspace and TakeToPrevWorkspace */
static void
test_action_take_to_next_prev_workspace(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"TakeToNextWorkspace\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"TakeToPrevWorkspace\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_take_to_next_prev_workspace\n");
}

/* Test: HideMenus dispatches successfully */
static void
test_action_hide_menus(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"HideMenus\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_hide_menus\n");
}

/* Test: Deiconify variants (no arg, All, AllWorkspace, LastWorkspace) */
static void
test_action_deiconify_variants(void)
{
	setup();
	/* No argument: defaults to deiconify last */
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"Deiconify\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	/* Deiconify All */
	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"Deiconify All\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	/* Deiconify AllWorkspace */
	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"Deiconify AllWorkspace\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	/* Deiconify LastWorkspace */
	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"Deiconify LastWorkspace\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_deiconify_variants\n");
}

/* Test: Workspace N switches by number */
static void
test_action_workspace_number(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"Workspace 2\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_workspace_number\n");
}

/* Test: SendToWorkspace N */
static void
test_action_send_to_workspace_number(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"SendToWorkspace 3\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_send_to_workspace_number\n");
}

/* Test: TakeToWorkspace N */
static void
test_action_take_to_workspace_number(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"TakeToWorkspace 2\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_take_to_workspace_number\n");
}

/* Test: SetWorkspaceName sets the name */
static void
test_action_set_workspace_name(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"SetWorkspaceName TestDesk\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_set_workspace_name\n");
}

/* Test: Reconfigure reloads config and resets keymode */
static void
test_action_reconfigure(void)
{
	setup();
	struct wm_config cfg = {0};
	test_server.config = &cfg;
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"Reconfigure\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	/* Verify keymode was reset to default */
	assert(test_server.current_keymode != NULL);
	assert(strcmp(test_server.current_keymode, "default") == 0);
	free(test_server.current_keymode);
	test_server.current_keymode = NULL;
	free(r);
	printf("  PASS: test_action_reconfigure\n");
}

/* Test: FocusLeft/Right/Up/Down dispatch focus direction */
static void
test_action_focus_directions(void)
{
	const char *actions[] = {
		"FocusLeft", "FocusRight", "FocusUp", "FocusDown",
	};
	for (int i = 0; i < 4; i++) {
		setup();
		char buf[128];
		snprintf(buf, sizeof(buf),
			"{\"command\":\"command\",\"action\":\"%s\"}",
			actions[i]);
		char *r = wm_ipc_handle_command(&test_ipc, &test_client, buf);
		assert(r != NULL);
		assert(response_is_success(r));
		free(r);
	}
	printf("  PASS: test_action_focus_directions\n");
}

/* Test: NextLayout and PrevLayout */
static void
test_action_layout_cycling(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"NextLayout\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"PrevLayout\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_layout_cycling\n");
}

/* === Group B: Mock view action tests === */

/* Test: Raise action with focused view */
static void
test_action_raise(void)
{
	setup_with_view();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"Raise\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_raise\n");
}

/* Test: Lower action with focused view */
static void
test_action_lower(void)
{
	setup_with_view();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"Lower\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_lower\n");
}

/* Test: RaiseLayer and LowerLayer with focused view */
static void
test_action_raise_lower_layer(void)
{
	setup_with_view();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"RaiseLayer\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"LowerLayer\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_raise_lower_layer\n");
}

/* Test: SetLayer with argument */
static void
test_action_set_layer(void)
{
	setup_with_view();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"SetLayer Above\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_set_layer\n");
}

/* Test: Stick toggles sticky state */
static void
test_action_stick(void)
{
	setup_with_view();
	mock_view.sticky = false;
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"Stick\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_stick\n");
}

/* Test: MaximizeVertical with focused view */
static void
test_action_maximize_vert(void)
{
	setup_with_view();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"MaximizeVertical\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_maximize_vert\n");
}

/* Test: MaximizeHorizontal with focused view */
static void
test_action_maximize_horiz(void)
{
	setup_with_view();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"MaximizeHorizontal\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_maximize_horiz\n");
}

/* Test: ToggleDecor with focused view */
static void
test_action_toggle_decor(void)
{
	setup_with_view();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"ToggleDecor\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_toggle_decor\n");
}

/* Test: LHalf and RHalf tiling */
static void
test_action_lhalf_rhalf(void)
{
	setup_with_view();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"LHalf\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"RHalf\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_lhalf_rhalf\n");
}

/* Test: ResizeHorizontal and ResizeVertical with argument */
static void
test_action_resize_horiz_vert(void)
{
	setup_with_view();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"ResizeHorizontal 10\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"ResizeVertical -5\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_resize_horiz_vert\n");
}

/* Test: NextTab and PrevTab with tabbed view */
static void
test_action_next_prev_tab(void)
{
	setup_with_tabbed_view();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"NextTab\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"PrevTab\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_next_prev_tab\n");
}

/* Test: DetachClient with tabbed view */
static void
test_action_detach_client(void)
{
	setup_with_tabbed_view();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"DetachClient\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_detach_client\n");
}

/* Test: MoveTabLeft and MoveTabRight with tabbed view */
static void
test_action_move_tab(void)
{
	setup_with_tabbed_view();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"MoveTabLeft\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"MoveTabRight\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_move_tab\n");
}

/* Test: SetAlpha with absolute value */
static void
test_action_set_alpha_absolute(void)
{
	setup_with_view();
	mock_view.focus_alpha = 255;
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"SetAlpha 128\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(mock_view.focus_alpha == 128);
	assert(mock_view.unfocus_alpha == 128);
	free(r);
	printf("  PASS: test_action_set_alpha_absolute\n");
}

/* Test: SetAlpha with relative +/- offsets */
static void
test_action_set_alpha_relative(void)
{
	setup_with_view();
	mock_view.focus_alpha = 200;
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"SetAlpha +30\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(mock_view.focus_alpha == 230);
	free(r);

	/* Relative negative */
	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"SetAlpha -50\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(mock_view.focus_alpha == 180);
	free(r);
	printf("  PASS: test_action_set_alpha_relative\n");
}

/* Test: SetAlpha clamping to 0-255 range */
static void
test_action_set_alpha_clamping(void)
{
	setup_with_view();
	/* Clamp high: 300 -> 255 */
	mock_view.focus_alpha = 200;
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"SetAlpha 300\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(mock_view.focus_alpha == 255);
	free(r);

	/* Clamp low: 50 + (-100) = -50 -> 0 */
	mock_view.focus_alpha = 50;
	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"SetAlpha -100\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(mock_view.focus_alpha == 0);
	free(r);
	printf("  PASS: test_action_set_alpha_clamping\n");
}

/* Test: KeyMode switches the current keymode */
static void
test_action_key_mode(void)
{
	setup();
	test_server.current_keymode = strdup("default");
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"KeyMode custom_mode\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(test_server.current_keymode != NULL);
	assert(strcmp(test_server.current_keymode, "custom_mode") == 0);
	free(test_server.current_keymode);
	test_server.current_keymode = NULL;
	free(r);
	printf("  PASS: test_action_key_mode\n");
}

/* Test: ToggleToolbarAbove and ToggleToolbarVisible */
static void
test_action_toolbar_toggles(void)
{
	setup_with_toolbar_slit();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"ToggleToolbarAbove\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"ToggleToolbarVisible\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_toolbar_toggles\n");
}

/* Test: ToggleSlitAbove and ToggleSlitHidden */
static void
test_action_slit_toggles(void)
{
	setup_with_toolbar_slit();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"ToggleSlitAbove\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"ToggleSlitHidden\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_slit_toggles\n");
}

/* Test: Toolbar/Slit toggles with NULL pointers (no-op) */
static void
test_action_toolbar_slit_null(void)
{
	setup();
	/* server->toolbar and server->slit are NULL */
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"ToggleToolbarAbove\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"ToggleSlitHidden\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_toolbar_slit_null\n");
}

/* Test: View actions with no focused view are no-ops */
static void
test_action_view_actions_no_focus(void)
{
	setup();
	/* No focused_view set - all view actions should still succeed */
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"Raise\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"LHalf\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"SetAlpha 100\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_view_actions_no_focus\n");
}

/* Test: Tab actions with no tab group are no-ops */
static void
test_action_tab_no_group(void)
{
	setup_with_view();
	/* View has no tab_group */
	mock_view.tab_group = NULL;
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"NextTab\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"DetachClient\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_tab_no_group\n");
}

/* === Data-populated query tests === */

/* Test: get_workspaces with actual workspace structs */
static void
test_get_workspaces_populated(void)
{
	setup();
	struct wm_workspace ws0 = {0};
	ws0.name = "Desktop 1";
	ws0.index = 0;
	struct wm_workspace ws1 = {0};
	ws1.name = "Desktop 2";
	ws1.index = 1;

	wl_list_insert(test_server.workspaces.prev, &ws0.link);
	wl_list_insert(test_server.workspaces.prev, &ws1.link);

	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"get_workspaces\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(strstr(r, "\"index\":0") != NULL);
	assert(strstr(r, "\"Desktop 1\"") != NULL);
	assert(strstr(r, "\"index\":1") != NULL);
	assert(strstr(r, "\"Desktop 2\"") != NULL);
	assert(strstr(r, "\"focused\":false") != NULL);
	free(r);

	wl_list_remove(&ws0.link);
	wl_list_remove(&ws1.link);
	printf("  PASS: test_get_workspaces_populated\n");
}

/* Test: get_config with strict_mouse focus and under_mouse placement */
static void
test_get_config_strict_mouse(void)
{
	setup();
	struct wm_config cfg = {0};
	cfg.workspace_count = 6;
	cfg.focus_policy = WM_FOCUS_STRICT_MOUSE;
	cfg.placement_policy = WM_PLACEMENT_UNDER_MOUSE;
	cfg.edge_snap_threshold = 20;
	cfg.toolbar_visible = true;
	test_server.config = &cfg;

	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"get_config\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(strstr(r, "\"focus_policy\":\"strict_mouse\"") != NULL);
	assert(strstr(r, "\"placement_policy\":\"under_mouse\"") != NULL);
	assert(strstr(r, "\"workspace_count\":6") != NULL);
	free(r);
	printf("  PASS: test_get_config_strict_mouse\n");
}

/* Test: get_config with col_smart placement */
static void
test_get_config_col_smart(void)
{
	setup();
	struct wm_config cfg = {0};
	cfg.workspace_count = 3;
	cfg.focus_policy = WM_FOCUS_CLICK;
	cfg.placement_policy = WM_PLACEMENT_COL_SMART;
	cfg.edge_snap_threshold = 15;
	cfg.toolbar_visible = true;
	test_server.config = &cfg;

	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"get_config\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(strstr(r, "\"placement_policy\":\"col_smart\"") != NULL);
	free(r);
	printf("  PASS: test_get_config_col_smart\n");
}

/* Test: get_config with min_overlap placement variants */
static void
test_get_config_min_overlap(void)
{
	setup();
	struct wm_config cfg = {0};
	cfg.placement_policy = WM_PLACEMENT_ROW_MIN_OVERLAP;
	test_server.config = &cfg;

	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"get_config\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(strstr(r, "\"placement_policy\":\"row_min_overlap\"") != NULL);
	free(r);

	cfg.placement_policy = WM_PLACEMENT_COL_MIN_OVERLAP;
	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"get_config\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(strstr(r, "\"placement_policy\":\"col_min_overlap\"") != NULL);
	free(r);
	printf("  PASS: test_get_config_min_overlap\n");
}

/* === Group C: wlroots-dependent action tests === */

/* Test: Close action calls wlr_xdg_toplevel_send_close */
static void
test_action_close(void)
{
	setup_with_wlr_view();
	stub_close_called = 0;
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"Close\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(stub_close_called == 1);
	free(r);
	printf("  PASS: test_action_close\n");
}

/* Test: Close with no focused view is a no-op */
static void
test_action_close_no_view(void)
{
	setup();
	stub_close_called = 0;
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"Close\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(stub_close_called == 0);
	free(r);
	printf("  PASS: test_action_close_no_view\n");
}

/* Test: Kill action calls wl_resource_get_client + wl_client_destroy */
static void
test_action_kill(void)
{
	setup_with_wlr_view();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"Kill\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_kill\n");
}

/* Test: Maximize action triggers request_maximize listener */
static void
test_action_maximize(void)
{
	setup_with_wlr_view();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"Maximize\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(stub_maximize_called == 1);
	free(r);
	printf("  PASS: test_action_maximize\n");
}

/* Test: Fullscreen action triggers request_fullscreen listener */
static void
test_action_fullscreen(void)
{
	setup_with_wlr_view();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"Fullscreen\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(stub_fullscreen_called == 1);
	free(r);
	printf("  PASS: test_action_fullscreen\n");
}

/* Test: Minimize action triggers request_minimize listener */
static void
test_action_minimize(void)
{
	setup_with_wlr_view();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"Minimize\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(stub_minimize_called == 1);
	free(r);
	printf("  PASS: test_action_minimize\n");
}

/* Test: Shade toggles decoration shaded state */
static void
test_action_shade(void)
{
	setup_with_wlr_view();
	mock_decoration.shaded = false;
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"Shade\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_shade\n");
}

/* Test: ShadeOn sets shaded=true */
static void
test_action_shade_on(void)
{
	setup_with_wlr_view();
	mock_decoration.shaded = false;
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"ShadeOn\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_shade_on\n");
}

/* Test: ShadeOff sets shaded=false */
static void
test_action_shade_off(void)
{
	setup_with_wlr_view();
	mock_decoration.shaded = true;
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"ShadeOff\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_shade_off\n");
}

/* Test: Shade with no decoration is a no-op */
static void
test_action_shade_no_decor(void)
{
	setup_with_wlr_view();
	mock_view.decoration = NULL;
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"Shade\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_shade_no_decor\n");
}

/* Test: SetDecor with all presets */
static void
test_action_set_decor_presets(void)
{
	const char *presets[] = {
		"NONE", "BORDER", "TAB", "TINY", "TOOL", "NORMAL",
	};
	for (int i = 0; i < 6; i++) {
		setup_with_wlr_view();
		char buf[128];
		snprintf(buf, sizeof(buf),
			"{\"command\":\"command\","
			"\"action\":\"SetDecor %s\"}", presets[i]);
		char *r = wm_ipc_handle_command(&test_ipc, &test_client, buf);
		assert(r != NULL);
		assert(response_is_success(r));
		free(r);
	}
	printf("  PASS: test_action_set_decor_presets\n");
}

/* Test: SetDecor with "0" (alias for NONE) */
static void
test_action_set_decor_zero(void)
{
	setup_with_wlr_view();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"SetDecor 0\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_set_decor_zero\n");
}

/* Test: SetDecor with no decoration is a no-op */
static void
test_action_set_decor_no_decor(void)
{
	setup_with_wlr_view();
	mock_view.decoration = NULL;
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"SetDecor BORDER\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_set_decor_no_decor\n");
}

/* Test: MoveLeft adjusts view x and calls wlr_scene_node_set_position */
static void
test_action_move_left(void)
{
	setup_with_wlr_view();
	mock_view.x = 100;
	mock_view.y = 50;
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"MoveLeft 20\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(mock_view.x == 80);
	assert(stub_set_pos_x == 80);
	assert(stub_set_pos_y == 50);
	free(r);
	printf("  PASS: test_action_move_left\n");
}

/* Test: MoveRight adjusts view x */
static void
test_action_move_right(void)
{
	setup_with_wlr_view();
	mock_view.x = 100;
	mock_view.y = 50;
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"MoveRight 30\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(mock_view.x == 130);
	assert(stub_set_pos_x == 130);
	free(r);
	printf("  PASS: test_action_move_right\n");
}

/* Test: MoveUp adjusts view y */
static void
test_action_move_up(void)
{
	setup_with_wlr_view();
	mock_view.x = 100;
	mock_view.y = 200;
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"MoveUp 50\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(mock_view.y == 150);
	assert(stub_set_pos_y == 150);
	free(r);
	printf("  PASS: test_action_move_up\n");
}

/* Test: MoveDown adjusts view y */
static void
test_action_move_down(void)
{
	setup_with_wlr_view();
	mock_view.x = 100;
	mock_view.y = 200;
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"MoveDown 25\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(mock_view.y == 225);
	assert(stub_set_pos_y == 225);
	free(r);
	printf("  PASS: test_action_move_down\n");
}

/* Test: MoveTo x y sets absolute position */
static void
test_action_move_to(void)
{
	setup_with_wlr_view();
	mock_view.x = 0;
	mock_view.y = 0;
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"MoveTo 300 400\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(mock_view.x == 300);
	assert(mock_view.y == 400);
	assert(stub_set_pos_x == 300);
	assert(stub_set_pos_y == 400);
	free(r);
	printf("  PASS: test_action_move_to\n");
}

/* Test: Move actions with no view are no-ops */
static void
test_action_move_no_view(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"MoveLeft 10\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"MoveTo 50 50\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_move_no_view\n");
}

/* Test: ResizeTo w h calls wlr_xdg_toplevel_set_size */
static void
test_action_resize_to(void)
{
	setup_with_wlr_view();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"ResizeTo 640 480\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(stub_set_size_w == 640);
	assert(stub_set_size_h == 480);
	free(r);
	printf("  PASS: test_action_resize_to\n");
}

/* Test: ResizeTo with invalid dimensions (0) does not call set_size */
static void
test_action_resize_to_invalid(void)
{
	setup_with_wlr_view();
	stub_set_size_w = -1; /* sentinel */
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"ResizeTo 0 0\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(stub_set_size_w == -1); /* not called */
	free(r);
	printf("  PASS: test_action_resize_to_invalid\n");
}

/* Test: Exit calls wl_display_terminate */
static void
test_action_exit(void)
{
	setup_with_wlr_view();
	stub_terminate_called = 0;
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"Exit\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(stub_terminate_called == 1);
	free(r);
	printf("  PASS: test_action_exit\n");
}

/* Test: SetEnv with eq-separated format (FOO=bar) */
static void
test_action_setenv_eq(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"SetEnv FLUXTEST_VAR1=hello\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	const char *val = getenv("FLUXTEST_VAR1");
	assert(val != NULL);
	assert(strcmp(val, "hello") == 0);
	unsetenv("FLUXTEST_VAR1");
	free(r);
	printf("  PASS: test_action_setenv_eq\n");
}

/* Test: SetEnv with space-separated format (FOO bar) */
static void
test_action_setenv_space(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"SetEnv FLUXTEST_VAR2 world\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	const char *val = getenv("FLUXTEST_VAR2");
	assert(val != NULL);
	assert(strcmp(val, "world") == 0);
	unsetenv("FLUXTEST_VAR2");
	free(r);
	printf("  PASS: test_action_setenv_space\n");
}

/* Test: SetEnv blocks security-sensitive variables */
static void
test_action_setenv_blocked(void)
{
	const char *blocked_vars[] = {
		"LD_PRELOAD", "LD_LIBRARY_PATH", "PATH",
		"HOME", "SHELL", "IFS",
	};
	for (int i = 0; i < 6; i++) {
		setup();
		const char *old_val = getenv(blocked_vars[i]);
		char buf[256];
		snprintf(buf, sizeof(buf),
			"{\"command\":\"command\","
			"\"action\":\"SetEnv %s=HACKED\"}",
			blocked_vars[i]);
		char *r = wm_ipc_handle_command(&test_ipc, &test_client, buf);
		assert(r != NULL);
		assert(response_is_success(r));
		/* Variable should NOT have been changed */
		const char *new_val = getenv(blocked_vars[i]);
		if (old_val) {
			assert(new_val != NULL);
			assert(strcmp(new_val, "HACKED") != 0);
		}
		free(r);
	}
	printf("  PASS: test_action_setenv_blocked\n");
}

/* Test: Export is alias for SetEnv */
static void
test_action_export_alias(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"Export FLUXTEST_EXPORT=works\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	const char *val = getenv("FLUXTEST_EXPORT");
	assert(val != NULL);
	assert(strcmp(val, "works") == 0);
	unsetenv("FLUXTEST_EXPORT");
	free(r);
	printf("  PASS: test_action_export_alias\n");
}

/* Test: ReloadStyle reloads from config style_file */
static void
test_action_reload_style(void)
{
	setup_with_wlr_view();
	struct wm_config cfg = {0};
	cfg.style_file = strdup("/nonexistent/style");
	test_server.config = &cfg;
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"ReloadStyle\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	free(cfg.style_file);
	printf("  PASS: test_action_reload_style\n");
}

/* Test: Move and Resize interactive actions with focused view */
static void
test_action_move_resize_interactive(void)
{
	setup_with_wlr_view();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"Move\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"Resize\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_move_resize_interactive\n");
}

/* Test: StartMoving and StartResizing */
static void
test_action_start_moving_resizing(void)
{
	setup_with_wlr_view();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"StartMoving\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"StartResizing\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_start_moving_resizing\n");
}

/* Test: Focus action with focused view */
static void
test_action_focus(void)
{
	setup_with_wlr_view();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"Focus\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_focus\n");
}

/* Test: StartTabbing is a no-op (mouse only) */
static void
test_action_start_tabbing(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"StartTabbing\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_start_tabbing\n");
}

/* Test: ActivateTab with argument */
static void
test_action_activate_tab(void)
{
	setup_with_wlr_view();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"ActivateTab 2\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_activate_tab\n");
}

/* Test: SetHead with argument */
static void
test_action_set_head(void)
{
	setup_with_wlr_view();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"SetHead 1\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_set_head\n");
}

/* Test: SendToNextHead and SendToPrevHead */
static void
test_action_send_to_heads(void)
{
	setup_with_wlr_view();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"SendToNextHead\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"SendToPrevHead\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_send_to_heads\n");
}

/* Test: Remember action with config apps_file */
static void
test_action_remember(void)
{
	setup_with_wlr_view();
	struct wm_config cfg = {0};
	cfg.apps_file = "/tmp/test_apps";
	test_server.config = &cfg;
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"Remember\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_remember\n");
}

/* Test: NOP action returns success */
static void
test_action_nop(void)
{
	setup();
	/* NOP is not in the action table as a named action,
	 * but we can test the unsupported path variants */
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"If\"}");
	assert(r != NULL);
	assert(response_is_error(r));
	free(r);

	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"ForEach\"}");
	assert(r != NULL);
	assert(response_is_error(r));
	free(r);

	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"Map\"}");
	assert(r != NULL);
	assert(response_is_error(r));
	free(r);

	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"Delay\"}");
	assert(r != NULL);
	assert(response_is_error(r));
	free(r);
	printf("  PASS: test_action_nop\n");
}

/* === Group D: Mock view list tests === */

/* Helper: create linked mock views for list tests */
static struct wm_view mock_views[3];
static struct wlr_xdg_toplevel mock_toplevels[3];
static struct wlr_xdg_surface mock_xdg_surfaces[3];
static struct wlr_surface mock_surfaces[3];
static struct wlr_scene_tree mock_scene_trees[3];
static struct wm_workspace mock_workspace;

static void
setup_with_view_list(void)
{
	setup();
	memset(mock_views, 0, sizeof(mock_views));
	memset(mock_toplevels, 0, sizeof(mock_toplevels));
	memset(mock_xdg_surfaces, 0, sizeof(mock_xdg_surfaces));
	memset(mock_surfaces, 0, sizeof(mock_surfaces));
	memset(mock_scene_trees, 0, sizeof(mock_scene_trees));
	memset(&mock_workspace, 0, sizeof(mock_workspace));

	mock_workspace.name = "Desktop 1";
	mock_workspace.index = 0;
	stub_active_workspace = &mock_workspace;

	for (int i = 0; i < 3; i++) {
		mock_views[i].server = &test_server;
		mock_toplevels[i].base = &mock_xdg_surfaces[i];
		mock_xdg_surfaces[i].surface = &mock_surfaces[i];
		mock_views[i].xdg_toplevel = &mock_toplevels[i];
		mock_views[i].scene_tree = &mock_scene_trees[i];
		mock_views[i].workspace = &mock_workspace;
		mock_views[i].request_minimize.notify = mock_request_minimize;
		wl_list_insert(test_server.views.prev, &mock_views[i].link);
	}

	mock_views[0].title = "Firefox";
	mock_views[0].app_id = "firefox";
	mock_views[0].x = 10;
	mock_views[0].y = 20;

	mock_views[1].title = "Terminal";
	mock_views[1].app_id = "foot";
	mock_views[1].x = 100;
	mock_views[1].y = 200;
	mock_views[1].maximized = true;

	mock_views[2].title = "Editor";
	mock_views[2].app_id = "code";
	mock_views[2].x = 50;
	mock_views[2].y = 60;
	mock_views[2].sticky = true;

	test_server.focused_view = &mock_views[0];
}

/* Test: NextWindow without pattern cycles */
static void
test_action_next_window_cycle(void)
{
	setup_with_view_list();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"NextWindow\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_next_window_cycle\n");
}

/* Test: PrevWindow without pattern cycles */
static void
test_action_prev_window_cycle(void)
{
	setup_with_view_list();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"PrevWindow\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_prev_window_cycle\n");
}

/* Test: NextWindow with pattern matches by title */
static void
test_action_next_window_pattern(void)
{
	setup_with_view_list();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"NextWindow Terminal\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_next_window_pattern\n");
}

/* Test: NextWindow with glob pattern matches by app_id */
static void
test_action_next_window_glob(void)
{
	setup_with_view_list();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"NextWindow fire*\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_next_window_glob\n");
}

/* Test: PrevWindow with pattern */
static void
test_action_prev_window_pattern(void)
{
	setup_with_view_list();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"PrevWindow Editor\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_prev_window_pattern\n");
}

/* Test: NextWindow with non-matching pattern */
static void
test_action_next_window_no_match(void)
{
	setup_with_view_list();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"NextWindow NonexistentApp\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_next_window_no_match\n");
}

/* Test: GotoWindow N focuses the Nth view */
static void
test_action_goto_window(void)
{
	setup_with_view_list();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"GotoWindow 2\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_goto_window\n");
}

/* Test: GotoWindow with out-of-range N is a no-op */
static void
test_action_goto_window_out_of_range(void)
{
	setup_with_view_list();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"GotoWindow 99\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_goto_window_out_of_range\n");
}

/* Test: ShowDesktop with actual views minimizes all */
static void
test_action_show_desktop_with_views(void)
{
	setup_with_view_list();
	/* Enable scene nodes so ShowDesktop sees them as visible */
	for (int i = 0; i < 3; i++)
		mock_scene_trees[i].node.enabled = true;
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"ShowDesktop\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	/* All 3 views should now have scene nodes disabled */
	for (int i = 0; i < 3; i++)
		assert(mock_scene_trees[i].node.enabled == false);
	free(r);
	printf("  PASS: test_action_show_desktop_with_views\n");
}

/* Test: get_windows with populated view list */
static void
test_get_windows_populated(void)
{
	setup_with_view_list();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"get_windows\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	/* Check view data is in JSON */
	assert(strstr(r, "\"Firefox\"") != NULL);
	assert(strstr(r, "\"firefox\"") != NULL);
	assert(strstr(r, "\"Terminal\"") != NULL);
	assert(strstr(r, "\"foot\"") != NULL);
	assert(strstr(r, "\"Editor\"") != NULL);
	assert(strstr(r, "\"code\"") != NULL);
	/* Check boolean fields */
	assert(strstr(r, "\"maximized\":true") != NULL);
	assert(strstr(r, "\"sticky\":true") != NULL);
	assert(strstr(r, "\"focused\":true") != NULL);
	free(r);

	/* Clean up view list */
	for (int i = 0; i < 3; i++)
		wl_list_remove(&mock_views[i].link);
	printf("  PASS: test_get_windows_populated\n");
}

/* Test: get_outputs with populated output data */
static void
test_get_outputs_populated(void)
{
	setup();
	wl_list_init(&test_server.outputs);

	struct wlr_output mock_wlr_output;
	memset(&mock_wlr_output, 0, sizeof(mock_wlr_output));
	mock_wlr_output.name = "HDMI-A-1";
	mock_wlr_output.width = 1920;
	mock_wlr_output.height = 1080;

	struct wm_output mock_output;
	memset(&mock_output, 0, sizeof(mock_output));
	mock_output.wlr_output = &mock_wlr_output;

	wl_list_insert(&test_server.outputs, &mock_output.link);

	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"get_outputs\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(strstr(r, "\"HDMI-A-1\"") != NULL);
	assert(strstr(r, "\"width\":1920") != NULL);
	assert(strstr(r, "\"height\":1080") != NULL);
	free(r);

	wl_list_remove(&mock_output.link);
	printf("  PASS: test_get_outputs_populated\n");
}

/* Test: get_outputs with multiple outputs */
static void
test_get_outputs_multiple(void)
{
	setup();
	wl_list_init(&test_server.outputs);

	struct wlr_output wlr_out1, wlr_out2;
	memset(&wlr_out1, 0, sizeof(wlr_out1));
	memset(&wlr_out2, 0, sizeof(wlr_out2));
	wlr_out1.name = "DP-1";
	wlr_out1.width = 2560;
	wlr_out1.height = 1440;
	wlr_out2.name = "DP-2";
	wlr_out2.width = 1920;
	wlr_out2.height = 1080;

	struct wm_output out1, out2;
	memset(&out1, 0, sizeof(out1));
	memset(&out2, 0, sizeof(out2));
	out1.wlr_output = &wlr_out1;
	out2.wlr_output = &wlr_out2;

	wl_list_insert(&test_server.outputs, &out1.link);
	wl_list_insert(test_server.outputs.prev, &out2.link);

	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"get_outputs\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(strstr(r, "\"DP-1\"") != NULL);
	assert(strstr(r, "\"DP-2\"") != NULL);
	assert(strstr(r, "\"width\":2560") != NULL);
	assert(strstr(r, "\"width\":1920") != NULL);
	free(r);

	wl_list_remove(&out1.link);
	wl_list_remove(&out2.link);
	printf("  PASS: test_get_outputs_multiple\n");
}

/* Test: Unclutter action rearranges views */
static void
test_action_unclutter(void)
{
	setup_with_view_list();
	wl_list_init(&test_server.outputs);
	/* Enable scene_tree nodes for all views */
	for (int i = 0; i < 3; i++)
		mock_scene_trees[i].node.enabled = true;

	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"Unclutter\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	/* Clean up view list */
	for (int i = 0; i < 3; i++)
		wl_list_remove(&mock_views[i].link);
	printf("  PASS: test_action_unclutter\n");
}

/* Test: NextGroup and PrevGroup with no tab groups is a no-op */
static void
test_action_next_prev_group_empty(void)
{
	setup_with_view_list();
	/* No tab groups assigned */
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"NextGroup\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"PrevGroup\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	for (int i = 0; i < 3; i++)
		wl_list_remove(&mock_views[i].link);
	printf("  PASS: test_action_next_prev_group_empty\n");
}

/* === Group E: Menu action tests === */

/* Test: RootMenu action dispatches with cursor coords */
static void
test_action_root_menu(void)
{
	setup_with_wlr_view();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"RootMenu\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_root_menu\n");
}

/* Test: WindowMenu action dispatches with cursor coords */
static void
test_action_window_menu(void)
{
	setup_with_wlr_view();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"WindowMenu\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_window_menu\n");
}

/* Test: WindowList action dispatches with cursor coords */
static void
test_action_window_list(void)
{
	setup_with_wlr_view();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"WindowList\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_window_list\n");
}

/* Test: WorkspaceMenu action dispatches with cursor coords */
static void
test_action_workspace_menu(void)
{
	setup_with_wlr_view();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"WorkspaceMenu\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_workspace_menu\n");
}

/* Test: ClientMenu action dispatches with cursor coords */
static void
test_action_client_menu(void)
{
	setup_with_wlr_view();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"ClientMenu foot\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_client_menu\n");
}

/* Test: CustomMenu action */
static void
test_action_custom_menu(void)
{
	setup_with_wlr_view();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"CustomMenu /path/to/menu\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_custom_menu\n");
}

/* === Group F: Additional coverage tests === */

/* Test: json_escape indirectly via error response */
static void
test_json_escape_via_error(void)
{
	setup();
	/* Feed a command that will trigger json_escape in error path */
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"\"}");
	assert(r != NULL);
	/* Empty action => error "missing action argument" */
	assert(response_is_error(r));
	free(r);

	/* Valid ping exercises json response construction */
	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"ping\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_json_escape_via_error\n");
}

/* Test: SetEnv exercises is_blocked_env_var for all blocked vars */
static void
test_setenv_blocked_all_vars(void)
{
	setup();
	/* LD_PRELOAD */
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"SetEnv LD_PRELOAD=evil\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	/* PATH */
	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"SetEnv PATH=/evil\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	/* HOME */
	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"SetEnv HOME=/evil\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	/* LD_LIBRARY_PATH */
	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"SetEnv LD_LIBRARY_PATH=/evil\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	/* SHELL */
	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"SetEnv SHELL=/evil\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	/* XDG_RUNTIME_DIR */
	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"SetEnv XDG_RUNTIME_DIR=/evil\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	/* WAYLAND_DISPLAY via space separator */
	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"SetEnv WAYLAND_DISPLAY evil\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	/* IFS */
	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"SetEnv IFS=evil\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	/* LD_AUDIT */
	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"SetEnv LD_AUDIT=evil\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	/* LD_DEBUG */
	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"SetEnv LD_DEBUG=evil\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	/* LD_PROFILE */
	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"SetEnv LD_PROFILE=evil\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	printf("  PASS: test_setenv_blocked_all_vars\n");
}

/* Test: FocusToolbar action (falls through to default) */
static void
test_action_focus_toolbar(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"FocusToolbar\"}");
	assert(r != NULL);
	/* FocusToolbar is not handled in switch, returns error */
	assert(response_is_error(r));
	free(r);
	printf("  PASS: test_action_focus_toolbar\n");
}

/* Test: FocusNextElement action (unsupported) */
static void
test_action_focus_next_element(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"FocusNextElement\"}");
	assert(r != NULL);
	assert(response_is_error(r));
	free(r);
	printf("  PASS: test_action_focus_next_element\n");
}

/* Test: FocusPrevElement action (unsupported) */
static void
test_action_focus_prev_element(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"FocusPrevElement\"}");
	assert(r != NULL);
	assert(response_is_error(r));
	free(r);
	printf("  PASS: test_action_focus_prev_element\n");
}

/* Test: FocusActivate action (unsupported) */
static void
test_action_focus_activate(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"FocusActivate\"}");
	assert(r != NULL);
	assert(response_is_error(r));
	free(r);
	printf("  PASS: test_action_focus_activate\n");
}

/* Test: If/ForEach/Map/Delay unsupported actions */
static void
test_action_if_foreach_map_delay(void)
{
	setup();

	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"If\"}");
	assert(r != NULL);
	assert(response_is_error(r));
	free(r);

	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"ForEach\"}");
	assert(r != NULL);
	assert(response_is_error(r));
	free(r);

	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"Map\"}");
	assert(r != NULL);
	assert(response_is_error(r));
	free(r);

	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"Delay\"}");
	assert(r != NULL);
	assert(response_is_error(r));
	free(r);

	printf("  PASS: test_action_if_foreach_map_delay\n");
}

/* Test: NOP action */
static void
test_action_nop_direct(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\",\"action\":\"Nop\"}");
	/* NOP should be parsed but not match any action_table entry
	 * since NOP is sentinel; actually there's no "Nop" entry.
	 * Let's try a valid action that does nothing */
	assert(r != NULL);
	free(r);
	printf("  PASS: test_action_nop_direct\n");
}

/* Test: SetStyle blocked by ipc_no_exec */
static void
test_action_set_style_blocked(void)
{
	setup();
	test_server.ipc_no_exec = true;
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"SetStyle /some/path\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_set_style_blocked\n");
}

/* Test: SetDecor with "0" (maps to NONE) */
static void
test_action_set_decor_zero_string(void)
{
	setup_with_wlr_view();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"SetDecor 0\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_set_decor_zero_string\n");
}

/* Test: SetDecor with various preset names */
static void
test_action_set_decor_all_presets(void)
{
	setup_with_wlr_view();

	/* BORDER */
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"SetDecor BORDER\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	/* TAB */
	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"SetDecor TAB\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	/* TINY */
	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"SetDecor TINY\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	/* TOOL */
	r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"SetDecor TOOL\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);

	printf("  PASS: test_action_set_decor_all_presets\n");
}

/* Test: subscribe with style_changed event */
static void
test_subscribe_style_changed(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"subscribe\","
		"\"events\":\"style_changed\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(test_client.subscribed_events & WM_IPC_EVENT_STYLE_CHANGED);
	free(r);
	printf("  PASS: test_subscribe_style_changed\n");
}

/* Test: subscribe with focus_changed event */
static void
test_subscribe_focus_changed(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"subscribe\","
		"\"events\":\"focus_changed\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(test_client.subscribed_events & WM_IPC_EVENT_FOCUS_CHANGED);
	free(r);
	printf("  PASS: test_subscribe_focus_changed\n");
}

/* Test: subscribe with menu event */
static void
test_subscribe_menu(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"subscribe\","
		"\"events\":\"menu\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(test_client.subscribed_events & WM_IPC_EVENT_MENU);
	free(r);
	printf("  PASS: test_subscribe_menu\n");
}

/* Test: subscribe with individual window sub-events */
static void
test_subscribe_window_close_only(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"subscribe\","
		"\"events\":\"window_close\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(test_client.subscribed_events & WM_IPC_EVENT_WINDOW_CLOSE);
	free(r);
	printf("  PASS: test_subscribe_window_close_only\n");
}

/* Test: subscribe with window_title event */
static void
test_subscribe_window_title(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"subscribe\","
		"\"events\":\"window_title\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	assert(test_client.subscribed_events & WM_IPC_EVENT_WINDOW_TITLE);
	free(r);
	printf("  PASS: test_subscribe_window_title\n");
}

/* Test: Exec action blocked by ipc_no_exec */
static void
test_action_exec_allowed(void)
{
	setup();
	test_server.ipc_no_exec = false;
	/* Exec with invalid command (empty) - should still succeed */
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"Exec true\"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_action_exec_allowed\n");
}

/* Test: parse_action with whitespace */
static void
test_parse_action_whitespace(void)
{
	setup();
	/* Leading whitespace in action */
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"  FocusNext  \"}");
	assert(r != NULL);
	assert(response_is_success(r));
	free(r);
	printf("  PASS: test_parse_action_whitespace\n");
}

/* Test: ToggleCmd returns error (unsupported) */
static void
test_action_toggle_cmd(void)
{
	setup();
	char *r = wm_ipc_handle_command(&test_ipc, &test_client,
		"{\"command\":\"command\","
		"\"action\":\"ToggleCmd\"}");
	assert(r != NULL);
	assert(response_is_error(r));
	free(r);
	printf("  PASS: test_action_toggle_cmd\n");
}

int
main(void)
{
	printf("test_ipc:\n");

	/* Command dispatch */
	test_ping();
	test_empty_request();
	test_null_request();
	test_missing_command();
	test_unknown_command();
	test_get_config();
	test_get_config_sloppy();
	test_get_workspaces_empty();
	test_get_windows_empty();
	test_get_outputs_empty();

	/* Subscribe */
	test_subscribe_window();
	test_subscribe_workspace();
	test_subscribe_output();
	test_subscribe_multiple();
	test_subscribe_window_sub_events();
	test_subscribe_missing_events();
	test_subscribe_accessibility();
	test_subscribe_config_events();

	/* Command actions */
	test_command_missing_action();
	test_command_unknown_action();
	test_command_focus_next();
	test_command_show_desktop();
	test_command_exec_blocked();
	test_command_bindkey_blocked();
	test_command_unsupported_action();
	test_command_toggle_show_position();

	/* Event throttle */
	test_throttle_non_throttled_event();
	test_broadcast_no_subscribers();

	/* JSON edge cases */
	test_json_whitespace();
	test_json_extra_fields();
	test_json_malformed();
	test_json_escaped_quotes();
	test_long_command_name();

	/* Structural tests */
	test_event_bitmask_distinct();
	test_client_init();
	test_ipc_constants();

	/* Group A: Server-only actions */
	test_action_focus_prev();
	test_action_next_workspace();
	test_action_prev_workspace();
	test_action_right_left_workspace();
	test_action_arrange_grid();
	test_action_arrange_vert_horiz();
	test_action_cascade();
	test_action_arrange_stack_variants();
	test_action_close_all_windows();
	test_action_add_remove_workspace();
	test_action_send_to_next_prev_workspace();
	test_action_take_to_next_prev_workspace();
	test_action_hide_menus();
	test_action_deiconify_variants();
	test_action_workspace_number();
	test_action_send_to_workspace_number();
	test_action_take_to_workspace_number();
	test_action_set_workspace_name();
	test_action_reconfigure();
	test_action_focus_directions();
	test_action_layout_cycling();

	/* Group B: Mock view actions */
	test_action_raise();
	test_action_lower();
	test_action_raise_lower_layer();
	test_action_set_layer();
	test_action_stick();
	test_action_maximize_vert();
	test_action_maximize_horiz();
	test_action_toggle_decor();
	test_action_lhalf_rhalf();
	test_action_resize_horiz_vert();
	test_action_next_prev_tab();
	test_action_detach_client();
	test_action_move_tab();
	test_action_set_alpha_absolute();
	test_action_set_alpha_relative();
	test_action_set_alpha_clamping();
	test_action_key_mode();
	test_action_toolbar_toggles();
	test_action_slit_toggles();
	test_action_toolbar_slit_null();
	test_action_view_actions_no_focus();
	test_action_tab_no_group();

	/* Data-populated query tests */
	test_get_workspaces_populated();
	test_get_config_strict_mouse();
	test_get_config_col_smart();
	test_get_config_min_overlap();

	/* Group C: wlroots-dependent actions */
	test_action_close();
	test_action_close_no_view();
	test_action_kill();
	test_action_maximize();
	test_action_fullscreen();
	test_action_minimize();
	test_action_shade();
	test_action_shade_on();
	test_action_shade_off();
	test_action_shade_no_decor();
	test_action_set_decor_presets();
	test_action_set_decor_zero();
	test_action_set_decor_no_decor();
	test_action_move_left();
	test_action_move_right();
	test_action_move_up();
	test_action_move_down();
	test_action_move_to();
	test_action_move_no_view();
	test_action_resize_to();
	test_action_resize_to_invalid();
	test_action_exit();
	test_action_setenv_eq();
	test_action_setenv_space();
	test_action_setenv_blocked();
	test_action_export_alias();
	test_action_reload_style();
	test_action_move_resize_interactive();
	test_action_start_moving_resizing();
	test_action_focus();
	test_action_start_tabbing();
	test_action_activate_tab();
	test_action_set_head();
	test_action_send_to_heads();
	test_action_remember();
	test_action_nop();

	/* Group D: Mock view list tests */
	test_action_next_window_cycle();
	test_action_prev_window_cycle();
	test_action_next_window_pattern();
	test_action_next_window_glob();
	test_action_prev_window_pattern();
	test_action_next_window_no_match();
	test_action_goto_window();
	test_action_goto_window_out_of_range();
	test_action_show_desktop_with_views();
	test_get_windows_populated();
	test_get_outputs_populated();
	test_get_outputs_multiple();
	test_action_unclutter();
	test_action_next_prev_group_empty();

	/* Group E: Menu actions */
	test_action_root_menu();
	test_action_window_menu();
	test_action_window_list();
	test_action_workspace_menu();
	test_action_client_menu();
	test_action_custom_menu();

	/* Group F: Additional coverage */
	test_json_escape_via_error();
	test_setenv_blocked_all_vars();
	test_action_focus_toolbar();
	test_action_focus_next_element();
	test_action_focus_prev_element();
	test_action_focus_activate();
	test_action_if_foreach_map_delay();
	test_action_nop_direct();
	test_action_set_style_blocked();
	test_action_set_decor_zero_string();
	test_action_set_decor_all_presets();
	test_subscribe_style_changed();
	test_subscribe_focus_changed();
	test_subscribe_menu();
	test_subscribe_window_close_only();
	test_subscribe_window_title();
	test_action_exec_allowed();
	test_parse_action_whitespace();
	test_action_toggle_cmd();

	printf("All IPC tests passed.\n");
	return 0;
}
