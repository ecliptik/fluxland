/*
 * test_autostart.c - Unit tests for autostart module
 *
 * Tests the startup file detection and validation logic in autostart.c.
 * Success-path tests will trigger double-fork but the spawned children
 * either exit immediately or exec trivial scripts, which is safe.
 */

#define _GNU_SOURCE

#include "autostart.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define TEST_DIR "/tmp/fluxland-test/wm-test-autostart"
#define TEST_STARTUP TEST_DIR "/startup"

static void
setup(void)
{
	mkdir("/tmp/fluxland-test", 0755);
	mkdir(TEST_DIR, 0755);
}

static void
cleanup(void)
{
	unlink(TEST_STARTUP);
	rmdir(TEST_DIR);
}

static void
write_script(const char *path, const char *contents, mode_t mode)
{
	FILE *f = fopen(path, "w");
	assert(f);
	fputs(contents, f);
	fclose(f);
	chmod(path, mode);
}

/* Test: NULL config_dir returns -1 */
static void
test_null_config_dir(void)
{
	int ret = wm_autostart_run(NULL, "wayland-0");
	assert(ret == -1);
	printf("  PASS: test_null_config_dir\n");
}

/* Test: nonexistent config directory returns -1 */
static void
test_nonexistent_config_dir(void)
{
	int ret = wm_autostart_run(
		"/tmp/fluxland-test/nonexistent_dir_12345", "wayland-0");
	assert(ret == -1);
	printf("  PASS: test_nonexistent_config_dir\n");
}

/* Test: empty string config_dir returns -1 */
static void
test_empty_config_dir(void)
{
	int ret = wm_autostart_run("", "wayland-0");
	assert(ret == -1);
	printf("  PASS: test_empty_config_dir\n");
}

/* Test: valid directory with no startup file returns -1 */
static void
test_no_startup_file(void)
{
	unlink(TEST_STARTUP);
	int ret = wm_autostart_run(TEST_DIR, "wayland-0");
	assert(ret == -1);
	printf("  PASS: test_no_startup_file\n");
}

/* Test: "startup" path that is a directory returns -1 */
static void
test_startup_is_directory(void)
{
	mkdir(TEST_STARTUP, 0755);
	int ret = wm_autostart_run(TEST_DIR, "wayland-0");
	assert(ret == -1);
	rmdir(TEST_STARTUP);
	printf("  PASS: test_startup_is_directory\n");
}

/* Test: non-executable startup file returns 0 (run via /bin/sh) */
static void
test_non_executable_startup(void)
{
	write_script(TEST_STARTUP, "#!/bin/sh\nexit 0\n", 0644);
	int ret = wm_autostart_run(TEST_DIR, "wayland-0");
	assert(ret == 0);
	/* Allow grandchild to finish */
	usleep(50000);
	printf("  PASS: test_non_executable_startup\n");
}

/* Test: executable startup file returns 0 (run directly) */
static void
test_executable_startup(void)
{
	write_script(TEST_STARTUP, "#!/bin/sh\nexit 0\n", 0755);
	int ret = wm_autostart_run(TEST_DIR, "wayland-0");
	assert(ret == 0);
	usleep(50000);
	printf("  PASS: test_executable_startup\n");
}

/* Test: NULL wayland_display is handled gracefully */
static void
test_null_wayland_display(void)
{
	write_script(TEST_STARTUP, "#!/bin/sh\nexit 0\n", 0755);
	int ret = wm_autostart_run(TEST_DIR, NULL);
	assert(ret == 0);
	usleep(50000);
	printf("  PASS: test_null_wayland_display\n");
}

/* Test: wm_autostart_run_cmd with NULL cmd doesn't crash */
static void
test_run_cmd_null(void)
{
	wm_autostart_run_cmd(NULL, "wayland-0");
	printf("  PASS: test_run_cmd_null\n");
}

/* Test: wm_autostart_run_cmd with valid command succeeds */
static void
test_run_cmd_valid(void)
{
	wm_autostart_run_cmd("true", "wayland-0");
	usleep(50000);
	printf("  PASS: test_run_cmd_valid\n");
}

/* Test: wm_autostart_run_cmd with NULL wayland_display */
static void
test_run_cmd_null_display(void)
{
	wm_autostart_run_cmd("true", NULL);
	usleep(50000);
	printf("  PASS: test_run_cmd_null_display\n");
}

/* Test: wm_autostart_run_cmd with empty command string */
static void
test_run_cmd_empty(void)
{
	/* Empty command should not crash; /bin/sh -c "" exits 0 */
	wm_autostart_run_cmd("", "wayland-0");
	usleep(50000);
	printf("  PASS: test_run_cmd_empty\n");
}

/* Test: startup script with only a comment still returns 0 */
static void
test_comment_only_script(void)
{
	write_script(TEST_STARTUP, "#!/bin/sh\n# nothing to do\n", 0644);
	int ret = wm_autostart_run(TEST_DIR, "wayland-0");
	assert(ret == 0);
	usleep(50000);
	printf("  PASS: test_comment_only_script\n");
}

int
main(void)
{
	printf("test_autostart:\n");
	setup();

	/* Error path tests (no fork happens) */
	test_null_config_dir();
	test_nonexistent_config_dir();
	test_empty_config_dir();
	test_no_startup_file();
	test_startup_is_directory();

	/* Success path tests (fork happens, children exit quickly) */
	test_non_executable_startup();
	test_executable_startup();
	test_null_wayland_display();
	test_comment_only_script();

	/* run_cmd tests */
	test_run_cmd_null();
	test_run_cmd_valid();
	test_run_cmd_null_display();
	test_run_cmd_empty();

	cleanup();
	printf("All autostart tests passed.\n");
	return 0;
}
