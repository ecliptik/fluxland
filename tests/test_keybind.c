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

	cleanup();
	printf("All keybind tests passed.\n");
	return 0;
}
