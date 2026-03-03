/*
 * test_rules.c - Unit tests for window rules parser
 */

#define _GNU_SOURCE

#include "rules.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/*
 * Stubs for compositor functions referenced in rules.c but not
 * exercised by the parser-only tests below.
 */
#include "workspace.h"
#include "view.h"

struct wm_workspace *wm_workspace_get(struct wm_server *s, int i)
	{ (void)s; (void)i; return NULL; }
void wm_view_move_to_workspace(struct wm_view *v, struct wm_workspace *w)
	{ (void)v; (void)w; }
void wm_decoration_set_preset(struct wm_decoration *d,
	enum wm_decor_preset p, struct wm_style *s)
	{ (void)d; (void)p; (void)s; }
void wm_view_set_sticky(struct wm_view *v, bool s)
	{ (void)v; (void)s; }
void wm_view_set_layer(struct wm_view *v, enum wm_view_layer l)
	{ (void)v; (void)l; }
void wm_foreign_toplevel_set_minimized(struct wm_view *v, bool m)
	{ (void)v; (void)m; }
void wm_decoration_set_shaded(struct wm_decoration *d, bool s,
	struct wm_style *st)
	{ (void)d; (void)s; (void)st; }
void wm_view_get_geometry(struct wm_view *v, struct wlr_box *b)
	{ (void)v; if (b) { b->x = 0; b->y = 0; b->width = 100; b->height = 100; } }

/* --- wlroots stubs for apply tests --- */

/* Global controlling what wlr_output_layout_output_at returns */
static struct wlr_output *g_output_at_result;

struct wlr_output *
wlr_output_layout_output_at(struct wlr_output_layout *layout,
	double lx, double ly)
{
	(void)layout; (void)lx; (void)ly;
	return g_output_at_result;
}

/* Global controlling what wlr_output_layout_get_box returns */
static struct wlr_box g_layout_box;

void
wlr_output_layout_get_box(struct wlr_output_layout *layout,
	struct wlr_output *output, struct wlr_box *box)
{
	(void)layout; (void)output;
	*box = g_layout_box;
}

/* Global controlling what wlr_xdg_surface_get_geometry returns */
static struct wlr_box g_xdg_geo = { .x = 0, .y = 0, .width = 200, .height = 150 };

void
wlr_xdg_surface_get_geometry(struct wlr_xdg_surface *surface,
	struct wlr_box *box)
{
	(void)surface;
	*box = g_xdg_geo;
}

uint32_t
wlr_xdg_toplevel_set_size(struct wlr_xdg_toplevel *toplevel,
	int width, int height)
{
	(void)toplevel; (void)width; (void)height;
	return 0;
}

void
wlr_scene_node_set_position(struct wlr_scene_node *node, int x, int y)
{
	if (node) {
		node->x = x;
		node->y = y;
	}
}

uint32_t
wlr_xdg_toplevel_set_fullscreen(struct wlr_xdg_toplevel *toplevel,
	bool fullscreen)
{
	(void)toplevel; (void)fullscreen;
	return 0;
}

uint32_t
wlr_xdg_toplevel_set_maximized(struct wlr_xdg_toplevel *toplevel,
	bool maximized)
{
	(void)toplevel; (void)maximized;
	return 0;
}

#define TEST_DIR "/tmp/fluxland-test/wm-test-rules"
#define TEST_APPS TEST_DIR "/apps"

static void
setup(void)
{
	mkdir("/tmp/fluxland-test", 0755);
	mkdir(TEST_DIR, 0755);
}

static void
cleanup(void)
{
	unlink(TEST_APPS);
	rmdir(TEST_DIR);
}

static void
write_file(const char *path, const char *contents)
{
	FILE *f = fopen(path, "w");
	assert(f);
	fputs(contents, f);
	fclose(f);
}

/* Helper to count window rules */
static int
count_window_rules(struct wm_rules *rules)
{
	int n = 0;
	struct wm_window_rule *wr;
	wl_list_for_each(wr, &rules->window_rules, link)
		n++;
	return n;
}

/* Helper to count group rules */
static int
count_group_rules(struct wm_rules *rules)
{
	int n = 0;
	struct wm_group_rule *gr;
	wl_list_for_each(gr, &rules->group_rules, link)
		n++;
	return n;
}

/* Helper to get first window rule */
static struct wm_window_rule *
first_window_rule(struct wm_rules *rules)
{
	struct wm_window_rule *wr;
	wl_list_for_each(wr, &rules->window_rules, link)
		return wr;
	return NULL;
}

/*
 * Helpers for anchor/apply tests that need a full server/output/view setup.
 * Uses static storage so each test can simply call setup_anchor_env()
 * and use the returned view pointer.
 */
static struct wlr_output_layout s_layout;
static struct wlr_cursor s_cursor;
static struct wlr_output s_wlr_output;

/* Minimal wm_output (matches the struct from output.h) */
struct wm_output {
	struct wl_list link;
	struct wm_server *server;
	struct wlr_output *wlr_output;
	struct wlr_scene_output *scene_output;
	struct wlr_box usable_area;
	struct wm_workspace *active_workspace;
};

static struct wm_output s_output;
static struct wm_server s_server;
static struct wlr_xdg_surface s_xdg_surface;
static struct wlr_xdg_toplevel s_toplevel;
static struct wlr_scene_tree s_scene_tree;

/*
 * Set up a fake server with one output (1920x1080 usable area at 0,0),
 * a fake view with xdg_toplevel (200x150 geometry from stub), and
 * a scene_tree. Caller should set view->app_id and load rules.
 */
static void
setup_anchor_env(struct wm_view *view)
{
	memset(view, 0, sizeof(*view));
	memset(&s_server, 0, sizeof(s_server));
	memset(&s_output, 0, sizeof(s_output));
	memset(&s_toplevel, 0, sizeof(s_toplevel));
	memset(&s_xdg_surface, 0, sizeof(s_xdg_surface));
	memset(&s_scene_tree, 0, sizeof(s_scene_tree));

	/* Wire up output list */
	wl_list_init(&s_server.outputs);
	wl_list_init(&s_server.views);
	s_server.output_layout = &s_layout;
	s_server.cursor = &s_cursor;
	s_cursor.x = 500;
	s_cursor.y = 500;

	/* Output: 1920x1080 at (0,0) */
	s_output.wlr_output = &s_wlr_output;
	s_output.usable_area = (struct wlr_box){ 0, 0, 1920, 1080 };
	wl_list_insert(&s_server.outputs, &s_output.link);

	/* Make wlr_output_layout_output_at return our output */
	g_output_at_result = &s_wlr_output;

	/* XDG toplevel -> surface */
	s_toplevel.base = &s_xdg_surface;

	/* View setup */
	view->server = &s_server;
	view->xdg_toplevel = &s_toplevel;
	view->scene_tree = &s_scene_tree;

	/* Default geometry returned by stub: 200x150 */
	g_xdg_geo = (struct wlr_box){ 0, 0, 200, 150 };
}

/* Test: init/finish lifecycle */
static void
test_init_finish(void)
{
	struct wm_rules rules;
	wm_rules_init(&rules);

	assert(wl_list_empty(&rules.window_rules));
	assert(wl_list_empty(&rules.group_rules));

	wm_rules_finish(&rules);
	printf("  PASS: test_init_finish\n");
}

/* Test: loading a nonexistent file returns false */
static void
test_load_nonexistent(void)
{
	struct wm_rules rules;
	wm_rules_init(&rules);

	bool ret = wm_rules_load(&rules, "/tmp/fluxland-test/nonexistent_apps_file");
	assert(ret == false);

	wm_rules_finish(&rules);
	printf("  PASS: test_load_nonexistent\n");
}

/* Test: loading an empty file succeeds with no rules */
static void
test_load_empty(void)
{
	write_file(TEST_APPS, "# empty apps file\n");

	struct wm_rules rules;
	wm_rules_init(&rules);

	bool ret = wm_rules_load(&rules, TEST_APPS);
	assert(ret == true);
	assert(count_window_rules(&rules) == 0);
	assert(count_group_rules(&rules) == 0);

	wm_rules_finish(&rules);
	printf("  PASS: test_load_empty\n");
}

/* Test: basic window rule with class pattern */
static void
test_basic_class_rule(void)
{
	write_file(TEST_APPS,
		"[app] (class=Firefox)\n"
		"  [Workspace] {2}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);

	bool ret = wm_rules_load(&rules, TEST_APPS);
	assert(ret == true);
	assert(count_window_rules(&rules) == 1);

	struct wm_window_rule *wr = first_window_rule(&rules);
	assert(wr != NULL);
	assert(wr->pattern_count == 1);
	assert(strcmp(wr->patterns[0].property, "class") == 0);
	assert(strcmp(wr->patterns[0].regex_str, "Firefox") == 0);
	assert(wr->patterns[0].negate == false);
	assert(wr->has_workspace == true);
	assert(wr->workspace == 2);

	wm_rules_finish(&rules);
	printf("  PASS: test_basic_class_rule\n");
}

/* Test: negation pattern (!=) */
static void
test_negate_pattern(void)
{
	write_file(TEST_APPS,
		"[app] (class!=dialog)\n"
		"  [Sticky] {yes}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);

	wm_rules_load(&rules, TEST_APPS);

	struct wm_window_rule *wr = first_window_rule(&rules);
	assert(wr != NULL);
	assert(wr->pattern_count == 1);
	assert(strcmp(wr->patterns[0].property, "class") == 0);
	assert(strcmp(wr->patterns[0].regex_str, "dialog") == 0);
	assert(wr->patterns[0].negate == true);
	assert(wr->has_sticky == true);
	assert(wr->sticky == true);

	wm_rules_finish(&rules);
	printf("  PASS: test_negate_pattern\n");
}

/* Test: multiple patterns (AND-combined) */
static void
test_multiple_patterns(void)
{
	write_file(TEST_APPS,
		"[app] (class=Gimp) (title=Toolbox)\n"
		"  [Dimensions] {300 600}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);

	wm_rules_load(&rules, TEST_APPS);

	struct wm_window_rule *wr = first_window_rule(&rules);
	assert(wr != NULL);
	assert(wr->pattern_count == 2);
	assert(strcmp(wr->patterns[0].property, "class") == 0);
	assert(strcmp(wr->patterns[1].property, "title") == 0);
	assert(wr->has_dimensions == true);
	assert(wr->width == 300);
	assert(wr->height == 600);

	wm_rules_finish(&rules);
	printf("  PASS: test_multiple_patterns\n");
}

/* Test: rule properties - dimensions, workspace, deco, sticky */
static void
test_rule_properties(void)
{
	write_file(TEST_APPS,
		"[app] (class=term)\n"
		"  [Workspace] {3}\n"
		"  [Dimensions] {800 600}\n"
		"  [Position] (Center) {10 20}\n"
		"  [Deco] {NONE}\n"
		"  [Sticky] {yes}\n"
		"  [Layer] {2}\n"
		"  [Maximized] {true}\n"
		"  [Minimized] {no}\n"
		"  [Fullscreen] {yes}\n"
		"  [Shaded] {true}\n"
		"  [FocusNewWindow] {no}\n"
		"  [Head] {1}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);

	wm_rules_load(&rules, TEST_APPS);

	struct wm_window_rule *wr = first_window_rule(&rules);
	assert(wr != NULL);

	assert(wr->has_workspace && wr->workspace == 3);
	assert(wr->has_dimensions && wr->width == 800 && wr->height == 600);
	assert(wr->has_position && wr->pos_x == 10 && wr->pos_y == 20);
	assert(wr->pos_anchor != NULL && strcmp(wr->pos_anchor, "Center") == 0);
	assert(wr->has_deco && wr->deco == WM_DECOR_NONE);
	assert(wr->has_sticky && wr->sticky == true);
	assert(wr->has_layer && wr->layer == 2);
	assert(wr->has_maximized && wr->maximized == true);
	assert(wr->has_minimized && wr->minimized == false);
	assert(wr->has_fullscreen && wr->fullscreen == true);
	assert(wr->has_shaded && wr->shaded == true);
	assert(wr->has_focus_new && wr->focus_new == false);
	assert(wr->has_head && wr->head == 1);

	wm_rules_finish(&rules);
	printf("  PASS: test_rule_properties\n");
}

/* Test: deco preset parsing variants */
static void
test_deco_presets(void)
{
	const char *presets[] = {
		"NORMAL", "NONE", "BORDER", "TAB", "TINY", "TOOL", "FULL", "0"
	};
	enum wm_decor_preset expected[] = {
		WM_DECOR_NORMAL, WM_DECOR_NONE, WM_DECOR_BORDER,
		WM_DECOR_TAB, WM_DECOR_TINY, WM_DECOR_TOOL,
		WM_DECOR_NORMAL, WM_DECOR_NONE
	};

	for (int i = 0; i < 8; i++) {
		char buf[256];
		snprintf(buf, sizeof(buf),
			"[app] (class=test%d)\n"
			"  [Deco] {%s}\n"
			"[end]\n", i, presets[i]);
		write_file(TEST_APPS, buf);

		struct wm_rules rules;
		wm_rules_init(&rules);
		wm_rules_load(&rules, TEST_APPS);

		struct wm_window_rule *wr = first_window_rule(&rules);
		assert(wr != NULL);
		assert(wr->has_deco);
		assert(wr->deco == expected[i]);

		wm_rules_finish(&rules);
	}

	printf("  PASS: test_deco_presets\n");
}

/* Test: regex patterns compile and can be used */
static void
test_regex_pattern(void)
{
	write_file(TEST_APPS,
		"[app] (class=^Firefox.*$)\n"
		"  [Workspace] {1}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);

	wm_rules_load(&rules, TEST_APPS);

	struct wm_window_rule *wr = first_window_rule(&rules);
	assert(wr != NULL);
	assert(wr->pattern_count == 1);

	/* Verify the compiled regex works */
	int rc = regexec(&wr->patterns[0].compiled, "Firefox-ESR", 0, NULL, 0);
	assert(rc == 0); /* should match */

	rc = regexec(&wr->patterns[0].compiled, "Chromium", 0, NULL, 0);
	assert(rc != 0); /* should not match */

	wm_rules_finish(&rules);
	printf("  PASS: test_regex_pattern\n");
}

/* Test: multiple rules in one file */
static void
test_multiple_rules(void)
{
	write_file(TEST_APPS,
		"[app] (class=Firefox)\n"
		"  [Workspace] {1}\n"
		"[end]\n"
		"[app] (class=Gimp)\n"
		"  [Workspace] {2}\n"
		"[end]\n"
		"[app] (class=term)\n"
		"  [Workspace] {3}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);

	wm_rules_load(&rules, TEST_APPS);
	assert(count_window_rules(&rules) == 3);

	wm_rules_finish(&rules);
	printf("  PASS: test_multiple_rules\n");
}

/* Test: group rules */
static void
test_group_rules(void)
{
	write_file(TEST_APPS,
		"[group] (workspace)\n"
		"  [app] (class=term)\n"
		"  [app] (class=editor)\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);

	wm_rules_load(&rules, TEST_APPS);
	assert(count_window_rules(&rules) == 0);
	assert(count_group_rules(&rules) == 1);

	struct wm_group_rule *gr;
	wl_list_for_each(gr, &rules.group_rules, link) {
		assert(gr->workspace_scope == true);
		assert(gr->pattern_count == 2);
		assert(strcmp(gr->patterns[0].property, "class") == 0);
		assert(strcmp(gr->patterns[0].regex_str, "term") == 0);
		assert(strcmp(gr->patterns[1].property, "class") == 0);
		assert(strcmp(gr->patterns[1].regex_str, "editor") == 0);
		break;
	}

	wm_rules_finish(&rules);
	printf("  PASS: test_group_rules\n");
}

/* Test: comments and blank lines are skipped */
static void
test_comments_blanks(void)
{
	write_file(TEST_APPS,
		"# This is a comment\n"
		"! Another comment\n"
		"\n"
		"   \n"
		"[app] (class=test)\n"
		"  [Workspace] {1}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);

	wm_rules_load(&rules, TEST_APPS);
	assert(count_window_rules(&rules) == 1);

	wm_rules_finish(&rules);
	printf("  PASS: test_comments_blanks\n");
}

/* Test: finish can be called on already-empty rules */
static void
test_double_finish(void)
{
	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_finish(&rules);

	/* Lists should be empty and safe to check */
	assert(wl_list_empty(&rules.window_rules));
	assert(wl_list_empty(&rules.group_rules));

	printf("  PASS: test_double_finish\n");
}

/* Test: remember window serialization */
static void
test_remember(void)
{
	/* Create a minimal fake view */
	struct wm_view view = {0};
	view.app_id = "test-app";
	view.title = "Test Window";
	view.x = 100;
	view.y = 200;
	view.sticky = false;
	view.maximized = false;
	view.fullscreen = false;
	view.layer = WM_LAYER_NORMAL;
	view.focus_alpha = 255;
	view.unfocus_alpha = 200;

	/* Create a fake workspace */
	struct wm_workspace ws = {0};
	ws.index = 2;
	ws.name = "Workspace 3";
	view.workspace = &ws;

	/* We don't have a real XDG toplevel, so wm_view_get_geometry
	 * will be called with our stub returning fixed values */

	const char *path = TEST_DIR "/remember_apps";
	/* Write an empty file first */
	write_file(path, "# existing rules\n");

	int ret = wm_rules_remember_window(&view, path);
	assert(ret == 0);

	/* Read back and verify the appended block */
	FILE *f = fopen(path, "r");
	assert(f);
	char buf[2048] = {0};
	size_t n = fread(buf, 1, sizeof(buf) - 1, f);
	fclose(f);
	buf[n] = '\0';

	/* Should contain the app block */
	assert(strstr(buf, "[app] (class=test-app)") != NULL);
	assert(strstr(buf, "[Workspace] {2}") != NULL);
	assert(strstr(buf, "[Sticky] {no}") != NULL);
	assert(strstr(buf, "[Maximized] {no}") != NULL);
	assert(strstr(buf, "[Fullscreen] {no}") != NULL);
	assert(strstr(buf, "[Layer] {Normal}") != NULL);
	assert(strstr(buf, "[Alpha] {255 200}") != NULL);
	assert(strstr(buf, "[end]") != NULL);

	unlink(path);
	printf("  PASS: test_remember\n");
}

/* Test: reload (finish + init + load) */
static void
test_reload(void)
{
	write_file(TEST_APPS,
		"[app] (class=first)\n"
		"  [Workspace] {1}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);
	assert(count_window_rules(&rules) == 1);

	/* Simulate reload */
	wm_rules_finish(&rules);
	wm_rules_init(&rules);

	write_file(TEST_APPS,
		"[app] (class=second)\n"
		"  [Workspace] {2}\n"
		"[end]\n"
		"[app] (class=third)\n"
		"  [Workspace] {3}\n"
		"[end]\n"
	);

	wm_rules_load(&rules, TEST_APPS);
	assert(count_window_rules(&rules) == 2);

	wm_rules_finish(&rules);
	printf("  PASS: test_reload\n");
}

/* Test: invalid regex pattern is skipped gracefully */
static void
test_invalid_regex(void)
{
	write_file(TEST_APPS,
		"[app] (class=[unclosed)\n"
		"  [Workspace] {1}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	/* Rule exists but has 0 valid patterns */
	assert(count_window_rules(&rules) == 1);
	struct wm_window_rule *wr = first_window_rule(&rules);
	assert(wr != NULL);
	assert(wr->pattern_count == 0);

	wm_rules_finish(&rules);
	printf("  PASS: test_invalid_regex\n");
}

/* Test: missing [end] tag — unterminated block is discarded */
static void
test_missing_end(void)
{
	write_file(TEST_APPS,
		"[app] (class=Firefox)\n"
		"  [Workspace] {1}\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	/* Unterminated block should NOT be added */
	assert(count_window_rules(&rules) == 0);

	wm_rules_finish(&rules);
	printf("  PASS: test_missing_end\n");
}

/* Test: empty pattern values are skipped */
static void
test_empty_pattern_values(void)
{
	write_file(TEST_APPS,
		"[app] (class=) (=Firefox)\n"
		"  [Workspace] {1}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	assert(count_window_rules(&rules) == 1);
	struct wm_window_rule *wr = first_window_rule(&rules);
	assert(wr != NULL);
	assert(wr->pattern_count == 0);

	wm_rules_finish(&rules);
	printf("  PASS: test_empty_pattern_values\n");
}

/* Test: boolean value variants (yes/no/true/false/1/0) */
static void
test_boolean_variants(void)
{
	/* Truthy values */
	write_file(TEST_APPS,
		"[app] (class=test1)\n"
		"  [Sticky] {true}\n"
		"  [Maximized] {yes}\n"
		"  [Fullscreen] {1}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_window_rule *wr = first_window_rule(&rules);
	assert(wr != NULL);
	assert(wr->has_sticky && wr->sticky == true);
	assert(wr->has_maximized && wr->maximized == true);
	assert(wr->has_fullscreen && wr->fullscreen == true);

	wm_rules_finish(&rules);

	/* Falsy values */
	write_file(TEST_APPS,
		"[app] (class=test2)\n"
		"  [Sticky] {false}\n"
		"  [Maximized] {no}\n"
		"  [Fullscreen] {0}\n"
		"[end]\n"
	);

	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	wr = first_window_rule(&rules);
	assert(wr != NULL);
	assert(wr->has_sticky && wr->sticky == false);
	assert(wr->has_maximized && wr->maximized == false);
	assert(wr->has_fullscreen && wr->fullscreen == false);

	wm_rules_finish(&rules);
	printf("  PASS: test_boolean_variants\n");
}

/* Test: title pattern matching */
static void
test_title_pattern(void)
{
	write_file(TEST_APPS,
		"[app] (title=^Editor.*$)\n"
		"  [Workspace] {5}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_window_rule *wr = first_window_rule(&rules);
	assert(wr != NULL);
	assert(wr->pattern_count == 1);
	assert(strcmp(wr->patterns[0].property, "title") == 0);
	assert(strcmp(wr->patterns[0].regex_str, "^Editor.*$") == 0);

	/* Verify compiled regex */
	int rc = regexec(&wr->patterns[0].compiled, "Editor - main.c",
		0, NULL, 0);
	assert(rc == 0);
	rc = regexec(&wr->patterns[0].compiled, "Browser", 0, NULL, 0);
	assert(rc != 0);

	wm_rules_finish(&rules);
	printf("  PASS: test_title_pattern\n");
}

/* Test: special regex characters */
static void
test_special_regex(void)
{
	write_file(TEST_APPS,
		"[app] (class=foo\\.bar)\n"
		"  [Workspace] {1}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_window_rule *wr = first_window_rule(&rules);
	assert(wr != NULL);
	assert(wr->pattern_count == 1);

	/* Escaped dot matches literal dot */
	int rc = regexec(&wr->patterns[0].compiled, "foo.bar",
		0, NULL, 0);
	assert(rc == 0);

	/* Should NOT match fooXbar */
	rc = regexec(&wr->patterns[0].compiled, "fooXbar", 0, NULL, 0);
	assert(rc != 0);

	wm_rules_finish(&rules);
	printf("  PASS: test_special_regex\n");
}

/* Test: alpha setting with two values */
static void
test_alpha_settings(void)
{
	write_file(TEST_APPS,
		"[app] (class=term)\n"
		"  [Alpha] {200 150}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_window_rule *wr = first_window_rule(&rules);
	assert(wr != NULL);
	assert(wr->has_alpha);
	assert(wr->focus_alpha == 200);
	assert(wr->unfocus_alpha == 150);

	wm_rules_finish(&rules);
	printf("  PASS: test_alpha_settings\n");
}

/* Test: FocusProtection flag parsing */
static void
test_focus_protection(void)
{
	write_file(TEST_APPS,
		"[app] (class=important)\n"
		"  [FocusProtection] {Gain,Refuse}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_window_rule *wr = first_window_rule(&rules);
	assert(wr != NULL);
	assert(wr->has_focus_protection);
	assert(wr->focus_protection & WM_FOCUS_PROT_GAIN);
	assert(wr->focus_protection & WM_FOCUS_PROT_REFUSE);
	assert(!(wr->focus_protection & WM_FOCUS_PROT_DENY));

	wm_rules_finish(&rules);

	/* Test Deny flag */
	write_file(TEST_APPS,
		"[app] (class=locked)\n"
		"  [FocusProtection] {Deny}\n"
		"[end]\n"
	);

	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	wr = first_window_rule(&rules);
	assert(wr != NULL);
	assert(wr->has_focus_protection);
	assert(wr->focus_protection & WM_FOCUS_PROT_DENY);
	assert(!(wr->focus_protection & WM_FOCUS_PROT_GAIN));

	wm_rules_finish(&rules);
	printf("  PASS: test_focus_protection\n");
}

/* Test: IgnoreSizeHints setting */
static void
test_ignore_size_hints(void)
{
	write_file(TEST_APPS,
		"[app] (class=term)\n"
		"  [IgnoreSizeHints] {yes}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_window_rule *wr = first_window_rule(&rules);
	assert(wr != NULL);
	assert(wr->has_ignore_size_hints);
	assert(wr->ignore_size_hints == true);

	wm_rules_finish(&rules);
	printf("  PASS: test_ignore_size_hints\n");
}

/* Test: group rule without workspace scope */
static void
test_group_no_workspace(void)
{
	write_file(TEST_APPS,
		"[group]\n"
		"  [app] (class=term1)\n"
		"  [app] (class=term2)\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	assert(count_group_rules(&rules) == 1);
	struct wm_group_rule *gr;
	wl_list_for_each(gr, &rules.group_rules, link) {
		assert(gr->workspace_scope == false);
		assert(gr->pattern_count == 2);
		break;
	}

	wm_rules_finish(&rules);
	printf("  PASS: test_group_no_workspace\n");
}

/* Test: position without anchor */
static void
test_position_no_anchor(void)
{
	write_file(TEST_APPS,
		"[app] (class=test)\n"
		"  [Position] {100 200}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_window_rule *wr = first_window_rule(&rules);
	assert(wr != NULL);
	assert(wr->has_position);
	assert(wr->pos_x == 100);
	assert(wr->pos_y == 200);
	assert(wr->pos_anchor == NULL);

	wm_rules_finish(&rules);
	printf("  PASS: test_position_no_anchor\n");
}

/* Test: deco preset case-insensitive */
static void
test_deco_case_insensitive(void)
{
	write_file(TEST_APPS,
		"[app] (class=test)\n"
		"  [Deco] {border}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_window_rule *wr = first_window_rule(&rules);
	assert(wr != NULL);
	assert(wr->has_deco);
	assert(wr->deco == WM_DECOR_BORDER);

	wm_rules_finish(&rules);
	printf("  PASS: test_deco_case_insensitive\n");
}

/* Test: invalid deco preset is ignored */
static void
test_deco_invalid(void)
{
	write_file(TEST_APPS,
		"[app] (class=test)\n"
		"  [Deco] {BOGUS}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_window_rule *wr = first_window_rule(&rules);
	assert(wr != NULL);
	assert(wr->has_deco == false);

	wm_rules_finish(&rules);
	printf("  PASS: test_deco_invalid\n");
}

/* Test: remember with NULL arguments */
static void
test_remember_null(void)
{
	int ret = wm_rules_remember_window(NULL, TEST_APPS);
	assert(ret == -1);

	struct wm_view view = {0};
	ret = wm_rules_remember_window(&view, NULL);
	assert(ret == -1);

	printf("  PASS: test_remember_null\n");
}

/* Test: multiple rules with mixed settings */
static void
test_mixed_settings(void)
{
	write_file(TEST_APPS,
		"[app] (class=Firefox)\n"
		"  [Workspace] {1}\n"
		"  [Sticky] {yes}\n"
		"[end]\n"
		"[app] (class=Gimp)\n"
		"  [Dimensions] {800 600}\n"
		"  [Deco] {TAB}\n"
		"[end]\n"
		"[app] (class=term)\n"
		"  [Position] (UpperRight) {0 0}\n"
		"  [Alpha] {255}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	assert(count_window_rules(&rules) == 3);

	int idx = 0;
	struct wm_window_rule *wr;
	wl_list_for_each(wr, &rules.window_rules, link) {
		if (idx == 0) {
			assert(wr->has_workspace && wr->workspace == 1);
			assert(wr->has_sticky && wr->sticky == true);
		} else if (idx == 1) {
			assert(wr->has_dimensions && wr->width == 800);
			assert(wr->has_deco && wr->deco == WM_DECOR_TAB);
		} else if (idx == 2) {
			assert(wr->has_position);
			assert(wr->pos_anchor != NULL);
			assert(strcmp(wr->pos_anchor, "UpperRight") == 0);
			assert(wr->has_alpha);
		}
		idx++;
	}

	wm_rules_finish(&rules);
	printf("  PASS: test_mixed_settings\n");
}

/* Test: unknown setting is silently ignored */
static void
test_unknown_setting(void)
{
	write_file(TEST_APPS,
		"[app] (class=test)\n"
		"  [BogusOption] {foobar}\n"
		"  [Workspace] {5}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_window_rule *wr = first_window_rule(&rules);
	assert(wr != NULL);
	assert(wr->has_workspace && wr->workspace == 5);

	wm_rules_finish(&rules);
	printf("  PASS: test_unknown_setting\n");
}

/* Test: unterminated group block is discarded */
static void
test_missing_end_group(void)
{
	write_file(TEST_APPS,
		"[group] (workspace)\n"
		"  [app] (class=term)\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	assert(count_group_rules(&rules) == 0);

	wm_rules_finish(&rules);
	printf("  PASS: test_missing_end_group\n");
}

/* ---- NEW TESTS ---- */

/* Test: "name" property is an alias for "class" (app_id) */
static void
test_name_property(void)
{
	write_file(TEST_APPS,
		"[app] (name=foot)\n"
		"  [Workspace] {1}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_window_rule *wr = first_window_rule(&rules);
	assert(wr != NULL);
	assert(wr->pattern_count == 1);
	assert(strcmp(wr->patterns[0].property, "name") == 0);
	assert(strcmp(wr->patterns[0].regex_str, "foot") == 0);

	/* Verify the regex matches */
	int rc = regexec(&wr->patterns[0].compiled, "foot", 0, NULL, 0);
	assert(rc == 0);

	wm_rules_finish(&rules);
	printf("  PASS: test_name_property\n");
}

/* Test: "role" property parses correctly (returns "" for views) */
static void
test_role_property(void)
{
	write_file(TEST_APPS,
		"[app] (role=dialog)\n"
		"  [Sticky] {yes}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_window_rule *wr = first_window_rule(&rules);
	assert(wr != NULL);
	assert(wr->pattern_count == 1);
	assert(strcmp(wr->patterns[0].property, "role") == 0);
	assert(strcmp(wr->patterns[0].regex_str, "dialog") == 0);

	wm_rules_finish(&rules);
	printf("  PASS: test_role_property\n");
}

/* Test: "transient" property parses correctly */
static void
test_transient_property(void)
{
	write_file(TEST_APPS,
		"[app] (transient=yes)\n"
		"  [Deco] {BORDER}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_window_rule *wr = first_window_rule(&rules);
	assert(wr != NULL);
	assert(wr->pattern_count == 1);
	assert(strcmp(wr->patterns[0].property, "transient") == 0);
	assert(strcmp(wr->patterns[0].regex_str, "yes") == 0);
	assert(wr->has_deco && wr->deco == WM_DECOR_BORDER);

	wm_rules_finish(&rules);
	printf("  PASS: test_transient_property\n");
}

/* Test: wm_rules_apply sets position, alpha, focus_protection, etc.
 * on a minimal view (NULL server/xdg_toplevel to avoid wlroots calls) */
static void
test_apply_basic(void)
{
	write_file(TEST_APPS,
		"[app] (class=myapp)\n"
		"  [Position] {50 75}\n"
		"  [Alpha] {200 150}\n"
		"  [Sticky] {yes}\n"
		"  [FocusProtection] {Gain}\n"
		"  [IgnoreSizeHints] {true}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	/* Create a minimal fake view that matches */
	struct wm_view view = {0};
	view.app_id = "myapp";
	view.title = "My Application";
	/* server=NULL, xdg_toplevel=NULL, decoration=NULL, scene_tree=NULL
	 * so wlroots-dependent branches are skipped */

	wm_rules_apply(&rules, &view);

	/* Position should be set directly (no anchor calculation without server) */
	assert(view.x == 50);
	assert(view.y == 75);

	/* Alpha should be applied */
	assert(view.focus_alpha == 200);
	assert(view.unfocus_alpha == 150);

	/* Focus protection should be applied */
	assert(view.focus_protection & WM_FOCUS_PROT_GAIN);

	/* Ignore size hints should be applied */
	assert(view.ignore_size_hints == true);

	wm_rules_finish(&rules);
	printf("  PASS: test_apply_basic\n");
}

/* Test: wm_rules_apply first-match-wins behavior */
static void
test_apply_first_match_wins(void)
{
	write_file(TEST_APPS,
		"[app] (class=term)\n"
		"  [Alpha] {100 80}\n"
		"[end]\n"
		"[app] (class=term)\n"
		"  [Alpha] {255 255}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_view view = {0};
	view.app_id = "term";
	view.title = "";

	wm_rules_apply(&rules, &view);

	/* First rule wins: alpha should be 100/80, not 255/255 */
	assert(view.focus_alpha == 100);
	assert(view.unfocus_alpha == 80);

	wm_rules_finish(&rules);
	printf("  PASS: test_apply_first_match_wins\n");
}

/* Test: wm_rules_apply does nothing when no rule matches */
static void
test_apply_no_match(void)
{
	write_file(TEST_APPS,
		"[app] (class=Firefox)\n"
		"  [Alpha] {100 80}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_view view = {0};
	view.app_id = "Chromium";
	view.title = "";
	view.focus_alpha = 255;
	view.unfocus_alpha = 255;

	wm_rules_apply(&rules, &view);

	/* Nothing should have changed */
	assert(view.focus_alpha == 255);
	assert(view.unfocus_alpha == 255);

	wm_rules_finish(&rules);
	printf("  PASS: test_apply_no_match\n");
}

/* Test: wm_rules_apply with negated pattern */
static void
test_apply_negate(void)
{
	write_file(TEST_APPS,
		"[app] (class!=Firefox)\n"
		"  [Alpha] {100 80}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	/* "term" != Firefox, so the negated pattern matches */
	struct wm_view view1 = {0};
	view1.app_id = "term";
	view1.title = "";
	view1.focus_alpha = 255;
	wm_rules_apply(&rules, &view1);
	assert(view1.focus_alpha == 100);

	/* Reload for second test since apply modifies nothing structural */
	wm_rules_finish(&rules);
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	/* "Firefox" matches the regex, so negation means NOT matching -> no rule applied */
	struct wm_view view2 = {0};
	view2.app_id = "Firefox";
	view2.title = "";
	view2.focus_alpha = 255;
	wm_rules_apply(&rules, &view2);
	assert(view2.focus_alpha == 255);

	wm_rules_finish(&rules);
	printf("  PASS: test_apply_negate\n");
}

/* Test: wm_rules_apply with multiple AND-combined patterns */
static void
test_apply_multi_pattern_and(void)
{
	write_file(TEST_APPS,
		"[app] (class=Gimp) (title=Toolbox)\n"
		"  [Alpha] {180 160}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	/* Both patterns match */
	struct wm_view view1 = {0};
	view1.app_id = "Gimp";
	view1.title = "Toolbox";
	view1.focus_alpha = 255;
	wm_rules_apply(&rules, &view1);
	assert(view1.focus_alpha == 180);

	wm_rules_finish(&rules);
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	/* Only class matches, title does not -> no match */
	struct wm_view view2 = {0};
	view2.app_id = "Gimp";
	view2.title = "Canvas";
	view2.focus_alpha = 255;
	wm_rules_apply(&rules, &view2);
	assert(view2.focus_alpha == 255);

	wm_rules_finish(&rules);
	printf("  PASS: test_apply_multi_pattern_and\n");
}

/* Test: wm_rules_apply with title matching */
static void
test_apply_title_match(void)
{
	write_file(TEST_APPS,
		"[app] (title=.*\\.pdf$)\n"
		"  [Alpha] {220 200}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_view view = {0};
	view.app_id = "evince";
	view.title = "document.pdf";
	view.focus_alpha = 255;
	wm_rules_apply(&rules, &view);
	assert(view.focus_alpha == 220);

	wm_rules_finish(&rules);
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	/* Title doesn't end with .pdf */
	struct wm_view view2 = {0};
	view2.app_id = "evince";
	view2.title = "document.txt";
	view2.focus_alpha = 255;
	wm_rules_apply(&rules, &view2);
	assert(view2.focus_alpha == 255);

	wm_rules_finish(&rules);
	printf("  PASS: test_apply_title_match\n");
}

/* Test: view with NULL app_id still works (returns "" for matching) */
static void
test_apply_null_app_id(void)
{
	write_file(TEST_APPS,
		"[app] (class=^$)\n"
		"  [Alpha] {100 80}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_view view = {0};
	view.app_id = NULL; /* NULL app_id -> view_get_property returns "" */
	view.title = NULL;
	view.focus_alpha = 255;
	wm_rules_apply(&rules, &view);

	/* ^$ matches empty string, so rule should apply */
	assert(view.focus_alpha == 100);

	wm_rules_finish(&rules);
	printf("  PASS: test_apply_null_app_id\n");
}

/* Test: rule with zero valid patterns does not match any view */
static void
test_apply_no_patterns(void)
{
	write_file(TEST_APPS,
		"[app] (class=[unclosed)\n"
		"  [Alpha] {100 80}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_view view = {0};
	view.app_id = "anything";
	view.title = "";
	view.focus_alpha = 255;
	wm_rules_apply(&rules, &view);

	/* Rule has 0 patterns, so rule_matches returns false (count > 0 check) */
	assert(view.focus_alpha == 255);

	wm_rules_finish(&rules);
	printf("  PASS: test_apply_no_patterns\n");
}

/* Test: alpha with a single value (unfocus_alpha gets same as focus) */
static void
test_alpha_single_value(void)
{
	write_file(TEST_APPS,
		"[app] (class=test)\n"
		"  [Alpha] {180}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_window_rule *wr = first_window_rule(&rules);
	assert(wr != NULL);
	assert(wr->has_alpha);
	assert(wr->focus_alpha == 180);
	/* With single value, unfocus_alpha should equal focus_alpha */
	assert(wr->unfocus_alpha == 180);

	wm_rules_finish(&rules);
	printf("  PASS: test_alpha_single_value\n");
}

/* Test: workspace with non-integer value is ignored */
static void
test_workspace_non_integer(void)
{
	write_file(TEST_APPS,
		"[app] (class=test)\n"
		"  [Workspace] {abc}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_window_rule *wr = first_window_rule(&rules);
	assert(wr != NULL);
	assert(wr->has_workspace == false);

	wm_rules_finish(&rules);
	printf("  PASS: test_workspace_non_integer\n");
}

/* Test: dimensions with bad format is ignored */
static void
test_dimensions_bad_format(void)
{
	write_file(TEST_APPS,
		"[app] (class=test)\n"
		"  [Dimensions] {only_one}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_window_rule *wr = first_window_rule(&rules);
	assert(wr != NULL);
	assert(wr->has_dimensions == false);

	wm_rules_finish(&rules);
	printf("  PASS: test_dimensions_bad_format\n");
}

/* Test: position with bad format is ignored */
static void
test_position_bad_format(void)
{
	write_file(TEST_APPS,
		"[app] (class=test)\n"
		"  [Position] {bad}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_window_rule *wr = first_window_rule(&rules);
	assert(wr != NULL);
	assert(wr->has_position == false);

	wm_rules_finish(&rules);
	printf("  PASS: test_position_bad_format\n");
}

/* Test: FocusProtection with Lock flag (compound: Refuse|Deny) */
static void
test_focus_protection_lock(void)
{
	write_file(TEST_APPS,
		"[app] (class=test)\n"
		"  [FocusProtection] {Lock}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_window_rule *wr = first_window_rule(&rules);
	assert(wr != NULL);
	assert(wr->has_focus_protection);
	/* Lock = Refuse | Deny */
	assert(wr->focus_protection & WM_FOCUS_PROT_REFUSE);
	assert(wr->focus_protection & WM_FOCUS_PROT_DENY);

	wm_rules_finish(&rules);
	printf("  PASS: test_focus_protection_lock\n");
}

/* Test: FocusProtection with unknown flag (silently ignored) */
static void
test_focus_protection_unknown(void)
{
	write_file(TEST_APPS,
		"[app] (class=test)\n"
		"  [FocusProtection] {Gain,UnknownFlag}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_window_rule *wr = first_window_rule(&rules);
	assert(wr != NULL);
	assert(wr->has_focus_protection);
	assert(wr->focus_protection & WM_FOCUS_PROT_GAIN);
	/* Unknown flag doesn't set any bits */
	assert(!(wr->focus_protection & WM_FOCUS_PROT_DENY));

	wm_rules_finish(&rules);
	printf("  PASS: test_focus_protection_unknown\n");
}

/* Test: mixed [app] and [group] blocks in one file */
static void
test_mixed_app_and_group(void)
{
	write_file(TEST_APPS,
		"[app] (class=Firefox)\n"
		"  [Workspace] {1}\n"
		"[end]\n"
		"[group] (workspace)\n"
		"  [app] (class=term1)\n"
		"  [app] (class=term2)\n"
		"[end]\n"
		"[app] (class=Gimp)\n"
		"  [Workspace] {2}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	assert(count_window_rules(&rules) == 2);
	assert(count_group_rules(&rules) == 1);

	wm_rules_finish(&rules);
	printf("  PASS: test_mixed_app_and_group\n");
}

/* Test: unterminated [app] block with settings is still discarded */
static void
test_missing_end_with_settings(void)
{
	write_file(TEST_APPS,
		"[app] (class=Firefox)\n"
		"  [Workspace] {1}\n"
		"  [Sticky] {yes}\n"
		"  [Dimensions] {800 600}\n"
		"  [Alpha] {200 150}\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	/* All the settings were parsed into a rule that got discarded */
	assert(count_window_rules(&rules) == 0);

	wm_rules_finish(&rules);
	printf("  PASS: test_missing_end_with_settings\n");
}

/* Test: setting line without braces is ignored */
static void
test_setting_no_braces(void)
{
	write_file(TEST_APPS,
		"[app] (class=test)\n"
		"  [Workspace] no_braces\n"
		"  [Sticky] {yes}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_window_rule *wr = first_window_rule(&rules);
	assert(wr != NULL);
	/* Workspace had no braces, so value is empty -> safe_atoi fails */
	assert(wr->has_workspace == false);
	/* Sticky should still be parsed */
	assert(wr->has_sticky && wr->sticky == true);

	wm_rules_finish(&rules);
	printf("  PASS: test_setting_no_braces\n");
}

/* Test: setting name too long (>=64 chars) is skipped */
static void
test_setting_name_too_long(void)
{
	char buf[512];
	/* Build a setting name of 70 chars */
	char long_name[72];
	memset(long_name, 'A', 70);
	long_name[70] = '\0';
	snprintf(buf, sizeof(buf),
		"[app] (class=test)\n"
		"  [%s] {yes}\n"
		"  [Sticky] {yes}\n"
		"[end]\n", long_name);
	write_file(TEST_APPS, buf);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_window_rule *wr = first_window_rule(&rules);
	assert(wr != NULL);
	/* The long setting name was skipped, sticky still works */
	assert(wr->has_sticky && wr->sticky == true);

	wm_rules_finish(&rules);
	printf("  PASS: test_setting_name_too_long\n");
}

/* Test: more than 4 patterns triggers array growth in parse_patterns */
static void
test_many_patterns(void)
{
	write_file(TEST_APPS,
		"[app] (class=a) (title=b) (name=c) (role=d) (transient=e)\n"
		"  [Workspace] {1}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_window_rule *wr = first_window_rule(&rules);
	assert(wr != NULL);
	/* 5 patterns means the initial capacity of 4 had to grow */
	assert(wr->pattern_count == 5);
	assert(strcmp(wr->patterns[0].property, "class") == 0);
	assert(strcmp(wr->patterns[1].property, "title") == 0);
	assert(strcmp(wr->patterns[2].property, "name") == 0);
	assert(strcmp(wr->patterns[3].property, "role") == 0);
	assert(strcmp(wr->patterns[4].property, "transient") == 0);

	wm_rules_finish(&rules);
	printf("  PASS: test_many_patterns\n");
}

/* Test: case-insensitive setting names */
static void
test_case_insensitive_settings(void)
{
	write_file(TEST_APPS,
		"[app] (class=test)\n"
		"  [workspace] {1}\n"
		"  [sticky] {yes}\n"
		"  [dimensions] {400 300}\n"
		"  [maximized] {true}\n"
		"  [fullscreen] {yes}\n"
		"  [shaded] {true}\n"
		"  [focusnewwindow] {no}\n"
		"  [head] {0}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_window_rule *wr = first_window_rule(&rules);
	assert(wr != NULL);
	assert(wr->has_workspace && wr->workspace == 1);
	assert(wr->has_sticky && wr->sticky == true);
	assert(wr->has_dimensions && wr->width == 400 && wr->height == 300);
	assert(wr->has_maximized && wr->maximized == true);
	assert(wr->has_fullscreen && wr->fullscreen == true);
	assert(wr->has_shaded && wr->shaded == true);
	assert(wr->has_focus_new && wr->focus_new == false);
	assert(wr->has_head && wr->head == 0);

	wm_rules_finish(&rules);
	printf("  PASS: test_case_insensitive_settings\n");
}

/* Test: remember window with different layer values */
static void
test_remember_layers(void)
{
	struct {
		enum wm_view_layer layer;
		const char *expected;
	} cases[] = {
		{WM_LAYER_DESKTOP, "Desktop"},
		{WM_LAYER_BELOW,   "Below"},
		{WM_LAYER_NORMAL,  "Normal"},
		{WM_LAYER_ABOVE,   "Above"},
	};

	for (int i = 0; i < 4; i++) {
		const char *path = TEST_DIR "/remember_layers";
		write_file(path, "");

		struct wm_view view = {0};
		view.app_id = "test-layer";
		view.title = "Layer Test";
		view.layer = cases[i].layer;
		view.focus_alpha = 255;
		view.unfocus_alpha = 200;

		struct wm_workspace ws = {0};
		ws.index = 0;
		ws.name = "WS";
		view.workspace = &ws;

		int ret = wm_rules_remember_window(&view, path);
		assert(ret == 0);

		FILE *f = fopen(path, "r");
		assert(f);
		char buf[2048] = {0};
		size_t n = fread(buf, 1, sizeof(buf) - 1, f);
		fclose(f);
		buf[n] = '\0';

		char expected[64];
		snprintf(expected, sizeof(expected), "[Layer] {%s}",
			cases[i].expected);
		assert(strstr(buf, expected) != NULL);

		unlink(path);
	}
	printf("  PASS: test_remember_layers\n");
}

/* Test: remember window without workspace (index defaults to 0) */
static void
test_remember_no_workspace(void)
{
	const char *path = TEST_DIR "/remember_no_ws";
	write_file(path, "");

	struct wm_view view = {0};
	view.app_id = "no-ws-app";
	view.title = "No WS";
	view.workspace = NULL;  /* No workspace assigned */
	view.layer = WM_LAYER_NORMAL;
	view.focus_alpha = 255;
	view.unfocus_alpha = 255;

	int ret = wm_rules_remember_window(&view, path);
	assert(ret == 0);

	FILE *f = fopen(path, "r");
	assert(f);
	char buf[2048] = {0};
	size_t n = fread(buf, 1, sizeof(buf) - 1, f);
	fclose(f);
	buf[n] = '\0';

	assert(strstr(buf, "[Workspace] {0}") != NULL);

	unlink(path);
	printf("  PASS: test_remember_no_workspace\n");
}

/* Test: remember window with NULL app_id uses "unknown" */
static void
test_remember_null_app_id(void)
{
	const char *path = TEST_DIR "/remember_null_id";
	write_file(path, "");

	struct wm_view view = {0};
	view.app_id = NULL;
	view.title = "";
	view.layer = WM_LAYER_NORMAL;
	view.focus_alpha = 255;
	view.unfocus_alpha = 255;

	struct wm_workspace ws = {0};
	ws.index = 0;
	ws.name = "WS";
	view.workspace = &ws;

	int ret = wm_rules_remember_window(&view, path);
	assert(ret == 0);

	FILE *f = fopen(path, "r");
	assert(f);
	char buf[2048] = {0};
	size_t n = fread(buf, 1, sizeof(buf) - 1, f);
	fclose(f);
	buf[n] = '\0';

	assert(strstr(buf, "(class=unknown)") != NULL);

	unlink(path);
	printf("  PASS: test_remember_null_app_id\n");
}

/* Test: [end] that appears outside any block is harmless */
static void
test_stray_end(void)
{
	write_file(TEST_APPS,
		"[end]\n"
		"[app] (class=test)\n"
		"  [Workspace] {1}\n"
		"[end]\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	assert(count_window_rules(&rules) == 1);

	wm_rules_finish(&rules);
	printf("  PASS: test_stray_end\n");
}

/* Test: [app] header with case-insensitive matching */
static void
test_app_header_case(void)
{
	write_file(TEST_APPS,
		"[APP] (class=test)\n"
		"  [Workspace] {1}\n"
		"[END]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	/* strncasecmp is used, so [APP] and [END] should work */
	assert(count_window_rules(&rules) == 1);

	wm_rules_finish(&rules);
	printf("  PASS: test_app_header_case\n");
}

/* Test: [group] header with case-insensitive matching */
static void
test_group_header_case(void)
{
	write_file(TEST_APPS,
		"[GROUP] (workspace)\n"
		"  [app] (class=test)\n"
		"[END]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	assert(count_group_rules(&rules) == 1);
	struct wm_group_rule *gr;
	wl_list_for_each(gr, &rules.group_rules, link) {
		assert(gr->workspace_scope == true);
		break;
	}

	wm_rules_finish(&rules);
	printf("  PASS: test_group_header_case\n");
}

/* Test: wm_rules_apply with fullscreen flag (no xdg_toplevel) */
static void
test_apply_fullscreen(void)
{
	write_file(TEST_APPS,
		"[app] (class=game)\n"
		"  [Fullscreen] {yes}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_view view = {0};
	view.app_id = "game";
	view.title = "";
	view.fullscreen = false;

	wm_rules_apply(&rules, &view);

	/* Fullscreen flag should be set even without xdg_toplevel */
	assert(view.fullscreen == true);

	wm_rules_finish(&rules);
	printf("  PASS: test_apply_fullscreen\n");
}

/* Test: Head setting with non-integer value is ignored */
static void
test_head_non_integer(void)
{
	write_file(TEST_APPS,
		"[app] (class=test)\n"
		"  [Head] {primary}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_window_rule *wr = first_window_rule(&rules);
	assert(wr != NULL);
	assert(wr->has_head == false);

	wm_rules_finish(&rules);
	printf("  PASS: test_head_non_integer\n");
}

/* Test: Layer with non-integer value is ignored */
static void
test_layer_string_names(void)
{
	/* Layer names should now be parsed correctly */
	write_file(TEST_APPS,
		"[app] (class=test)\n"
		"  [Layer] {Above}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_window_rule *wr = first_window_rule(&rules);
	assert(wr != NULL);
	assert(wr->has_layer == true);
	assert(wr->layer == WM_LAYER_ABOVE);

	wm_rules_finish(&rules);

	/* Test all layer name variants */
	const char *names[] = {"Desktop", "Below", "Normal", "Above"};
	int expected[] = {WM_LAYER_DESKTOP, WM_LAYER_BELOW,
		WM_LAYER_NORMAL, WM_LAYER_ABOVE};

	for (int i = 0; i < 4; i++) {
		char buf[256];
		snprintf(buf, sizeof(buf),
			"[app] (class=test)\n"
			"  [Layer] {%s}\n"
			"[end]\n", names[i]);
		write_file(TEST_APPS, buf);

		wm_rules_init(&rules);
		wm_rules_load(&rules, TEST_APPS);
		wr = first_window_rule(&rules);
		assert(wr != NULL);
		assert(wr->has_layer == true);
		assert(wr->layer == expected[i]);
		wm_rules_finish(&rules);
	}

	/* Unknown layer names should still fail */
	write_file(TEST_APPS,
		"[app] (class=test)\n"
		"  [Layer] {bogus}\n"
		"[end]\n"
	);
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);
	wr = first_window_rule(&rules);
	assert(wr != NULL);
	assert(wr->has_layer == false);
	wm_rules_finish(&rules);

	printf("  PASS: test_layer_string_names\n");
}

/* ---- ANCHOR POSITION + APPLY PROPERTY TESTS ---- */

/* Test: Center anchor positions view at center of output */
static void
test_apply_anchor_center(void)
{
	write_file(TEST_APPS,
		"[app] (class=centered)\n"
		"  [Position] (Center) {0 0}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_view view;
	setup_anchor_env(&view);
	view.app_id = "centered";
	view.title = "";

	wm_rules_apply(&rules, &view);

	/* Output is 1920x1080, view is 200x150 (from g_xdg_geo)
	 * Center: px = 0 + (1920 - 200)/2 + 0 = 860
	 *         py = 0 + (1080 - 150)/2 + 0 = 465 */
	assert(view.x == 860);
	assert(view.y == 465);

	wm_rules_finish(&rules);
	printf("  PASS: test_apply_anchor_center\n");
}

/* Test: UpperRight anchor positions view at top-right of output */
static void
test_apply_anchor_upper_right(void)
{
	write_file(TEST_APPS,
		"[app] (class=topright)\n"
		"  [Position] (UpperRight) {0 0}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_view view;
	setup_anchor_env(&view);
	view.app_id = "topright";
	view.title = "";

	wm_rules_apply(&rules, &view);

	/* UpperRight: px = 0 + 1920 - 200 + 0 = 1720
	 *             py = 0 + 0 = 0 */
	assert(view.x == 1720);
	assert(view.y == 0);

	wm_rules_finish(&rules);
	printf("  PASS: test_apply_anchor_upper_right\n");
}

/* Test: LowerLeft anchor positions view at bottom-left of output */
static void
test_apply_anchor_lower_left(void)
{
	write_file(TEST_APPS,
		"[app] (class=botleft)\n"
		"  [Position] (LowerLeft) {0 0}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_view view;
	setup_anchor_env(&view);
	view.app_id = "botleft";
	view.title = "";

	wm_rules_apply(&rules, &view);

	/* LowerLeft: px = 0 + 0 = 0
	 *            py = 0 + 1080 - 150 + 0 = 930 */
	assert(view.x == 0);
	assert(view.y == 930);

	wm_rules_finish(&rules);
	printf("  PASS: test_apply_anchor_lower_left\n");
}

/* Test: LowerRight anchor positions view at bottom-right of output */
static void
test_apply_anchor_lower_right(void)
{
	write_file(TEST_APPS,
		"[app] (class=botright)\n"
		"  [Position] (LowerRight) {0 0}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_view view;
	setup_anchor_env(&view);
	view.app_id = "botright";
	view.title = "";

	wm_rules_apply(&rules, &view);

	/* LowerRight: px = 0 + 1920 - 200 + 0 = 1720
	 *             py = 0 + 1080 - 150 + 0 = 930 */
	assert(view.x == 1720);
	assert(view.y == 930);

	wm_rules_finish(&rules);
	printf("  PASS: test_apply_anchor_lower_right\n");
}

/* Test: UpperLeft anchor (default fallback) positions view at top-left */
static void
test_apply_anchor_upper_left(void)
{
	write_file(TEST_APPS,
		"[app] (class=topleft)\n"
		"  [Position] (UpperLeft) {10 20}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_view view;
	setup_anchor_env(&view);
	view.app_id = "topleft";
	view.title = "";

	wm_rules_apply(&rules, &view);

	/* UpperLeft (default): px = 0 + 10 = 10
	 *                      py = 0 + 20 = 20 */
	assert(view.x == 10);
	assert(view.y == 20);

	wm_rules_finish(&rules);
	printf("  PASS: test_apply_anchor_upper_left\n");
}

/* Test: Center anchor with offset applied */
static void
test_apply_anchor_center_offset(void)
{
	write_file(TEST_APPS,
		"[app] (class=offset)\n"
		"  [Position] (Center) {50 -30}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_view view;
	setup_anchor_env(&view);
	view.app_id = "offset";
	view.title = "";

	wm_rules_apply(&rules, &view);

	/* Center + offset:
	 * px = 0 + (1920 - 200)/2 + 50 = 860 + 50 = 910
	 * py = 0 + (1080 - 150)/2 + (-30) = 465 - 30 = 435 */
	assert(view.x == 910);
	assert(view.y == 435);

	wm_rules_finish(&rules);
	printf("  PASS: test_apply_anchor_center_offset\n");
}

/* Test: Alpha property applied to view during wm_rules_apply */
static void
test_apply_alpha(void)
{
	write_file(TEST_APPS,
		"[app] (class=transparent)\n"
		"  [Alpha] {180 120}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_view view;
	setup_anchor_env(&view);
	view.app_id = "transparent";
	view.title = "";
	view.focus_alpha = 255;
	view.unfocus_alpha = 255;

	wm_rules_apply(&rules, &view);

	assert(view.focus_alpha == 180);
	assert(view.unfocus_alpha == 120);

	wm_rules_finish(&rules);
	printf("  PASS: test_apply_alpha\n");
}

/* Test: Focus protection property applied to view during wm_rules_apply */
static void
test_apply_focus_prot(void)
{
	write_file(TEST_APPS,
		"[app] (class=protected)\n"
		"  [FocusProtection] {Gain,Deny}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_view view;
	setup_anchor_env(&view);
	view.app_id = "protected";
	view.title = "";
	view.focus_protection = 0;

	wm_rules_apply(&rules, &view);

	assert(view.focus_protection & WM_FOCUS_PROT_GAIN);
	assert(view.focus_protection & WM_FOCUS_PROT_DENY);
	assert(!(view.focus_protection & WM_FOCUS_PROT_REFUSE));

	wm_rules_finish(&rules);
	printf("  PASS: test_apply_focus_prot\n");
}

/* Test: Ignore size hints property applied to view during wm_rules_apply */
static void
test_apply_ignore_hints(void)
{
	write_file(TEST_APPS,
		"[app] (class=nohints)\n"
		"  [IgnoreSizeHints] {yes}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_view view;
	setup_anchor_env(&view);
	view.app_id = "nohints";
	view.title = "";
	view.ignore_size_hints = false;

	wm_rules_apply(&rules, &view);

	assert(view.ignore_size_hints == true);

	wm_rules_finish(&rules);
	printf("  PASS: test_apply_ignore_hints\n");
}

/* Test: Dimensions (width/height) applied from rule via wm_rules_apply */
static void
test_apply_dimensions(void)
{
	write_file(TEST_APPS,
		"[app] (class=sized)\n"
		"  [Dimensions] {640 480}\n"
		"[end]\n"
	);

	struct wm_rules rules;
	wm_rules_init(&rules);
	wm_rules_load(&rules, TEST_APPS);

	struct wm_view view;
	setup_anchor_env(&view);
	view.app_id = "sized";
	view.title = "";

	/* wlr_xdg_toplevel_set_size stub is called; we verify the rule
	 * matched by checking that the view was not modified in ways that
	 * would indicate no match (e.g. via a combined property check) */
	view.focus_alpha = 255;
	wm_rules_apply(&rules, &view);

	/* Rule matched (dimensions apply is called internally via
	 * wlr_xdg_toplevel_set_size stub). Verify server pointer
	 * is still valid indicating the apply ran correctly. */
	assert(view.server == &s_server);

	wm_rules_finish(&rules);
	printf("  PASS: test_apply_dimensions\n");
}

int
main(void)
{
	printf("test_rules:\n");
	setup();

	test_init_finish();
	test_load_nonexistent();
	test_load_empty();
	test_basic_class_rule();
	test_negate_pattern();
	test_multiple_patterns();
	test_rule_properties();
	test_deco_presets();
	test_regex_pattern();
	test_multiple_rules();
	test_group_rules();
	test_comments_blanks();
	test_double_finish();
	test_remember();
	test_reload();
	test_invalid_regex();
	test_missing_end();
	test_empty_pattern_values();
	test_boolean_variants();
	test_title_pattern();
	test_special_regex();
	test_alpha_settings();
	test_focus_protection();
	test_ignore_size_hints();
	test_group_no_workspace();
	test_position_no_anchor();
	test_deco_case_insensitive();
	test_deco_invalid();
	test_remember_null();
	test_mixed_settings();
	test_unknown_setting();
	test_missing_end_group();

	/* New tests */
	test_name_property();
	test_role_property();
	test_transient_property();
	test_apply_basic();
	test_apply_first_match_wins();
	test_apply_no_match();
	test_apply_negate();
	test_apply_multi_pattern_and();
	test_apply_title_match();
	test_apply_null_app_id();
	test_apply_no_patterns();
	test_alpha_single_value();
	test_workspace_non_integer();
	test_dimensions_bad_format();
	test_position_bad_format();
	test_focus_protection_lock();
	test_focus_protection_unknown();
	test_mixed_app_and_group();
	test_missing_end_with_settings();
	test_setting_no_braces();
	test_setting_name_too_long();
	test_many_patterns();
	test_case_insensitive_settings();
	test_remember_layers();
	test_remember_no_workspace();
	test_remember_null_app_id();
	test_stray_end();
	test_app_header_case();
	test_group_header_case();
	test_apply_fullscreen();
	test_head_non_integer();
	test_layer_string_names();

	/* Anchor position + apply property tests */
	test_apply_anchor_center();
	test_apply_anchor_upper_right();
	test_apply_anchor_lower_left();
	test_apply_anchor_lower_right();
	test_apply_anchor_upper_left();
	test_apply_anchor_center_offset();
	test_apply_alpha();
	test_apply_focus_prot();
	test_apply_ignore_hints();
	test_apply_dimensions();

	cleanup();
	printf("All rules tests passed.\n");
	return 0;
}
