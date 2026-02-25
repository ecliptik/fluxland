/*
 * test_config.c - Unit tests for configuration parser
 */

#define _POSIX_C_SOURCE 200809L

#include "config.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST_DIR "/tmp/fluxland-test/wm-test-config"
#define TEST_INIT TEST_DIR "/init"

static void
setup(void)
{
	mkdir("/tmp/fluxland-test", 0755);
	mkdir(TEST_DIR, 0755);
}

static void
cleanup(void)
{
	unlink(TEST_INIT);
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

/* Test: config_create returns valid defaults, config_destroy frees cleanly */
static void
test_create_destroy(void)
{
	struct wm_config *c = config_create();
	assert(c != NULL);

	/* Check defaults */
	assert(c->workspace_count == 4);
	assert(c->focus_policy == WM_FOCUS_CLICK);
	assert(c->click_raises == true);
	assert(c->focus_new_windows == true);
	assert(c->opaque_move == true);
	assert(c->opaque_resize == false);
	assert(c->edge_snap_threshold == 10);
	assert(c->placement_policy == WM_PLACEMENT_ROW_SMART);
	assert(c->toolbar_visible == true);
	assert(c->toolbar_placement == WM_TOOLBAR_BOTTOM_CENTER);
	assert(c->toolbar_auto_hide == false);
	assert(c->toolbar_auto_hide_delay_ms == 500);
	assert(c->toolbar_width_percent == 100);
	assert(c->workspace_warping == true);
	assert(c->raise_on_focus == false);
	assert(c->auto_raise_delay_ms == 250);

	/* Default workspace names */
	assert(c->workspace_name_count == 4);
	assert(c->workspace_names != NULL);
	assert(strcmp(c->workspace_names[0], "Workspace 1") == 0);
	assert(strcmp(c->workspace_names[3], "Workspace 4") == 0);

	/* New config defaults */
	assert(c->toolbar_height == 0);
	assert(c->toolbar_layer == 4);
	assert(c->toolbar_alpha == 255);
	assert(c->toolbar_on_head == 0);
	assert(c->titlebar_left != NULL);
	assert(strcmp(c->titlebar_left, "Stick") == 0);
	assert(c->titlebar_right != NULL);
	assert(strcmp(c->titlebar_right, "Shade Minimize Maximize Close") == 0);
	assert(c->clock_format != NULL);
	assert(strcmp(c->clock_format, "%H:%M") == 0);
	assert(c->iconbar_mode == 0);
	assert(c->full_maximization == false);
	assert(c->window_focus_alpha == 255);
	assert(c->window_unfocus_alpha == 255);
	assert(c->slit_auto_hide == false);
	assert(c->slit_placement == 4);
	assert(c->slit_direction == 0);
	assert(c->slit_layer == 4);
	assert(c->slit_on_head == 0);

	config_destroy(c);
	printf("  PASS: test_create_destroy\n");
}

/* Test: config_destroy(NULL) is safe */
static void
test_destroy_null(void)
{
	config_destroy(NULL);
	printf("  PASS: test_destroy_null\n");
}

/* Test: config_load with non-existent file uses defaults */
static void
test_load_nonexistent(void)
{
	struct wm_config *c = config_create();
	assert(c != NULL);

	/* Point config_dir to a temp dir that has no init file */
	free(c->config_dir);
	c->config_dir = strdup(TEST_DIR);

	int ret = config_load(c, NULL);
	/* Should succeed (no file = use defaults) */
	assert(ret == 0);

	/* Defaults should still be in place */
	assert(c->workspace_count == 4);
	assert(c->focus_policy == WM_FOCUS_CLICK);

	config_destroy(c);
	printf("  PASS: test_load_nonexistent\n");
}

/* Test: config_load with explicit path parses settings */
static void
test_load_basic(void)
{
	write_file(TEST_INIT,
		"session.screen0.workspaces: 6\n"
		"session.screen0.focusModel: MouseFocus\n"
		"session.screen0.clickRaises: false\n"
		"session.screen0.opaqueMove: false\n"
		"session.screen0.opaqueResize: true\n"
		"session.screen0.edgeSnapThreshold: 20\n"
		"session.screen0.toolbar.visible: false\n"
		"session.autoRaiseDelay: 100\n"
	);

	struct wm_config *c = config_create();
	assert(c != NULL);

	int ret = config_load(c, TEST_INIT);
	assert(ret == 0);

	assert(c->workspace_count == 6);
	assert(c->focus_policy == WM_FOCUS_SLOPPY);
	assert(c->click_raises == false);
	assert(c->opaque_move == false);
	assert(c->opaque_resize == true);
	assert(c->edge_snap_threshold == 20);
	assert(c->toolbar_visible == false);
	assert(c->auto_raise_delay_ms == 100);

	config_destroy(c);
	printf("  PASS: test_load_basic\n");
}

/* Test: focus model parsing - all variants */
static void
test_focus_models(void)
{
	/* ClickFocus (default) */
	write_file(TEST_INIT,
		"session.screen0.focusModel: ClickToFocus\n");
	struct wm_config *c = config_create();
	config_load(c, TEST_INIT);
	assert(c->focus_policy == WM_FOCUS_CLICK);
	config_destroy(c);

	/* MouseFocus (sloppy) */
	write_file(TEST_INIT,
		"session.screen0.focusModel: MouseFocus\n");
	c = config_create();
	config_load(c, TEST_INIT);
	assert(c->focus_policy == WM_FOCUS_SLOPPY);
	config_destroy(c);

	/* StrictMouseFocus */
	write_file(TEST_INIT,
		"session.screen0.focusModel: StrictMouseFocus\n");
	c = config_create();
	config_load(c, TEST_INIT);
	assert(c->focus_policy == WM_FOCUS_STRICT_MOUSE);
	config_destroy(c);

	printf("  PASS: test_focus_models\n");
}

/* Test: placement policy parsing */
static void
test_placement_policies(void)
{
	write_file(TEST_INIT,
		"session.screen0.windowPlacement: ColSmartPlacement\n");
	struct wm_config *c = config_create();
	config_load(c, TEST_INIT);
	assert(c->placement_policy == WM_PLACEMENT_COL_SMART);
	config_destroy(c);

	write_file(TEST_INIT,
		"session.screen0.windowPlacement: CascadePlacement\n");
	c = config_create();
	config_load(c, TEST_INIT);
	assert(c->placement_policy == WM_PLACEMENT_CASCADE);
	config_destroy(c);

	write_file(TEST_INIT,
		"session.screen0.windowPlacement: UnderMousePlacement\n");
	c = config_create();
	config_load(c, TEST_INIT);
	assert(c->placement_policy == WM_PLACEMENT_UNDER_MOUSE);
	config_destroy(c);

	printf("  PASS: test_placement_policies\n");
}

/* Test: workspace names parsing */
static void
test_workspace_names(void)
{
	write_file(TEST_INIT,
		"session.screen0.workspaces: 3\n"
		"session.screen0.workspaceNames: Main,Code,Web\n");

	struct wm_config *c = config_create();
	config_load(c, TEST_INIT);

	assert(c->workspace_count == 3);
	assert(c->workspace_name_count == 3);
	assert(strcmp(c->workspace_names[0], "Main") == 0);
	assert(strcmp(c->workspace_names[1], "Code") == 0);
	assert(strcmp(c->workspace_names[2], "Web") == 0);

	config_destroy(c);
	printf("  PASS: test_workspace_names\n");
}

/* Test: workspace count clamping */
static void
test_workspace_count_clamp(void)
{
	/* Too low */
	write_file(TEST_INIT,
		"session.screen0.workspaces: 0\n");
	struct wm_config *c = config_create();
	config_load(c, TEST_INIT);
	assert(c->workspace_count == 1);
	config_destroy(c);

	/* Too high */
	write_file(TEST_INIT,
		"session.screen0.workspaces: 100\n");
	c = config_create();
	config_load(c, TEST_INIT);
	assert(c->workspace_count == 32);
	config_destroy(c);

	printf("  PASS: test_workspace_count_clamp\n");
}

/* Test: XKB keyboard layout settings */
static void
test_xkb_settings(void)
{
	write_file(TEST_INIT,
		"session.xkb.rules: evdev\n"
		"session.xkb.model: pc105\n"
		"session.xkb.layout: us,de\n"
		"session.xkb.variant: ,nodeadkeys\n"
		"session.xkb.options: grp:alt_shift_toggle\n");

	struct wm_config *c = config_create();
	config_load(c, TEST_INIT);

	assert(c->xkb_rules != NULL);
	assert(strcmp(c->xkb_rules, "evdev") == 0);
	assert(strcmp(c->xkb_model, "pc105") == 0);
	assert(strcmp(c->xkb_layout, "us,de") == 0);
	assert(strcmp(c->xkb_variant, ",nodeadkeys") == 0);
	assert(strcmp(c->xkb_options, "grp:alt_shift_toggle") == 0);

	config_destroy(c);
	printf("  PASS: test_xkb_settings\n");
}

/* Test: new config options (Phase 1) */
static void
test_new_config_options(void)
{
	write_file(TEST_INIT,
		"session.screen0.toolbar.height: 30\n"
		"session.screen0.toolbar.layer: 2\n"
		"session.screen0.toolbar.alpha: 200\n"
		"session.screen0.toolbar.onhead: 1\n"
		"session.titlebar.left: Close\n"
		"session.titlebar.right: Minimize Maximize\n"
		"session.screen0.strftimeFormat: %Y-%m-%d %H:%M:%S\n"
		"session.screen0.iconbar.mode: AllWindows\n"
		"session.screen0.fullMaximization: true\n"
		"session.screen0.window.focus.alpha: 240\n"
		"session.screen0.window.unfocus.alpha: 180\n"
		"session.screen0.slit.autoHide: true\n"
		"session.screen0.slit.placement: BottomRight\n"
		"session.screen0.slit.direction: Horizontal\n"
		"session.screen0.slit.layer: 6\n"
		"session.screen0.slit.onhead: 2\n"
	);

	struct wm_config *c = config_create();
	assert(c != NULL);
	int ret = config_load(c, TEST_INIT);
	assert(ret == 0);

	assert(c->toolbar_height == 30);
	assert(c->toolbar_layer == 2);
	assert(c->toolbar_alpha == 200);
	assert(c->toolbar_on_head == 1);
	assert(strcmp(c->titlebar_left, "Close") == 0);
	assert(strcmp(c->titlebar_right, "Minimize Maximize") == 0);
	assert(strcmp(c->clock_format, "%Y-%m-%d %H:%M:%S") == 0);
	assert(c->iconbar_mode == 1); /* AllWindows */
	assert(c->full_maximization == true);
	assert(c->window_focus_alpha == 240);
	assert(c->window_unfocus_alpha == 180);
	assert(c->slit_auto_hide == true);
	assert(c->slit_placement == 8); /* BottomRight */
	assert(c->slit_direction == 1); /* Horizontal */
	assert(c->slit_layer == 6);
	assert(c->slit_on_head == 2);

	config_destroy(c);
	printf("  PASS: test_new_config_options\n");
}

/* Test: alpha clamping */
static void
test_alpha_clamping(void)
{
	write_file(TEST_INIT,
		"session.screen0.toolbar.alpha: 300\n"
		"session.screen0.window.focus.alpha: -10\n"
		"session.screen0.window.unfocus.alpha: 999\n"
	);

	struct wm_config *c = config_create();
	config_load(c, TEST_INIT);

	assert(c->toolbar_alpha == 255);
	assert(c->window_focus_alpha == 0);
	assert(c->window_unfocus_alpha == 255);

	config_destroy(c);
	printf("  PASS: test_alpha_clamping\n");
}

/* Test: iconbar mode parsing - all variants */
static void
test_iconbar_modes(void)
{
	/* Workspace (default) */
	write_file(TEST_INIT,
		"session.screen0.iconbar.mode: Workspace\n");
	struct wm_config *c = config_create();
	config_load(c, TEST_INIT);
	assert(c->iconbar_mode == 0);
	config_destroy(c);

	/* Icons */
	write_file(TEST_INIT,
		"session.screen0.iconbar.mode: Icons\n");
	c = config_create();
	config_load(c, TEST_INIT);
	assert(c->iconbar_mode == 2);
	config_destroy(c);

	/* NoIcons */
	write_file(TEST_INIT,
		"session.screen0.iconbar.mode: NoIcons\n");
	c = config_create();
	config_load(c, TEST_INIT);
	assert(c->iconbar_mode == 3);
	config_destroy(c);

	/* WorkspaceIcons */
	write_file(TEST_INIT,
		"session.screen0.iconbar.mode: WorkspaceIcons\n");
	c = config_create();
	config_load(c, TEST_INIT);
	assert(c->iconbar_mode == 4);
	config_destroy(c);

	printf("  PASS: test_iconbar_modes\n");
}

/* Test: config_reload re-reads file */
static void
test_reload(void)
{
	write_file(TEST_INIT,
		"session.screen0.workspaces: 2\n");

	struct wm_config *c = config_create();
	free(c->config_dir);
	c->config_dir = strdup(TEST_DIR);

	config_load(c, NULL);
	assert(c->workspace_count == 2);

	/* Change file and reload */
	write_file(TEST_INIT,
		"session.screen0.workspaces: 8\n");

	config_reload(c);
	assert(c->workspace_count == 8);

	config_destroy(c);
	printf("  PASS: test_reload\n");
}

/* Test: Phase 5B new config options */
static void
test_phase5b_config_options(void)
{
	write_file(TEST_INIT,
		"session.doubleClickInterval: 500\n"
		"session.screen0.rootCommand: xsetroot -solid grey\n"
		"session.screen0.menu.alpha: 200\n"
		"session.screen0.toolbar.maxOver: true\n"
		"session.screen0.edgeResizeSnapThreshold: 15\n"
		"session.screen0.defaultDeco: BORDER\n"
		"session.screen0.struts: 0 0 32 0\n"
		"session.styleOverlay: /tmp/claude-test-fluxland/overlay\n"
	);

	struct wm_config *c = config_create();
	config_load(c, TEST_INIT);

	assert(c->double_click_interval == 500);
	assert(c->root_command &&
		strcmp(c->root_command, "xsetroot -solid grey") == 0);
	assert(c->menu_alpha == 200);
	assert(c->toolbar_max_over == true);
	assert(c->edge_resize_snap_threshold == 15);
	assert(c->default_deco &&
		strcmp(c->default_deco, "BORDER") == 0);
	assert(c->struts[0] == 0);
	assert(c->struts[1] == 0);
	assert(c->struts[2] == 32);
	assert(c->struts[3] == 0);
	assert(c->style_overlay != NULL);

	config_destroy(c);
	printf("  PASS: test_phase5b_config_options\n");
}

/* Test: double-click interval clamping */
static void
test_double_click_interval_clamp(void)
{
	/* Too small */
	write_file(TEST_INIT,
		"session.doubleClickInterval: 10\n");
	struct wm_config *c = config_create();
	config_load(c, TEST_INIT);
	assert(c->double_click_interval == 50);
	config_destroy(c);

	/* Too large */
	write_file(TEST_INIT,
		"session.doubleClickInterval: 5000\n");
	c = config_create();
	config_load(c, TEST_INIT);
	assert(c->double_click_interval == 2000);
	config_destroy(c);

	printf("  PASS: test_double_click_interval_clamp\n");
}

/* Test: menu alpha clamping */
static void
test_menu_alpha_clamp(void)
{
	write_file(TEST_INIT,
		"session.screen0.menu.alpha: -5\n");
	struct wm_config *c = config_create();
	config_load(c, TEST_INIT);
	assert(c->menu_alpha == 0);
	config_destroy(c);

	write_file(TEST_INIT,
		"session.screen0.menu.alpha: 300\n");
	c = config_create();
	config_load(c, TEST_INIT);
	assert(c->menu_alpha == 255);
	config_destroy(c);

	printf("  PASS: test_menu_alpha_clamp\n");
}

/* Test: Phase 5C menu search, delay, and window menu file config */
static void
test_phase5c_config_options(void)
{
	/* Test menuSearch = itemstart */
	write_file(TEST_INIT,
		"session.menuSearch: itemstart\n"
		"session.screen0.menuDelay: 300\n"
		"session.screen0.windowMenu: /tmp/claude-test-fluxland/windowmenu\n"
	);
	struct wm_config *c = config_create();
	config_load(c, TEST_INIT);
	assert(c->menu_search == WM_MENU_SEARCH_ITEMSTART);
	assert(c->menu_delay == 300);
	assert(c->window_menu_file &&
		strcmp(c->window_menu_file,
			"/tmp/claude-test-fluxland/windowmenu") == 0);
	config_destroy(c);

	/* Test menuSearch = somewhere */
	write_file(TEST_INIT,
		"session.menuSearch: somewhere\n");
	c = config_create();
	config_load(c, TEST_INIT);
	assert(c->menu_search == WM_MENU_SEARCH_SOMEWHERE);
	config_destroy(c);

	/* Test menuSearch = nowhere (default) */
	write_file(TEST_INIT,
		"session.menuSearch: nowhere\n");
	c = config_create();
	config_load(c, TEST_INIT);
	assert(c->menu_search == WM_MENU_SEARCH_NOWHERE);
	config_destroy(c);

	/* Test defaults (no config set) */
	write_file(TEST_INIT, "\n");
	c = config_create();
	config_load(c, TEST_INIT);
	assert(c->menu_search == WM_MENU_SEARCH_NOWHERE);
	assert(c->menu_delay == 200);
	assert(c->window_menu_file == NULL);
	config_destroy(c);

	/* Test menu_delay clamping */
	write_file(TEST_INIT,
		"session.screen0.menuDelay: -10\n");
	c = config_create();
	config_load(c, TEST_INIT);
	assert(c->menu_delay == 0);
	config_destroy(c);

	write_file(TEST_INIT,
		"session.screen0.menuDelay: 9999\n");
	c = config_create();
	config_load(c, TEST_INIT);
	assert(c->menu_delay == 5000);
	config_destroy(c);

	printf("  PASS: test_phase5c_config_options\n");
}

/* Test: malformed lines (missing colon) are silently skipped */
static void
test_malformed_lines(void)
{
	write_file(TEST_INIT,
		"session.screen0.workspaces: 6\n"
		"this line has no colon\n"
		"also no separator here\n"
		"session.screen0.toolbar.visible: false\n"
	);

	struct wm_config *c = config_create();
	int ret = config_load(c, TEST_INIT);
	assert(ret == 0);

	/* Valid lines should still be parsed */
	assert(c->workspace_count == 6);
	assert(c->toolbar_visible == false);

	config_destroy(c);
	printf("  PASS: test_malformed_lines\n");
}

/* Test: empty values use defaults */
static void
test_empty_values(void)
{
	write_file(TEST_INIT,
		"session.screen0.workspaces:\n"
		"session.screen0.focusModel:\n"
	);

	struct wm_config *c = config_create();
	int ret = config_load(c, TEST_INIT);
	assert(ret == 0);

	/* rcparser stores empty string; rc_get_int returns default for "" */
	assert(c->workspace_count == 4);
	/* parse_focus_model returns default for unknown string */
	assert(c->focus_policy == WM_FOCUS_CLICK);

	config_destroy(c);
	printf("  PASS: test_empty_values\n");
}

/* Test: duplicate keys - last value wins */
static void
test_duplicate_keys(void)
{
	write_file(TEST_INIT,
		"session.screen0.workspaces: 2\n"
		"session.screen0.workspaces: 8\n"
	);

	struct wm_config *c = config_create();
	int ret = config_load(c, TEST_INIT);
	assert(ret == 0);

	/* rc_set overwrites existing key, so last value wins */
	assert(c->workspace_count == 8);

	config_destroy(c);
	printf("  PASS: test_duplicate_keys\n");
}

/* Test: very long lines are skipped by rcparser */
static void
test_very_long_lines(void)
{
	/* Build a file with a line > 1024 chars, then a normal line */
	FILE *f = fopen(TEST_INIT, "w");
	assert(f);
	fprintf(f, "session.screen0.workspaces: ");
	for (int i = 0; i < 1100; i++)
		fputc('1', f);
	fprintf(f, "\n");
	fprintf(f, "session.screen0.toolbar.visible: false\n");
	fclose(f);

	struct wm_config *c = config_create();
	int ret = config_load(c, TEST_INIT);
	assert(ret == 0);

	/* The long line should have been skipped */
	assert(c->workspace_count == 4); /* default */
	/* The short line after should have been parsed */
	assert(c->toolbar_visible == false);

	config_destroy(c);
	printf("  PASS: test_very_long_lines\n");
}

/* Test: workspace names with empty middle entry */
static void
test_empty_workspace_names(void)
{
	/* strtok coalesces consecutive delimiters, so ",, " yields only
	 * one token. Use "A, , C" to get a trimmed-empty middle name. */
	write_file(TEST_INIT,
		"session.screen0.workspaces: 3\n"
		"session.screen0.workspaceNames: A, , C\n"
	);

	struct wm_config *c = config_create();
	int ret = config_load(c, TEST_INIT);
	assert(ret == 0);

	assert(c->workspace_name_count == 3);
	assert(strcmp(c->workspace_names[0], "A") == 0);
	/* Middle name is space-only, trims to empty string */
	assert(strcmp(c->workspace_names[1], "") == 0);
	assert(strcmp(c->workspace_names[2], "C") == 0);

	config_destroy(c);
	printf("  PASS: test_empty_workspace_names\n");
}

/* Test: all toolbar placement variants */
static void
test_all_toolbar_placements(void)
{
	const struct {
		const char *value;
		enum wm_toolbar_placement expected;
	} cases[] = {
		{"TopLeft", WM_TOOLBAR_TOP_LEFT},
		{"TopCenter", WM_TOOLBAR_TOP_CENTER},
		{"TopRight", WM_TOOLBAR_TOP_RIGHT},
		{"BottomLeft", WM_TOOLBAR_BOTTOM_LEFT},
		{"BottomCenter", WM_TOOLBAR_BOTTOM_CENTER},
		{"BottomRight", WM_TOOLBAR_BOTTOM_RIGHT},
	};

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		char buf[256];
		snprintf(buf, sizeof(buf),
			"session.screen0.toolbar.placement: %s\n",
			cases[i].value);
		write_file(TEST_INIT, buf);

		struct wm_config *c = config_create();
		config_load(c, TEST_INIT);
		assert(c->toolbar_placement == cases[i].expected);
		config_destroy(c);
	}

	/* Unknown value should default to BottomCenter */
	write_file(TEST_INIT,
		"session.screen0.toolbar.placement: InvalidPlace\n");
	struct wm_config *c = config_create();
	config_load(c, TEST_INIT);
	assert(c->toolbar_placement == WM_TOOLBAR_BOTTOM_CENTER);
	config_destroy(c);

	printf("  PASS: test_all_toolbar_placements\n");
}

/* Test: all slit placement variants */
static void
test_all_slit_placements(void)
{
	const struct {
		const char *value;
		int expected;
	} cases[] = {
		{"TopLeft", 0},
		{"TopCenter", 1},
		{"TopRight", 2},
		{"RightTop", 3},
		{"RightCenter", 4},
		{"RightBottom", 5},
		{"BottomLeft", 6},
		{"BottomCenter", 7},
		{"BottomRight", 8},
		{"LeftTop", 9},
		{"LeftCenter", 10},
		{"LeftBottom", 11},
	};

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		char buf[256];
		snprintf(buf, sizeof(buf),
			"session.screen0.slit.placement: %s\n",
			cases[i].value);
		write_file(TEST_INIT, buf);

		struct wm_config *c = config_create();
		config_load(c, TEST_INIT);
		assert(c->slit_placement == cases[i].expected);
		config_destroy(c);
	}

	/* Unknown value defaults to 4 (RightCenter) */
	write_file(TEST_INIT,
		"session.screen0.slit.placement: Nonsense\n");
	struct wm_config *c = config_create();
	config_load(c, TEST_INIT);
	assert(c->slit_placement == 4);
	config_destroy(c);

	printf("  PASS: test_all_slit_placements\n");
}

/* Test: incomplete struts (fewer than 4 values) */
static void
test_incomplete_struts(void)
{
	/* Only 2 values provided */
	write_file(TEST_INIT,
		"session.screen0.struts: 10 20\n"
	);

	struct wm_config *c = config_create();
	config_load(c, TEST_INIT);

	assert(c->struts[0] == 10);
	assert(c->struts[1] == 20);
	/* sscanf only reads what it can; remaining should be 0 (calloc) */
	assert(c->struts[2] == 0);
	assert(c->struts[3] == 0);

	config_destroy(c);
	printf("  PASS: test_incomplete_struts\n");
}

/* Test: slit layer parsing (named and numeric) */
static void
test_slit_layer_parsing(void)
{
	/* AboveDock / Above = 6 */
	write_file(TEST_INIT,
		"session.screen0.slit.layer: AboveDock\n");
	struct wm_config *c = config_create();
	config_load(c, TEST_INIT);
	assert(c->slit_layer == 6);
	config_destroy(c);

	write_file(TEST_INIT,
		"session.screen0.slit.layer: Above\n");
	c = config_create();
	config_load(c, TEST_INIT);
	assert(c->slit_layer == 6);
	config_destroy(c);

	/* Bottom = 2 */
	write_file(TEST_INIT,
		"session.screen0.slit.layer: Bottom\n");
	c = config_create();
	config_load(c, TEST_INIT);
	assert(c->slit_layer == 2);
	config_destroy(c);

	/* Normal = 4, Dock = 4 */
	write_file(TEST_INIT,
		"session.screen0.slit.layer: Normal\n");
	c = config_create();
	config_load(c, TEST_INIT);
	assert(c->slit_layer == 4);
	config_destroy(c);

	write_file(TEST_INIT,
		"session.screen0.slit.layer: Dock\n");
	c = config_create();
	config_load(c, TEST_INIT);
	assert(c->slit_layer == 4);
	config_destroy(c);

	printf("  PASS: test_slit_layer_parsing\n");
}

/* Test: slit direction parsing */
static void
test_slit_direction_parsing(void)
{
	write_file(TEST_INIT,
		"session.screen0.slit.direction: Horizontal\n");
	struct wm_config *c = config_create();
	config_load(c, TEST_INIT);
	assert(c->slit_direction == 1);
	config_destroy(c);

	write_file(TEST_INIT,
		"session.screen0.slit.direction: Vertical\n");
	c = config_create();
	config_load(c, TEST_INIT);
	assert(c->slit_direction == 0);
	config_destroy(c);

	/* Unknown defaults to Vertical (0) */
	write_file(TEST_INIT,
		"session.screen0.slit.direction: Sideways\n");
	c = config_create();
	config_load(c, TEST_INIT);
	assert(c->slit_direction == 0);
	config_destroy(c);

	printf("  PASS: test_slit_direction_parsing\n");
}

/* Test: slit alpha clamping */
static void
test_slit_alpha_clamp(void)
{
	write_file(TEST_INIT,
		"session.screen0.slit.alpha: -10\n");
	struct wm_config *c = config_create();
	config_load(c, TEST_INIT);
	assert(c->slit_alpha == 0);
	config_destroy(c);

	write_file(TEST_INIT,
		"session.screen0.slit.alpha: 999\n");
	c = config_create();
	config_load(c, TEST_INIT);
	assert(c->slit_alpha == 255);
	config_destroy(c);

	printf("  PASS: test_slit_alpha_clamp\n");
}

/* Test: toolbar width percent clamping */
static void
test_toolbar_width_percent_clamp(void)
{
	/* Below minimum (10) */
	write_file(TEST_INIT,
		"session.screen0.toolbar.widthPercent: 1\n");
	struct wm_config *c = config_create();
	config_load(c, TEST_INIT);
	assert(c->toolbar_width_percent == 10);
	config_destroy(c);

	/* Above maximum (100) */
	write_file(TEST_INIT,
		"session.screen0.toolbar.widthPercent: 200\n");
	c = config_create();
	config_load(c, TEST_INIT);
	assert(c->toolbar_width_percent == 100);
	config_destroy(c);

	printf("  PASS: test_toolbar_width_percent_clamp\n");
}

/* Test: animation duration clamping */
static void
test_animation_duration_clamp(void)
{
	write_file(TEST_INIT,
		"session.screen0.animationDuration: -100\n");
	struct wm_config *c = config_create();
	config_load(c, TEST_INIT);
	assert(c->animation_duration_ms == 0);
	config_destroy(c);

	write_file(TEST_INIT,
		"session.screen0.animationDuration: 99999\n");
	c = config_create();
	config_load(c, TEST_INIT);
	assert(c->animation_duration_ms == 5000);
	config_destroy(c);

	printf("  PASS: test_animation_duration_clamp\n");
}

/* Test: all remaining placement policies */
static void
test_remaining_placement_policies(void)
{
	write_file(TEST_INIT,
		"session.screen0.windowPlacement: RowMinOverlapPlacement\n");
	struct wm_config *c = config_create();
	config_load(c, TEST_INIT);
	assert(c->placement_policy == WM_PLACEMENT_ROW_MIN_OVERLAP);
	config_destroy(c);

	write_file(TEST_INIT,
		"session.screen0.windowPlacement: ColMinOverlapPlacement\n");
	c = config_create();
	config_load(c, TEST_INIT);
	assert(c->placement_policy == WM_PLACEMENT_COL_MIN_OVERLAP);
	config_destroy(c);

	/* Unknown placement defaults to RowSmart */
	write_file(TEST_INIT,
		"session.screen0.windowPlacement: UnknownPlacement\n");
	c = config_create();
	config_load(c, TEST_INIT);
	assert(c->placement_policy == WM_PLACEMENT_ROW_SMART);
	config_destroy(c);

	printf("  PASS: test_remaining_placement_policies\n");
}

/* Test: workspace mode (per-output vs global) */
static void
test_workspace_mode(void)
{
	write_file(TEST_INIT,
		"session.screen0.workspaceMode: per-output\n");
	struct wm_config *c = config_create();
	config_load(c, TEST_INIT);
	assert(c->workspace_mode == WM_WORKSPACE_PER_OUTPUT);
	config_destroy(c);

	write_file(TEST_INIT,
		"session.screen0.workspaceMode: global\n");
	c = config_create();
	config_load(c, TEST_INIT);
	assert(c->workspace_mode == WM_WORKSPACE_GLOBAL);
	config_destroy(c);

	printf("  PASS: test_workspace_mode\n");
}

/* Test: snap zone threshold clamping */
static void
test_snap_zone_threshold_clamp(void)
{
	write_file(TEST_INIT,
		"session.screen0.enableWindowSnapping: true\n"
		"session.screen0.snapZoneThreshold: 0\n");
	struct wm_config *c = config_create();
	config_load(c, TEST_INIT);
	assert(c->snap_zone_threshold == 1);
	config_destroy(c);

	write_file(TEST_INIT,
		"session.screen0.snapZoneThreshold: 999\n");
	c = config_create();
	config_load(c, TEST_INIT);
	assert(c->snap_zone_threshold == 100);
	config_destroy(c);

	printf("  PASS: test_snap_zone_threshold_clamp\n");
}

/* Test: config_load with NULL config_dir returns error */
static void
test_load_null_config_dir(void)
{
	struct wm_config *c = config_create();
	free(c->config_dir);
	c->config_dir = NULL;

	int ret = config_load(c, NULL);
	assert(ret == -1);

	config_destroy(c);
	printf("  PASS: test_load_null_config_dir\n");
}

/* Test: comments and blank lines are skipped */
static void
test_comments_and_blank_lines(void)
{
	write_file(TEST_INIT,
		"# This is a comment\n"
		"! This is also a comment\n"
		"\n"
		"   \n"
		"session.screen0.workspaces: 7\n"
	);

	struct wm_config *c = config_create();
	int ret = config_load(c, TEST_INIT);
	assert(ret == 0);
	assert(c->workspace_count == 7);

	config_destroy(c);
	printf("  PASS: test_comments_and_blank_lines\n");
}

int
main(void)
{
	printf("test_config:\n");
	setup();

	test_create_destroy();
	test_destroy_null();
	test_load_nonexistent();
	test_load_basic();
	test_focus_models();
	test_placement_policies();
	test_workspace_names();
	test_workspace_count_clamp();
	test_xkb_settings();
	test_new_config_options();
	test_alpha_clamping();
	test_iconbar_modes();
	test_reload();
	test_phase5b_config_options();
	test_double_click_interval_clamp();
	test_menu_alpha_clamp();
	test_phase5c_config_options();

	/* New coverage tests */
	test_malformed_lines();
	test_empty_values();
	test_duplicate_keys();
	test_very_long_lines();
	test_empty_workspace_names();
	test_all_toolbar_placements();
	test_all_slit_placements();
	test_incomplete_struts();
	test_slit_layer_parsing();
	test_slit_direction_parsing();
	test_slit_alpha_clamp();
	test_toolbar_width_percent_clamp();
	test_animation_duration_clamp();
	test_remaining_placement_policies();
	test_workspace_mode();
	test_snap_zone_threshold_clamp();
	test_load_null_config_dir();
	test_comments_and_blank_lines();

	cleanup();
	printf("All config tests passed.\n");
	return 0;
}
