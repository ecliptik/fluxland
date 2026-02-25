/*
 * test_mousebind.c - Unit tests for mouse binding parser
 */

#define _POSIX_C_SOURCE 200809L

#include "mousebind.h"
#include <assert.h>
#include <linux/input-event-codes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wlr/types/wlr_keyboard.h>

#define TEST_DIR "/tmp/fluxland-test/wm-test-mousebind"
#define TEST_KEYS TEST_DIR "/keys"

static void
setup(void)
{
	mkdir("/tmp/fluxland-test", 0755);
	mkdir(TEST_DIR, 0755);
}

static void
cleanup(void)
{
	unlink(TEST_KEYS);
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

/* Test: basic mouse binding loading and lookup */
static void
test_load_basic(void)
{
	write_file(TEST_KEYS,
		"OnTitlebar Mouse1 :Raise\n"
		"OnDesktop Mouse3 :RootMenu\n"
	);

	struct wl_list bindings;
	wl_list_init(&bindings);

	int ret = mousebind_load(&bindings, TEST_KEYS, NULL);
	assert(ret == 0);

	/* Find OnTitlebar Mouse1 -> Raise */
	struct wm_mousebind *bind = mousebind_find(&bindings,
		WM_MOUSE_CTX_TITLEBAR, WM_MOUSE_PRESS, BTN_LEFT, 0);
	assert(bind != NULL);
	assert(bind->action == WM_ACTION_RAISE);

	/* Find OnDesktop Mouse3 -> RootMenu */
	bind = mousebind_find(&bindings,
		WM_MOUSE_CTX_DESKTOP, WM_MOUSE_PRESS, BTN_RIGHT, 0);
	assert(bind != NULL);
	assert(bind->action == WM_ACTION_ROOT_MENU);

	mousebind_destroy_list(&bindings);
	printf("  PASS: test_load_basic\n");
}

/* Test: all context types */
static void
test_contexts(void)
{
	write_file(TEST_KEYS,
		"OnDesktop Mouse1 :Focus\n"
		"OnToolbar Mouse2 :Raise\n"
		"OnTitlebar Mouse3 :Raise\n"
		"OnTab Mouse1 :ActivateTab\n"
		"OnWindow Mouse2 :Focus\n"
		"OnWindowBorder Mouse3 :StartResizing\n"
		"OnLeftGrip Mouse1 :StartResizing\n"
		"OnRightGrip Mouse2 :StartResizing\n"
		"OnSlit Mouse3 :Raise\n"
	);

	struct wl_list bindings;
	wl_list_init(&bindings);
	mousebind_load(&bindings, TEST_KEYS, NULL);

	assert(mousebind_find(&bindings, WM_MOUSE_CTX_DESKTOP,
		WM_MOUSE_PRESS, BTN_LEFT, 0) != NULL);
	assert(mousebind_find(&bindings, WM_MOUSE_CTX_TOOLBAR,
		WM_MOUSE_PRESS, BTN_MIDDLE, 0) != NULL);
	assert(mousebind_find(&bindings, WM_MOUSE_CTX_TITLEBAR,
		WM_MOUSE_PRESS, BTN_RIGHT, 0) != NULL);
	assert(mousebind_find(&bindings, WM_MOUSE_CTX_TAB,
		WM_MOUSE_PRESS, BTN_LEFT, 0) != NULL);
	assert(mousebind_find(&bindings, WM_MOUSE_CTX_WINDOW,
		WM_MOUSE_PRESS, BTN_MIDDLE, 0) != NULL);
	assert(mousebind_find(&bindings, WM_MOUSE_CTX_WINDOW_BORDER,
		WM_MOUSE_PRESS, BTN_RIGHT, 0) != NULL);
	assert(mousebind_find(&bindings, WM_MOUSE_CTX_LEFT_GRIP,
		WM_MOUSE_PRESS, BTN_LEFT, 0) != NULL);
	assert(mousebind_find(&bindings, WM_MOUSE_CTX_RIGHT_GRIP,
		WM_MOUSE_PRESS, BTN_MIDDLE, 0) != NULL);
	assert(mousebind_find(&bindings, WM_MOUSE_CTX_SLIT,
		WM_MOUSE_PRESS, BTN_RIGHT, 0) != NULL);

	mousebind_destroy_list(&bindings);
	printf("  PASS: test_contexts\n");
}

/* Test: all event types (Mouse, Click, Double, Move) */
static void
test_event_types(void)
{
	write_file(TEST_KEYS,
		"OnTitlebar Mouse1 :Raise\n"
		"OnTitlebar Click1 :Focus\n"
		"OnTitlebar Double1 :Maximize\n"
		"OnTitlebar Move1 :StartMoving\n"
	);

	struct wl_list bindings;
	wl_list_init(&bindings);
	mousebind_load(&bindings, TEST_KEYS, NULL);

	struct wm_mousebind *bind;

	bind = mousebind_find(&bindings, WM_MOUSE_CTX_TITLEBAR,
		WM_MOUSE_PRESS, BTN_LEFT, 0);
	assert(bind && bind->action == WM_ACTION_RAISE);

	bind = mousebind_find(&bindings, WM_MOUSE_CTX_TITLEBAR,
		WM_MOUSE_CLICK, BTN_LEFT, 0);
	assert(bind && bind->action == WM_ACTION_FOCUS);

	bind = mousebind_find(&bindings, WM_MOUSE_CTX_TITLEBAR,
		WM_MOUSE_DOUBLE, BTN_LEFT, 0);
	assert(bind && bind->action == WM_ACTION_MAXIMIZE);

	bind = mousebind_find(&bindings, WM_MOUSE_CTX_TITLEBAR,
		WM_MOUSE_MOVE, BTN_LEFT, 0);
	assert(bind && bind->action == WM_ACTION_START_MOVING);

	mousebind_destroy_list(&bindings);
	printf("  PASS: test_event_types\n");
}

/* Test: all button numbers (1-5) */
static void
test_button_numbers(void)
{
	write_file(TEST_KEYS,
		"OnDesktop Mouse1 :Focus\n"
		"OnDesktop Mouse2 :ShowDesktop\n"
		"OnDesktop Mouse3 :RootMenu\n"
		"OnDesktop Mouse4 :NextWorkspace\n"
		"OnDesktop Mouse5 :PrevWorkspace\n"
	);

	struct wl_list bindings;
	wl_list_init(&bindings);
	mousebind_load(&bindings, TEST_KEYS, NULL);

	assert(mousebind_find(&bindings, WM_MOUSE_CTX_DESKTOP,
		WM_MOUSE_PRESS, BTN_LEFT, 0) != NULL);
	assert(mousebind_find(&bindings, WM_MOUSE_CTX_DESKTOP,
		WM_MOUSE_PRESS, BTN_MIDDLE, 0) != NULL);
	assert(mousebind_find(&bindings, WM_MOUSE_CTX_DESKTOP,
		WM_MOUSE_PRESS, BTN_RIGHT, 0) != NULL);
	assert(mousebind_find(&bindings, WM_MOUSE_CTX_DESKTOP,
		WM_MOUSE_PRESS, BTN_SIDE, 0) != NULL);
	assert(mousebind_find(&bindings, WM_MOUSE_CTX_DESKTOP,
		WM_MOUSE_PRESS, BTN_EXTRA, 0) != NULL);

	mousebind_destroy_list(&bindings);
	printf("  PASS: test_button_numbers\n");
}

/* Test: modifier combinations */
static void
test_modifiers(void)
{
	write_file(TEST_KEYS,
		"Mod4 Mouse1 :StartMoving\n"
		"Mod1 Mouse3 :StartResizing\n"
		"Control Shift Mouse1 :Close\n"
		"None Mouse2 :ShowDesktop\n"
	);

	struct wl_list bindings;
	wl_list_init(&bindings);
	mousebind_load(&bindings, TEST_KEYS, NULL);

	struct wm_mousebind *bind;

	/* Mod4+Mouse1 (global, no context) */
	bind = mousebind_find(&bindings, WM_MOUSE_CTX_NONE,
		WM_MOUSE_PRESS, BTN_LEFT, WLR_MODIFIER_LOGO);
	assert(bind && bind->action == WM_ACTION_START_MOVING);

	/* Mod1+Mouse3 */
	bind = mousebind_find(&bindings, WM_MOUSE_CTX_NONE,
		WM_MOUSE_PRESS, BTN_RIGHT, WLR_MODIFIER_ALT);
	assert(bind && bind->action == WM_ACTION_START_RESIZING);

	/* Ctrl+Shift+Mouse1 */
	bind = mousebind_find(&bindings, WM_MOUSE_CTX_NONE,
		WM_MOUSE_PRESS, BTN_LEFT,
		WLR_MODIFIER_CTRL | WLR_MODIFIER_SHIFT);
	assert(bind && bind->action == WM_ACTION_CLOSE);

	/* None+Mouse2 (no modifiers) */
	bind = mousebind_find(&bindings, WM_MOUSE_CTX_NONE,
		WM_MOUSE_PRESS, BTN_MIDDLE, 0);
	assert(bind && bind->action == WM_ACTION_SHOW_DESKTOP);

	mousebind_destroy_list(&bindings);
	printf("  PASS: test_modifiers\n");
}

/* Test: context combined with modifier */
static void
test_context_with_modifier(void)
{
	write_file(TEST_KEYS,
		"OnTitlebar Mod4 Mouse1 :StartMoving\n"
		"OnWindow Mod1 Mouse3 :StartResizing\n"
	);

	struct wl_list bindings;
	wl_list_init(&bindings);
	mousebind_load(&bindings, TEST_KEYS, NULL);

	struct wm_mousebind *bind;

	bind = mousebind_find(&bindings, WM_MOUSE_CTX_TITLEBAR,
		WM_MOUSE_PRESS, BTN_LEFT, WLR_MODIFIER_LOGO);
	assert(bind && bind->action == WM_ACTION_START_MOVING);

	bind = mousebind_find(&bindings, WM_MOUSE_CTX_WINDOW,
		WM_MOUSE_PRESS, BTN_RIGHT, WLR_MODIFIER_ALT);
	assert(bind && bind->action == WM_ACTION_START_RESIZING);

	mousebind_destroy_list(&bindings);
	printf("  PASS: test_context_with_modifier\n");
}

/* Test: global bindings (CTX_NONE) match any context as fallback */
static void
test_global_fallback(void)
{
	write_file(TEST_KEYS,
		"Mod4 Mouse1 :StartMoving\n"
	);

	struct wl_list bindings;
	wl_list_init(&bindings);
	mousebind_load(&bindings, TEST_KEYS, NULL);

	/* Global binding should match any context via fallback */
	assert(mousebind_find(&bindings, WM_MOUSE_CTX_TITLEBAR,
		WM_MOUSE_PRESS, BTN_LEFT, WLR_MODIFIER_LOGO) != NULL);
	assert(mousebind_find(&bindings, WM_MOUSE_CTX_DESKTOP,
		WM_MOUSE_PRESS, BTN_LEFT, WLR_MODIFIER_LOGO) != NULL);
	assert(mousebind_find(&bindings, WM_MOUSE_CTX_WINDOW,
		WM_MOUSE_PRESS, BTN_LEFT, WLR_MODIFIER_LOGO) != NULL);
	assert(mousebind_find(&bindings, WM_MOUSE_CTX_TOOLBAR,
		WM_MOUSE_PRESS, BTN_LEFT, WLR_MODIFIER_LOGO) != NULL);

	mousebind_destroy_list(&bindings);
	printf("  PASS: test_global_fallback\n");
}

/* Test: OnWindow matches titlebar, border, grips, and tab */
static void
test_window_context_fallback(void)
{
	write_file(TEST_KEYS,
		"OnWindow Mouse1 :Focus\n"
	);

	struct wl_list bindings;
	wl_list_init(&bindings);
	mousebind_load(&bindings, TEST_KEYS, NULL);

	/* OnWindow should match titlebar, border, grips, tab */
	assert(mousebind_find(&bindings, WM_MOUSE_CTX_TITLEBAR,
		WM_MOUSE_PRESS, BTN_LEFT, 0) != NULL);
	assert(mousebind_find(&bindings, WM_MOUSE_CTX_WINDOW_BORDER,
		WM_MOUSE_PRESS, BTN_LEFT, 0) != NULL);
	assert(mousebind_find(&bindings, WM_MOUSE_CTX_LEFT_GRIP,
		WM_MOUSE_PRESS, BTN_LEFT, 0) != NULL);
	assert(mousebind_find(&bindings, WM_MOUSE_CTX_RIGHT_GRIP,
		WM_MOUSE_PRESS, BTN_LEFT, 0) != NULL);
	assert(mousebind_find(&bindings, WM_MOUSE_CTX_TAB,
		WM_MOUSE_PRESS, BTN_LEFT, 0) != NULL);

	/* But NOT desktop, toolbar, or slit */
	assert(mousebind_find(&bindings, WM_MOUSE_CTX_DESKTOP,
		WM_MOUSE_PRESS, BTN_LEFT, 0) == NULL);
	assert(mousebind_find(&bindings, WM_MOUSE_CTX_TOOLBAR,
		WM_MOUSE_PRESS, BTN_LEFT, 0) == NULL);
	assert(mousebind_find(&bindings, WM_MOUSE_CTX_SLIT,
		WM_MOUSE_PRESS, BTN_LEFT, 0) == NULL);

	mousebind_destroy_list(&bindings);
	printf("  PASS: test_window_context_fallback\n");
}

/* Test: exact context takes priority over global fallback */
static void
test_context_priority(void)
{
	write_file(TEST_KEYS,
		"Mouse1 :Lower\n"
		"OnTitlebar Mouse1 :Raise\n"
	);

	struct wl_list bindings;
	wl_list_init(&bindings);
	mousebind_load(&bindings, TEST_KEYS, NULL);

	/* Exact context match should win over global */
	struct wm_mousebind *bind = mousebind_find(&bindings,
		WM_MOUSE_CTX_TITLEBAR, WM_MOUSE_PRESS, BTN_LEFT, 0);
	assert(bind && bind->action == WM_ACTION_RAISE);

	/* Desktop should fall through to global binding */
	bind = mousebind_find(&bindings,
		WM_MOUSE_CTX_DESKTOP, WM_MOUSE_PRESS, BTN_LEFT, 0);
	assert(bind && bind->action == WM_ACTION_LOWER);

	mousebind_destroy_list(&bindings);
	printf("  PASS: test_context_priority\n");
}

/* Test: MacroCmd parsing with sub-commands */
static void
test_macrocmd(void)
{
	write_file(TEST_KEYS,
		"OnTitlebar Mouse1 :MacroCmd {Raise} {Focus} {ActivateTab}\n"
	);

	struct wl_list bindings;
	wl_list_init(&bindings);
	mousebind_load(&bindings, TEST_KEYS, NULL);

	struct wm_mousebind *bind = mousebind_find(&bindings,
		WM_MOUSE_CTX_TITLEBAR, WM_MOUSE_PRESS, BTN_LEFT, 0);
	assert(bind != NULL);
	assert(bind->action == WM_ACTION_MACRO_CMD);
	assert(bind->subcmd_count == 3);
	assert(bind->subcmds != NULL);
	assert(bind->subcmds->action == WM_ACTION_RAISE);
	assert(bind->subcmds->next != NULL);
	assert(bind->subcmds->next->action == WM_ACTION_FOCUS);
	assert(bind->subcmds->next->next != NULL);
	assert(bind->subcmds->next->next->action == WM_ACTION_ACTIVATE_TAB);

	mousebind_destroy_list(&bindings);
	printf("  PASS: test_macrocmd\n");
}

/* Test: ToggleCmd parsing */
static void
test_togglecmd(void)
{
	write_file(TEST_KEYS,
		"OnTitlebar Mouse1 :ToggleCmd {Shade} {ShadeOff}\n"
	);

	struct wl_list bindings;
	wl_list_init(&bindings);
	mousebind_load(&bindings, TEST_KEYS, NULL);

	struct wm_mousebind *bind = mousebind_find(&bindings,
		WM_MOUSE_CTX_TITLEBAR, WM_MOUSE_PRESS, BTN_LEFT, 0);
	assert(bind != NULL);
	assert(bind->action == WM_ACTION_TOGGLE_CMD);
	assert(bind->subcmd_count == 2);
	assert(bind->subcmds != NULL);
	assert(bind->subcmds->action == WM_ACTION_SHADE);
	assert(bind->subcmds->next != NULL);
	assert(bind->subcmds->next->action == WM_ACTION_SHADE_OFF);

	mousebind_destroy_list(&bindings);
	printf("  PASS: test_togglecmd\n");
}

/* Test: action with argument */
static void
test_action_argument(void)
{
	write_file(TEST_KEYS,
		"OnDesktop Mouse1 :Exec xterm -e top\n"
		"OnDesktop Mouse2 :Workspace 2\n"
	);

	struct wl_list bindings;
	wl_list_init(&bindings);
	mousebind_load(&bindings, TEST_KEYS, NULL);

	struct wm_mousebind *bind;

	bind = mousebind_find(&bindings, WM_MOUSE_CTX_DESKTOP,
		WM_MOUSE_PRESS, BTN_LEFT, 0);
	assert(bind && bind->action == WM_ACTION_EXEC);
	assert(bind->argument != NULL);
	assert(strcmp(bind->argument, "xterm -e top") == 0);

	bind = mousebind_find(&bindings, WM_MOUSE_CTX_DESKTOP,
		WM_MOUSE_PRESS, BTN_MIDDLE, 0);
	assert(bind && bind->action == WM_ACTION_WORKSPACE);
	assert(bind->argument && strcmp(bind->argument, "2") == 0);

	mousebind_destroy_list(&bindings);
	printf("  PASS: test_action_argument\n");
}

/* Test: comments and blank lines are skipped */
static void
test_comments_blanks(void)
{
	write_file(TEST_KEYS,
		"# Comment line\n"
		"! Another comment\n"
		"\n"
		"   \n"
		"OnDesktop Mouse1 :RootMenu\n"
	);

	struct wl_list bindings;
	wl_list_init(&bindings);
	mousebind_load(&bindings, TEST_KEYS, NULL);

	assert(mousebind_find(&bindings, WM_MOUSE_CTX_DESKTOP,
		WM_MOUSE_PRESS, BTN_LEFT, 0) != NULL);

	mousebind_destroy_list(&bindings);
	printf("  PASS: test_comments_blanks\n");
}

/* Test: keyboard-only lines are not loaded as mouse bindings */
static void
test_skips_keybinds(void)
{
	write_file(TEST_KEYS,
		"Mod4 Return :Exec foot\n"
		"Mod4 q :Close\n"
		"OnDesktop Mouse3 :RootMenu\n"
	);

	struct wl_list bindings;
	wl_list_init(&bindings);
	mousebind_load(&bindings, TEST_KEYS, NULL);

	/* Count total bindings - should only contain the mouse binding */
	int count = 0;
	struct wm_mousebind *b;
	wl_list_for_each(b, &bindings, link) {
		count++;
	}
	assert(count == 1);

	struct wm_mousebind *bind = mousebind_find(&bindings,
		WM_MOUSE_CTX_DESKTOP, WM_MOUSE_PRESS, BTN_RIGHT, 0);
	assert(bind != NULL);
	assert(bind->action == WM_ACTION_ROOT_MENU);

	mousebind_destroy_list(&bindings);
	printf("  PASS: test_skips_keybinds\n");
}

/* Test: loading nonexistent file returns error */
static void
test_load_nonexistent(void)
{
	struct wl_list bindings;
	wl_list_init(&bindings);

	int ret = mousebind_load(&bindings,
		"/tmp/fluxland-test/nonexistent_mouse_file", NULL);
	assert(ret == -1);

	mousebind_destroy_list(&bindings);
	printf("  PASS: test_load_nonexistent\n");
}

/* Test: custom button mapping */
static void
test_button_mapping(void)
{
	write_file(TEST_KEYS,
		"OnTitlebar Mouse1 :Raise\n"
		"OnTitlebar Mouse3 :Lower\n"
	);

	/* Custom mapping: swap left and right */
	uint32_t button_map[6] = {
		0, BTN_RIGHT, BTN_MIDDLE, BTN_LEFT, BTN_SIDE, BTN_EXTRA
	};

	struct wl_list bindings;
	wl_list_init(&bindings);
	mousebind_load(&bindings, TEST_KEYS, button_map);

	/* Mouse1 with swapped map -> BTN_RIGHT */
	struct wm_mousebind *bind = mousebind_find(&bindings,
		WM_MOUSE_CTX_TITLEBAR, WM_MOUSE_PRESS, BTN_RIGHT, 0);
	assert(bind && bind->action == WM_ACTION_RAISE);

	/* Mouse3 with swapped map -> BTN_LEFT */
	bind = mousebind_find(&bindings,
		WM_MOUSE_CTX_TITLEBAR, WM_MOUSE_PRESS, BTN_LEFT, 0);
	assert(bind && bind->action == WM_ACTION_LOWER);

	mousebind_destroy_list(&bindings);
	printf("  PASS: test_button_mapping\n");
}

/* Test: action name aliases */
static void
test_action_aliases(void)
{
	write_file(TEST_KEYS,
		"OnTitlebar Mouse1 :ExecCommand echo hi\n"
		"OnTitlebar Mouse2 :Iconify\n"
		"OnTitlebar Mouse3 :ShadeWindow\n"
		"OnDesktop Mouse1 :Execute echo world\n"
		"OnDesktop Mouse2 :KillWindow\n"
		"OnDesktop Mouse3 :StickWindow\n"
	);

	struct wl_list bindings;
	wl_list_init(&bindings);
	mousebind_load(&bindings, TEST_KEYS, NULL);

	struct wm_mousebind *bind;

	/* ExecCommand -> EXEC */
	bind = mousebind_find(&bindings, WM_MOUSE_CTX_TITLEBAR,
		WM_MOUSE_PRESS, BTN_LEFT, 0);
	assert(bind && bind->action == WM_ACTION_EXEC);

	/* Iconify -> MINIMIZE */
	bind = mousebind_find(&bindings, WM_MOUSE_CTX_TITLEBAR,
		WM_MOUSE_PRESS, BTN_MIDDLE, 0);
	assert(bind && bind->action == WM_ACTION_MINIMIZE);

	/* ShadeWindow -> SHADE */
	bind = mousebind_find(&bindings, WM_MOUSE_CTX_TITLEBAR,
		WM_MOUSE_PRESS, BTN_RIGHT, 0);
	assert(bind && bind->action == WM_ACTION_SHADE);

	/* Execute -> EXEC */
	bind = mousebind_find(&bindings, WM_MOUSE_CTX_DESKTOP,
		WM_MOUSE_PRESS, BTN_LEFT, 0);
	assert(bind && bind->action == WM_ACTION_EXEC);

	/* KillWindow -> KILL */
	bind = mousebind_find(&bindings, WM_MOUSE_CTX_DESKTOP,
		WM_MOUSE_PRESS, BTN_MIDDLE, 0);
	assert(bind && bind->action == WM_ACTION_KILL);

	/* StickWindow -> STICK */
	bind = mousebind_find(&bindings, WM_MOUSE_CTX_DESKTOP,
		WM_MOUSE_PRESS, BTN_RIGHT, 0);
	assert(bind && bind->action == WM_ACTION_STICK);

	mousebind_destroy_list(&bindings);
	printf("  PASS: test_action_aliases\n");
}

/* Test: no match returns NULL */
static void
test_no_match(void)
{
	write_file(TEST_KEYS,
		"OnTitlebar Mouse1 :Raise\n"
	);

	struct wl_list bindings;
	wl_list_init(&bindings);
	mousebind_load(&bindings, TEST_KEYS, NULL);

	/* Wrong button */
	assert(mousebind_find(&bindings, WM_MOUSE_CTX_TITLEBAR,
		WM_MOUSE_PRESS, BTN_RIGHT, 0) == NULL);

	/* Wrong event type */
	assert(mousebind_find(&bindings, WM_MOUSE_CTX_TITLEBAR,
		WM_MOUSE_CLICK, BTN_LEFT, 0) == NULL);

	/* Wrong modifiers */
	assert(mousebind_find(&bindings, WM_MOUSE_CTX_TITLEBAR,
		WM_MOUSE_PRESS, BTN_LEFT, WLR_MODIFIER_LOGO) == NULL);

	mousebind_destroy_list(&bindings);
	printf("  PASS: test_no_match\n");
}

/* Test: destroy empty list is safe */
static void
test_destroy_empty(void)
{
	struct wl_list bindings;
	wl_list_init(&bindings);
	mousebind_destroy_list(&bindings);
	printf("  PASS: test_destroy_empty\n");
}

/* Test: various window management actions */
static void
test_window_actions(void)
{
	write_file(TEST_KEYS,
		"OnTitlebar Mouse1 :Raise\n"
		"OnTitlebar Mouse2 :Lower\n"
		"OnTitlebar Mouse3 :WindowMenu\n"
		"OnTitlebar Move1 :StartMoving\n"
		"OnWindowBorder Move1 :StartResizing\n"
		"OnTitlebar Double1 :Maximize\n"
		"OnDesktop Mouse1 :HideMenus\n"
		"OnDesktop Click3 :RootMenu\n"
	);

	struct wl_list bindings;
	wl_list_init(&bindings);
	mousebind_load(&bindings, TEST_KEYS, NULL);

	struct wm_mousebind *bind;

	bind = mousebind_find(&bindings, WM_MOUSE_CTX_TITLEBAR,
		WM_MOUSE_PRESS, BTN_LEFT, 0);
	assert(bind && bind->action == WM_ACTION_RAISE);

	bind = mousebind_find(&bindings, WM_MOUSE_CTX_TITLEBAR,
		WM_MOUSE_PRESS, BTN_MIDDLE, 0);
	assert(bind && bind->action == WM_ACTION_LOWER);

	bind = mousebind_find(&bindings, WM_MOUSE_CTX_TITLEBAR,
		WM_MOUSE_PRESS, BTN_RIGHT, 0);
	assert(bind && bind->action == WM_ACTION_WINDOW_MENU);

	bind = mousebind_find(&bindings, WM_MOUSE_CTX_TITLEBAR,
		WM_MOUSE_MOVE, BTN_LEFT, 0);
	assert(bind && bind->action == WM_ACTION_START_MOVING);

	bind = mousebind_find(&bindings, WM_MOUSE_CTX_WINDOW_BORDER,
		WM_MOUSE_MOVE, BTN_LEFT, 0);
	assert(bind && bind->action == WM_ACTION_START_RESIZING);

	bind = mousebind_find(&bindings, WM_MOUSE_CTX_TITLEBAR,
		WM_MOUSE_DOUBLE, BTN_LEFT, 0);
	assert(bind && bind->action == WM_ACTION_MAXIMIZE);

	bind = mousebind_find(&bindings, WM_MOUSE_CTX_DESKTOP,
		WM_MOUSE_PRESS, BTN_LEFT, 0);
	assert(bind && bind->action == WM_ACTION_HIDE_MENUS);

	bind = mousebind_find(&bindings, WM_MOUSE_CTX_DESKTOP,
		WM_MOUSE_CLICK, BTN_RIGHT, 0);
	assert(bind && bind->action == WM_ACTION_ROOT_MENU);

	mousebind_destroy_list(&bindings);
	printf("  PASS: test_window_actions\n");
}

int
main(void)
{
	printf("test_mousebind:\n");
	setup();

	test_load_basic();
	test_contexts();
	test_event_types();
	test_button_numbers();
	test_modifiers();
	test_context_with_modifier();
	test_global_fallback();
	test_window_context_fallback();
	test_context_priority();
	test_macrocmd();
	test_togglecmd();
	test_action_argument();
	test_comments_blanks();
	test_skips_keybinds();
	test_load_nonexistent();
	test_button_mapping();
	test_action_aliases();
	test_no_match();
	test_destroy_empty();
	test_window_actions();

	cleanup();
	printf("All mousebind tests passed.\n");
	return 0;
}
