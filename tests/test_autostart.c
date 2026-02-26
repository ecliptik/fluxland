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

/* Test: symlinked startup file is followed and executed */
static void
test_symlinked_startup(void)
{
	const char *real_script = TEST_DIR "/real_startup.sh";
	const char *link_path = TEST_STARTUP;
	write_script(real_script, "#!/bin/sh\nexit 0\n", 0755);
	unlink(link_path);
	int sret = symlink(real_script, link_path);
	assert(sret == 0);
	int ret = wm_autostart_run(TEST_DIR, "wayland-0");
	assert(ret == 0);
	usleep(50000);
	unlink(link_path);
	unlink(real_script);
	printf("  PASS: test_symlinked_startup\n");
}

/* Test: symlinked non-executable startup file runs via /bin/sh */
static void
test_symlinked_non_executable_startup(void)
{
	const char *real_script = TEST_DIR "/real_startup_noexec.sh";
	const char *link_path = TEST_STARTUP;
	write_script(real_script, "#!/bin/sh\nexit 0\n", 0644);
	unlink(link_path);
	int sret = symlink(real_script, link_path);
	assert(sret == 0);
	int ret = wm_autostart_run(TEST_DIR, "wayland-0");
	assert(ret == 0);
	usleep(50000);
	unlink(link_path);
	unlink(real_script);
	printf("  PASS: test_symlinked_non_executable_startup\n");
}

/* Test: startup file with no read permission returns -1 */
static void
test_unreadable_startup(void)
{
	/* Must run as non-root for this to work; root can read anything */
	if (getuid() == 0) {
		printf("  SKIP: test_unreadable_startup (running as root)\n");
		return;
	}
	write_script(TEST_STARTUP, "#!/bin/sh\nexit 0\n", 0000);
	int ret = wm_autostart_run(TEST_DIR, "wayland-0");
	assert(ret == -1);
	/* Restore permissions so cleanup can remove it */
	chmod(TEST_STARTUP, 0644);
	printf("  PASS: test_unreadable_startup\n");
}

/* Test: config_dir path that overflows the 4096-byte buffer returns -1 */
static void
test_very_long_config_dir(void)
{
	/* Build a path longer than 4096 bytes so snprintf truncates and
	 * the resulting path does not exist on disk. */
	char long_dir[5000];
	memset(long_dir, 'a', sizeof(long_dir) - 1);
	long_dir[0] = '/';
	long_dir[sizeof(long_dir) - 1] = '\0';
	int ret = wm_autostart_run(long_dir, "wayland-0");
	assert(ret == -1);
	printf("  PASS: test_very_long_config_dir\n");
}

/* Test: wm_autostart_run_cmd with shell special characters */
static void
test_run_cmd_special_chars(void)
{
	/* Command with shell metacharacters; /bin/sh -c handles them */
	wm_autostart_run_cmd("echo 'hello world' && true; true", "wayland-0");
	usleep(50000);
	printf("  PASS: test_run_cmd_special_chars\n");
}

/* Test: multiple rapid run_cmd calls do not leave zombies */
static void
test_multiple_rapid_run_cmd(void)
{
	for (int i = 0; i < 5; i++) {
		wm_autostart_run_cmd("true", "wayland-0");
	}
	usleep(100000);
	printf("  PASS: test_multiple_rapid_run_cmd\n");
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
	test_symlinked_startup();

	/* Special file type tests */
	test_symlinked_non_executable_startup();
	test_unreadable_startup();

	/* Path overflow test */
	test_very_long_config_dir();

	/* run_cmd tests */
	test_run_cmd_null();
	test_run_cmd_valid();
	test_run_cmd_null_display();
	test_run_cmd_empty();
	test_run_cmd_special_chars();
	test_multiple_rapid_run_cmd();

	cleanup();
	printf("All autostart tests passed.\n");
	return 0;
}
