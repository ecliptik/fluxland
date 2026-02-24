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
void wm_decoration_set_shaded(struct wm_decoration *d, bool s,
	struct wm_style *st)
	{ (void)d; (void)s; (void)st; }
void wm_view_get_geometry(struct wm_view *v, struct wlr_box *b)
	{ (void)v; if (b) { b->x = 0; b->y = 0; b->width = 100; b->height = 100; } }

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

	cleanup();
	printf("All rules tests passed.\n");
	return 0;
}
