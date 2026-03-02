/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * test_rcparser.c - Unit tests for rcparser.c (RC config file parser)
 *
 * rcparser.c has no wayland/wlroots dependencies — it only uses
 * standard C and util.h inline helpers, so no stubs are needed.
 */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "rcparser.c"

/* ------------------------------------------------------------------ */
/* Test helpers                                                       */
/* ------------------------------------------------------------------ */

#define TEST_DIR "/tmp/fluxland-test"
#define TEST_RC  TEST_DIR "/test_rc_file"

static void
setup(void)
{
	mkdir(TEST_DIR, 0755);
}

static void
cleanup(void)
{
	unlink(TEST_RC);
}

static void
write_file(const char *path, const char *contents)
{
	FILE *f = fopen(path, "w");
	assert(f);
	fputs(contents, f);
	fclose(f);
}

/* ------------------------------------------------------------------ */
/* Database lifecycle tests                                           */
/* ------------------------------------------------------------------ */

static void
test_rc_create(void)
{
	struct rc_database *db = rc_create();
	assert(db != NULL);
	assert(db->count == 0);
	assert(db->capacity == 32);  /* RC_INITIAL_CAPACITY */
	assert(db->entries != NULL);
	rc_destroy(db);
	printf("PASS: test_rc_create\n");
}

static void
test_rc_destroy_null(void)
{
	/* Should not crash */
	rc_destroy(NULL);
	printf("PASS: test_rc_destroy_null\n");
}

/* ------------------------------------------------------------------ */
/* Set and get tests                                                  */
/* ------------------------------------------------------------------ */

static void
test_rc_set_and_get_string(void)
{
	struct rc_database *db = rc_create();
	rc_set(db, "session.screen0.workspaces", "4");
	const char *val = rc_get_string(db, "session.screen0.workspaces");
	assert(val != NULL);
	assert(strcmp(val, "4") == 0);
	rc_destroy(db);
	printf("PASS: test_rc_set_and_get_string\n");
}

static void
test_rc_set_overwrite(void)
{
	struct rc_database *db = rc_create();
	rc_set(db, "key", "first");
	assert(strcmp(rc_get_string(db, "key"), "first") == 0);

	rc_set(db, "key", "second");
	assert(strcmp(rc_get_string(db, "key"), "second") == 0);
	/* Count should not increase on overwrite */
	assert(db->count == 1);
	rc_destroy(db);
	printf("PASS: test_rc_set_overwrite\n");
}

static void
test_rc_get_string_missing(void)
{
	struct rc_database *db = rc_create();
	assert(rc_get_string(db, "nonexistent") == NULL);
	rc_destroy(db);
	printf("PASS: test_rc_get_string_missing\n");
}

static void
test_rc_set_many(void)
{
	struct rc_database *db = rc_create();
	/* Fill beyond initial capacity (32) to test realloc */
	for (int i = 0; i < 50; i++) {
		char key[32], val[32];
		snprintf(key, sizeof(key), "key.%d", i);
		snprintf(val, sizeof(val), "val_%d", i);
		rc_set(db, key, val);
	}
	assert(db->count == 50);
	assert(db->capacity >= 50);

	/* Verify first and last */
	assert(strcmp(rc_get_string(db, "key.0"), "val_0") == 0);
	assert(strcmp(rc_get_string(db, "key.49"), "val_49") == 0);
	rc_destroy(db);
	printf("PASS: test_rc_set_many\n");
}

/* ------------------------------------------------------------------ */
/* Integer parsing tests                                              */
/* ------------------------------------------------------------------ */

static void
test_rc_get_int(void)
{
	struct rc_database *db = rc_create();
	rc_set(db, "count", "42");
	assert(rc_get_int(db, "count", 0) == 42);

	rc_set(db, "negative", "-7");
	assert(rc_get_int(db, "negative", 0) == -7);

	rc_set(db, "zero", "0");
	assert(rc_get_int(db, "zero", -1) == 0);
	rc_destroy(db);
	printf("PASS: test_rc_get_int\n");
}

static void
test_rc_get_int_missing(void)
{
	struct rc_database *db = rc_create();
	assert(rc_get_int(db, "missing", 99) == 99);
	rc_destroy(db);
	printf("PASS: test_rc_get_int_missing\n");
}

static void
test_rc_get_int_invalid(void)
{
	struct rc_database *db = rc_create();
	rc_set(db, "bad", "notanumber");
	assert(rc_get_int(db, "bad", -1) == -1);

	rc_set(db, "trailing", "42abc");
	assert(rc_get_int(db, "trailing", -1) == -1);

	rc_set(db, "empty", "");
	assert(rc_get_int(db, "empty", 55) == 55);

	rc_set(db, "float", "3.14");
	assert(rc_get_int(db, "float", -1) == -1);
	rc_destroy(db);
	printf("PASS: test_rc_get_int_invalid\n");
}

/* ------------------------------------------------------------------ */
/* Boolean parsing tests                                              */
/* ------------------------------------------------------------------ */

static void
test_rc_get_bool_true_values(void)
{
	struct rc_database *db = rc_create();
	rc_set(db, "a", "true");
	assert(rc_get_bool(db, "a", false) == true);

	rc_set(db, "b", "True");
	assert(rc_get_bool(db, "b", false) == true);

	rc_set(db, "c", "TRUE");
	assert(rc_get_bool(db, "c", false) == true);

	rc_set(db, "d", "1");
	assert(rc_get_bool(db, "d", false) == true);

	rc_set(db, "e", "yes");
	assert(rc_get_bool(db, "e", false) == true);

	rc_set(db, "f", "Yes");
	assert(rc_get_bool(db, "f", false) == true);

	rc_set(db, "g", "YES");
	assert(rc_get_bool(db, "g", false) == true);
	rc_destroy(db);
	printf("PASS: test_rc_get_bool_true_values\n");
}

static void
test_rc_get_bool_false_values(void)
{
	struct rc_database *db = rc_create();
	rc_set(db, "a", "false");
	assert(rc_get_bool(db, "a", true) == false);

	rc_set(db, "b", "False");
	assert(rc_get_bool(db, "b", true) == false);

	rc_set(db, "c", "FALSE");
	assert(rc_get_bool(db, "c", true) == false);

	rc_set(db, "d", "0");
	assert(rc_get_bool(db, "d", true) == false);

	rc_set(db, "e", "no");
	assert(rc_get_bool(db, "e", true) == false);

	rc_set(db, "f", "No");
	assert(rc_get_bool(db, "f", true) == false);

	rc_set(db, "g", "NO");
	assert(rc_get_bool(db, "g", true) == false);
	rc_destroy(db);
	printf("PASS: test_rc_get_bool_false_values\n");
}

static void
test_rc_get_bool_missing(void)
{
	struct rc_database *db = rc_create();
	assert(rc_get_bool(db, "missing", true) == true);
	assert(rc_get_bool(db, "missing", false) == false);
	rc_destroy(db);
	printf("PASS: test_rc_get_bool_missing\n");
}

static void
test_rc_get_bool_invalid(void)
{
	struct rc_database *db = rc_create();
	rc_set(db, "garbage", "maybe");
	/* Invalid value should return default */
	assert(rc_get_bool(db, "garbage", true) == true);
	assert(rc_get_bool(db, "garbage", false) == false);
	rc_destroy(db);
	printf("PASS: test_rc_get_bool_invalid\n");
}

/* ------------------------------------------------------------------ */
/* File loading tests                                                 */
/* ------------------------------------------------------------------ */

static void
test_rc_load_basic(void)
{
	setup();
	write_file(TEST_RC,
		"session.screen0.workspaces: 4\n"
		"session.screen0.focusModel: MouseFocus\n"
		"session.screen0.toolbar.visible: true\n");

	struct rc_database *db = rc_create();
	assert(rc_load(db, TEST_RC) == 0);
	assert(db->count == 3);
	assert(strcmp(rc_get_string(db, "session.screen0.workspaces"), "4") == 0);
	assert(strcmp(rc_get_string(db, "session.screen0.focusModel"),
		"MouseFocus") == 0);
	assert(rc_get_bool(db, "session.screen0.toolbar.visible", false) == true);
	rc_destroy(db);
	cleanup();
	printf("PASS: test_rc_load_basic\n");
}

static void
test_rc_load_comments(void)
{
	setup();
	write_file(TEST_RC,
		"# This is a comment\n"
		"! This is also a comment\n"
		"key1: value1\n"
		"# Another comment\n"
		"key2: value2\n");

	struct rc_database *db = rc_create();
	assert(rc_load(db, TEST_RC) == 0);
	assert(db->count == 2);
	assert(strcmp(rc_get_string(db, "key1"), "value1") == 0);
	assert(strcmp(rc_get_string(db, "key2"), "value2") == 0);
	rc_destroy(db);
	cleanup();
	printf("PASS: test_rc_load_comments\n");
}

static void
test_rc_load_whitespace(void)
{
	setup();
	write_file(TEST_RC,
		"  key.with.spaces  :  value with spaces  \n"
		"\tkey.with.tabs\t:\ttabbed value\t\n"
		"\n"
		"   \n"
		"key.plain: plain\n");

	struct rc_database *db = rc_create();
	assert(rc_load(db, TEST_RC) == 0);
	assert(db->count == 3);
	/* Values should have leading/trailing whitespace stripped */
	assert(strcmp(rc_get_string(db, "key.with.spaces"),
		"value with spaces") == 0);
	assert(strcmp(rc_get_string(db, "key.with.tabs"),
		"tabbed value") == 0);
	assert(strcmp(rc_get_string(db, "key.plain"), "plain") == 0);
	rc_destroy(db);
	cleanup();
	printf("PASS: test_rc_load_whitespace\n");
}

static void
test_rc_load_malformed(void)
{
	setup();
	write_file(TEST_RC,
		"valid.key: valid.value\n"
		"no colon here\n"
		": empty key\n"
		"another.valid: ok\n");

	struct rc_database *db = rc_create();
	assert(rc_load(db, TEST_RC) == 0);
	/* "no colon here" should be skipped, ": empty key" has empty key */
	assert(db->count == 2);
	assert(strcmp(rc_get_string(db, "valid.key"), "valid.value") == 0);
	assert(strcmp(rc_get_string(db, "another.valid"), "ok") == 0);
	rc_destroy(db);
	cleanup();
	printf("PASS: test_rc_load_malformed\n");
}

static void
test_rc_load_missing_file(void)
{
	struct rc_database *db = rc_create();
	assert(rc_load(db, "/nonexistent/path/file") == -1);
	assert(db->count == 0);
	rc_destroy(db);
	printf("PASS: test_rc_load_missing_file\n");
}

static void
test_rc_load_empty_file(void)
{
	setup();
	write_file(TEST_RC, "");

	struct rc_database *db = rc_create();
	assert(rc_load(db, TEST_RC) == 0);
	assert(db->count == 0);
	rc_destroy(db);
	cleanup();
	printf("PASS: test_rc_load_empty_file\n");
}

static void
test_rc_load_value_with_colon(void)
{
	setup();
	/* Value itself contains a colon (e.g., color spec "#aa:bb:cc") */
	write_file(TEST_RC,
		"theme.color: rgb:FF/00/CC\n");

	struct rc_database *db = rc_create();
	assert(rc_load(db, TEST_RC) == 0);
	/* Only the first colon is the separator */
	assert(strcmp(rc_get_string(db, "theme.color"),
		"rgb:FF/00/CC") == 0);
	rc_destroy(db);
	cleanup();
	printf("PASS: test_rc_load_value_with_colon\n");
}

static void
test_rc_load_duplicate_keys(void)
{
	setup();
	write_file(TEST_RC,
		"key: first\n"
		"key: second\n");

	struct rc_database *db = rc_create();
	assert(rc_load(db, TEST_RC) == 0);
	/* Later value should overwrite earlier */
	assert(strcmp(rc_get_string(db, "key"), "second") == 0);
	assert(db->count == 1);
	rc_destroy(db);
	cleanup();
	printf("PASS: test_rc_load_duplicate_keys\n");
}

/* ------------------------------------------------------------------ */
/* Internal strip() function tests                                    */
/* ------------------------------------------------------------------ */

static void
test_strip_function(void)
{
	/* strip() modifies in place and returns pointer into the buffer */
	char buf1[] = "  hello  ";
	assert(strcmp(strip(buf1), "hello") == 0);

	char buf2[] = "no_spaces";
	assert(strcmp(strip(buf2), "no_spaces") == 0);

	char buf3[] = "   ";
	assert(strcmp(strip(buf3), "") == 0);

	char buf4[] = "";
	assert(strcmp(strip(buf4), "") == 0);

	char buf5[] = "\t\n  spaced  \t\n";
	assert(strcmp(strip(buf5), "spaced") == 0);
	printf("PASS: test_strip_function\n");
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

int
main(void)
{
	/* Lifecycle */
	test_rc_create();
	test_rc_destroy_null();

	/* Set and get */
	test_rc_set_and_get_string();
	test_rc_set_overwrite();
	test_rc_get_string_missing();
	test_rc_set_many();

	/* Integer parsing */
	test_rc_get_int();
	test_rc_get_int_missing();
	test_rc_get_int_invalid();

	/* Boolean parsing */
	test_rc_get_bool_true_values();
	test_rc_get_bool_false_values();
	test_rc_get_bool_missing();
	test_rc_get_bool_invalid();

	/* File loading */
	test_rc_load_basic();
	test_rc_load_comments();
	test_rc_load_whitespace();
	test_rc_load_malformed();
	test_rc_load_missing_file();
	test_rc_load_empty_file();
	test_rc_load_value_with_colon();
	test_rc_load_duplicate_keys();

	/* Internal functions */
	test_strip_function();

	printf("\nAll rcparser tests passed (%d tests)\n", 21);
	return 0;
}
