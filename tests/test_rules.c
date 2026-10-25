/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * test_rules.c - The apps file read and applied the way Fluxbox does
 *
 * Each test writes an apps file, loads it, and checks what happens to
 * windows: which entry they match, and the geometry and state the entry
 * gives them. The compositor around rules.c is stubbed below and records
 * what it was asked to do.
 */

#define _GNU_SOURCE

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>

#include "client_pattern.h"
#include "decoration.h"
#include "fbcompat.h"
#include "output.h"
#include "rules.h"
#include "server.h"
#include "view.h"
#include "view_geometry.h"
#include "workspace.h"

/* --- The world rules.c sees --- */

static struct wm_server g_server;
static struct wlr_cursor g_cursor;
static struct wlr_output g_wlr_out[2];
static struct wm_output g_out[2];
static int g_out_count;
static struct wm_workspace g_ws[4];
static int g_ws_current;
static int g_ws_switched_to;

static struct wlr_box g_geo;          /* client size */
static int g_ext_top = 20, g_ext_bottom = 6, g_ext_side = 1;
static int g_size_w, g_size_h;        /* last wm_view_set_size */
static int g_head_set;                /* last wm_view_set_head */
static int g_minimized;
static struct wm_view *g_transient;   /* the one window with a parent */

/* view.c / view_geometry.c */

struct wm_view *
wm_view_parent(struct wm_view *view)
{
	return view == g_transient ? view : NULL;
}

struct wlr_surface *
wm_view_surface(struct wm_view *view)
{
	static struct wlr_surface surface;
	(void)view;
	return &surface;
}

void
wm_view_content_geometry(struct wm_view *view, struct wlr_box *geo)
{
	(void)view;
	*geo = g_geo;
}

void
wm_view_get_geometry(struct wm_view *view, struct wlr_box *geo)
{
	(void)view;
	*geo = g_geo;
}

void
wm_view_set_size(struct wm_view *view, int w, int h)
{
	(void)view;
	g_size_w = w;
	g_size_h = h;
}

void
wm_view_frame_extents(struct wm_view *view, int *top, int *bottom,
	int *left, int *right)
{
	(void)view;
	*top = g_ext_top;
	*bottom = g_ext_bottom;
	*left = *right = g_ext_side;
}

void
wm_view_frame_box(struct wm_view *view, struct wlr_box *box)
{
	box->x = view->x;
	box->y = view->y;
	box->width = g_geo.width + 2 * g_ext_side;
	box->height = g_geo.height + g_ext_top + g_ext_bottom;
}

void
wm_view_move(struct wm_view *view, int x, int y)
{
	view->x = x;
	view->y = y;
}

void
wm_view_toggle_maximize(struct wm_view *view)
{
	view->maximized = !view->maximized;
}

void
wm_view_toggle_fullscreen(struct wm_view *view)
{
	view->fullscreen = !view->fullscreen;
}

void
wm_view_maximize_horiz(struct wm_view *view)
{
	view->maximized_horiz = !view->maximized_horiz;
}

void
wm_view_maximize_vert(struct wm_view *view)
{
	view->maximized_vert = !view->maximized_vert;
}

void
wm_view_minimize(struct wm_view *view)
{
	(void)view;
	g_minimized++;
}

void
wm_view_set_head(struct wm_view *view, int head)
{
	(void)view;
	g_head_set = head;
}

void
wm_view_set_layer(struct wm_view *view, enum wm_view_layer layer)
{
	view->layer = layer;
}

void
wm_view_set_sticky(struct wm_view *view, bool sticky)
{
	view->sticky = sticky;
}

void
wm_view_move_to_workspace(struct wm_view *view, struct wm_workspace *ws)
{
	view->workspace = ws;
}

/* decoration.c */

void
wm_decoration_set_preset(struct wm_decoration *d, enum wm_decor_preset p,
	struct wm_style *style)
{
	(void)style;
	d->preset = p;
}

void
wm_decoration_set_shaded(struct wm_decoration *d, bool shaded,
	struct wm_style *style)
{
	(void)style;
	d->shaded = shaded;
}

/* workspace.c */

struct wm_workspace *
wm_workspace_get(struct wm_server *server, int index)
{
	(void)server;
	return index >= 0 && index < 4 ? &g_ws[index] : NULL;
}

struct wm_workspace *
wm_workspace_get_active(struct wm_server *server)
{
	(void)server;
	return &g_ws[g_ws_current];
}

void
wm_workspace_switch(struct wm_server *server, int index)
{
	(void)server;
	g_ws_switched_to = index;
}

/* output.c: heads count from 1 */

struct wm_output *
wm_output_head(struct wm_server *server, int head)
{
	(void)server;
	return head >= 1 && head <= g_out_count ? &g_out[head - 1] : NULL;
}

struct wm_output *
wm_output_for_view(struct wm_view *view)
{
	(void)view;
	return &g_out[0];
}

int
wm_output_head_count(struct wm_server *server)
{
	(void)server;
	return g_out_count;
}

int
wm_output_head_number(struct wm_output *output)
{
	return (int)(output - g_out) + 1;
}

/* wlroots */

struct wlr_output *
wlr_output_layout_output_at(struct wlr_output_layout *layout,
	double lx, double ly)
{
	(void)layout; (void)lx; (void)ly;
	return &g_wlr_out[0];
}

void
wlr_output_layout_get_box(struct wlr_output_layout *layout,
	struct wlr_output *output, struct wlr_box *box)
{
	(void)layout; (void)output;
	*box = (struct wlr_box){0};
}

void
wlr_scene_node_set_position(struct wlr_scene_node *node, int x, int y)
{
	node->x = x;
	node->y = y;
}

/* --- Helpers --- */

#define TEST_DIR "/tmp/fluxland-test/rules"
#define APPS TEST_DIR "/apps"

static struct wm_rules rules;
static int tests_run;

static void
write_apps(const char *text)
{
	FILE *f = fopen(APPS, "w");
	assert(f);
	fputs(text, f);
	fclose(f);
}

static void
load(const char *text)
{
	wm_rules_forget_views(&g_server);
	wm_rules_finish(&rules);
	wm_rules_init(&rules);
	write_apps(text);
	assert(wm_rules_load(&rules, APPS));
}

static void
reset_world(void)
{
	memset(&g_server, 0, sizeof(g_server));
	wl_list_init(&g_server.views);
	wl_list_init(&g_server.outputs);
	g_server.cursor = &g_cursor;
	g_out_count = 2;
	for (int i = 0; i < 2; i++) {
		memset(&g_out[i], 0, sizeof(g_out[i]));
		g_out[i].wlr_output = &g_wlr_out[i];
		g_out[i].server = &g_server;
		wl_list_insert(g_server.outputs.prev, &g_out[i].link);
	}
	/* Head 1: 1920x1080 with a 30px toolbar; head 2: 1280x1024 beside it */
	g_out[0].usable_area = (struct wlr_box){0, 0, 1920, 1050};
	g_out[1].usable_area = (struct wlr_box){1920, 0, 1280, 1024};
	for (int i = 0; i < 4; i++) {
		memset(&g_ws[i], 0, sizeof(g_ws[i]));
		g_ws[i].index = i;
	}
	g_ws_current = 0;
	g_ws_switched_to = -1;
	g_geo = (struct wlr_box){0, 0, 640, 480};
	g_size_w = g_size_h = -1;
	g_head_set = 0;
	g_minimized = 0;
	g_transient = NULL;
}

/* A window being mapped: in the server, not shown yet */
static void
new_view(struct wm_view *v, const char *app_id, const char *instance,
	const char *title)
{
	static struct wlr_scene_tree trees[16];
	static struct wm_decoration decos[16];
	static int next;
	memset(v, 0, sizeof(*v));
	int i = next++ % 16;
	memset(&trees[i], 0, sizeof(trees[i]));
	memset(&decos[i], 0, sizeof(decos[i]));
	v->server = &g_server;
	v->scene_tree = &trees[i];
	v->scene_tree->node.enabled = false;
	v->decoration = &decos[i];
	v->app_id = (char *)app_id;
	v->instance = (char *)instance;
	v->title = (char *)title;
	v->layer = WM_LAYER_NORMAL;
	v->focus_alpha = v->unfocus_alpha = 255;
	v->workspace = &g_ws[0];
	wl_list_insert(&g_server.views, &v->link);
}

static void
map_view(struct wm_view *v)
{
	wm_rules_apply(&rules, v);
	v->mapped = true;
}

static void
close_view(struct wm_view *v)
{
	wm_rules_release(&rules, v, APPS);
	wl_list_remove(&v->link);
}

#define TEST(name) \
	static void name(void); \
	static void run_##name(void) \
	{ \
		reset_world(); \
		name(); \
		tests_run++; \
		printf("  PASS: %s\n", #name); \
	} \
	static void name(void)

/* --- Patterns --- */

TEST(test_bare_word_is_name)
{
	load("[app] (xterm)\n  [Workspace] {2}\n[end]\n");
	struct wm_view x11, xdg, other;
	new_view(&x11, "XTerm", "xterm", "bash");     /* X11: class, instance */
	new_view(&xdg, "xterm", NULL, "bash");        /* xdg: app_id only */
	new_view(&other, "uxterm", NULL, "bash");
	map_view(&x11);
	map_view(&xdg);
	map_view(&other);
	assert(x11.workspace == &g_ws[2]);
	assert(xdg.workspace == &g_ws[2]);
	assert(other.workspace == &g_ws[0]);  /* anchored: no substring hit */
}

TEST(test_name_class_role_and_negation)
{
	load("[app] (name=xterm) (class=XTerm) (role=main)\n"
		"  [Workspace] {1}\n[end]\n"
		"[app] (class=XTerm) (title!=root.*)\n"
		"  [Workspace] {3}\n[end]\n");
	struct wm_view a, b, c;
	new_view(&a, "XTerm", "xterm", "bash");
	a.role = "main";
	new_view(&b, "XTerm", "xterm", "bash");
	new_view(&c, "XTerm", "xterm", "root shell");
	map_view(&a);
	map_view(&b);
	map_view(&c);
	assert(a.workspace == &g_ws[1]);
	assert(b.workspace == &g_ws[3]);   /* no role: second entry */
	assert(c.workspace == &g_ws[0]);   /* title!= excludes it */
}

TEST(test_first_match_wins_and_transient)
{
	load("[transient] (class=gimp)\n  [Layer] {Top}\n[end]\n"
		"[app] (class=gimp)\n  [Workspace] {2}\n[end]\n"
		"[app] (class=.*)\n  [Workspace] {1}\n[end]\n");
	struct wm_view main, dialog;
	new_view(&main, "gimp", NULL, "GIMP");
	new_view(&dialog, "gimp", NULL, "Open Image");
	g_transient = &dialog;
	map_view(&main);
	map_view(&dialog);
	assert(main.workspace == &g_ws[2]);
	assert(main.layer == WM_LAYER_NORMAL);
	/* [app] entries don't match dialogs; [transient] ones do */
	assert(dialog.workspace == &g_ws[0]);
	assert(dialog.layer == WM_LAYER_ABOVE);
}

TEST(test_match_limit)
{
	load("[app] (foot) {2}\n  [Workspace] {3}\n[end]\n");
	struct wm_view a, b, c, d;
	new_view(&a, "foot", NULL, "1");
	new_view(&b, "foot", NULL, "2");
	new_view(&c, "foot", NULL, "3");
	map_view(&a);
	map_view(&b);
	map_view(&c);
	assert(a.workspace == &g_ws[3] && b.workspace == &g_ws[3]);
	assert(c.workspace == &g_ws[0]);
	/* A matched window closing frees its place */
	close_view(&a);
	new_view(&d, "foot", NULL, "4");
	map_view(&d);
	assert(d.workspace == &g_ws[3]);
}

TEST(test_bad_pattern_skips_its_entry)
{
	load("[app] (class=[unclosed)\n  [Workspace] {2}\n[end]\n"
		"[app] (class=foot)\n  [Workspace] {1}\n[end]\n");
	struct wm_view v;
	new_view(&v, "foot", NULL, "");
	map_view(&v);
	assert(v.workspace == &g_ws[1]);
}

/* --- Settings --- */

TEST(test_layer_numbers_and_names)
{
	static const struct { const char *val; enum wm_view_layer want; } t[] = {
		{ "0", WM_LAYER_ABOVE }, { "2", WM_LAYER_ABOVE },
		{ "4", WM_LAYER_ABOVE }, { "6", WM_LAYER_ABOVE },
		{ "8", WM_LAYER_NORMAL }, { "10", WM_LAYER_BELOW },
		{ "12", WM_LAYER_DESKTOP }, { "Menu", WM_LAYER_ABOVE },
		{ "AboveDock", WM_LAYER_ABOVE }, { "Dock", WM_LAYER_ABOVE },
		{ "Top", WM_LAYER_ABOVE }, { "Normal", WM_LAYER_NORMAL },
		{ "Bottom", WM_LAYER_BELOW }, { "desktop", WM_LAYER_DESKTOP },
		{ "Above", WM_LAYER_ABOVE }, { "Below", WM_LAYER_BELOW },
	};
	for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
		char buf[128];
		snprintf(buf, sizeof(buf),
			"[app] (a)\n  [Layer] {%s}\n[end]\n", t[i].val);
		load(buf);
		struct wm_view v;
		new_view(&v, "a", NULL, "");
		v.layer = WM_VIEW_LAYER_COUNT;
		map_view(&v);
		assert(v.layer == t[i].want);
		wl_list_remove(&v.link);
	}
	/* Not a layer: the setting is ignored */
	load("[app] (a)\n  [Layer] {Sky}\n[end]\n");
	struct wm_view v;
	new_view(&v, "a", NULL, "");
	map_view(&v);
	assert(v.layer == WM_LAYER_NORMAL);
}

TEST(test_dimensions_pixels_and_percent)
{
	/* Fluxbox sizes are the whole window, decorations included */
	load("[app] (a)\n  [Dimensions] {800 600}\n[end]\n"
		"[app] (b)\n  [Dimensions] {50% 50%}\n[end]\n");
	struct wm_view a, b;
	new_view(&a, "a", NULL, "");
	map_view(&a);
	assert(g_size_w == 800 - 2 && g_size_h == 600 - 26);
	new_view(&b, "b", NULL, "");
	map_view(&b);
	assert(g_size_w == 960 - 2 && g_size_h == 525 - 26);
}

TEST(test_position_corners)
{
	/* Frame of 640x480 client: 642x506 */
	struct { const char *rule; int x, y; } t[] = {
		{ "{10 20}", 10, 20 },
		{ "(UPPERLEFT) {10 20}", 10, 20 },
		{ "(TopRight) {10 20}", 1920 - 642 - 10, 20 },
		{ "(BottomRight) {0 0}", 1920 - 642, 1050 - 506 },
		{ "(LowerLeft) {5 5}", 5, 1050 - 506 - 5 },
		{ "(Center) {0 0}", (1920 - 642) / 2, (1050 - 506) / 2 },
		{ "(WINCENTER) {10 -10}", (1920 - 642) / 2 + 10,
			(1050 - 506) / 2 - 10 },
		{ "(Top) {0 0}", (1920 - 642) / 2, 0 },
		{ "(Right) {0 0}", 1920 - 642, (1050 - 506) / 2 },
		{ "{50% 10%}", 960, 105 },
	};
	for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
		char buf[128];
		snprintf(buf, sizeof(buf),
			"[app] (a)\n  [Position] %s\n[end]\n", t[i].rule);
		load(buf);
		struct wm_view v;
		new_view(&v, "a", NULL, "");
		map_view(&v);
		if (v.x != t[i].x || v.y != t[i].y) {
			fprintf(stderr, "%s: got %d,%d want %d,%d\n",
				t[i].rule, v.x, v.y, t[i].x, t[i].y);
			assert(0);
		}
		assert(v.rule_placed);
		wl_list_remove(&v.link);
	}
	/* An unknown corner makes the setting an error, as in Fluxbox */
	load("[app] (a)\n  [Position] (Middle) {5 5}\n[end]\n");
	struct wm_view v;
	new_view(&v, "a", NULL, "");
	map_view(&v);
	assert(!v.rule_placed && v.x == 0 && v.y == 0);
}

TEST(test_position_uses_new_dimensions)
{
	load("[app] (a)\n  [Dimensions] {400 300}\n"
		"  [Position] (BottomRight) {0 0}\n[end]\n");
	struct wm_view v;
	new_view(&v, "a", NULL, "");
	map_view(&v);
	assert(v.x == 1920 - 400 && v.y == 1050 - 300);
}

TEST(test_head)
{
	/* Position on head 2 */
	load("[app] (a)\n  [Head] {2}\n  [Position] {10 10}\n[end]\n"
		"[app] (b)\n  [Head] {2}\n[end]\n");
	struct wm_view a, b;
	new_view(&a, "a", NULL, "");
	map_view(&a);
	assert(a.x == 1930 && a.y == 10);
	/* Head alone: moved there once placed */
	new_view(&b, "b", NULL, "");
	map_view(&b);
	assert(b.pending_head == 2 && !b.rule_placed);
}

TEST(test_states)
{
	load("[app] (full)\n  [Maximized] {yes}\n[end]\n"
		"[app] (horz)\n  [Maximized] {horz}\n[end]\n"
		"[app] (vert)\n  [Maximized] {Vert}\n[end]\n"
		"[app] (never)\n  [Maximized] {no}\n  [Fullscreen] {no}\n[end]\n"
		"[app] (fs)\n  [Fullscreen] {yes}\n  [Minimized] {yes}\n"
		"  [Sticky] {yes}\n  [Shaded] {yes}\n[end]\n");
	struct wm_view full, horz, vert, never, fs;
	new_view(&full, "full", NULL, "");
	new_view(&horz, "horz", NULL, "");
	new_view(&vert, "vert", NULL, "");
	new_view(&never, "never", NULL, "");
	new_view(&fs, "fs", NULL, "");
	map_view(&full);
	map_view(&horz);
	map_view(&vert);
	map_view(&never);
	map_view(&fs);
	/* Not shown yet: left for the map path, after placement */
	assert(full.pending_maximize && !full.maximized);
	assert(horz.pending_maximize_horiz && !horz.pending_maximize);
	assert(vert.pending_maximize_vert);
	assert(never.refuse_maximize && never.refuse_fullscreen);
	assert(fs.pending_fullscreen && fs.pending_minimize);
	assert(fs.sticky && fs.decoration->shaded);
	assert(g_minimized == 0);

	/* A shown window (app_id arrived late) changes now */
	struct wm_view late;
	new_view(&late, "horz", NULL, "");
	late.scene_tree->node.enabled = true;
	map_view(&late);
	assert(late.maximized_horiz);
}

TEST(test_hidden_alpha_deco_protection)
{
	load("[app] (a)\n  [Hidden] {yes}\n  [Alpha] {300}\n"
		"  [Deco] {0x5}\n  [FocusNewWindow] {no}\n[end]\n"
		"[app] (b)\n  [IconHidden] {yes}\n  [Alpha] {200 -4}\n"
		"  [Deco] {TOOL}\n  [FocusProtection] {Lock, Deny, Gain}\n[end]\n"
		"[app] (c)\n  [FocusProtection] {Gain}\n"
		"  [FocusNewWindow] {no}\n  [Deco] {1027}\n[end]\n");
	struct wm_view a, b, c;
	new_view(&a, "a", NULL, "");
	new_view(&b, "b", NULL, "");
	new_view(&c, "c", NULL, "");
	map_view(&a);
	map_view(&b);
	map_view(&c);
	assert(a.focus_hidden && a.icon_hidden);
	assert(a.focus_alpha == 255 && a.unfocus_alpha == 255);
	assert(a.decoration->preset == WM_DECOR_TOOL);  /* titlebar|border */
	assert(a.focus_protection == WM_FOCUS_PROT_REFUSE);
	assert(!b.focus_hidden && b.icon_hidden);
	assert(b.focus_alpha == 200 && b.unfocus_alpha == 0);
	/* Deny after Lock replaces it */
	assert(b.focus_protection == (WM_FOCUS_PROT_DENY | WM_FOCUS_PROT_GAIN));
	/* FocusNewWindow doesn't contradict an earlier Gain */
	assert(c.focus_protection == WM_FOCUS_PROT_GAIN);
	assert(c.decoration->preset == WM_DECOR_NORMAL);  /* 1027: +handle */
}

TEST(test_workspace_jump)
{
	load("[app] (a)\n  [Workspace] {2}\n  [Jump] {yes}\n[end]\n"
		"[app] (b)\n  [Workspace] {9}\n  [Jump] {yes}\n[end]\n");
	struct wm_view a, b;
	new_view(&a, "a", NULL, "");
	map_view(&a);
	assert(a.workspace == &g_ws[2] && g_ws_switched_to == 2);
	g_ws_switched_to = -1;
	new_view(&b, "b", NULL, "");
	map_view(&b);
	assert(b.workspace == &g_ws[0] && g_ws_switched_to == -1);
}

TEST(test_fluxland_1x_syntax)
{
	/* What fluxland 1.x documented still reads the same */
	load("# comment\n! comment too\n"
		"[app] (class=zenity)\n  [Position] (Center) {0 0}\n"
		"  [Deco] {TOOL}\n  [Layer] {Above}\n[end]\n");
	struct wm_view v;
	new_view(&v, "zenity", NULL, "");
	map_view(&v);
	assert(v.rule_placed && v.layer == WM_LAYER_ABOVE);
	assert(v.decoration->preset == WM_DECOR_TOOL);
}

/* --- Groups --- */

TEST(test_group)
{
	load("[group] (workspace)\n"
		"  [app] (class=foot)\n"
		"  [app] (class=Alacritty)\n"
		"  [Layer] {Bottom}\n"
		"[end]\n");
	struct wm_view a, b, c;
	new_view(&a, "foot", NULL, "");
	map_view(&a);
	assert(a.layer == WM_LAYER_BELOW);    /* the group's settings */
	assert(wm_rules_find_group(&rules, &a, &g_server) == NULL);

	new_view(&b, "Alacritty", NULL, "");
	map_view(&b);
	assert(b.layer == WM_LAYER_BELOW);
	assert(wm_rules_find_group(&rules, &b, &g_server) == &a);

	/* (workspace): only with a group window on the current workspace */
	a.workspace = &g_ws[1];
	new_view(&c, "foot", NULL, "");
	map_view(&c);
	struct wm_view *peer = wm_rules_find_group(&rules, &c, &g_server);
	assert(peer == &b);
	b.workspace = &g_ws[1];
	assert(wm_rules_find_group(&rules, &c, &g_server) == NULL);
}

TEST(test_group_without_settings_and_startup)
{
	load("[startup] {xsetroot -solid black}\n"
		"[startup] (screen=0) {conky}\n"
		"[group]\n  [app] (a)\n  [app] (b)\n[end]\n"
		"[app] (c)\n  [Workspace] {1}\n[end]\n");
	assert(rules.startup_count == 2);
	assert(strcmp(rules.startup[0], "xsetroot -solid black") == 0);
	assert(strcmp(rules.startup[1], "conky") == 0);
	struct wm_view a, b, c;
	new_view(&a, "a", NULL, "");
	new_view(&b, "b", NULL, "");
	new_view(&c, "c", NULL, "");
	map_view(&a);
	map_view(&b);
	map_view(&c);
	assert(wm_rules_find_group(&rules, &b, &g_server) == &a);
	assert(c.workspace == &g_ws[1]);
}

/* --- Remember --- */

static char *
read_apps(void)
{
	FILE *f = fopen(APPS, "r");
	assert(f);
	static char buf[8192];
	size_t n = fread(buf, 1, sizeof(buf) - 1, f);
	buf[n] = '\0';
	fclose(f);
	return buf;
}

TEST(test_remember_round_trip)
{
	load("# my rules\n[app] (class=other)\n  [Sticky] {yes}\n[end]\n");
	struct wm_view v;
	new_view(&v, "org.gnome.Terminal", "gnome-terminal", "");
	v.workspace = &g_ws[2];
	v.layer = WM_LAYER_ABOVE;
	v.x = 100;
	v.y = 50;
	v.maximized_vert = true;
	assert(wm_rules_remember_window(&v, APPS) == 0);
	char *text = read_apps();
	assert(strstr(text, "# my rules"));                /* kept */
	assert(strstr(text, "[app] (class=other)"));
	assert(strstr(text, "[app] (name=gnome-terminal) "
		"(class=org\\.gnome\\.Terminal)"));
	assert(strstr(text, "[Layer]\t{6}"));               /* Fluxbox number */
	assert(strstr(text, "[Maximized]\t{vert}"));

	/* Written once, and read back to the same window */
	v.x = 300;
	assert(wm_rules_remember_window(&v, APPS) == 0);
	text = read_apps();
	char *first = strstr(text, "(name=gnome-terminal)");
	assert(first && !strstr(first + 1, "(name=gnome-terminal)"));
	load(text);
	struct wm_view again;
	new_view(&again, "org.gnome.Terminal", "gnome-terminal", "");
	map_view(&again);
	assert(again.workspace == &g_ws[2]);
	assert(again.x == 300 && again.y == 50);
	assert(again.layer == WM_LAYER_ABOVE);
	assert(again.pending_maximize_vert);
}

TEST(test_remember_replaces_fluxland_1x_entry)
{
	load("[app] (class=foot)\n  [Workspace] {3}\n[end]\n");
	struct wm_view v;
	new_view(&v, "foot", NULL, "");
	assert(wm_rules_remember_window(&v, APPS) == 0);
	char *text = read_apps();
	assert(!strstr(text, "[Workspace] {3}"));
	assert(strstr(text, "[app] (name=foot) (class=foot)"));
}

TEST(test_close_saves_state)
{
	load("[app] (class=mpv)\n  [Close] {yes}\n[end]\n");
	struct wm_view v;
	new_view(&v, "mpv", NULL, "");
	map_view(&v);
	v.workspace = &g_ws[1];
	close_view(&v);
	char *text = read_apps();
	assert(strstr(text, "[app] (name=mpv) (class=mpv)"));
	assert(strstr(text, "[Workspace]\t{1}"));
	assert(strstr(text, "[Close]\t{yes}"));
}

int
main(void)
{
	printf("rules tests:\n");
	mkdir("/tmp/fluxland-test", 0755);
	mkdir(TEST_DIR, 0755);
	wm_rules_init(&rules);

	run_test_bare_word_is_name();
	run_test_name_class_role_and_negation();
	run_test_first_match_wins_and_transient();
	run_test_match_limit();
	run_test_bad_pattern_skips_its_entry();
	run_test_layer_numbers_and_names();
	run_test_dimensions_pixels_and_percent();
	run_test_position_corners();
	run_test_position_uses_new_dimensions();
	run_test_head();
	run_test_states();
	run_test_hidden_alpha_deco_protection();
	run_test_workspace_jump();
	run_test_fluxland_1x_syntax();
	run_test_group();
	run_test_group_without_settings_and_startup();
	run_test_remember_round_trip();
	run_test_remember_replaces_fluxland_1x_entry();
	run_test_close_saves_state();

	wm_rules_finish(&rules);
	unlink(APPS);
	rmdir(TEST_DIR);
	printf("All %d rules tests passed.\n", tests_run);
	return 0;
}
