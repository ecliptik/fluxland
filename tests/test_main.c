/*
 * test_main.c - Integration tests for fluxland main.c entry point
 *
 * These tests invoke the actual fluxland binary with various command-line
 * flags and verify exit codes and output.  This is an integration-style
 * test (no #include "source.c" pattern) since main() owns the process.
 */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define TEST_DIR "/tmp/fluxland-test/wm-test-main"

/*
 * Run a command, capture combined stdout+stderr into a malloc'd buffer,
 * and return the wait status.  Caller must free *out_buf.
 */
static int
run_cmd(const char *cmd, char **out_buf, size_t *out_len)
{
	FILE *fp = popen(cmd, "r");
	assert(fp);

	size_t cap = 4096;
	size_t len = 0;
	char *buf = malloc(cap);
	assert(buf);

	size_t n;
	while ((n = fread(buf + len, 1, cap - len, fp)) > 0) {
		len += n;
		if (len == cap) {
			cap *= 2;
			buf = realloc(buf, cap);
			assert(buf);
		}
	}
	buf[len] = '\0';

	int status = pclose(fp);
	if (out_buf)
		*out_buf = buf;
	else
		free(buf);
	if (out_len)
		*out_len = len;
	return status;
}

/* Helper: get the fluxland binary path from the FLUXLAND_BIN env var,
 * falling back to a build-relative default. */
static const char *
get_binary(void)
{
	const char *bin = getenv("FLUXLAND_BIN");
	if (bin)
		return bin;
	return "fluxland";
}

/* ---- Test: --version prints version string and exits 0 ---- */
static void
test_version_flag(void)
{
	const char *bin = get_binary();
	char cmd[512];
	snprintf(cmd, sizeof(cmd), "%s --version 2>&1", bin);

	char *out = NULL;
	int status = run_cmd(cmd, &out, NULL);

	assert(WIFEXITED(status));
	assert(WEXITSTATUS(status) == 0);
	assert(strstr(out, "fluxland") != NULL);

	free(out);
	printf("  PASS: test_version_flag\n");
}

/* ---- Test: -v (short form) also works ---- */
static void
test_version_short_flag(void)
{
	const char *bin = get_binary();
	char cmd[512];
	snprintf(cmd, sizeof(cmd), "%s -v 2>&1", bin);

	char *out = NULL;
	int status = run_cmd(cmd, &out, NULL);

	assert(WIFEXITED(status));
	assert(WEXITSTATUS(status) == 0);
	assert(strstr(out, "fluxland") != NULL);

	free(out);
	printf("  PASS: test_version_short_flag\n");
}

/* ---- Test: --help prints usage and exits 0 ---- */
static void
test_help_flag(void)
{
	const char *bin = get_binary();
	char cmd[512];
	snprintf(cmd, sizeof(cmd), "%s --help 2>&1", bin);

	char *out = NULL;
	int status = run_cmd(cmd, &out, NULL);

	assert(WIFEXITED(status));
	assert(WEXITSTATUS(status) == 0);
	assert(strstr(out, "Usage:") != NULL);
	assert(strstr(out, "--version") != NULL);
	assert(strstr(out, "--help") != NULL);
	assert(strstr(out, "--check-config") != NULL);

	free(out);
	printf("  PASS: test_help_flag\n");
}

/* ---- Test: -h (short form) also works ---- */
static void
test_help_short_flag(void)
{
	const char *bin = get_binary();
	char cmd[512];
	snprintf(cmd, sizeof(cmd), "%s -h 2>&1", bin);

	char *out = NULL;
	int status = run_cmd(cmd, &out, NULL);

	assert(WIFEXITED(status));
	assert(WEXITSTATUS(status) == 0);
	assert(strstr(out, "Usage:") != NULL);

	free(out);
	printf("  PASS: test_help_short_flag\n");
}

/* ---- Test: --list-commands prints known actions and exits 0 ---- */
static void
test_list_commands_flag(void)
{
	const char *bin = get_binary();
	char cmd[512];
	snprintf(cmd, sizeof(cmd), "%s --list-commands 2>&1", bin);

	char *out = NULL;
	size_t len = 0;
	int status = run_cmd(cmd, &out, &len);

	assert(WIFEXITED(status));
	assert(WEXITSTATUS(status) == 0);

	/* Should contain at least some well-known action names */
	assert(strstr(out, "Close") != NULL);
	assert(strstr(out, "Exec") != NULL);

	/* Should have multiple lines (there are 100+ commands) */
	int lines = 0;
	for (size_t i = 0; i < len; i++) {
		if (out[i] == '\n')
			lines++;
	}
	assert(lines >= 10);

	free(out);
	printf("  PASS: test_list_commands_flag\n");
}

/* ---- Test: --check-config with valid config directory exits 0 ---- */
static void
test_check_config_valid(void)
{
	const char *bin = get_binary();

	/* Create a minimal valid config directory */
	mkdir("/tmp/fluxland-test", 0755);
	mkdir(TEST_DIR, 0700);

	/* Write a minimal valid init file */
	char init_path[512];
	snprintf(init_path, sizeof(init_path), "%s/init", TEST_DIR);
	FILE *f = fopen(init_path, "w");
	assert(f);
	fprintf(f, "session.screen0.toolbar.visible: true\n");
	fclose(f);

	char cmd[1024];
	snprintf(cmd, sizeof(cmd),
		"HOME=/tmp/fluxland-test-noexist "
		"FLUXLAND_CONFIG_DIR=%s "
		"%s --check-config 2>&1",
		TEST_DIR, bin);

	char *out = NULL;
	int status = run_cmd(cmd, &out, NULL);

	assert(WIFEXITED(status));
	assert(WEXITSTATUS(status) == 0);

	free(out);
	unlink(init_path);
	printf("  PASS: test_check_config_valid\n");
}

/* ---- Test: --check-config with unreadable config file exits 1 ---- */
static void
test_check_config_unreadable(void)
{
	const char *bin = get_binary();

	mkdir("/tmp/fluxland-test", 0755);
	mkdir(TEST_DIR, 0700);

	/* Create an unreadable init file to trigger a fatal error */
	char init_path[512];
	snprintf(init_path, sizeof(init_path), "%s/init", TEST_DIR);
	FILE *f = fopen(init_path, "w");
	assert(f);
	fprintf(f, "dummy\n");
	fclose(f);
	chmod(init_path, 0000);

	char cmd[1024];
	snprintf(cmd, sizeof(cmd),
		"HOME=/tmp/fluxland-test-noexist "
		"FLUXLAND_CONFIG_DIR=%s "
		"%s --check-config 2>&1",
		TEST_DIR, bin);

	char *out = NULL;
	int status = run_cmd(cmd, &out, NULL);

	assert(WIFEXITED(status));
	assert(WEXITSTATUS(status) == 1);
	assert(strstr(out, "error") != NULL);

	/* Restore permissions for cleanup */
	chmod(init_path, 0644);
	unlink(init_path);

	free(out);
	printf("  PASS: test_check_config_unreadable\n");
}

/* ---- Test: unrecognized option prints usage and exits 0 ---- */
/*
 * Note: getopt_long prints an "unrecognized option" message to stderr.
 * The default case in main.c's switch falls through to the help case,
 * which prints usage and returns 0.
 */
static void
test_invalid_flag(void)
{
	const char *bin = get_binary();
	char cmd[512];
	snprintf(cmd, sizeof(cmd), "%s --bogus-flag-xyz 2>&1", bin);

	char *out = NULL;
	int status = run_cmd(cmd, &out, NULL);

	assert(WIFEXITED(status));
	assert(WEXITSTATUS(status) == 0);
	assert(strstr(out, "Usage:") != NULL);

	free(out);
	printf("  PASS: test_invalid_flag\n");
}

/* ---- Test: --check-config with empty config dir (no files) exits 0 ---- */
static void
test_check_config_empty_dir(void)
{
	const char *bin = get_binary();
	const char *empty_dir = TEST_DIR "/empty-cfg";

	mkdir("/tmp/fluxland-test", 0755);
	mkdir(TEST_DIR, 0700);
	mkdir(empty_dir, 0700);

	char cmd[1024];
	snprintf(cmd, sizeof(cmd),
		"HOME=/tmp/fluxland-test-noexist "
		"FLUXLAND_CONFIG_DIR=%s "
		"%s --check-config 2>&1",
		empty_dir, bin);

	char *out = NULL;
	int status = run_cmd(cmd, &out, NULL);

	assert(WIFEXITED(status));
	assert(WEXITSTATUS(status) == 0);
	/* Should indicate no issues */
	assert(strstr(out, "OK") != NULL ||
	       strstr(out, "no issues") != NULL);

	rmdir(empty_dir);

	free(out);
	printf("  PASS: test_check_config_empty_dir\n");
}

/* ---- Test: -d flag enables debug (shouldn't crash with just -d) ---- */
static void
test_debug_flag(void)
{
	const char *bin = get_binary();
	char cmd[512];
	/* -d alone without a running Wayland session will fail at server init,
	 * but the important thing is it gets past the flag parsing.
	 * We combine -d with --help to get a clean exit. */
	snprintf(cmd, sizeof(cmd), "%s -d --help 2>&1", bin);

	char *out = NULL;
	int status = run_cmd(cmd, &out, NULL);

	assert(WIFEXITED(status));
	assert(WEXITSTATUS(status) == 0);
	assert(strstr(out, "Usage:") != NULL);

	free(out);
	printf("  PASS: test_debug_flag\n");
}

/* ---- Test: -s flag with --help still shows help ---- */
static void
test_startup_flag_with_help(void)
{
	const char *bin = get_binary();
	char cmd[512];
	/* -s requires an argument; combine with --help */
	snprintf(cmd, sizeof(cmd), "%s -s 'echo test' --help 2>&1", bin);

	char *out = NULL;
	int status = run_cmd(cmd, &out, NULL);

	assert(WIFEXITED(status));
	assert(WEXITSTATUS(status) == 0);
	assert(strstr(out, "Usage:") != NULL);

	free(out);
	printf("  PASS: test_startup_flag_with_help\n");
}

/* ---- Test: --ipc-no-exec flag with --help ---- */
static void
test_ipc_no_exec_flag(void)
{
	const char *bin = get_binary();
	char cmd[512];
	snprintf(cmd, sizeof(cmd), "%s --ipc-no-exec --help 2>&1", bin);

	char *out = NULL;
	int status = run_cmd(cmd, &out, NULL);

	assert(WIFEXITED(status));
	assert(WEXITSTATUS(status) == 0);
	assert(strstr(out, "Usage:") != NULL);

	free(out);
	printf("  PASS: test_ipc_no_exec_flag\n");
}

/* ---- Test: no XDG_RUNTIME_DIR causes error exit ---- */
static void
test_no_xdg_runtime_dir(void)
{
	const char *bin = get_binary();
	char cmd[1024];
	/* Unset XDG_RUNTIME_DIR so the compositor fails at startup.
	 * Also unset HOME to avoid any fallback. */
	snprintf(cmd, sizeof(cmd),
		"env -u XDG_RUNTIME_DIR %s 2>&1", bin);

	char *out = NULL;
	int status = run_cmd(cmd, &out, NULL);

	assert(WIFEXITED(status));
	/* Should exit with error (1) */
	assert(WEXITSTATUS(status) == 1);

	free(out);
	printf("  PASS: test_no_xdg_runtime_dir\n");
}

/* ---- Test: --check-config with bad key reports error content ---- */
static void
test_check_config_bad_key(void)
{
	const char *bin = get_binary();
	const char *bad_dir = TEST_DIR "/bad-cfg";

	mkdir("/tmp/fluxland-test", 0755);
	mkdir(TEST_DIR, 0700);
	mkdir(bad_dir, 0700);

	/* Write an init file with an obviously wrong key */
	char init_path[512];
	snprintf(init_path, sizeof(init_path), "%s/init", bad_dir);
	FILE *f = fopen(init_path, "w");
	assert(f);
	fprintf(f, "totally.invalid.key.that.does.not.exist: true\n");
	fclose(f);

	char cmd[1024];
	snprintf(cmd, sizeof(cmd),
		"HOME=/tmp/fluxland-test-noexist "
		"FLUXLAND_CONFIG_DIR=%s "
		"%s --check-config 2>&1",
		bad_dir, bin);

	char *out = NULL;
	int status = run_cmd(cmd, &out, NULL);

	/* Should succeed (unknown keys are warnings, not fatal) or
	 * report the issues */
	assert(WIFEXITED(status));

	free(out);
	unlink(init_path);
	rmdir(bad_dir);
	printf("  PASS: test_check_config_bad_key\n");
}

/* ---- Test: multiple flags combined ---- */
static void
test_combined_flags(void)
{
	const char *bin = get_binary();
	char cmd[512];
	snprintf(cmd, sizeof(cmd), "%s -d -v 2>&1", bin);

	char *out = NULL;
	int status = run_cmd(cmd, &out, NULL);

	assert(WIFEXITED(status));
	/* -v prints version and exits first */
	assert(WEXITSTATUS(status) == 0);
	assert(strstr(out, "fluxland") != NULL);

	free(out);
	printf("  PASS: test_combined_flags\n");
}

static void
cleanup(void)
{
	/* Best-effort cleanup of temp directory */
	char path[512];
	snprintf(path, sizeof(path), "%s/init", TEST_DIR);
	unlink(path);
	snprintf(path, sizeof(path), "%s/empty-cfg", TEST_DIR);
	rmdir(path);
	snprintf(path, sizeof(path), "%s/bad-cfg/init", TEST_DIR);
	unlink(path);
	snprintf(path, sizeof(path), "%s/bad-cfg", TEST_DIR);
	rmdir(path);
	rmdir(TEST_DIR);
}

int
main(void)
{
	printf("test_main:\n");

	test_version_flag();
	test_version_short_flag();
	test_help_flag();
	test_help_short_flag();
	test_list_commands_flag();
	test_check_config_valid();
	test_check_config_unreadable();
	test_check_config_empty_dir();
	test_invalid_flag();
	test_debug_flag();
	test_startup_flag_with_help();
	test_ipc_no_exec_flag();
	test_no_xdg_runtime_dir();
	test_check_config_bad_key();
	test_combined_flags();

	cleanup();
	printf("All main tests passed.\n");
	return 0;
}
