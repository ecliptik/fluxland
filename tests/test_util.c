/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 *
 * test_util.c - Unit tests for util.c and util.h inline helpers
 *
 * Tests safe_atoi(), wm_is_blocked_env_var(), fopen_nofollow(),
 * and wm_spawn_command() basic behavior.
 */

#define _GNU_SOURCE

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* --- Block wlr/util/log.h and provide stub --- */
#define WLR_UTIL_LOG_H
enum wlr_log_importance { WLR_SILENT = 0, WLR_ERROR = 1, WLR_INFO = 2, WLR_DEBUG = 3 };
#define wlr_log(verb, fmt, ...) ((void)0)

/* Pull in util.c (includes util.h which has the inline helpers) */
#include "util.c"

/* ------------------------------------------------------------------ */
/* Test helpers                                                       */
/* ------------------------------------------------------------------ */

#define TEST_DIR "/tmp/fluxland-test"
#define TEST_FILE TEST_DIR "/test_util_file"
#define TEST_LINK TEST_DIR "/test_util_symlink"

static void
setup(void)
{
	mkdir(TEST_DIR, 0755);
}

static void
cleanup(void)
{
	unlink(TEST_FILE);
	unlink(TEST_LINK);
}

/* ------------------------------------------------------------------ */
/* safe_atoi() tests                                                  */
/* ------------------------------------------------------------------ */

static void
test_safe_atoi_valid(void)
{
	int out = -1;
	assert(safe_atoi("42", &out) == true);
	assert(out == 42);

	assert(safe_atoi("0", &out) == true);
	assert(out == 0);

	assert(safe_atoi("1", &out) == true);
	assert(out == 1);

	assert(safe_atoi("999999", &out) == true);
	assert(out == 999999);
	printf("PASS: test_safe_atoi_valid\n");
}

static void
test_safe_atoi_negative(void)
{
	int out = 0;
	assert(safe_atoi("-1", &out) == true);
	assert(out == -1);

	assert(safe_atoi("-42", &out) == true);
	assert(out == -42);

	assert(safe_atoi("-999999", &out) == true);
	assert(out == -999999);
	printf("PASS: test_safe_atoi_negative\n");
}

static void
test_safe_atoi_limits(void)
{
	int out = 0;
	char buf[32];

	/* INT_MAX should succeed */
	snprintf(buf, sizeof(buf), "%d", INT_MAX);
	assert(safe_atoi(buf, &out) == true);
	assert(out == INT_MAX);

	/* INT_MIN should succeed */
	snprintf(buf, sizeof(buf), "%d", INT_MIN);
	assert(safe_atoi(buf, &out) == true);
	assert(out == INT_MIN);
	printf("PASS: test_safe_atoi_limits\n");
}

static void
test_safe_atoi_overflow(void)
{
	int out = 0;

	/* Larger than INT_MAX */
	assert(safe_atoi("99999999999999999999", &out) == false);

	/* Just beyond INT_MAX: 2147483648 */
	assert(safe_atoi("2147483648", &out) == false);

	/* Just beyond INT_MIN: -2147483649 */
	assert(safe_atoi("-2147483649", &out) == false);
	printf("PASS: test_safe_atoi_overflow\n");
}

static void
test_safe_atoi_non_numeric(void)
{
	int out = 0;
	assert(safe_atoi("abc", &out) == false);
	assert(safe_atoi("hello world", &out) == false);
	assert(safe_atoi("--5", &out) == false);
	assert(safe_atoi("1e5", &out) == false);
	printf("PASS: test_safe_atoi_non_numeric\n");
}

static void
test_safe_atoi_trailing(void)
{
	int out = 0;
	/* Trailing non-numeric characters should fail */
	assert(safe_atoi("42abc", &out) == false);
	assert(safe_atoi("42 ", &out) == false);
	assert(safe_atoi("42.0", &out) == false);
	printf("PASS: test_safe_atoi_trailing\n");
}

static void
test_safe_atoi_empty_and_null(void)
{
	int out = 0;
	assert(safe_atoi("", &out) == false);
	assert(safe_atoi(NULL, &out) == false);
	printf("PASS: test_safe_atoi_empty_and_null\n");
}

static void
test_safe_atoi_leading_whitespace(void)
{
	int out = 0;
	/* strtol skips leading whitespace, but we don't mandate rejection.
	 * The function uses strtol, so leading spaces are accepted. */
	bool result = safe_atoi(" 123", &out);
	if (result) {
		assert(out == 123);
	}
	/* Either behavior is acceptable — the test just confirms no crash */
	printf("PASS: test_safe_atoi_leading_whitespace\n");
}

/* ------------------------------------------------------------------ */
/* wm_is_blocked_env_var() tests                                      */
/* ------------------------------------------------------------------ */

static void
test_blocked_env_ld_vars(void)
{
	assert(wm_is_blocked_env_var("LD_PRELOAD") == true);
	assert(wm_is_blocked_env_var("LD_LIBRARY_PATH") == true);
	assert(wm_is_blocked_env_var("LD_AUDIT") == true);
	assert(wm_is_blocked_env_var("LD_DEBUG") == true);
	assert(wm_is_blocked_env_var("LD_PROFILE") == true);
	printf("PASS: test_blocked_env_ld_vars\n");
}

static void
test_blocked_env_system_vars(void)
{
	assert(wm_is_blocked_env_var("PATH") == true);
	assert(wm_is_blocked_env_var("IFS") == true);
	assert(wm_is_blocked_env_var("SHELL") == true);
	assert(wm_is_blocked_env_var("HOME") == true);
	assert(wm_is_blocked_env_var("XDG_RUNTIME_DIR") == true);
	assert(wm_is_blocked_env_var("WAYLAND_DISPLAY") == true);
	printf("PASS: test_blocked_env_system_vars\n");
}

static void
test_blocked_env_safe_vars(void)
{
	/* These should NOT be blocked */
	assert(wm_is_blocked_env_var("TERM") == false);
	assert(wm_is_blocked_env_var("EDITOR") == false);
	assert(wm_is_blocked_env_var("MY_CUSTOM_VAR") == false);
	assert(wm_is_blocked_env_var("DISPLAY") == false);
	assert(wm_is_blocked_env_var("LANG") == false);
	printf("PASS: test_blocked_env_safe_vars\n");
}

static void
test_blocked_env_case_insensitive(void)
{
	/* wm_is_blocked_env_var uses strcasecmp */
	assert(wm_is_blocked_env_var("ld_preload") == true);
	assert(wm_is_blocked_env_var("Ld_Preload") == true);
	assert(wm_is_blocked_env_var("path") == true);
	assert(wm_is_blocked_env_var("Path") == true);
	printf("PASS: test_blocked_env_case_insensitive\n");
}

/* ------------------------------------------------------------------ */
/* fopen_nofollow() tests                                             */
/* ------------------------------------------------------------------ */

static void
test_fopen_nofollow_regular_file(void)
{
	setup();

	/* Create a regular file */
	FILE *f = fopen(TEST_FILE, "w");
	assert(f);
	fputs("test content\n", f);
	fclose(f);

	/* fopen_nofollow should succeed on a regular file */
	FILE *f2 = fopen_nofollow(TEST_FILE, "r");
	assert(f2 != NULL);
	char buf[64] = {0};
	fgets(buf, sizeof(buf), f2);
	assert(strstr(buf, "test content") != NULL);
	fclose(f2);

	cleanup();
	printf("PASS: test_fopen_nofollow_regular_file\n");
}

static void
test_fopen_nofollow_missing_file(void)
{
	FILE *f = fopen_nofollow("/nonexistent/path/file", "r");
	assert(f == NULL);
	printf("PASS: test_fopen_nofollow_missing_file\n");
}

static void
test_fopen_nofollow_symlink(void)
{
	setup();

	/* Create a regular file and a symlink to it */
	FILE *f = fopen(TEST_FILE, "w");
	assert(f);
	fputs("target\n", f);
	fclose(f);

	/* Create a symlink */
	unlink(TEST_LINK);
	assert(symlink(TEST_FILE, TEST_LINK) == 0);

	/* fopen_nofollow should reject the symlink */
	FILE *f2 = fopen_nofollow(TEST_LINK, "r");
	assert(f2 == NULL);

	cleanup();
	printf("PASS: test_fopen_nofollow_symlink\n");
}

static void
test_fopen_nofollow_write(void)
{
	setup();
	unlink(TEST_FILE);

	/* Write mode should create the file */
	FILE *f = fopen_nofollow(TEST_FILE, "w");
	assert(f != NULL);
	fputs("written via nofollow\n", f);
	fclose(f);

	/* Verify content */
	FILE *f2 = fopen(TEST_FILE, "r");
	assert(f2);
	char buf[64] = {0};
	fgets(buf, sizeof(buf), f2);
	assert(strstr(buf, "written via nofollow") != NULL);
	fclose(f2);

	cleanup();
	printf("PASS: test_fopen_nofollow_write\n");
}

/* ------------------------------------------------------------------ */
/* wm_spawn_command() tests                                           */
/* ------------------------------------------------------------------ */

static void
test_spawn_command_null(void)
{
	/* Should not crash on NULL or empty */
	wm_spawn_command(NULL);
	wm_spawn_command("");
	printf("PASS: test_spawn_command_null\n");
}

static void
test_spawn_command_basic(void)
{
	setup();
	/* Spawn a command that creates a marker file */
	char cmd[256];
	snprintf(cmd, sizeof(cmd), "touch %s/spawn_marker", TEST_DIR);
	wm_spawn_command(cmd);

	/* Give the double-fork grandchild a moment to execute */
	usleep(200000);  /* 200ms */

	char marker[256];
	snprintf(marker, sizeof(marker), "%s/spawn_marker", TEST_DIR);
	assert(access(marker, F_OK) == 0);

	unlink(marker);
	printf("PASS: test_spawn_command_basic\n");
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

int
main(void)
{
	/* safe_atoi */
	test_safe_atoi_valid();
	test_safe_atoi_negative();
	test_safe_atoi_limits();
	test_safe_atoi_overflow();
	test_safe_atoi_non_numeric();
	test_safe_atoi_trailing();
	test_safe_atoi_empty_and_null();
	test_safe_atoi_leading_whitespace();

	/* wm_is_blocked_env_var */
	test_blocked_env_ld_vars();
	test_blocked_env_system_vars();
	test_blocked_env_safe_vars();
	test_blocked_env_case_insensitive();

	/* fopen_nofollow */
	test_fopen_nofollow_regular_file();
	test_fopen_nofollow_missing_file();
	test_fopen_nofollow_symlink();
	test_fopen_nofollow_write();

	/* wm_spawn_command */
	test_spawn_command_null();
	test_spawn_command_basic();

	printf("\nAll util tests passed (%d tests)\n", 18);
	return 0;
}
