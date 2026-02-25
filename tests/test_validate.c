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
	rmdir(TEST_DIR);
}

/* Clean up individual test files between tests */
static void
clean_files(void)
{
	unlink(TEST_INIT);
	unlink(TEST_KEYS);
	unlink(TEST_STYLE);
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
		    strstr(result.errors[i].message, "unknown action"))
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
	write_file(TEST_KEYS,
		"OnDesktop Mouse9 :RootMenu\n"
	);

	struct config_result result;
	config_result_init(&result);
	validate_keys_file(TEST_KEYS, &result);

	assert(config_result_has_errors(&result) == true);
	bool found_button = false;
	for (int i = 0; i < result.count; i++) {
		if (result.errors[i].fatal &&
		    result.errors[i].message &&
		    strstr(result.errors[i].message, "button number"))
			found_button = true;
	}
	assert(found_button);

	config_result_destroy(&result);
	printf("  PASS: test_invalid_button_number\n");
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

int
main(void)
{
	printf("test_validate:\n");
	setup();

	/* Result helpers */
	test_result_init_destroy();
	test_result_no_errors();

	/* Init file validation */
	test_valid_init_file();
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

	cleanup();
	printf("All validate tests passed.\n");
	return 0;
}
