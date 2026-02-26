/*
 * test_workspace.c - Unit tests for workspace management logic
 *
 * Includes workspace.c directly with stub implementations to avoid
 * needing wlroots/wayland libraries at link time.
 */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

/* --- Block real headers via their include guards --- */
#define WAYLAND_SERVER_CORE_H
#define WAYLAND_SERVER_H
#define WAYLAND_UTIL_H
#define WLR_UTIL_LOG_H
#define WLR_TYPES_WLR_SCENE_H
#define WLR_TYPES_WLR_OUTPUT_LAYOUT_H
#define WLR_TYPES_WLR_XDG_SHELL_H
#define WM_CONFIG_H
#define WM_IPC_H
#define WM_OUTPUT_H
#define WM_SERVER_H
#define WM_TOOLBAR_H
#define WM_VIEW_H
#define WM_WORKSPACE_H

/* --- Stub wayland types --- */

struct wl_list {
	struct wl_list *prev;
	struct wl_list *next;
};

static inline void
wl_list_init(struct wl_list *list)
{
	list->prev = list;
	list->next = list;
}

static inline void
wl_list_insert(struct wl_list *list, struct wl_list *elm)
{
	elm->prev = list;
	elm->next = list->next;
	list->next = elm;
	elm->next->prev = elm;
}

static inline void
wl_list_remove(struct wl_list *elm)
{
	elm->prev->next = elm->next;
	elm->next->prev = elm->prev;
	elm->prev = NULL;
	elm->next = NULL;
}

static inline int
wl_list_empty(const struct wl_list *list)
{
	return list->next == list;
}

#define wl_container_of(ptr, sample, member) \
	(__typeof__(sample))((char *)(ptr) - \
		offsetof(__typeof__(*sample), member))

#define wl_list_for_each(pos, head, member) \
	for (pos = wl_container_of((head)->next, pos, member); \
	     &pos->member != (head); \
	     pos = wl_container_of(pos->member.next, pos, member))

#define wl_list_for_each_safe(pos, tmp, head, member) \
	for (pos = wl_container_of((head)->next, pos, member), \
	     tmp = wl_container_of((pos)->member.next, tmp, member); \
	     &(pos)->member != (head); \
	     pos = tmp, \
	     tmp = wl_container_of((pos)->member.next, tmp, member))

/* --- Stub wlr types --- */

struct wlr_scene_node {
	int enabled;
	int x, y;
	void *data;
	struct wlr_scene_tree *parent;
	struct wl_list link;
};

struct wlr_scene_tree {
	struct wlr_scene_node node;
	struct wl_list children;
};

struct wlr_output_layout {
	int dummy;
};

struct wlr_output {
	char *name;
};

struct wlr_box {
	int x, y, width, height;
};

struct wlr_surface {
	int dummy;
};

struct wlr_xdg_surface {
	struct wlr_surface *surface;
};

struct wlr_xdg_toplevel {
	struct wlr_xdg_surface *base;
};

/* wlr_log no-op */
#define wlr_log(verb, fmt, ...) ((void)0)
#define WLR_ERROR 1
#define WLR_DEBUG 7

/* --- Stub wm types (workspace.c dependencies) --- */

enum wm_workspace_mode {
	WM_WORKSPACE_GLOBAL = 0,
	WM_WORKSPACE_PER_OUTPUT,
};

struct wm_config {
	int workspace_count;
	char **workspace_names;
	int workspace_name_count;
	bool workspace_warping;
	enum wm_workspace_mode workspace_mode;
};

/* IPC event types */
enum wm_ipc_event {
	WM_IPC_EVENT_WORKSPACE = (1 << 4),
};

struct wm_ipc_server {
	int dummy;
};

struct wm_toolbar {
	int dummy;
};

struct wm_style {
	int dummy;
};

struct wm_decoration {
	struct wlr_scene_tree *tree;
};

struct wm_animation {
	int dummy;
};

struct wm_tab_group {
	struct wl_list views;
	int count;
};

struct wm_rules {
	int dummy;
};

/* Workspace struct (from workspace.h, blocked) */
struct wm_workspace {
	struct wl_list link;
	struct wm_server *server;
	struct wlr_scene_tree *tree;
	char *name;
	int index;
};

#define WM_DEFAULT_WORKSPACE_COUNT 4

/* Output struct */
struct wm_output {
	struct wl_list link;
	struct wm_server *server;
	struct wlr_output *wlr_output;
	void *scene_output;
	struct wlr_box usable_area;
	struct wm_workspace *active_workspace;
};

/* View struct (minimal fields workspace.c accesses) */
struct wm_view {
	struct wl_list link;
	struct wm_server *server;
	struct wlr_xdg_toplevel *xdg_toplevel;
	struct wlr_scene_tree *scene_tree;
	uint32_t id;
	int x, y;
	struct wm_workspace *workspace;
	bool sticky;
	int layer;
	struct wm_tab_group *tab_group;
	struct wl_list tab_link;
	struct wm_decoration *decoration;
	struct wm_animation *animation;
};

/* Server struct (minimal fields workspace.c accesses) */
struct wm_server {
	struct wl_list workspaces;
	struct wm_workspace *current_workspace;
	int workspace_count;
	struct wm_config *config;
	struct wm_toolbar *toolbar;
	struct wm_ipc_server ipc;
	struct wl_list views;
	struct wm_view *focused_view;
	struct wlr_output_layout *output_layout;
	struct wl_list outputs;
	struct wlr_scene_tree *view_layer_normal;
	struct wlr_scene_tree *sticky_tree;
	struct wm_style *style;
	void *seat;
};

/* --- Tracking globals --- */

static int g_toolbar_ws_update_count;
static int g_toolbar_iconbar_update_count;
static int g_ipc_broadcast_count;
static int g_focus_view_count;
static int g_unfocus_count;
static struct wm_view *g_last_focused_view;
static struct wm_output *g_mock_focused_output;

/* --- Scene tree pool for wlr_scene_tree_create --- */

#define MAX_SCENE_TREES 32
static struct wlr_scene_tree g_scene_tree_pool[MAX_SCENE_TREES];
static int g_scene_tree_next;

/* --- Stub wlr functions --- */

static struct wlr_scene_tree *
wlr_scene_tree_create(struct wlr_scene_tree *parent)
{
	if (g_scene_tree_next >= MAX_SCENE_TREES)
		return NULL;
	struct wlr_scene_tree *t = &g_scene_tree_pool[g_scene_tree_next++];
	memset(t, 0, sizeof(*t));
	wl_list_init(&t->children);
	return t;
}

static void
wlr_scene_node_set_enabled(struct wlr_scene_node *node, bool enabled)
{
	if (node)
		node->enabled = enabled ? 1 : 0;
}

static void
wlr_scene_node_raise_to_top(struct wlr_scene_node *node)
{
	(void)node;
}

static void
wlr_scene_node_destroy(struct wlr_scene_node *node)
{
	(void)node;
}

static void
wlr_scene_node_reparent(struct wlr_scene_node *node,
	struct wlr_scene_tree *new_parent)
{
	(void)node;
	(void)new_parent;
}

static struct wlr_output *
wlr_output_layout_output_at(struct wlr_output_layout *layout,
	double x, double y)
{
	(void)layout;
	(void)x;
	(void)y;
	return NULL; /* Views won't have a resolved output in tests */
}

/* --- Stub wm functions --- */

static void
wm_ipc_broadcast_event(struct wm_ipc_server *ipc, uint32_t event,
	const char *buf)
{
	(void)ipc;
	(void)event;
	(void)buf;
	g_ipc_broadcast_count++;
}

static void
wm_toolbar_update_workspace(struct wm_toolbar *toolbar)
{
	(void)toolbar;
	g_toolbar_ws_update_count++;
}

static void
wm_toolbar_update_iconbar(struct wm_toolbar *toolbar)
{
	(void)toolbar;
	g_toolbar_iconbar_update_count++;
}

static void
wm_focus_view(struct wm_view *view, struct wlr_surface *surface)
{
	(void)surface;
	g_last_focused_view = view;
	g_focus_view_count++;
}

static void
wm_unfocus_current(struct wm_server *server)
{
	(void)server;
	g_unfocus_count++;
}

static struct wm_output *
wm_server_get_focused_output(struct wm_server *server)
{
	(void)server;
	return g_mock_focused_output;
}

static struct wm_output *
wm_output_from_wlr(struct wm_server *server, struct wlr_output *wlr_output)
{
	(void)server;
	(void)wlr_output;
	return NULL;
}

/* --- Forward declarations for functions defined later in workspace.c --- */
struct wm_workspace *wm_workspace_get(struct wm_server *server, int index);
struct wm_workspace *wm_workspace_get_active(struct wm_server *server);
void wm_view_set_sticky(struct wm_view *view, bool sticky);
void wm_workspace_switch(struct wm_server *server, int index);
void wm_view_move_to_workspace(struct wm_view *view,
	struct wm_workspace *workspace);

/* --- Include workspace source directly --- */

#include "workspace.c"

/* --- Test helpers --- */

static struct wm_server test_server;
static struct wm_config test_config;
static struct wm_toolbar test_toolbar;
static struct wlr_output_layout test_output_layout;
static struct wlr_scene_tree test_normal_tree;
static struct wlr_scene_tree test_sticky_tree_init;

/* Mock views for workspace switch tests */
#define MAX_TEST_VIEWS 8
static struct wm_view test_views[MAX_TEST_VIEWS];
static struct wlr_scene_tree test_view_trees[MAX_TEST_VIEWS];
static struct wlr_xdg_toplevel test_toplevels[MAX_TEST_VIEWS];
static struct wlr_xdg_surface test_xdg_surfaces[MAX_TEST_VIEWS];
static struct wlr_surface test_surfaces[MAX_TEST_VIEWS];

/* Mock output for per-output tests */
static struct wm_output test_output;
static struct wlr_output test_wlr_output;

static void
reset_globals(void)
{
	g_toolbar_ws_update_count = 0;
	g_toolbar_iconbar_update_count = 0;
	g_ipc_broadcast_count = 0;
	g_focus_view_count = 0;
	g_unfocus_count = 0;
	g_last_focused_view = NULL;
	g_mock_focused_output = NULL;
	g_scene_tree_next = 0;
}

static void
init_test_server(int ws_count, enum wm_workspace_mode mode)
{
	reset_globals();
	memset(&test_server, 0, sizeof(test_server));
	memset(&test_config, 0, sizeof(test_config));
	memset(&test_toolbar, 0, sizeof(test_toolbar));
	memset(&test_normal_tree, 0, sizeof(test_normal_tree));

	test_config.workspace_mode = mode;
	test_config.workspace_name_count = 0;
	test_config.workspace_names = NULL;

	test_server.config = &test_config;
	test_server.toolbar = &test_toolbar;
	test_server.output_layout = &test_output_layout;
	test_server.view_layer_normal = &test_normal_tree;
	test_server.focused_view = NULL;
	wl_list_init(&test_server.views);
	wl_list_init(&test_server.outputs);

	wm_workspaces_init(&test_server, ws_count);
}

static void
cleanup_test_server(void)
{
	wm_workspaces_finish(&test_server);
}

/* Create a mock view on a given workspace */
static void
setup_mock_view(int idx, struct wm_workspace *ws)
{
	assert(idx < MAX_TEST_VIEWS);
	memset(&test_views[idx], 0, sizeof(test_views[idx]));
	memset(&test_view_trees[idx], 0, sizeof(test_view_trees[idx]));

	test_surfaces[idx].dummy = 0;
	test_xdg_surfaces[idx].surface = &test_surfaces[idx];
	test_toplevels[idx].base = &test_xdg_surfaces[idx];

	test_view_trees[idx].node.enabled = 1;
	test_views[idx].server = &test_server;
	test_views[idx].xdg_toplevel = &test_toplevels[idx];
	test_views[idx].scene_tree = &test_view_trees[idx];
	test_views[idx].workspace = ws;
	test_views[idx].id = idx + 1;
	test_views[idx].x = 100;
	test_views[idx].y = 100;

	wl_list_insert(test_server.views.prev, &test_views[idx].link);
}

/* ===== json_escape_buf tests ===== */

static void
test_json_escape_buf_normal(void)
{
	char buf[64];
	json_escape_buf(buf, sizeof(buf), "hello world");
	assert(strcmp(buf, "hello world") == 0);
	printf("  PASS: json_escape_buf_normal\n");
}

static void
test_json_escape_buf_quotes(void)
{
	char buf[64];
	json_escape_buf(buf, sizeof(buf), "say \"hello\"");
	assert(strcmp(buf, "say \\\"hello\\\"") == 0);
	printf("  PASS: json_escape_buf_quotes\n");
}

static void
test_json_escape_buf_backslash(void)
{
	char buf[64];
	json_escape_buf(buf, sizeof(buf), "path\\to\\file");
	assert(strcmp(buf, "path\\\\to\\\\file") == 0);
	printf("  PASS: json_escape_buf_backslash\n");
}

static void
test_json_escape_buf_control_chars(void)
{
	char buf[64];
	json_escape_buf(buf, sizeof(buf), "line1\nline2\ttab");
	/* Control chars (< 0x20) are skipped */
	assert(strcmp(buf, "line1line2tab") == 0);
	printf("  PASS: json_escape_buf_control_chars\n");
}

static void
test_json_escape_buf_null(void)
{
	char buf[64];
	buf[0] = 'x';
	json_escape_buf(buf, sizeof(buf), NULL);
	assert(buf[0] == '\0');
	printf("  PASS: json_escape_buf_null\n");
}

static void
test_json_escape_buf_empty(void)
{
	char buf[64];
	json_escape_buf(buf, sizeof(buf), "");
	assert(buf[0] == '\0');
	printf("  PASS: json_escape_buf_empty\n");
}

static void
test_json_escape_buf_mixed(void)
{
	char buf[64];
	json_escape_buf(buf, sizeof(buf), "a\"b\\c\nd");
	/* quote escaped, backslash escaped, newline skipped */
	assert(strcmp(buf, "a\\\"b\\\\cd") == 0);
	printf("  PASS: json_escape_buf_mixed\n");
}

/* ===== wm_workspace_get tests ===== */

static void
test_workspace_get_valid(void)
{
	init_test_server(4, WM_WORKSPACE_GLOBAL);

	struct wm_workspace *ws0 = wm_workspace_get(&test_server, 0);
	assert(ws0 != NULL);
	assert(ws0->index == 0);

	struct wm_workspace *ws3 = wm_workspace_get(&test_server, 3);
	assert(ws3 != NULL);
	assert(ws3->index == 3);

	cleanup_test_server();
	printf("  PASS: workspace_get_valid\n");
}

static void
test_workspace_get_invalid(void)
{
	init_test_server(4, WM_WORKSPACE_GLOBAL);

	struct wm_workspace *ws = wm_workspace_get(&test_server, 10);
	assert(ws == NULL);

	ws = wm_workspace_get(&test_server, -1);
	assert(ws == NULL);

	cleanup_test_server();
	printf("  PASS: workspace_get_invalid\n");
}

/* ===== wm_workspace_get_active tests ===== */

static void
test_workspace_get_active_global(void)
{
	init_test_server(4, WM_WORKSPACE_GLOBAL);

	/* In global mode, get_active returns current_workspace */
	struct wm_workspace *active = wm_workspace_get_active(&test_server);
	assert(active == test_server.current_workspace);
	assert(active->index == 0);

	/* Switch to workspace 2 and verify */
	test_server.current_workspace = wm_workspace_get(&test_server, 2);
	active = wm_workspace_get_active(&test_server);
	assert(active->index == 2);

	cleanup_test_server();
	printf("  PASS: workspace_get_active_global\n");
}

static void
test_workspace_get_active_per_output(void)
{
	init_test_server(4, WM_WORKSPACE_PER_OUTPUT);

	/* Set up a mock focused output */
	memset(&test_output, 0, sizeof(test_output));
	test_output.active_workspace = wm_workspace_get(&test_server, 2);
	g_mock_focused_output = &test_output;

	struct wm_workspace *active = wm_workspace_get_active(&test_server);
	assert(active != NULL);
	assert(active->index == 2);

	/* Without focused output, falls back to current_workspace */
	g_mock_focused_output = NULL;
	active = wm_workspace_get_active(&test_server);
	assert(active == test_server.current_workspace);
	assert(active->index == 0);

	cleanup_test_server();
	printf("  PASS: workspace_get_active_per_output\n");
}

/* ===== wm_workspace_set_name tests ===== */

static void
test_workspace_set_name(void)
{
	init_test_server(4, WM_WORKSPACE_GLOBAL);

	wm_workspace_set_name(&test_server, "My Desktop");
	assert(strcmp(test_server.current_workspace->name, "My Desktop") == 0);
	assert(g_toolbar_ws_update_count == 1);

	cleanup_test_server();
	printf("  PASS: workspace_set_name\n");
}

static void
test_workspace_set_name_null(void)
{
	init_test_server(4, WM_WORKSPACE_GLOBAL);
	const char *old_name = test_server.current_workspace->name;

	wm_workspace_set_name(&test_server, NULL);
	/* Should be a no-op, name unchanged */
	assert(test_server.current_workspace->name == old_name);

	cleanup_test_server();
	printf("  PASS: workspace_set_name_null\n");
}

/* ===== Switch next/prev (wrapping) tests ===== */

static void
test_workspace_switch_next_wrap(void)
{
	init_test_server(4, WM_WORKSPACE_GLOBAL);

	/* Move to last workspace */
	wm_workspace_switch(&test_server, 3);
	assert(test_server.current_workspace->index == 3);

	/* switch_next from last should wrap to 0 */
	wm_workspace_switch_next(&test_server);
	assert(test_server.current_workspace->index == 0);

	cleanup_test_server();
	printf("  PASS: workspace_switch_next_wrap\n");
}

static void
test_workspace_switch_prev_wrap(void)
{
	init_test_server(4, WM_WORKSPACE_GLOBAL);

	/* At workspace 0 */
	assert(test_server.current_workspace->index == 0);

	/* switch_prev from first should wrap to last */
	wm_workspace_switch_prev(&test_server);
	assert(test_server.current_workspace->index == 3);

	cleanup_test_server();
	printf("  PASS: workspace_switch_prev_wrap\n");
}

static void
test_workspace_switch_next_sequential(void)
{
	init_test_server(4, WM_WORKSPACE_GLOBAL);

	/* Sequential next from 0 */
	wm_workspace_switch_next(&test_server);
	assert(test_server.current_workspace->index == 1);
	wm_workspace_switch_next(&test_server);
	assert(test_server.current_workspace->index == 2);

	cleanup_test_server();
	printf("  PASS: workspace_switch_next_sequential\n");
}

/* ===== Switch right/left (clamping, no wrap) tests ===== */

static void
test_workspace_switch_right_clamp(void)
{
	init_test_server(4, WM_WORKSPACE_GLOBAL);

	/* Move to last workspace */
	wm_workspace_switch(&test_server, 3);
	assert(test_server.current_workspace->index == 3);

	/* switch_right at boundary should not wrap */
	wm_workspace_switch_right(&test_server);
	assert(test_server.current_workspace->index == 3);

	cleanup_test_server();
	printf("  PASS: workspace_switch_right_clamp\n");
}

static void
test_workspace_switch_left_clamp(void)
{
	init_test_server(4, WM_WORKSPACE_GLOBAL);

	/* At workspace 0 */
	assert(test_server.current_workspace->index == 0);

	/* switch_left at boundary should not wrap */
	wm_workspace_switch_left(&test_server);
	assert(test_server.current_workspace->index == 0);

	cleanup_test_server();
	printf("  PASS: workspace_switch_left_clamp\n");
}

static void
test_workspace_switch_right_normal(void)
{
	init_test_server(4, WM_WORKSPACE_GLOBAL);

	/* switch_right from 0 -> 1 */
	wm_workspace_switch_right(&test_server);
	assert(test_server.current_workspace->index == 1);

	cleanup_test_server();
	printf("  PASS: workspace_switch_right_normal\n");
}

/* ===== wm_workspace_add/remove tests ===== */

static void
test_workspace_add(void)
{
	init_test_server(4, WM_WORKSPACE_GLOBAL);
	assert(test_server.workspace_count == 4);

	wm_workspace_add(&test_server);
	assert(test_server.workspace_count == 5);

	struct wm_workspace *ws4 = wm_workspace_get(&test_server, 4);
	assert(ws4 != NULL);
	assert(ws4->index == 4);
	assert(strstr(ws4->name, "Workspace 5") != NULL);

	cleanup_test_server();
	printf("  PASS: workspace_add\n");
}

static void
test_workspace_remove_last(void)
{
	init_test_server(4, WM_WORKSPACE_GLOBAL);
	assert(test_server.workspace_count == 4);

	wm_workspace_remove_last(&test_server);
	assert(test_server.workspace_count == 3);

	/* Workspace 3 should no longer exist */
	struct wm_workspace *ws3 = wm_workspace_get(&test_server, 3);
	assert(ws3 == NULL);

	cleanup_test_server();
	printf("  PASS: workspace_remove_last\n");
}

static void
test_workspace_remove_last_refuses_single(void)
{
	init_test_server(1, WM_WORKSPACE_GLOBAL);
	assert(test_server.workspace_count == 1);

	wm_workspace_remove_last(&test_server);
	/* Should refuse to remove last workspace */
	assert(test_server.workspace_count == 1);

	cleanup_test_server();
	printf("  PASS: workspace_remove_last_refuses_single\n");
}

/* ===== Workspace switch with views ===== */

static void
test_workspace_switch_focuses_view(void)
{
	init_test_server(4, WM_WORKSPACE_GLOBAL);

	/* Put a view on workspace 1 */
	struct wm_workspace *ws1 = wm_workspace_get(&test_server, 1);
	setup_mock_view(0, ws1);

	/* Switch to workspace 1 — should focus the view */
	reset_globals();
	wm_workspace_switch(&test_server, 1);
	assert(test_server.current_workspace->index == 1);
	assert(g_focus_view_count == 1);
	assert(g_last_focused_view == &test_views[0]);

	/* Clean up view from list */
	wl_list_remove(&test_views[0].link);
	cleanup_test_server();
	printf("  PASS: workspace_switch_focuses_view\n");
}

static void
test_workspace_switch_empty_unfocuses(void)
{
	init_test_server(4, WM_WORKSPACE_GLOBAL);

	/* No views on workspace 2 */
	reset_globals();
	wm_workspace_switch(&test_server, 2);
	assert(test_server.current_workspace->index == 2);
	assert(g_unfocus_count == 1);

	cleanup_test_server();
	printf("  PASS: workspace_switch_empty_unfocuses\n");
}

/* ===== Default workspace names ===== */

static void
test_workspace_default_names(void)
{
	init_test_server(3, WM_WORKSPACE_GLOBAL);

	struct wm_workspace *ws0 = wm_workspace_get(&test_server, 0);
	struct wm_workspace *ws1 = wm_workspace_get(&test_server, 1);
	struct wm_workspace *ws2 = wm_workspace_get(&test_server, 2);

	assert(strcmp(ws0->name, "Workspace 1") == 0);
	assert(strcmp(ws1->name, "Workspace 2") == 0);
	assert(strcmp(ws2->name, "Workspace 3") == 0);

	cleanup_test_server();
	printf("  PASS: workspace_default_names\n");
}

/* ===== Main ===== */

int
main(void)
{
	printf("test_workspace:\n");

	/* json_escape_buf */
	test_json_escape_buf_normal();
	test_json_escape_buf_quotes();
	test_json_escape_buf_backslash();
	test_json_escape_buf_control_chars();
	test_json_escape_buf_null();
	test_json_escape_buf_empty();
	test_json_escape_buf_mixed();

	/* workspace_get */
	test_workspace_get_valid();
	test_workspace_get_invalid();

	/* workspace_get_active */
	test_workspace_get_active_global();
	test_workspace_get_active_per_output();

	/* workspace_set_name */
	test_workspace_set_name();
	test_workspace_set_name_null();

	/* switch next/prev (wrapping) */
	test_workspace_switch_next_wrap();
	test_workspace_switch_prev_wrap();
	test_workspace_switch_next_sequential();

	/* switch right/left (clamping) */
	test_workspace_switch_right_clamp();
	test_workspace_switch_left_clamp();
	test_workspace_switch_right_normal();

	/* add/remove */
	test_workspace_add();
	test_workspace_remove_last();
	test_workspace_remove_last_refuses_single();

	/* switch with views */
	test_workspace_switch_focuses_view();
	test_workspace_switch_empty_unfocuses();

	/* naming */
	test_workspace_default_names();

	printf("All workspace tests passed.\n");
	return 0;
}
