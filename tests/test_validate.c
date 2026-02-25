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
		    strstr(result.errors[i].message, "missing ':'"))
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
