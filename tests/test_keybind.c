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

#define TEST_DIR "/tmp/claude-1001/wm-test-keybind"
#define TEST_KEYS TEST_DIR "/keys"

static void
setup(void)
{
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
	assert(bind && bind->action == WM_ACTION_FOCUS_NEXT);

	bind = keybind_find(&dm->bindings,
		WLR_MODIFIER_LOGO | WLR_MODIFIER_SHIFT, XKB_KEY_Tab);
	assert(bind && bind->action == WM_ACTION_FOCUS_PREV);

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

	int ret = keybind_load(&keymodes, "/tmp/claude-1001/nonexistent_keys_file");
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

	cleanup();
	printf("All keybind tests passed.\n");
	return 0;
}
