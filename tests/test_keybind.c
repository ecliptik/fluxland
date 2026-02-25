/*
 * test_keybind.c - Unit tests for keybinding parser
 */

#define _POSIX_C_SOURCE 200809L

#include "keybind.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wlr/types/wlr_keyboard.h>
#include <xkbcommon/xkbcommon.h>

#define TEST_DIR "/tmp/fluxland-test/wm-test-keybind"
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

/* Test: basic keybind loading and lookup */
static void
test_load_basic(void)
{
	write_file(TEST_KEYS,
		"Mod4 Return :Exec foot\n"
		"Mod4 q :Close\n"
		"Mod1 F4 :Kill\n"
	);

	struct wl_list keymodes;
	wl_list_init(&keymodes);

	int ret = keybind_load(&keymodes, TEST_KEYS);
	assert(ret == 0);

	/* Find the default mode */
	struct wm_keymode *mode;
	struct wm_keymode *default_mode = NULL;
	wl_list_for_each(mode, &keymodes, link) {
		if (strcmp(mode->name, "default") == 0) {
			default_mode = mode;
			break;
		}
	}
	assert(default_mode != NULL);

	/* Look up Mod4+Return */
	struct wm_keybind *bind = keybind_find(&default_mode->bindings,
		WLR_MODIFIER_LOGO, XKB_KEY_Return);
	assert(bind != NULL);
	assert(bind->action == WM_ACTION_EXEC);
	assert(bind->argument != NULL);
	assert(strcmp(bind->argument, "foot") == 0);

	/* Look up Mod4+q */
	bind = keybind_find(&default_mode->bindings,
		WLR_MODIFIER_LOGO, XKB_KEY_q);
	assert(bind != NULL);
	assert(bind->action == WM_ACTION_CLOSE);

	/* Look up Mod1+F4 */
	bind = keybind_find(&default_mode->bindings,
		WLR_MODIFIER_ALT, XKB_KEY_F4);
	assert(bind != NULL);
	assert(bind->action == WM_ACTION_KILL);

	keybind_destroy_all(&keymodes);
	printf("  PASS: test_load_basic\n");
}

/* Test: modifier combinations */
static void
test_modifier_combos(void)
{
	write_file(TEST_KEYS,
		"Control Shift t :Exec foot\n"
		"Mod4 Shift q :Exit\n"
		"None F11 :Fullscreen\n"
	);

	struct wl_list keymodes;
	wl_list_init(&keymodes);
	keybind_load(&keymodes, TEST_KEYS);

	struct wm_keymode *dm = NULL;
	struct wm_keymode *mode;
	wl_list_for_each(mode, &keymodes, link) {
		if (strcmp(mode->name, "default") == 0) {
			dm = mode;
			break;
		}
	}
	assert(dm != NULL);

	/* Ctrl+Shift+t */
	struct wm_keybind *bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_CTRL | WLR_MODIFIER_SHIFT, XKB_KEY_t);
	assert(bind != NULL);
	assert(bind->action == WM_ACTION_EXEC);

	/* Mod4+Shift+q */
	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT, XKB_KEY_q);
	assert(bind != NULL);
	assert(bind->action == WM_ACTION_EXIT);

	/* None+F11 (no modifiers) */
	bind = keybind_find(&dm->bindings, 0, XKB_KEY_F11);
	assert(bind != NULL);
	assert(bind->action == WM_ACTION_FULLSCREEN);

	keybind_destroy_all(&keymodes);
	printf("  PASS: test_modifier_combos\n");
}

/* Test: action parsing for various actions */
static void
test_actions(void)
{
	write_file(TEST_KEYS,
		"Mod4 f :Fullscreen\n"
		"Mod4 m :Maximize\n"
		"Mod4 n :Minimize\n"
		"Mod4 s :Shade\n"
		"Mod4 d :ShowDesktop\n"
		"Mod4 1 :Workspace 1\n"
		"Mod4 Shift 1 :SendToWorkspace 1\n"
		"Mod4 Tab :NextWindow\n"
		"Mod4 Shift Tab :PrevWindow\n"
		"Mod4 Right :NextWorkspace\n"
		"Mod4 Left :PrevWorkspace\n"
	);

	struct wl_list keymodes;
	wl_list_init(&keymodes);
	keybind_load(&keymodes, TEST_KEYS);

	struct wm_keymode *dm = NULL;
	struct wm_keymode *mode;
	wl_list_for_each(mode, &keymodes, link) {
		if (strcmp(mode->name, "default") == 0) {
			dm = mode;
			break;
		}
	}

	struct wm_keybind *bind;

	bind = keybind_find(&dm->bindings, WLR_MODIFIER_LOGO, XKB_KEY_f);
	assert(bind && bind->action == WM_ACTION_FULLSCREEN);

	bind = keybind_find(&dm->bindings, WLR_MODIFIER_LOGO, XKB_KEY_m);
	assert(bind && bind->action == WM_ACTION_MAXIMIZE);

	bind = keybind_find(&dm->bindings, WLR_MODIFIER_LOGO, XKB_KEY_n);
	assert(bind && bind->action == WM_ACTION_MINIMIZE);

	bind = keybind_find(&dm->bindings, WLR_MODIFIER_LOGO, XKB_KEY_s);
	assert(bind && bind->action == WM_ACTION_SHADE);

	bind = keybind_find(&dm->bindings, WLR_MODIFIER_LOGO, XKB_KEY_d);
	assert(bind && bind->action == WM_ACTION_SHOW_DESKTOP);

	bind = keybind_find(&dm->bindings, WLR_MODIFIER_LOGO, XKB_KEY_1);
	assert(bind && bind->action == WM_ACTION_WORKSPACE);
	assert(bind->argument && strcmp(bind->argument, "1") == 0);

	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT, XKB_KEY_1);
	assert(bind && bind->action == WM_ACTION_SEND_TO_WORKSPACE);

	bind = keybind_find(&dm->bindings, WLR_MODIFIER_LOGO, XKB_KEY_Tab);
	assert(bind && bind->action == WM_ACTION_NEXT_WINDOW);

	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT, XKB_KEY_Tab);
	assert(bind && bind->action == WM_ACTION_PREV_WINDOW);

	bind = keybind_find(&dm->bindings, WLR_MODIFIER_LOGO, XKB_KEY_Right);
	assert(bind && bind->action == WM_ACTION_NEXT_WORKSPACE);

	bind = keybind_find(&dm->bindings, WLR_MODIFIER_LOGO, XKB_KEY_Left);
	assert(bind && bind->action == WM_ACTION_PREV_WORKSPACE);

	keybind_destroy_all(&keymodes);
	printf("  PASS: test_actions\n");
}

/* Test: chained keybindings (multi-key sequences) */
static void
test_chain_bindings(void)
{
	write_file(TEST_KEYS,
		"Mod4 x Mod4 t :Exec foot\n"
		"Mod4 x Mod4 e :Exec emacs\n"
	);

	struct wl_list keymodes;
	wl_list_init(&keymodes);
	keybind_load(&keymodes, TEST_KEYS);

	struct wm_keymode *dm = NULL;
	struct wm_keymode *mode;
	wl_list_for_each(mode, &keymodes, link) {
		if (strcmp(mode->name, "default") == 0) {
			dm = mode;
			break;
		}
	}

	/* First key in chain: Mod4+x should be an intermediate node */
	struct wm_keybind *chain = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO, XKB_KEY_x);
	assert(chain != NULL);
	assert(chain->action == WM_ACTION_NOP); /* intermediate */
	assert(!wl_list_empty(&chain->children));

	/* Second key: Mod4+t under the chain */
	struct wm_keybind *leaf = keybind_find(&chain->children,
		WLR_MODIFIER_LOGO, XKB_KEY_t);
	assert(leaf != NULL);
	assert(leaf->action == WM_ACTION_EXEC);
	assert(leaf->argument && strcmp(leaf->argument, "foot") == 0);

	/* Second key: Mod4+e under the chain */
	leaf = keybind_find(&chain->children,
		WLR_MODIFIER_LOGO, XKB_KEY_e);
	assert(leaf != NULL);
	assert(leaf->action == WM_ACTION_EXEC);
	assert(leaf->argument && strcmp(leaf->argument, "emacs") == 0);

	keybind_destroy_all(&keymodes);
	printf("  PASS: test_chain_bindings\n");
}

/* Test: keymodes */
static void
test_keymodes(void)
{
	write_file(TEST_KEYS,
		"Mod4 r :KeyMode resize\n"
		"resize: h :MoveLeft 10\n"
		"resize: l :MoveRight 10\n"
		"resize: Escape :KeyMode default\n"
	);

	struct wl_list keymodes;
	wl_list_init(&keymodes);
	keybind_load(&keymodes, TEST_KEYS);

	/* Find resize mode */
	struct wm_keymode *resize_mode = NULL;
	struct wm_keymode *mode;
	wl_list_for_each(mode, &keymodes, link) {
		if (strcmp(mode->name, "resize") == 0) {
			resize_mode = mode;
			break;
		}
	}
	assert(resize_mode != NULL);

	/* Check bindings in resize mode */
	struct wm_keybind *bind = keybind_find(&resize_mode->bindings,
		0, XKB_KEY_h);
	assert(bind != NULL);
	assert(bind->action == WM_ACTION_MOVE_LEFT);
	assert(bind->argument && strcmp(bind->argument, "10") == 0);

	bind = keybind_find(&resize_mode->bindings, 0, XKB_KEY_l);
	assert(bind != NULL);
	assert(bind->action == WM_ACTION_MOVE_RIGHT);

	bind = keybind_find(&resize_mode->bindings, 0, XKB_KEY_Escape);
	assert(bind != NULL);
	assert(bind->action == WM_ACTION_KEY_MODE);

	keybind_destroy_all(&keymodes);
	printf("  PASS: test_keymodes\n");
}

/* Test: comments and blank lines are skipped */
static void
test_comments_blanks(void)
{
	write_file(TEST_KEYS,
		"# This is a comment\n"
		"! Another comment style\n"
		"\n"
		"   \n"
		"Mod4 a :Close\n"
	);

	struct wl_list keymodes;
	wl_list_init(&keymodes);
	keybind_load(&keymodes, TEST_KEYS);

	struct wm_keymode *dm = NULL;
	struct wm_keymode *mode;
	wl_list_for_each(mode, &keymodes, link) {
		if (strcmp(mode->name, "default") == 0) {
			dm = mode;
			break;
		}
	}

	struct wm_keybind *bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO, XKB_KEY_a);
	assert(bind != NULL);
	assert(bind->action == WM_ACTION_CLOSE);

	keybind_destroy_all(&keymodes);
	printf("  PASS: test_comments_blanks\n");
}

/* Test: Exec with complex argument (spaces) */
static void
test_exec_argument(void)
{
	write_file(TEST_KEYS,
		"Mod4 p :Exec dmenu_run -fn monospace-12\n"
	);

	struct wl_list keymodes;
	wl_list_init(&keymodes);
	keybind_load(&keymodes, TEST_KEYS);

	struct wm_keymode *dm = NULL;
	struct wm_keymode *mode;
	wl_list_for_each(mode, &keymodes, link) {
		if (strcmp(mode->name, "default") == 0) {
			dm = mode;
			break;
		}
	}

	struct wm_keybind *bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO, XKB_KEY_p);
	assert(bind != NULL);
	assert(bind->action == WM_ACTION_EXEC);
	assert(bind->argument != NULL);
	assert(strcmp(bind->argument, "dmenu_run -fn monospace-12") == 0);

	keybind_destroy_all(&keymodes);
	printf("  PASS: test_exec_argument\n");
}

/* Test: MacroCmd parsing */
static void
test_macrocmd(void)
{
	write_file(TEST_KEYS,
		"Mod4 t :MacroCmd {Maximize} {Raise}\n"
	);

	struct wl_list keymodes;
	wl_list_init(&keymodes);
	keybind_load(&keymodes, TEST_KEYS);

	struct wm_keymode *dm = NULL;
	struct wm_keymode *mode;
	wl_list_for_each(mode, &keymodes, link) {
		if (strcmp(mode->name, "default") == 0) {
			dm = mode;
			break;
		}
	}

	struct wm_keybind *bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO, XKB_KEY_t);
	assert(bind != NULL);
	assert(bind->action == WM_ACTION_MACRO_CMD);
	assert(bind->subcmd_count == 2);
	assert(bind->subcmds != NULL);
	assert(bind->subcmds->action == WM_ACTION_MAXIMIZE);
	assert(bind->subcmds->next != NULL);
	assert(bind->subcmds->next->action == WM_ACTION_RAISE);

	keybind_destroy_all(&keymodes);
	printf("  PASS: test_macrocmd\n");
}

/* Test: loading a nonexistent file returns error */
static void
test_load_nonexistent(void)
{
	struct wl_list keymodes;
	wl_list_init(&keymodes);

	int ret = keybind_load(&keymodes, "/tmp/fluxland-test/nonexistent_keys_file");
	assert(ret == -1);

	keybind_destroy_all(&keymodes);
	printf("  PASS: test_load_nonexistent\n");
}

/* Test: action name aliases (ExecCommand, Execute -> Exec) */
static void
test_action_aliases(void)
{
	write_file(TEST_KEYS,
		"Mod4 a :ExecCommand echo hello\n"
		"Mod4 b :Execute echo world\n"
		"Mod4 c :Iconify\n"
		"Mod4 e :KillWindow\n"
		"Mod4 g :ShadeWindow\n"
		"Mod4 h :StickWindow\n"
		"Mod4 i :Quit\n"
	);

	struct wl_list keymodes;
	wl_list_init(&keymodes);
	keybind_load(&keymodes, TEST_KEYS);

	struct wm_keymode *dm = NULL;
	struct wm_keymode *mode;
	wl_list_for_each(mode, &keymodes, link) {
		if (strcmp(mode->name, "default") == 0) {
			dm = mode;
			break;
		}
	}

	/* ExecCommand -> EXEC */
	struct wm_keybind *bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO, XKB_KEY_a);
	assert(bind && bind->action == WM_ACTION_EXEC);

	/* Execute -> EXEC */
	bind = keybind_find(&dm->bindings, WLR_MODIFIER_LOGO, XKB_KEY_b);
	assert(bind && bind->action == WM_ACTION_EXEC);

	/* Iconify -> MINIMIZE */
	bind = keybind_find(&dm->bindings, WLR_MODIFIER_LOGO, XKB_KEY_c);
	assert(bind && bind->action == WM_ACTION_MINIMIZE);

	/* KillWindow -> KILL */
	bind = keybind_find(&dm->bindings, WLR_MODIFIER_LOGO, XKB_KEY_e);
	assert(bind && bind->action == WM_ACTION_KILL);

	/* ShadeWindow -> SHADE */
	bind = keybind_find(&dm->bindings, WLR_MODIFIER_LOGO, XKB_KEY_g);
	assert(bind && bind->action == WM_ACTION_SHADE);

	/* StickWindow -> STICK */
	bind = keybind_find(&dm->bindings, WLR_MODIFIER_LOGO, XKB_KEY_h);
	assert(bind && bind->action == WM_ACTION_STICK);

	/* Quit -> EXIT */
	bind = keybind_find(&dm->bindings, WLR_MODIFIER_LOGO, XKB_KEY_i);
	assert(bind && bind->action == WM_ACTION_EXIT);

	keybind_destroy_all(&keymodes);
	printf("  PASS: test_action_aliases\n");
}

/* Test: workspace management action names */
static void
test_workspace_actions(void)
{
	write_file(TEST_KEYS,
		"Mod4 Shift 2 :TakeToWorkspace 2\n"
		"Mod4 Control Right :SendToNextWorkspace\n"
		"Mod4 Control Left :SendToPrevWorkspace\n"
		"Mod4 Shift Right :TakeToNextWorkspace\n"
		"Mod4 Shift Left :TakeToPrevWorkspace\n"
		"Mod4 plus :AddWorkspace\n"
		"Mod4 minus :RemoveLastWorkspace\n"
	);

	struct wl_list keymodes;
	wl_list_init(&keymodes);
	keybind_load(&keymodes, TEST_KEYS);

	struct wm_keymode *dm = NULL;
	struct wm_keymode *mode;
	wl_list_for_each(mode, &keymodes, link) {
		if (strcmp(mode->name, "default") == 0) {
			dm = mode;
			break;
		}
	}
	assert(dm != NULL);

	struct wm_keybind *bind;

	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT, XKB_KEY_2);
	assert(bind && bind->action == WM_ACTION_TAKE_TO_WORKSPACE);
	assert(bind->argument && strcmp(bind->argument, "2") == 0);

	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO | WLR_MODIFIER_CTRL, XKB_KEY_Right);
	assert(bind && bind->action == WM_ACTION_SEND_TO_NEXT_WORKSPACE);

	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO | WLR_MODIFIER_CTRL, XKB_KEY_Left);
	assert(bind && bind->action == WM_ACTION_SEND_TO_PREV_WORKSPACE);

	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT, XKB_KEY_Right);
	assert(bind && bind->action == WM_ACTION_TAKE_TO_NEXT_WORKSPACE);

	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT, XKB_KEY_Left);
	assert(bind && bind->action == WM_ACTION_TAKE_TO_PREV_WORKSPACE);

	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO, XKB_KEY_plus);
	assert(bind && bind->action == WM_ACTION_ADD_WORKSPACE);

	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO, XKB_KEY_minus);
	assert(bind && bind->action == WM_ACTION_REMOVE_LAST_WORKSPACE);

	keybind_destroy_all(&keymodes);
	printf("  PASS: test_workspace_actions\n");
}

/* Test: Phase 2 window management action names */
static void
test_window_management_actions(void)
{
	write_file(TEST_KEYS,
		"Mod4 h :LHalf\n"
		"Mod4 l :RHalf\n"
		"Mod4 Control h :ResizeHorizontal -10\n"
		"Mod4 Control l :ResizeHorizontal 10\n"
		"Mod4 Control j :ResizeVertical 10\n"
		"Mod4 Control k :ResizeVertical -10\n"
		"Mod4 Shift h :FocusLeft\n"
		"Mod4 Shift l :FocusRight\n"
		"Mod4 Shift k :FocusUp\n"
		"Mod4 Shift j :FocusDown\n"
		"Mod4 o :SetHead 0\n"
		"Mod4 period :SendToNextHead\n"
		"Mod4 comma :SendToPrevHead\n"
		"Mod4 Shift s :ArrangeWindowsStackRight\n"
		"Mod4 Shift a :ArrangeWindowsStackLeft\n"
		"Mod4 Shift t :ArrangeWindowsStackTop\n"
		"Mod4 Shift b :ArrangeWindowsStackBottom\n"
		"Mod4 Shift c :CloseAllWindows\n"
	);

	struct wl_list keymodes;
	wl_list_init(&keymodes);
	keybind_load(&keymodes, TEST_KEYS);

	struct wm_keymode *dm = NULL;
	struct wm_keymode *mode;
	wl_list_for_each(mode, &keymodes, link) {
		if (strcmp(mode->name, "default") == 0) {
			dm = mode;
			break;
		}
	}
	assert(dm != NULL);

	struct wm_keybind *bind;

	/* LHalf / RHalf */
	bind = keybind_find(&dm->bindings, WLR_MODIFIER_LOGO, XKB_KEY_h);
	assert(bind && bind->action == WM_ACTION_LHALF);

	bind = keybind_find(&dm->bindings, WLR_MODIFIER_LOGO, XKB_KEY_l);
	assert(bind && bind->action == WM_ACTION_RHALF);

	/* ResizeHorizontal / ResizeVertical */
	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO | WLR_MODIFIER_CTRL, XKB_KEY_h);
	assert(bind && bind->action == WM_ACTION_RESIZE_HORIZ);
	assert(bind->argument && strcmp(bind->argument, "-10") == 0);

	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO | WLR_MODIFIER_CTRL, XKB_KEY_l);
	assert(bind && bind->action == WM_ACTION_RESIZE_HORIZ);
	assert(bind->argument && strcmp(bind->argument, "10") == 0);

	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO | WLR_MODIFIER_CTRL, XKB_KEY_j);
	assert(bind && bind->action == WM_ACTION_RESIZE_VERT);

	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO | WLR_MODIFIER_CTRL, XKB_KEY_k);
	assert(bind && bind->action == WM_ACTION_RESIZE_VERT);

	/* Directional focus */
	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT, XKB_KEY_h);
	assert(bind && bind->action == WM_ACTION_FOCUS_LEFT);

	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT, XKB_KEY_l);
	assert(bind && bind->action == WM_ACTION_FOCUS_RIGHT);

	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT, XKB_KEY_k);
	assert(bind && bind->action == WM_ACTION_FOCUS_UP);

	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT, XKB_KEY_j);
	assert(bind && bind->action == WM_ACTION_FOCUS_DOWN);

	/* Multi-monitor */
	bind = keybind_find(&dm->bindings, WLR_MODIFIER_LOGO, XKB_KEY_o);
	assert(bind && bind->action == WM_ACTION_SET_HEAD);
	assert(bind->argument && strcmp(bind->argument, "0") == 0);

	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO, XKB_KEY_period);
	assert(bind && bind->action == WM_ACTION_SEND_TO_NEXT_HEAD);

	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO, XKB_KEY_comma);
	assert(bind && bind->action == WM_ACTION_SEND_TO_PREV_HEAD);

	/* Stack layouts */
	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT, XKB_KEY_s);
	assert(bind && bind->action == WM_ACTION_ARRANGE_STACK_RIGHT);

	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT, XKB_KEY_a);
	assert(bind && bind->action == WM_ACTION_ARRANGE_STACK_LEFT);

	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT, XKB_KEY_t);
	assert(bind && bind->action == WM_ACTION_ARRANGE_STACK_TOP);

	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT, XKB_KEY_b);
	assert(bind && bind->action == WM_ACTION_ARRANGE_STACK_BOTTOM);

	/* CloseAllWindows */
	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT, XKB_KEY_c);
	assert(bind && bind->action == WM_ACTION_CLOSE_ALL_WINDOWS);

	keybind_destroy_all(&keymodes);
	printf("  PASS: test_window_management_actions\n");
}

/* Test: Phase 5A workspace and remember action names */
static void
test_phase5a_actions(void)
{
	write_file(TEST_KEYS,
		"Mod4 bracketright :RightWorkspace\n"
		"Mod4 bracketleft :LeftWorkspace\n"
		"Mod4 F2 :SetWorkspaceName MyWorkspace\n"
		"Mod4 r :Remember\n"
	);

	struct wl_list keymodes;
	wl_list_init(&keymodes);
	keybind_load(&keymodes, TEST_KEYS);

	struct wm_keymode *dm = NULL;
	struct wm_keymode *mode;
	wl_list_for_each(mode, &keymodes, link) {
		if (strcmp(mode->name, "default") == 0) {
			dm = mode;
			break;
		}
	}
	assert(dm != NULL);

	struct wm_keybind *bind;

	/* RightWorkspace */
	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO, XKB_KEY_bracketright);
	assert(bind && bind->action == WM_ACTION_RIGHT_WORKSPACE);

	/* LeftWorkspace */
	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO, XKB_KEY_bracketleft);
	assert(bind && bind->action == WM_ACTION_LEFT_WORKSPACE);

	/* SetWorkspaceName */
	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO, XKB_KEY_F2);
	assert(bind && bind->action == WM_ACTION_SET_WORKSPACE_NAME);
	assert(bind->argument &&
		strcmp(bind->argument, "MyWorkspace") == 0);

	/* Remember */
	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO, XKB_KEY_r);
	assert(bind && bind->action == WM_ACTION_REMEMBER);

	keybind_destroy_all(&keymodes);
	printf("  PASS: test_phase5a_actions\n");
}

/* Test: Phase 4 menu/style action names */
static void
test_menu_style_actions(void)
{
	write_file(TEST_KEYS,
		"Mod4 w :WorkspaceMenu\n"
		"Mod4 c :ClientMenu\n"
		"Mod4 Shift m :CustomMenu /tmp/mymenu\n"
		"Mod4 Shift s :SetStyle /tmp/style\n"
		"Mod4 Shift r :ReloadStyle\n"
		"Mod4 i :Deiconify All\n"
	);

	struct wl_list keymodes;
	wl_list_init(&keymodes);
	keybind_load(&keymodes, TEST_KEYS);

	struct wm_keymode *dm = NULL;
	struct wm_keymode *mode;
	wl_list_for_each(mode, &keymodes, link) {
		if (strcmp(mode->name, "default") == 0) {
			dm = mode;
			break;
		}
	}
	assert(dm != NULL);

	struct wm_keybind *bind;

	/* WorkspaceMenu */
	bind = keybind_find(&dm->bindings, WLR_MODIFIER_LOGO, XKB_KEY_w);
	assert(bind && bind->action == WM_ACTION_WORKSPACE_MENU);

	/* ClientMenu */
	bind = keybind_find(&dm->bindings, WLR_MODIFIER_LOGO, XKB_KEY_c);
	assert(bind && bind->action == WM_ACTION_CLIENT_MENU);

	/* CustomMenu */
	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT, XKB_KEY_m);
	assert(bind && bind->action == WM_ACTION_CUSTOM_MENU);
	assert(bind->argument &&
		strcmp(bind->argument, "/tmp/mymenu") == 0);

	/* SetStyle */
	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT, XKB_KEY_s);
	assert(bind && bind->action == WM_ACTION_SET_STYLE);
	assert(bind->argument &&
		strcmp(bind->argument, "/tmp/style") == 0);

	/* ReloadStyle */
	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT, XKB_KEY_r);
	assert(bind && bind->action == WM_ACTION_RELOAD_STYLE);

	/* Deiconify with argument */
	bind = keybind_find(&dm->bindings, WLR_MODIFIER_LOGO, XKB_KEY_i);
	assert(bind && bind->action == WM_ACTION_DEICONIFY);
	assert(bind->argument && strcmp(bind->argument, "All") == 0);

	keybind_destroy_all(&keymodes);
	printf("  PASS: test_menu_style_actions\n");
}

/* Test: Phase 5B slit toggle action names */
static void
test_slit_toggle_actions(void)
{
	write_file(TEST_KEYS,
		"Mod4 Shift F9 :ToggleSlitAbove\n"
		"Mod4 Shift F10 :ToggleSlitHidden\n"
	);

	struct wl_list keymodes;
	wl_list_init(&keymodes);
	keybind_load(&keymodes, TEST_KEYS);

	struct wm_keymode *dm = NULL;
	struct wm_keymode *mode;
	wl_list_for_each(mode, &keymodes, link) {
		if (strcmp(mode->name, "default") == 0) {
			dm = mode;
			break;
		}
	}
	assert(dm != NULL);

	struct wm_keybind *bind;

	/* ToggleSlitAbove */
	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT, XKB_KEY_F9);
	assert(bind && bind->action == WM_ACTION_TOGGLE_SLIT_ABOVE);

	/* ToggleSlitHidden */
	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT, XKB_KEY_F10);
	assert(bind && bind->action == WM_ACTION_TOGGLE_SLIT_HIDDEN);

	keybind_destroy_all(&keymodes);
	printf("  PASS: test_slit_toggle_actions\n");
}

/* Test: Phase 5C toolbar toggle action names */
static void
test_toolbar_toggle_actions(void)
{
	write_file(TEST_KEYS,
		"Mod4 Shift F11 :ToggleToolbarAbove\n"
		"Mod4 Shift F12 :ToggleToolbarVisible\n"
	);

	struct wl_list keymodes;
	wl_list_init(&keymodes);
	keybind_load(&keymodes, TEST_KEYS);

	struct wm_keymode *dm = NULL;
	struct wm_keymode *mode;
	wl_list_for_each(mode, &keymodes, link) {
		if (strcmp(mode->name, "default") == 0) {
			dm = mode;
			break;
		}
	}
	assert(dm != NULL);

	struct wm_keybind *bind;

	/* ToggleToolbarAbove */
	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT, XKB_KEY_F11);
	assert(bind && bind->action == WM_ACTION_TOGGLE_TOOLBAR_ABOVE);

	/* ToggleToolbarVisible */
	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT, XKB_KEY_F12);
	assert(bind && bind->action == WM_ACTION_TOGGLE_TOOLBAR_VISIBLE);

	keybind_destroy_all(&keymodes);
	printf("  PASS: test_toolbar_toggle_actions\n");
}

/* Test: keybind_find returns NULL for non-existent binding */
static void
test_find_no_match(void)
{
	write_file(TEST_KEYS,
		"Mod4 a :Close\n"
	);

	struct wl_list keymodes;
	wl_list_init(&keymodes);
	keybind_load(&keymodes, TEST_KEYS);

	struct wm_keymode *dm = NULL;
	struct wm_keymode *mode;
	wl_list_for_each(mode, &keymodes, link) {
		if (strcmp(mode->name, "default") == 0) {
			dm = mode;
			break;
		}
	}
	assert(dm != NULL);

	/* Search for a key that doesn't exist */
	struct wm_keybind *bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO, XKB_KEY_z);
	assert(bind == NULL);

	/* Wrong modifier */
	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_ALT, XKB_KEY_a);
	assert(bind == NULL);

	/* No modifier when binding has one */
	bind = keybind_find(&dm->bindings, 0, XKB_KEY_a);
	assert(bind == NULL);

	keybind_destroy_all(&keymodes);
	printf("  PASS: test_find_no_match\n");
}

/* Test: empty keys file produces default mode with no bindings */
static void
test_empty_keys_file(void)
{
	write_file(TEST_KEYS, "# only comments\n\n   \n");

	struct wl_list keymodes;
	wl_list_init(&keymodes);
	int ret = keybind_load(&keymodes, TEST_KEYS);
	assert(ret == 0);

	struct wm_keymode *dm = NULL;
	struct wm_keymode *mode;
	wl_list_for_each(mode, &keymodes, link) {
		if (strcmp(mode->name, "default") == 0) {
			dm = mode;
			break;
		}
	}
	assert(dm != NULL);
	assert(wl_list_empty(&dm->bindings));

	keybind_destroy_all(&keymodes);
	printf("  PASS: test_empty_keys_file\n");
}

/* Test: OnTitlebar/OnDesktop/OnWindow mouse context lines are skipped */
static void
test_on_context_skipped(void)
{
	write_file(TEST_KEYS,
		"OnTitlebar Mouse1 :Raise\n"
		"OnDesktop Mouse3 :RootMenu\n"
		"OnWindow Mod4 Mouse1 :StartMoving\n"
		"OnTitlebar Double Mouse1 :Maximize\n"
		"Mod4 a :Close\n"
	);

	struct wl_list keymodes;
	wl_list_init(&keymodes);
	keybind_load(&keymodes, TEST_KEYS);

	struct wm_keymode *dm = NULL;
	struct wm_keymode *mode;
	wl_list_for_each(mode, &keymodes, link) {
		if (strcmp(mode->name, "default") == 0) {
			dm = mode;
			break;
		}
	}
	assert(dm != NULL);

	/* Only the keyboard binding should be loaded */
	struct wm_keybind *bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO, XKB_KEY_a);
	assert(bind != NULL);
	assert(bind->action == WM_ACTION_CLOSE);

	/* Count bindings — should be exactly 1 */
	int count = 0;
	struct wm_keybind *b;
	wl_list_for_each(b, &dm->bindings, link)
		count++;
	assert(count == 1);

	keybind_destroy_all(&keymodes);
	printf("  PASS: test_on_context_skipped\n");
}

/* Test: deeply nested chain (3 levels) */
static void
test_deep_chain(void)
{
	write_file(TEST_KEYS,
		"Mod4 x Mod4 y Mod4 z :Exec deep\n"
	);

	struct wl_list keymodes;
	wl_list_init(&keymodes);
	keybind_load(&keymodes, TEST_KEYS);

	struct wm_keymode *dm = NULL;
	struct wm_keymode *mode;
	wl_list_for_each(mode, &keymodes, link) {
		if (strcmp(mode->name, "default") == 0) {
			dm = mode;
			break;
		}
	}

	/* Level 1: Mod4+x */
	struct wm_keybind *chain1 = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO, XKB_KEY_x);
	assert(chain1 != NULL);
	assert(chain1->action == WM_ACTION_NOP);

	/* Level 2: Mod4+y */
	struct wm_keybind *chain2 = keybind_find(&chain1->children,
		WLR_MODIFIER_LOGO, XKB_KEY_y);
	assert(chain2 != NULL);
	assert(chain2->action == WM_ACTION_NOP);

	/* Level 3: Mod4+z (leaf) */
	struct wm_keybind *leaf = keybind_find(&chain2->children,
		WLR_MODIFIER_LOGO, XKB_KEY_z);
	assert(leaf != NULL);
	assert(leaf->action == WM_ACTION_EXEC);
	assert(leaf->argument && strcmp(leaf->argument, "deep") == 0);

	keybind_destroy_all(&keymodes);
	printf("  PASS: test_deep_chain\n");
}

/* Test: keybind_destroy_all on empty keymodes list */
static void
test_destroy_empty(void)
{
	struct wl_list keymodes;
	wl_list_init(&keymodes);
	keybind_destroy_all(&keymodes);
	assert(wl_list_empty(&keymodes));
	printf("  PASS: test_destroy_empty\n");
}

/* Test: ToggleCmd parsing */
static void
test_togglecmd(void)
{
	write_file(TEST_KEYS,
		"Mod4 t :ToggleCmd {Shade} {ShadeOff}\n"
	);

	struct wl_list keymodes;
	wl_list_init(&keymodes);
	keybind_load(&keymodes, TEST_KEYS);

	struct wm_keymode *dm = NULL;
	struct wm_keymode *mode;
	wl_list_for_each(mode, &keymodes, link) {
		if (strcmp(mode->name, "default") == 0) {
			dm = mode;
			break;
		}
	}

	struct wm_keybind *bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO, XKB_KEY_t);
	assert(bind != NULL);
	assert(bind->action == WM_ACTION_TOGGLE_CMD);
	assert(bind->subcmd_count == 2);
	assert(bind->subcmds != NULL);
	assert(bind->subcmds->action == WM_ACTION_SHADE);
	assert(bind->subcmds->next != NULL);
	assert(bind->subcmds->next->action == WM_ACTION_SHADE_OFF);

	keybind_destroy_all(&keymodes);
	printf("  PASS: test_togglecmd\n");
}

/* Test: keybind_add_from_string */
static void
test_add_from_string(void)
{
	struct wl_list bindings;
	wl_list_init(&bindings);

	bool ret = keybind_add_from_string(&bindings,
		"Mod4 Return :Exec foot");
	assert(ret == true);

	struct wm_keybind *bind = keybind_find(&bindings,
		WLR_MODIFIER_LOGO, XKB_KEY_Return);
	assert(bind != NULL);
	assert(bind->action == WM_ACTION_EXEC);
	assert(bind->argument && strcmp(bind->argument, "foot") == 0);

	/* Invalid line (no colon) should fail */
	ret = keybind_add_from_string(&bindings, "Mod4 x NoAction");
	assert(ret == false);

	keybind_destroy_list(&bindings);
	printf("  PASS: test_add_from_string\n");
}

/* Test: Ctrl alias for Control modifier */
static void
test_ctrl_alias(void)
{
	write_file(TEST_KEYS,
		"Ctrl t :Exec foot\n"
	);

	struct wl_list keymodes;
	wl_list_init(&keymodes);
	keybind_load(&keymodes, TEST_KEYS);

	struct wm_keymode *dm = NULL;
	struct wm_keymode *mode;
	wl_list_for_each(mode, &keymodes, link) {
		if (strcmp(mode->name, "default") == 0) {
			dm = mode;
			break;
		}
	}

	struct wm_keybind *bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_CTRL, XKB_KEY_t);
	assert(bind != NULL);
	assert(bind->action == WM_ACTION_EXEC);

	keybind_destroy_all(&keymodes);
	printf("  PASS: test_ctrl_alias\n");
}

/* Test: MacroCmd with arguments in sub-commands */
static void
test_macrocmd_with_args(void)
{
	write_file(TEST_KEYS,
		"Mod4 m :MacroCmd {Exec foot} {SendToWorkspace 3} {Maximize}\n"
	);

	struct wl_list keymodes;
	wl_list_init(&keymodes);
	keybind_load(&keymodes, TEST_KEYS);

	struct wm_keymode *dm = NULL;
	struct wm_keymode *mode;
	wl_list_for_each(mode, &keymodes, link) {
		if (strcmp(mode->name, "default") == 0) {
			dm = mode;
			break;
		}
	}

	struct wm_keybind *bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO, XKB_KEY_m);
	assert(bind != NULL);
	assert(bind->action == WM_ACTION_MACRO_CMD);
	assert(bind->subcmd_count == 3);

	/* First: Exec foot */
	assert(bind->subcmds->action == WM_ACTION_EXEC);
	assert(bind->subcmds->argument != NULL);
	assert(strcmp(bind->subcmds->argument, "foot") == 0);

	/* Second: SendToWorkspace 3 */
	struct wm_subcmd *s2 = bind->subcmds->next;
	assert(s2 != NULL);
	assert(s2->action == WM_ACTION_SEND_TO_WORKSPACE);
	assert(s2->argument && strcmp(s2->argument, "3") == 0);

	/* Third: Maximize */
	struct wm_subcmd *s3 = s2->next;
	assert(s3 != NULL);
	assert(s3->action == WM_ACTION_MAXIMIZE);
	assert(s3->argument == NULL);

	keybind_destroy_all(&keymodes);
	printf("  PASS: test_macrocmd_with_args\n");
}

/* Test: invalid/unrecognized action is silently skipped */
static void
test_invalid_action(void)
{
	write_file(TEST_KEYS,
		"Mod4 a :BogusAction\n"
		"Mod4 b :Close\n"
	);

	struct wl_list keymodes;
	wl_list_init(&keymodes);
	keybind_load(&keymodes, TEST_KEYS);

	struct wm_keymode *dm = NULL;
	struct wm_keymode *mode;
	wl_list_for_each(mode, &keymodes, link) {
		if (strcmp(mode->name, "default") == 0) {
			dm = mode;
			break;
		}
	}

	/* BogusAction should not create a binding */
	struct wm_keybind *bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO, XKB_KEY_a);
	assert(bind == NULL);

	/* Close should still work */
	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO, XKB_KEY_b);
	assert(bind != NULL);
	assert(bind->action == WM_ACTION_CLOSE);

	keybind_destroy_all(&keymodes);
	printf("  PASS: test_invalid_action\n");
}

/* Test: invalid key name is silently skipped */
static void
test_invalid_key_name(void)
{
	write_file(TEST_KEYS,
		"Mod4 NotARealKey123 :Close\n"
		"Mod4 a :Maximize\n"
	);

	struct wl_list keymodes;
	wl_list_init(&keymodes);
	keybind_load(&keymodes, TEST_KEYS);

	struct wm_keymode *dm = NULL;
	struct wm_keymode *mode;
	wl_list_for_each(mode, &keymodes, link) {
		if (strcmp(mode->name, "default") == 0) {
			dm = mode;
			break;
		}
	}

	/* Only the valid binding should exist */
	struct wm_keybind *bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO, XKB_KEY_a);
	assert(bind != NULL);
	assert(bind->action == WM_ACTION_MAXIMIZE);

	int count = 0;
	struct wm_keybind *b;
	wl_list_for_each(b, &dm->bindings, link)
		count++;
	assert(count == 1);

	keybind_destroy_all(&keymodes);
	printf("  PASS: test_invalid_key_name\n");
}

/* Test: duplicate keybindings both stored */
static void
test_duplicate_bindings(void)
{
	write_file(TEST_KEYS,
		"Mod4 a :Close\n"
		"Mod4 a :Maximize\n"
	);

	struct wl_list keymodes;
	wl_list_init(&keymodes);
	keybind_load(&keymodes, TEST_KEYS);

	struct wm_keymode *dm = NULL;
	struct wm_keymode *mode;
	wl_list_for_each(mode, &keymodes, link) {
		if (strcmp(mode->name, "default") == 0) {
			dm = mode;
			break;
		}
	}

	/* Both bindings exist — count should be 2 */
	int count = 0;
	struct wm_keybind *b;
	wl_list_for_each(b, &dm->bindings, link)
		count++;
	assert(count == 2);

	/* keybind_find returns the first match */
	struct wm_keybind *bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO, XKB_KEY_a);
	assert(bind != NULL);
	assert(bind->action == WM_ACTION_CLOSE ||
		bind->action == WM_ACTION_MAXIMIZE);

	keybind_destroy_all(&keymodes);
	printf("  PASS: test_duplicate_bindings\n");
}

/* Test: keybind_get_mode returns existing mode */
static void
test_get_mode_existing(void)
{
	struct wl_list keymodes;
	wl_list_init(&keymodes);

	struct wm_keymode *mode1 = keybind_get_mode(&keymodes, "test");
	assert(mode1 != NULL);
	assert(strcmp(mode1->name, "test") == 0);

	/* Getting same mode again returns the same pointer */
	struct wm_keymode *mode2 = keybind_get_mode(&keymodes, "test");
	assert(mode2 == mode1);

	keybind_destroy_all(&keymodes);
	printf("  PASS: test_get_mode_existing\n");
}

/* Test: multiple keymodes */
static void
test_multiple_keymodes(void)
{
	write_file(TEST_KEYS,
		"Mod4 r :KeyMode resize\n"
		"Mod4 m :KeyMode move\n"
		"resize: h :MoveLeft 10\n"
		"resize: Escape :KeyMode default\n"
		"move: h :FocusLeft\n"
		"move: Escape :KeyMode default\n"
	);

	struct wl_list keymodes;
	wl_list_init(&keymodes);
	keybind_load(&keymodes, TEST_KEYS);

	int mode_count = 0;
	struct wm_keymode *mode;
	struct wm_keymode *resize = NULL;
	struct wm_keymode *move_mode = NULL;
	wl_list_for_each(mode, &keymodes, link) {
		if (strcmp(mode->name, "resize") == 0)
			resize = mode;
		else if (strcmp(mode->name, "move") == 0)
			move_mode = mode;
		mode_count++;
	}

	assert(mode_count == 3); /* default + resize + move */
	assert(resize != NULL);
	assert(move_mode != NULL);

	/* Resize mode: h -> MoveLeft */
	struct wm_keybind *bind = keybind_find(&resize->bindings,
		0, XKB_KEY_h);
	assert(bind != NULL);
	assert(bind->action == WM_ACTION_MOVE_LEFT);

	/* Move mode: h -> FocusLeft */
	bind = keybind_find(&move_mode->bindings, 0, XKB_KEY_h);
	assert(bind != NULL);
	assert(bind->action == WM_ACTION_FOCUS_LEFT);

	keybind_destroy_all(&keymodes);
	printf("  PASS: test_multiple_keymodes\n");
}

/* Test: If conditional command parsing */
static void
test_if_condition(void)
{
	write_file(TEST_KEYS,
		"Mod4 i :If {Matches (class=Firefox)} {Maximize} {Minimize}\n"
	);

	struct wl_list keymodes;
	wl_list_init(&keymodes);
	keybind_load(&keymodes, TEST_KEYS);

	struct wm_keymode *dm = NULL;
	struct wm_keymode *mode;
	wl_list_for_each(mode, &keymodes, link) {
		if (strcmp(mode->name, "default") == 0) {
			dm = mode;
			break;
		}
	}

	struct wm_keybind *bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO, XKB_KEY_i);
	assert(bind != NULL);
	assert(bind->action == WM_ACTION_IF);
	assert(bind->condition != NULL);
	assert(bind->condition->type == WM_COND_MATCHES);
	assert(bind->condition->property != NULL);
	assert(strcmp(bind->condition->property, "class") == 0);
	assert(bind->condition->pattern != NULL);
	assert(strcmp(bind->condition->pattern, "Firefox") == 0);

	/* Then branch */
	assert(bind->subcmds != NULL);
	assert(bind->subcmds->action == WM_ACTION_MAXIMIZE);

	/* Else branch */
	assert(bind->else_cmd != NULL);
	assert(bind->else_cmd->action == WM_ACTION_MINIMIZE);

	keybind_destroy_all(&keymodes);
	printf("  PASS: test_if_condition\n");
}

/* Test: ForEach conditional command */
static void
test_foreach_condition(void)
{
	write_file(TEST_KEYS,
		"Mod4 f :ForEach {Matches (class=.*)} {Minimize}\n"
	);

	struct wl_list keymodes;
	wl_list_init(&keymodes);
	keybind_load(&keymodes, TEST_KEYS);

	struct wm_keymode *dm = NULL;
	struct wm_keymode *mode;
	wl_list_for_each(mode, &keymodes, link) {
		if (strcmp(mode->name, "default") == 0) {
			dm = mode;
			break;
		}
	}

	struct wm_keybind *bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO, XKB_KEY_f);
	assert(bind != NULL);
	assert(bind->action == WM_ACTION_FOREACH);
	assert(bind->condition != NULL);
	assert(bind->condition->type == WM_COND_MATCHES);
	assert(bind->subcmds != NULL);
	assert(bind->subcmds->action == WM_ACTION_MINIMIZE);

	keybind_destroy_all(&keymodes);
	printf("  PASS: test_foreach_condition\n");
}

/* Test: Not and And condition operators */
static void
test_condition_operators(void)
{
	/* Not condition */
	write_file(TEST_KEYS,
		"Mod4 a :If {Not {Matches (class=Firefox)}} {Close}\n"
	);

	struct wl_list keymodes;
	wl_list_init(&keymodes);
	keybind_load(&keymodes, TEST_KEYS);

	struct wm_keymode *dm = NULL;
	struct wm_keymode *mode;
	wl_list_for_each(mode, &keymodes, link) {
		if (strcmp(mode->name, "default") == 0) {
			dm = mode;
			break;
		}
	}

	struct wm_keybind *bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO, XKB_KEY_a);
	assert(bind != NULL);
	assert(bind->condition != NULL);
	assert(bind->condition->type == WM_COND_NOT);
	assert(bind->condition->child != NULL);
	assert(bind->condition->child->type == WM_COND_MATCHES);

	keybind_destroy_all(&keymodes);

	/* And condition */
	write_file(TEST_KEYS,
		"Mod4 b :If {And {Matches (class=Firefox)} {Matches (title=Home)}} {Maximize}\n"
	);

	wl_list_init(&keymodes);
	keybind_load(&keymodes, TEST_KEYS);

	dm = NULL;
	wl_list_for_each(mode, &keymodes, link) {
		if (strcmp(mode->name, "default") == 0) {
			dm = mode;
			break;
		}
	}

	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO, XKB_KEY_b);
	assert(bind != NULL);
	assert(bind->condition != NULL);
	assert(bind->condition->type == WM_COND_AND);
	assert(bind->condition->left != NULL);
	assert(bind->condition->left->type == WM_COND_MATCHES);
	assert(bind->condition->right != NULL);
	assert(bind->condition->right->type == WM_COND_MATCHES);

	keybind_destroy_all(&keymodes);
	printf("  PASS: test_condition_operators\n");
}

/* Test: Or and Xor condition types */
static void
test_or_xor_conditions(void)
{
	write_file(TEST_KEYS,
		"Mod4 o :If {Or {Matches (class=Firefox)} {Matches (class=Chrome)}} {Raise}\n"
		"Mod4 p :If {Xor {Matches (class=A)} {Matches (class=B)}} {Lower}\n"
	);

	struct wl_list keymodes;
	wl_list_init(&keymodes);
	keybind_load(&keymodes, TEST_KEYS);

	struct wm_keymode *dm = NULL;
	struct wm_keymode *mode;
	wl_list_for_each(mode, &keymodes, link) {
		if (strcmp(mode->name, "default") == 0) {
			dm = mode;
			break;
		}
	}

	/* Or */
	struct wm_keybind *bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO, XKB_KEY_o);
	assert(bind != NULL);
	assert(bind->condition != NULL);
	assert(bind->condition->type == WM_COND_OR);
	assert(bind->condition->left != NULL);
	assert(bind->condition->right != NULL);

	/* Xor */
	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO, XKB_KEY_p);
	assert(bind != NULL);
	assert(bind->condition != NULL);
	assert(bind->condition->type == WM_COND_XOR);

	keybind_destroy_all(&keymodes);
	printf("  PASS: test_or_xor_conditions\n");
}

/* Test: Some and Every condition types */
static void
test_some_every_conditions(void)
{
	write_file(TEST_KEYS,
		"Mod4 s :If {Some (class=Firefox)} {Maximize}\n"
		"Mod4 e :If {Every (class=.*term.*)} {Minimize}\n"
	);

	struct wl_list keymodes;
	wl_list_init(&keymodes);
	keybind_load(&keymodes, TEST_KEYS);

	struct wm_keymode *dm = NULL;
	struct wm_keymode *mode;
	wl_list_for_each(mode, &keymodes, link) {
		if (strcmp(mode->name, "default") == 0) {
			dm = mode;
			break;
		}
	}

	struct wm_keybind *bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO, XKB_KEY_s);
	assert(bind != NULL);
	assert(bind->condition != NULL);
	assert(bind->condition->type == WM_COND_SOME);

	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO, XKB_KEY_e);
	assert(bind != NULL);
	assert(bind->condition != NULL);
	assert(bind->condition->type == WM_COND_EVERY);

	keybind_destroy_all(&keymodes);
	printf("  PASS: test_some_every_conditions\n");
}

/* Test: Map command parsing */
static void
test_map_command(void)
{
	write_file(TEST_KEYS,
		"Mod4 m :Map {Maximize}\n"
	);

	struct wl_list keymodes;
	wl_list_init(&keymodes);
	keybind_load(&keymodes, TEST_KEYS);

	struct wm_keymode *dm = NULL;
	struct wm_keymode *mode;
	wl_list_for_each(mode, &keymodes, link) {
		if (strcmp(mode->name, "default") == 0) {
			dm = mode;
			break;
		}
	}

	struct wm_keybind *bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO, XKB_KEY_m);
	assert(bind != NULL);
	assert(bind->action == WM_ACTION_MAP);
	assert(bind->subcmds != NULL);
	assert(bind->subcmds->action == WM_ACTION_MAXIMIZE);

	keybind_destroy_all(&keymodes);
	printf("  PASS: test_map_command\n");
}

/* Test: Delay command with explicit value */
static void
test_delay_command(void)
{
	write_file(TEST_KEYS,
		"Mod4 d :Delay {Maximize} 500000\n"
	);

	struct wl_list keymodes;
	wl_list_init(&keymodes);
	keybind_load(&keymodes, TEST_KEYS);

	struct wm_keymode *dm = NULL;
	struct wm_keymode *mode;
	wl_list_for_each(mode, &keymodes, link) {
		if (strcmp(mode->name, "default") == 0) {
			dm = mode;
			break;
		}
	}

	struct wm_keybind *bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO, XKB_KEY_d);
	assert(bind != NULL);
	assert(bind->action == WM_ACTION_DELAY);
	assert(bind->subcmds != NULL);
	assert(bind->subcmds->action == WM_ACTION_MAXIMIZE);
	assert(bind->delay_us == 500000);

	keybind_destroy_all(&keymodes);
	printf("  PASS: test_delay_command\n");
}

/* Test: Delay command with default value */
static void
test_delay_default(void)
{
	write_file(TEST_KEYS,
		"Mod4 d :Delay {Maximize}\n"
	);

	struct wl_list keymodes;
	wl_list_init(&keymodes);
	keybind_load(&keymodes, TEST_KEYS);

	struct wm_keymode *dm = NULL;
	struct wm_keymode *mode;
	wl_list_for_each(mode, &keymodes, link) {
		if (strcmp(mode->name, "default") == 0) {
			dm = mode;
			break;
		}
	}

	struct wm_keybind *bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO, XKB_KEY_d);
	assert(bind != NULL);
	assert(bind->action == WM_ACTION_DELAY);
	assert(bind->delay_us == 1000000); /* default 1 second */

	keybind_destroy_all(&keymodes);
	printf("  PASS: test_delay_default\n");
}

/* Test: wm_condition_destroy handles NULL */
static void
test_condition_destroy_null(void)
{
	wm_condition_destroy(NULL);
	printf("  PASS: test_condition_destroy_null\n");
}

/* Test: keybind_destroy_list on populated list */
static void
test_destroy_list(void)
{
	struct wl_list bindings;
	wl_list_init(&bindings);

	keybind_add_from_string(&bindings, "Mod4 a :Close");
	keybind_add_from_string(&bindings, "Mod4 b :Maximize");
	assert(!wl_list_empty(&bindings));

	keybind_destroy_list(&bindings);
	assert(wl_list_empty(&bindings));
	printf("  PASS: test_destroy_list\n");
}

int
main(void)
{
	printf("test_keybind:\n");
	setup();

	test_load_basic();
	test_modifier_combos();
	test_actions();
	test_chain_bindings();
	test_keymodes();
	test_comments_blanks();
	test_exec_argument();
	test_macrocmd();
	test_load_nonexistent();
	test_action_aliases();
	test_workspace_actions();
	test_window_management_actions();
	test_menu_style_actions();
	test_phase5a_actions();
	test_slit_toggle_actions();
	test_toolbar_toggle_actions();
	test_find_no_match();
	test_empty_keys_file();
	test_on_context_skipped();
	test_deep_chain();
	test_destroy_empty();
	test_togglecmd();
	test_add_from_string();
	test_ctrl_alias();
	test_macrocmd_with_args();
	test_invalid_action();
	test_invalid_key_name();
	test_duplicate_bindings();
	test_get_mode_existing();
	test_multiple_keymodes();
	test_if_condition();
	test_foreach_condition();
	test_condition_operators();
	test_or_xor_conditions();
	test_some_every_conditions();
	test_map_command();
	test_delay_command();
	test_delay_default();
	test_condition_destroy_null();
	test_destroy_list();

	cleanup();
	printf("All keybind tests passed.\n");
	return 0;
}
