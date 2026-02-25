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

	cleanup();
	printf("All rules tests passed.\n");
	return 0;
}
