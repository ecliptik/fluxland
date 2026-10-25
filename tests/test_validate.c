/*
 * test_validate.c - Unit tests for configuration validation
 */

#define _POSIX_C_SOURCE 200809L

#include "validate.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST_DIR "/tmp/fluxland-test/wm-test-validate"
#define TEST_INIT TEST_DIR "/init"
#define TEST_KEYS TEST_DIR "/keys"
#define TEST_STYLE TEST_DIR "/style"
#define TEST_APPS TEST_DIR "/apps"

/* client_pattern.c's matching is linked but not used here */
struct wm_view;
struct wm_server;
struct wlr_box;
struct wm_view *wm_view_parent(struct wm_view *v) { (void)v; return NULL; }
void wm_view_get_geometry(struct wm_view *v, struct wlr_box *b)
	{ (void)v; (void)b; }
struct wm_workspace *wm_workspace_get_active(struct wm_server *s)
	{ (void)s; return NULL; }
/* menu.c's parts menu_parse.c uses */
#include "menu.h"
#include "menu_parse.h"
void wm_menu_destroy(struct wm_menu *menu)
{
	if (!menu)
		return;
	struct wm_menu_item *item, *tmp;
	wl_list_for_each_safe(item, tmp, &menu->items, link) {
		wl_list_remove(&item->link);
		menu_item_destroy(item);
	}
	free(menu->title);
	free(menu);
}
struct wm_menu *wm_menu_create_config_menu(struct wm_server *s)
	{ (void)s; return NULL; }

/* command.c runs commands through the dispatcher: not here */
#include "keybind.h"
bool wm_execute_action_on(struct wm_server *s, enum wm_action a,
	const char *arg, struct wm_view *v)
	{ (void)s; (void)a; (void)arg; (void)v; return true; }

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
	unlink(TEST_KEYS);
	unlink(TEST_STYLE);
	unlink(TEST_APPS);
	rmdir(TEST_DIR);
}

/* Clean up individual test files between tests */
static void
clean_files(void)
{
	unlink(TEST_INIT);
	unlink(TEST_KEYS);
	unlink(TEST_STYLE);
	unlink(TEST_APPS);
}

static void
write_file(const char *path, const char *contents)
{
	FILE *f = fopen(path, "w");
	assert(f);
	fputs(contents, f);
	fclose(f);
}

/* ---- Result helper tests ---- */

static void
test_result_init_destroy(void)
{
	struct config_result result;
	config_result_init(&result);
	assert(result.count == 0);
	assert(result.errors == NULL);

	config_result_add_error(&result, "test.conf", 1, "test error %d", 42);
	assert(result.count == 1);
	assert(result.errors[0].fatal == true);
	assert(result.errors[0].line == 1);
	assert(strcmp(result.errors[0].message, "test error 42") == 0);

	config_result_add_warning(&result, "test.conf", 2, "test warning");
	assert(result.count == 2);
	assert(result.errors[1].fatal == false);

	assert(config_result_has_errors(&result) == true);

	config_result_destroy(&result);
	assert(result.count == 0);

	printf("  PASS: test_result_init_destroy\n");
}

static void
test_result_no_errors(void)
{
	struct config_result result;
	config_result_init(&result);

	config_result_add_warning(&result, "test.conf", 1, "just a warning");
	assert(config_result_has_errors(&result) == false);

	config_result_destroy(&result);
	printf("  PASS: test_result_no_errors\n");
}

/* ---- Init file validation tests ---- */

static void
test_valid_init_file(void)
{
	write_file(TEST_INIT,
		"session.screen0.workspaces: 4\n"
		"session.screen0.focusModel: MouseFocus\n"
		"session.screen0.toolbar.alpha: 200\n"
		"session.screen0.toolbar.placement: BottomCenter\n"
		"session.screen0.toolbar.widthPercent: 80\n"
		"session.screen0.slit.placement: RightCenter\n"
		"session.screen0.slit.direction: Vertical\n"
		"session.screen0.iconbar.mode: AllWindows\n"
		"session.menuSearch: itemstart\n"
	);

	struct config_result result;
	config_result_init(&result);
	validate_init_file(TEST_INIT, &result);

	assert(config_result_has_errors(&result) == false);
	/* Might have warnings (e.g., if paths don't exist), but no errors */

	config_result_destroy(&result);
	printf("  PASS: test_valid_init_file\n");
}

static void
test_input_settings(void)
{
	write_file(TEST_INIT,
		"session.input.naturalScroll: true\n"
		"session.input.touchpad.tap: yes\n"
		"session.input.pointer.accelSpeed: -0.3\n"
		"session.input.device.SynPS_2_Synaptics_TouchPad.clickMethod: "
		"clickFinger\n"
	);
	struct config_result result;
	config_result_init(&result);
	validate_init_file(TEST_INIT, &result);
	assert(result.count == 0);
	config_result_destroy(&result);

	write_file(TEST_INIT,
		"session.input.tap: maybe\n"           /* bad bool */
		"session.input.accelSpeed: 2\n"        /* out of range */
		"session.input.scrollMethod: diagonal\n" /* bad enum */
		"session.input.tapp: true\n"           /* unknown setting */
		"session.input.device..tap: true\n"    /* no device name */
	);
	config_result_init(&result);
	validate_init_file(TEST_INIT, &result);
	assert(result.count == 5);
	assert(config_result_has_errors(&result));
	config_result_destroy(&result);
	printf("  PASS: test_input_settings\n");
}

static void
test_output_settings(void)
{
	write_file(TEST_INIT,
		"session.output.eDP-1.scale: 1.5\n"
		"session.output.eDP-1.mode: 1920x1080@60\n"
		"session.output.HDMI-A-1.position: -1920,0\n"
		"session.output.HDMI-A-1.transform: flipped-90\n"
		"session.output.DP-1.enabled: false\n"
		"session.output.DP-1.adaptiveSync: true\n"
	);
	struct config_result result;
	config_result_init(&result);
	validate_init_file(TEST_INIT, &result);
	assert(result.count == 0);
	config_result_destroy(&result);

	write_file(TEST_INIT,
		"session.output.eDP-1.scale: 20\n"       /* out of range */
		"session.output.eDP-1.mode: big\n"       /* bad mode */
		"session.output.eDP-1.position: 10\n"    /* no y */
		"session.output.eDP-1.transform: 45\n"   /* bad transform */
		"session.output.eDP-1.enabled: maybe\n"  /* bad bool */
		"session.output.eDP-1.colour: red\n"     /* unknown: warning */
	);
	config_result_init(&result);
	validate_init_file(TEST_INIT, &result);
	assert(result.count == 6);
	config_result_destroy(&result);
	printf("  PASS: test_output_settings\n");
}

static void
test_invalid_workspace_count(void)
{
	write_file(TEST_INIT,
		"session.screen0.workspaces: 100\n"
	);

	struct config_result result;
	config_result_init(&result);
	validate_init_file(TEST_INIT, &result);

	/* Should have a warning about out-of-range value */
	assert(result.count > 0);
	bool found_range = false;
	for (int i = 0; i < result.count; i++) {
		if (result.errors[i].message &&
		    strstr(result.errors[i].message, "out of range"))
			found_range = true;
	}
	assert(found_range);

	config_result_destroy(&result);
	printf("  PASS: test_invalid_workspace_count\n");
}

static void
test_invalid_alpha(void)
{
	write_file(TEST_INIT,
		"session.screen0.toolbar.alpha: 300\n"
	);

	struct config_result result;
	config_result_init(&result);
	validate_init_file(TEST_INIT, &result);

	assert(result.count > 0);
	bool found_range = false;
	for (int i = 0; i < result.count; i++) {
		if (result.errors[i].message &&
		    strstr(result.errors[i].message, "out of range"))
			found_range = true;
	}
	assert(found_range);

	config_result_destroy(&result);
	printf("  PASS: test_invalid_alpha\n");
}

static void
test_invalid_focus_model(void)
{
	write_file(TEST_INIT,
		"session.screen0.focusModel: InvalidFocus\n"
	);

	struct config_result result;
	config_result_init(&result);
	validate_init_file(TEST_INIT, &result);

	assert(config_result_has_errors(&result) == true);
	bool found_invalid = false;
	for (int i = 0; i < result.count; i++) {
		if (result.errors[i].fatal &&
		    result.errors[i].message &&
		    strstr(result.errors[i].message, "invalid value"))
			found_invalid = true;
	}
	assert(found_invalid);

	config_result_destroy(&result);
	printf("  PASS: test_invalid_focus_model\n");
}

static void
test_invalid_placement(void)
{
	write_file(TEST_INIT,
		"session.screen0.windowPlacement: BadPlacement\n"
	);

	struct config_result result;
	config_result_init(&result);
	validate_init_file(TEST_INIT, &result);

	assert(config_result_has_errors(&result) == true);

	config_result_destroy(&result);
	printf("  PASS: test_invalid_placement\n");
}

static void
test_invalid_bool(void)
{
	write_file(TEST_INIT,
		"session.screen0.toolbar.visible: maybe\n"
	);

	struct config_result result;
	config_result_init(&result);
	validate_init_file(TEST_INIT, &result);

	assert(config_result_has_errors(&result) == true);
	bool found_bool = false;
	for (int i = 0; i < result.count; i++) {
		if (result.errors[i].fatal &&
		    result.errors[i].message &&
		    strstr(result.errors[i].message, "boolean"))
			found_bool = true;
	}
	assert(found_bool);

	config_result_destroy(&result);
	printf("  PASS: test_invalid_bool\n");
}

static void
test_invalid_int(void)
{
	write_file(TEST_INIT,
		"session.screen0.workspaces: abc\n"
	);

	struct config_result result;
	config_result_init(&result);
	validate_init_file(TEST_INIT, &result);

	assert(config_result_has_errors(&result) == true);
	bool found_int = false;
	for (int i = 0; i < result.count; i++) {
		if (result.errors[i].fatal &&
		    result.errors[i].message &&
		    strstr(result.errors[i].message, "integer"))
			found_int = true;
	}
	assert(found_int);

	config_result_destroy(&result);
	printf("  PASS: test_invalid_int\n");
}

static void
test_unknown_key(void)
{
	write_file(TEST_INIT,
		"session.screen0.nonExistentOption: foo\n"
	);

	struct config_result result;
	config_result_init(&result);
	validate_init_file(TEST_INIT, &result);

	/* Should have a warning about unknown key */
	bool found_unknown = false;
	for (int i = 0; i < result.count; i++) {
		if (!result.errors[i].fatal &&
		    result.errors[i].message &&
		    strstr(result.errors[i].message, "unknown key"))
			found_unknown = true;
	}
	assert(found_unknown);

	config_result_destroy(&result);
	printf("  PASS: test_unknown_key\n");
}

static void
test_missing_file_path(void)
{
	write_file(TEST_INIT,
		"session.keyFile: /nonexistent/path/keys\n"
	);

	struct config_result result;
	config_result_init(&result);
	validate_init_file(TEST_INIT, &result);

	/* Should have a warning about non-existent path */
	bool found_path = false;
	for (int i = 0; i < result.count; i++) {
		if (result.errors[i].message &&
		    strstr(result.errors[i].message, "non-existent"))
			found_path = true;
	}
	assert(found_path);

	config_result_destroy(&result);
	printf("  PASS: test_missing_file_path\n");
}

/* ---- Keys file validation tests ---- */

static void
test_valid_keys_file(void)
{
	write_file(TEST_KEYS,
		"# Comment line\n"
		"Mod4 Return :Exec foot\n"
		"Mod4 q :Close\n"
		"Mod4 Shift space :Fullscreen\n"
		"OnTitlebar Mouse1 :MacroCmd {Raise} {Focus}\n"
		"OnDesktop Mouse3 :RootMenu\n"
	);

	struct config_result result;
	config_result_init(&result);
	validate_keys_file(TEST_KEYS, &result);

	assert(config_result_has_errors(&result) == false);

	config_result_destroy(&result);
	printf("  PASS: test_valid_keys_file\n");
}

static void
test_unknown_action(void)
{
	write_file(TEST_KEYS,
		"Mod4 x :NonExistentAction\n"
	);

	struct config_result result;
	config_result_init(&result);
	validate_keys_file(TEST_KEYS, &result);

	assert(config_result_has_errors(&result) == true);
	bool found_action = false;
	for (int i = 0; i < result.count; i++) {
		if (result.errors[i].fatal &&
		    result.errors[i].message &&
		    strstr(result.errors[i].message, "unknown or malformed command"))
			found_action = true;
	}
	assert(found_action);

	config_result_destroy(&result);
	printf("  PASS: test_unknown_action\n");
}

static void
test_missing_action(void)
{
	write_file(TEST_KEYS,
		"Mod4 x :\n"
	);

	struct config_result result;
	config_result_init(&result);
	validate_keys_file(TEST_KEYS, &result);

	assert(config_result_has_errors(&result) == true);

	config_result_destroy(&result);
	printf("  PASS: test_missing_action\n");
}

static void
test_invalid_button_number(void)
{
	/* Fluxbox numbers buttons from 1: Mouse9 is fine, Mouse0 isn't */
	write_file(TEST_KEYS,
		"OnDesktop Mouse9 :RootMenu\n"
		"OnDesktop Mouse0 :RootMenu\n"
	);

	struct config_result result;
	config_result_init(&result);
	validate_keys_file(TEST_KEYS, &result);

	assert(result.count == 1 && result.errors[0].fatal &&
		result.errors[0].line == 2 &&
		strstr(result.errors[0].message, "mouse binding"));

	config_result_destroy(&result);
	printf("  PASS: test_invalid_button_number\n");
}

/* Fluxbox's own keys file (data/keys of the Fluxbox sources) checks clean */
static void
test_fluxbox_keys_file(void)
{
	write_file(TEST_KEYS,
		"OnDesktop Mouse1 :HideMenus\n"
		"OnDesktop Mouse2 :WorkspaceMenu\n"
		"OnDesktop Mouse3 :RootMenu\n"
		"OnDesktop Mouse4 :PrevWorkspace\n"
		"OnDesktop Mouse5 :NextWorkspace\n"
		"OnToolbar Mouse4 :PrevWorkspace\n"
		"OnWindow Mod1 Mouse1 :MacroCmd {Raise} {Focus} {StartMoving}\n"
		"OnWindowBorder Move1 :StartMoving\n"
		"OnLeftGrip Move1 :StartResizing bottomleft\n"
		"OnRightGrip Move1 :StartResizing bottomright\n"
		"OnWindow Mod1 Mouse3 :MacroCmd {Raise} {Focus} "
			"{StartResizing NearestCorner}\n"
		"OnWindow Mod1 Mouse2 :Lower\n"
		"OnTitlebar Control Mouse1 :StartTabbing\n"
		"OnTitlebar Double Mouse1 :Shade\n"
		"OnTitlebar Mouse1 :MacroCmd {Raise} {Focus} {ActivateTab}\n"
		"OnTitlebar Move1  :StartMoving\n"
		"OnTitlebar Mouse2 :Lower\n"
		"OnTitlebar Mouse3 :WindowMenu\n"
		"Mod1 Tab :NextWindow {groups} (workspace=[current])\n"
		"Mod1 Shift Tab :PrevWindow {groups} (workspace=[current])\n"
		"Mod4 Tab :NextTab\n"
		"Mod4 Shift Tab :PrevTab\n"
		"Mod4 1 :Tab 1\n"
		"Mod4 9 :Tab 9\n"
		"Mod1 F1 :Exec xterm\n"
		"Mod1 F2 :Exec fbrun\n"
		"Mod1 F4 :Close\n"
		"Mod1 F5 :Kill\n"
		"Mod1 F9 :Minimize\n"
		"Mod1 F10 :Maximize\n"
		"Mod1 F11 :Fullscreen\n"
		"Mod1 space :WindowMenu\n"
		"Control Mod1 Delete :Exit\n"
		"Control Mod1 BackSpace :Exit\n"
		"Control Mod1 Left :PrevWorkspace\n"
		"Control Mod1 Right :NextWorkspace\n"
		"Control Mod1 Shift Left :SendToPrevWorkspace\n"
		"Control F1 :Workspace 1\n"
		"Shift F1 :SendToWorkspace 1\n"
		"Control Shift F1 :TakeToWorkspace 1\n"
		"176 :Exec amixer sset Master,0 1+\n"
		"174 :Exec amixer sset Master,0 1-\n"
		"160 :Exec amixer sset Master,0 toggle\n"
		"Control Mod1 r :Reconfigure\n"
		"Mod4 r :Restart\n"
		"Mod5 a :MinimizeWindow\n"
	);

	struct config_result result;
	config_result_init(&result);
	validate_keys_file(TEST_KEYS, &result);
	for (int i = 0; i < result.count; i++)
		fprintf(stderr, "line %d: %s\n", result.errors[i].line,
			result.errors[i].message);
	assert(result.count == 0);

	config_result_destroy(&result);
	printf("  PASS: test_fluxbox_keys_file\n");
}

/* ---- Style file validation tests ---- */

static void
test_valid_style_file(void)
{
	write_file(TEST_STYLE,
		"window.title.focus: Raised Gradient Vertical\n"
		"window.title.focus.color: #333333\n"
		"window.title.focus.colorTo: #222222\n"
		"window.label.focus.textColor: #E0E0E0\n"
		"window.label.focus.font: sans-10:bold\n"
		"window.borderWidth: 1\n"
		"menu.title: Raised Gradient Vertical\n"
		"menu.title.textColor: white\n"
	);

	struct config_result result;
	config_result_init(&result);
	validate_style_file(TEST_STYLE, &result);

	assert(config_result_has_errors(&result) == false);

	config_result_destroy(&result);
	printf("  PASS: test_valid_style_file\n");
}

static void
test_invalid_color(void)
{
	write_file(TEST_STYLE,
		"window.label.focus.textColor: notacolor\n"
	);

	struct config_result result;
	config_result_init(&result);
	validate_style_file(TEST_STYLE, &result);

	assert(config_result_has_errors(&result) == true);
	bool found_color = false;
	for (int i = 0; i < result.count; i++) {
		if (result.errors[i].fatal &&
		    result.errors[i].message &&
		    strstr(result.errors[i].message, "invalid color"))
			found_color = true;
	}
	assert(found_color);

	config_result_destroy(&result);
	printf("  PASS: test_invalid_color\n");
}

static void
test_valid_color_formats(void)
{
	write_file(TEST_STYLE,
		"window.label.focus.textColor: #FF0000\n"
		"window.label.unfocus.textColor: #F00\n"
		"menu.title.textColor: white\n"
		"menu.frame.textColor: rgb:255/128/0\n"
	);

	struct config_result result;
	config_result_init(&result);
	validate_style_file(TEST_STYLE, &result);

	assert(config_result_has_errors(&result) == false);

	config_result_destroy(&result);
	printf("  PASS: test_valid_color_formats\n");
}

static void
test_invalid_border_width(void)
{
	write_file(TEST_STYLE,
		"window.borderWidth: notanumber\n"
	);

	struct config_result result;
	config_result_init(&result);
	validate_style_file(TEST_STYLE, &result);

	assert(config_result_has_errors(&result) == true);

	config_result_destroy(&result);
	printf("  PASS: test_invalid_border_width\n");
}

static void
test_nonexistent_file(void)
{
	struct config_result result;
	config_result_init(&result);
	validate_init_file("/nonexistent/path/init", &result);

	assert(config_result_has_errors(&result) == true);

	config_result_destroy(&result);
	printf("  PASS: test_nonexistent_file\n");
}

/* ---- Apps file ---- */

static int
count_fatal(const struct config_result *r, bool fatal)
{
	int n = 0;
	for (int i = 0; i < r->count; i++)
		n += r->errors[i].fatal == fatal;
	return n;
}

static void
test_apps_file(void)
{
	clean_files();
	/* What Fluxbox writes and documents: no problems */
	write_file(TEST_APPS,
		"[startup] {xsetroot -solid grey}\n"
		"[app] (name=xterm) (class=XTerm) {2}\n"
		"  [Workspace]\t{1}\n"
		"  [Dimensions]\t{50% 100%}\n"
		"  [Position]\t(WINCENTER)\t{0 0}\n"
		"  [Layer]\t{8}\n"
		"  [Deco]\t{0x5}\n"
		"  [FocusProtection] {Gain, Lock}\n"
		"[end]\n"
		"[group] (workspace)\n"
		"  [app] (foot)\n"
		"  [Layer] {Top}\n"
		"[end]\n");
	struct config_result r;
	config_result_init(&r);
	validate_apps_file(TEST_APPS, &r);
	assert(r.count == 0);
	config_result_destroy(&r);

	/* Errors with their lines; an unknown key is a warning */
	write_file(TEST_APPS,
		"[app] (class=[oops)\n"           /* 1: bad pattern */
		"[end]\n"
		"[app] (a)\n"
		"  [Layer] {Sky}\n"               /* 4: bad layer */
		"  [Position] (Middle) {1 1}\n"   /* 5: bad corner */
		"  [Colour] {red}\n"              /* 6: unknown key */
		"[end]\n"
		"stray text\n");                  /* 8: not an entry */
	config_result_init(&r);
	validate_apps_file(TEST_APPS, &r);
	assert(count_fatal(&r, true) == 4);
	assert(count_fatal(&r, false) == 1);
	int lines[] = {1, 4, 5, 6, 8};
	for (int i = 0; i < 5; i++)
		assert(r.errors[i].line == lines[i]);
	config_result_destroy(&r);

	/* validate_config checks it too */
	assert(validate_config(TEST_DIR) == 1);
	clean_files();
	printf("  PASS: test_apps_file\n");
}

/* ---- A Fluxbox init: what Fluxbox 1.3 writes ---- */

static void
test_fluxbox_init(void)
{
	struct config_result r;
	config_result_init(&r);
	validate_init_file(FLUXLAND_FIXTURES "/fluxbox/init", &r);
	int errors = 0, unsupported = 0;
	for (int i = 0; i < r.count; i++) {
		if (r.errors[i].fatal) {
			fprintf(stderr, "line %d: %s\n", r.errors[i].line,
				r.errors[i].message);
			errors++;
		}
		if (strstr(r.errors[i].message, "isn't supported"))
			unsupported++;
	}
	/* No errors; warnings only for what fluxland doesn't do yet (and
	 * the paths, which aren't there) */
	assert(errors == 0);
	assert(unsupported > 0);
	for (int i = 0; i < r.count; i++)
		assert(strstr(r.errors[i].message, "isn't supported") ||
			strstr(r.errors[i].message, "non-existent path"));
	config_result_destroy(&r);
	printf("  PASS: test_fluxbox_init\n");
}

/* ---- Menu file ---- */

static void
test_menu_file(void)
{
	clean_files();
	const char *menu = TEST_DIR "/menu";
	/* What fluxbox-generate_menu writes: no problems */
	write_file(menu,
		"[begin] (Fluxbox)\n"
		"[encoding] {UTF-8}\n"
		"  [exec] (xterm) {xterm} <~/.fluxbox/icons/xterm.xpm>\n"
		"  [submenu] (Styles) {Choose a style...}\n"
		"    [stylesdir] (/usr/share/fluxbox/styles)\n"
		"  [end]\n"
		"  [commanddialog] (Fluxbox Command)\n"
		"  [workspaces] (Workspace List)\n"
		"  [config] (Configure)\n"
		"  [reconfig] (Reload config)\n"
		"  [exit] (Exit)\n"
		"[endencoding]\n"
		"[end]\n");
	struct config_result r;
	config_result_init(&r);
	validate_menu_file(menu, &r);
	/* [commanddialog] is CommandDialog: nothing to say */
	assert(r.count == 0);
	config_result_destroy(&r);

	write_file(menu,
		"[begin] (Menu)\n"
		"[include] (/nonexistent/menu)\n"     /* 2: error */
		"[submenu] (Open)\n"                  /* 3: no [end] */
		"  [exec (broken) {x}\n");            /* 4: no ] */
	config_result_init(&r);
	validate_menu_file(menu, &r);
	int fatal = 0;
	for (int i = 0; i < r.count; i++)
		fatal += r.errors[i].fatal;
	assert(r.count == 3 && fatal == 2);
	assert(r.errors[0].line == 2 && r.errors[1].line == 4 &&
		r.errors[2].line == 3);
	config_result_destroy(&r);
	unlink(menu);
	printf("  PASS: test_menu_file\n");
}

/* ---- Top-level validation test ---- */

static void
test_validate_config_valid(void)
{
	clean_files();
	write_file(TEST_INIT,
		"session.screen0.workspaces: 4\n"
		"session.screen0.focusModel: ClickToFocus\n"
	);

	int ret = validate_config(TEST_DIR);
	assert(ret == 0);

	printf("  PASS: test_validate_config_valid\n");
}

static void
test_validate_config_errors(void)
{
	clean_files();
	write_file(TEST_INIT,
		"session.screen0.focusModel: BadFocus\n"
	);

	int ret = validate_config(TEST_DIR);
	assert(ret == 1);

	printf("  PASS: test_validate_config_errors\n");
}

/* ---- Empty file validation (all 3 types) ---- */

static void
test_empty_init_file(void)
{
	write_file(TEST_INIT, "");

	struct config_result result;
	config_result_init(&result);
	validate_init_file(TEST_INIT, &result);

	/* Empty file should produce no errors */
	assert(config_result_has_errors(&result) == false);
	assert(result.count == 0);

	config_result_destroy(&result);
	printf("  PASS: test_empty_init_file\n");
}

static void
test_empty_keys_file(void)
{
	write_file(TEST_KEYS, "");

	struct config_result result;
	config_result_init(&result);
	validate_keys_file(TEST_KEYS, &result);

	assert(config_result_has_errors(&result) == false);
	assert(result.count == 0);

	config_result_destroy(&result);
	printf("  PASS: test_empty_keys_file\n");
}

static void
test_empty_style_file(void)
{
	write_file(TEST_STYLE, "");

	struct config_result result;
	config_result_init(&result);
	validate_style_file(TEST_STYLE, &result);

	assert(config_result_has_errors(&result) == false);
	assert(result.count == 0);

	config_result_destroy(&result);
	printf("  PASS: test_empty_style_file\n");
}

/* ---- Invalid color formats in style validation ---- */

static void
test_validate_invalid_color_hash_short(void)
{
	/* #GGG - not valid hex */
	write_file(TEST_STYLE,
		"window.label.focus.textColor: #GGG\n"
	);

	struct config_result result;
	config_result_init(&result);
	validate_style_file(TEST_STYLE, &result);

	assert(config_result_has_errors(&result) == true);
	bool found = false;
	for (int i = 0; i < result.count; i++) {
		if (result.errors[i].fatal &&
		    result.errors[i].message &&
		    strstr(result.errors[i].message, "invalid color"))
			found = true;
	}
	assert(found);

	config_result_destroy(&result);
	printf("  PASS: test_validate_invalid_color_hash_short\n");
}

static void
test_validate_invalid_color_too_long(void)
{
	/* #FFFFFFFFF - 9 hex digits, neither 3 nor 6 */
	write_file(TEST_STYLE,
		"window.borderColor: #FFFFFFFFF\n"
	);

	struct config_result result;
	config_result_init(&result);
	validate_style_file(TEST_STYLE, &result);

	assert(config_result_has_errors(&result) == true);

	config_result_destroy(&result);
	printf("  PASS: test_validate_invalid_color_too_long\n");
}

/* ---- Invalid font strings in style validation ---- */

static void
test_validate_invalid_font_string(void)
{
	/* Font with no alphabetic characters - just digits */
	write_file(TEST_STYLE,
		"window.label.focus.font: 12345\n"
	);

	struct config_result result;
	config_result_init(&result);
	validate_style_file(TEST_STYLE, &result);

	/* is_valid_font_string checks for at least one alpha character;
	 * "12345" has digits which pass the check. Actually let's look again. */
	/* Actually '1', '2', etc. are not alpha, so the function returns false */
	assert(config_result_has_errors(&result) == true);
	bool found = false;
	for (int i = 0; i < result.count; i++) {
		if (result.errors[i].fatal &&
		    result.errors[i].message &&
		    strstr(result.errors[i].message, "invalid font"))
			found = true;
	}
	assert(found);

	config_result_destroy(&result);
	printf("  PASS: test_validate_invalid_font_string\n");
}

/* ---- Malformed struts ---- */

static void
test_validate_malformed_struts(void)
{
	/* Struts is VT_STRING in validate, so no specific validation error.
	 * But we can test with non-integer characters to see it's accepted. */
	write_file(TEST_INIT,
		"session.screen0.struts: not numbers\n"
	);

	struct config_result result;
	config_result_init(&result);
	validate_init_file(TEST_INIT, &result);

	/* struts is VT_STRING, so any value is accepted by the validator */
	assert(config_result_has_errors(&result) == false);

	config_result_destroy(&result);
	printf("  PASS: test_validate_malformed_struts\n");
}

/* ---- Multiple errors in same file ---- */

static void
test_multiple_errors_same_file(void)
{
	write_file(TEST_INIT,
		"session.screen0.focusModel: BadModel\n"
		"session.screen0.toolbar.visible: maybe\n"
		"session.screen0.workspaces: abc\n"
		"session.screen0.slit.placement: Nowhere\n"
		"session.screen0.toolbar.placement: Upside\n"
	);

	struct config_result result;
	config_result_init(&result);
	validate_init_file(TEST_INIT, &result);

	/* Should have at least 5 errors (one per invalid value) */
	int error_count = 0;
	for (int i = 0; i < result.count; i++) {
		if (result.errors[i].fatal)
			error_count++;
	}
	assert(error_count >= 5);

	config_result_destroy(&result);
	printf("  PASS: test_multiple_errors_same_file\n");
}

/* ---- Nonexistent file for keys and style validators ---- */

static void
test_nonexistent_keys_file(void)
{
	struct config_result result;
	config_result_init(&result);
	validate_keys_file("/nonexistent/path/keys", &result);

	/* Keys file validator adds a warning (not fatal) for missing file */
	assert(result.count > 0);
	bool found = false;
	for (int i = 0; i < result.count; i++) {
		if (result.errors[i].message &&
		    strstr(result.errors[i].message, "cannot open"))
			found = true;
	}
	assert(found);

	config_result_destroy(&result);
	printf("  PASS: test_nonexistent_keys_file\n");
}

static void
test_nonexistent_style_file(void)
{
	struct config_result result;
	config_result_init(&result);
	validate_style_file("/nonexistent/path/style", &result);

	assert(result.count > 0);
	bool found = false;
	for (int i = 0; i < result.count; i++) {
		if (result.errors[i].message &&
		    strstr(result.errors[i].message, "cannot open"))
			found = true;
	}
	assert(found);

	config_result_destroy(&result);
	printf("  PASS: test_nonexistent_style_file\n");
}

/* ---- Init file: empty value warning ---- */

static void
test_init_empty_value(void)
{
	write_file(TEST_INIT,
		"session.screen0.workspaces:\n"
	);

	struct config_result result;
	config_result_init(&result);
	validate_init_file(TEST_INIT, &result);

	/* Should produce a warning about empty value */
	bool found = false;
	for (int i = 0; i < result.count; i++) {
		if (result.errors[i].message &&
		    strstr(result.errors[i].message, "empty value"))
			found = true;
	}
	assert(found);

	config_result_destroy(&result);
	printf("  PASS: test_init_empty_value\n");
}

/* ---- Init file: missing colon warning ---- */

static void
test_init_missing_colon(void)
{
	write_file(TEST_INIT,
		"this line has no colon separator\n"
	);

	struct config_result result;
	config_result_init(&result);
	validate_init_file(TEST_INIT, &result);

	bool found = false;
	for (int i = 0; i < result.count; i++) {
		if (result.errors[i].message &&
		    strstr(result.errors[i].message, "missing ':'"))
			found = true;
	}
	assert(found);

	config_result_destroy(&result);
	printf("  PASS: test_init_missing_colon\n");
}

/* ---- Style file: negative integer value ---- */

static void
test_style_negative_integer(void)
{
	write_file(TEST_STYLE,
		"window.borderWidth: -5\n"
	);

	struct config_result result;
	config_result_init(&result);
	validate_style_file(TEST_STYLE, &result);

	/* Should produce warning about negative value */
	bool found = false;
	for (int i = 0; i < result.count; i++) {
		if (!result.errors[i].fatal &&
		    result.errors[i].message &&
		    strstr(result.errors[i].message, "negative"))
			found = true;
	}
	assert(found);

	config_result_destroy(&result);
	printf("  PASS: test_style_negative_integer\n");
}

/* ---- Keys file: missing colon ---- */

static void
test_keys_missing_colon(void)
{
	write_file(TEST_KEYS,
		"Mod4 x Close\n"
	);

	struct config_result result;
	config_result_init(&result);
	validate_keys_file(TEST_KEYS, &result);

	assert(config_result_has_errors(&result) == true);
	bool found = false;
	for (int i = 0; i < result.count; i++) {
		if (result.errors[i].fatal &&
		    result.errors[i].message &&
		    strstr(result.errors[i].message, "no ':'"))
			found = true;
	}
	assert(found);

	config_result_destroy(&result);
	printf("  PASS: test_keys_missing_colon\n");
}

/* ---- Init file: all slit enum validations ---- */

static void
test_validate_slit_enums(void)
{
	/* Valid slit placement */
	write_file(TEST_INIT,
		"session.screen0.slit.placement: LeftBottom\n"
		"session.screen0.slit.direction: Horizontal\n"
		"session.screen0.slit.layer: AboveDock\n"
	);

	struct config_result result;
	config_result_init(&result);
	validate_init_file(TEST_INIT, &result);

	assert(config_result_has_errors(&result) == false);

	config_result_destroy(&result);

	/* Invalid slit direction */
	write_file(TEST_INIT,
		"session.screen0.slit.direction: Sideways\n"
	);

	config_result_init(&result);
	validate_init_file(TEST_INIT, &result);

	assert(config_result_has_errors(&result) == true);

	config_result_destroy(&result);

	/* Invalid slit layer */
	write_file(TEST_INIT,
		"session.screen0.slit.layer: UnknownLayer\n"
	);

	config_result_init(&result);
	validate_init_file(TEST_INIT, &result);

	assert(config_result_has_errors(&result) == true);

	config_result_destroy(&result);
	printf("  PASS: test_validate_slit_enums\n");
}

/* ---- Style file: colorTo validation ---- */

static void
test_validate_color_to(void)
{
	write_file(TEST_STYLE,
		"window.title.focus.colorTo: invalidcolor\n"
	);

	struct config_result result;
	config_result_init(&result);
	validate_style_file(TEST_STYLE, &result);

	assert(config_result_has_errors(&result) == true);
	bool found = false;
	for (int i = 0; i < result.count; i++) {
		if (result.errors[i].fatal &&
		    result.errors[i].message &&
		    strstr(result.errors[i].message, "invalid color"))
			found = true;
	}
	assert(found);

	config_result_destroy(&result);
	printf("  PASS: test_validate_color_to\n");
}

/* ---- Result: warnings-only has no fatal errors ---- */

static void
test_result_warnings_only(void)
{
	struct config_result result;
	config_result_init(&result);

	config_result_add_warning(&result, "test.conf", 1, "warn 1");
	config_result_add_warning(&result, "test.conf", 2, "warn 2");
	config_result_add_warning(&result, "test.conf", 3, "warn 3");

	assert(result.count == 3);
	assert(config_result_has_errors(&result) == false);

	config_result_destroy(&result);
	printf("  PASS: test_result_warnings_only\n");
}

/* ---- Style file: valid integer properties ---- */

static void
test_validate_style_valid_integers(void)
{
	write_file(TEST_STYLE,
		"window.borderWidth: 2\n"
		"window.bevelWidth: 3\n"
		"window.handleWidth: 8\n"
		"window.title.height: 24\n"
		"menu.borderWidth: 1\n"
		"menu.itemHeight: 20\n"
		"menu.titleHeight: 22\n"
		"slit.borderWidth: 1\n"
	);

	struct config_result result;
	config_result_init(&result);
	validate_style_file(TEST_STYLE, &result);

	assert(config_result_has_errors(&result) == false);

	config_result_destroy(&result);
	printf("  PASS: test_validate_style_valid_integers\n");
}

/* ---- Validate config with no init file (defaults used) ---- */

static void
test_validate_config_no_files(void)
{
	clean_files();

	/* No init/keys/style files = defaults used, should be OK */
	int ret = validate_config(TEST_DIR);
	assert(ret == 0);

	printf("  PASS: test_validate_config_no_files\n");
}

/* ---- Comments and blank lines in keys file ---- */

static void
test_keys_comments_and_blanks(void)
{
	write_file(TEST_KEYS,
		"# Comment\n"
		"! Another comment\n"
		"\n"
		"   \n"
		"Mod4 Return :Exec foot\n"
	);

	struct config_result result;
	config_result_init(&result);
	validate_keys_file(TEST_KEYS, &result);

	assert(config_result_has_errors(&result) == false);

	config_result_destroy(&result);
	printf("  PASS: test_keys_comments_and_blanks\n");
}

int
main(void)
{
	printf("test_validate:\n");
	setup();

	/* Result helpers */
	test_result_init_destroy();
	test_apps_file();
	test_fluxbox_init();
	test_menu_file();
	test_result_no_errors();

	/* Init file validation */
	test_valid_init_file();
	test_input_settings();
	test_output_settings();
	test_invalid_workspace_count();
	test_invalid_alpha();
	test_invalid_focus_model();
	test_invalid_placement();
	test_invalid_bool();
	test_invalid_int();
	test_unknown_key();
	test_missing_file_path();

	/* Keys file validation */
	test_valid_keys_file();
	test_unknown_action();
	test_missing_action();
	test_invalid_button_number();
	test_fluxbox_keys_file();

	/* Style file validation */
	test_valid_style_file();
	test_invalid_color();
	test_valid_color_formats();
	test_invalid_border_width();

	/* Edge cases */
	test_nonexistent_file();

	/* Top-level validation */
	test_validate_config_valid();
	test_validate_config_errors();

	/* New coverage tests */
	test_empty_init_file();
	test_empty_keys_file();
	test_empty_style_file();
	test_validate_invalid_color_hash_short();
	test_validate_invalid_color_too_long();
	test_validate_invalid_font_string();
	test_validate_malformed_struts();
	test_multiple_errors_same_file();
	test_nonexistent_keys_file();
	test_nonexistent_style_file();
	test_init_empty_value();
	test_init_missing_colon();
	test_style_negative_integer();
	test_keys_missing_colon();
	test_validate_slit_enums();
	test_validate_color_to();
	test_result_warnings_only();
	test_validate_style_valid_integers();
	test_validate_config_no_files();
	test_keys_comments_and_blanks();

	cleanup();
	printf("All validate tests passed.\n");
	return 0;
}
