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

	cleanup();
	printf("All config tests passed.\n");
	return 0;
}
