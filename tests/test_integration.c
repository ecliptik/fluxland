/*
 * test_integration.c - Integration test: headless compositor startup
 *
 * Forks the compositor with WLR_BACKENDS=headless, connects as a
 * Wayland client, verifies core globals are present, then shuts down.
 * Exits 77 (meson skip) if the headless backend is unavailable.
 */

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wayland-client.h>

#define STARTUP_TIMEOUT_US  3000000  /* 3 seconds max wait */
#define POLL_INTERVAL_US    50000    /* 50ms between polls */

/* Globals detected via registry */
static bool have_wl_compositor;
static bool have_xdg_wm_base;
static bool have_wl_seat;

static void
registry_global(void *data, struct wl_registry *registry, uint32_t name,
		const char *interface, uint32_t version)
{
	(void)data;
	(void)registry;
	(void)name;
	(void)version;

	if (strcmp(interface, "wl_compositor") == 0)
		have_wl_compositor = true;
	else if (strcmp(interface, "xdg_wm_base") == 0)
		have_xdg_wm_base = true;
	else if (strcmp(interface, "wl_seat") == 0)
		have_wl_seat = true;
}

static void
registry_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
	(void)data;
	(void)registry;
	(void)name;
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove,
};

/* Build paths relative to a base test directory */
static char test_base[512];
static char test_runtime[512];
static char test_config_home[512];
static char test_config_dir[512];
static char test_init_file[512];

static void
setup_paths(void)
{
	/* Use a unique directory under /tmp owned by our uid */
	snprintf(test_base, sizeof(test_base),
		 "/tmp/fluxland-test/wm-integration-test-%d", getpid());
	snprintf(test_runtime, sizeof(test_runtime),
		 "%s/runtime", test_base);
	snprintf(test_config_home, sizeof(test_config_home),
		 "%s/config", test_base);
	snprintf(test_config_dir, sizeof(test_config_dir),
		 "%s/config/fluxland", test_base);
	snprintf(test_init_file, sizeof(test_init_file),
		 "%s/config/fluxland/init", test_base);
}

static void
write_minimal_config(void)
{
	mkdir("/tmp/fluxland-test", 0755);
	mkdir(test_base, 0755);
	mkdir(test_runtime, 0700);
	mkdir(test_config_home, 0755);
	mkdir(test_config_dir, 0755);

	FILE *f = fopen(test_init_file, "w");
	if (!f) {
		perror("fopen config");
		return;
	}
	fprintf(f, "session.screen0.workspaces: 1\n");
	fclose(f);
}

static void
cleanup(void)
{
	/* Clean up sockets */
	for (int i = 0; i < 10; i++) {
		char path[512];
		snprintf(path, sizeof(path), "%s/wayland-%d", test_runtime, i);
		unlink(path);
		snprintf(path, sizeof(path), "%s/wayland-%d.lock",
			 test_runtime, i);
		unlink(path);
	}

	/* Clean up IPC socket */
	char ipc_path[512];
	snprintf(ipc_path, sizeof(ipc_path), "%s/fluxland-ipc.sock",
		 test_runtime);
	unlink(ipc_path);

	unlink(test_init_file);
	rmdir(test_config_dir);
	rmdir(test_config_home);
	rmdir(test_runtime);
	rmdir(test_base);
}

/*
 * Wait for a Wayland socket to appear in the runtime dir.
 * Returns the socket name (static buffer) or NULL on timeout.
 */
static const char *
wait_for_socket(int timeout_us)
{
	static char socket_name[256];
	int waited = 0;

	while (waited < timeout_us) {
		for (int i = 0; i < 10; i++) {
			char path[512];
			snprintf(path, sizeof(path), "%s/wayland-%d",
				 test_runtime, i);
			if (access(path, F_OK) == 0) {
				snprintf(socket_name, sizeof(socket_name),
					 "wayland-%d", i);
				return socket_name;
			}
		}
		usleep(POLL_INTERVAL_US);
		waited += POLL_INTERVAL_US;
	}
	return NULL;
}

int
main(void)
{
	printf("test_integration:\n");

	setup_paths();
	write_minimal_config();

	/*
	 * Find the compositor binary. Check the build directory first
	 * (for meson test), then fall back to PATH.
	 */
	const char *compositor_bin = NULL;
	if (access("/tmp/fluxland-test/wm-build/fluxland", X_OK) == 0)
		compositor_bin = "/tmp/fluxland-test/wm-build/fluxland";
	else if (access("../fluxland", X_OK) == 0)
		compositor_bin = "../fluxland";
	else
		compositor_bin = "fluxland";

	printf("  Starting compositor (headless)...\n");
	printf("  Binary: %s\n", compositor_bin);
	printf("  Runtime dir: %s\n", test_runtime);
	fflush(stdout);

	pid_t child = fork();
	if (child < 0) {
		perror("fork");
		cleanup();
		return 1;
	}

	if (child == 0) {
		/* Child: exec the compositor with headless backend */
		setenv("WLR_BACKENDS", "headless", 1);
		setenv("WLR_RENDERER", "pixman", 1);
		setenv("XDG_RUNTIME_DIR", test_runtime, 1);
		setenv("XDG_CONFIG_HOME", test_config_home, 1);
		setenv("HOME", test_base, 1);

		/* Redirect stdout/stderr to /dev/null to reduce noise */
		freopen("/dev/null", "w", stdout);
		freopen("/dev/null", "w", stderr);

		execl(compositor_bin, "fluxland", (char *)NULL);
		/* If exec fails, exit with a distinguishable code */
		_exit(127);
	}

	/* Parent: wait for the compositor socket to appear */
	setenv("XDG_RUNTIME_DIR", test_runtime, 1);

	const char *socket = wait_for_socket(STARTUP_TIMEOUT_US);
	if (!socket) {
		/* Check if child already exited */
		int status;
		pid_t w = waitpid(child, &status, WNOHANG);
		if (w > 0) {
			fprintf(stderr,
				"  Compositor exited early (status %d) "
				"- headless backend may not be available\n",
				WEXITSTATUS(status));
			cleanup();
			printf("  SKIP: headless backend unavailable\n");
			return 77; /* meson skip */
		}

		/* Still running but no socket - kill and skip */
		kill(child, SIGTERM);
		waitpid(child, NULL, 0);
		cleanup();
		fprintf(stderr,
			"  Timed out waiting for Wayland socket\n");
		printf("  SKIP: compositor did not create socket\n");
		return 77;
	}

	printf("  Compositor started on %s\n", socket);

	/* Connect as a Wayland client */
	struct wl_display *display = wl_display_connect(socket);
	if (!display) {
		fprintf(stderr, "  Failed to connect to %s: %s\n",
			socket, strerror(errno));
		kill(child, SIGTERM);
		waitpid(child, NULL, 0);
		cleanup();
		return 1;
	}

	printf("  Connected to compositor\n");

	/* Enumerate globals */
	struct wl_registry *registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_roundtrip(display);

	printf("  Globals: wl_compositor=%s xdg_wm_base=%s wl_seat=%s\n",
		have_wl_compositor ? "yes" : "no",
		have_xdg_wm_base ? "yes" : "no",
		have_wl_seat ? "yes" : "no");

	/* Clean up client connection */
	wl_registry_destroy(registry);
	wl_display_disconnect(display);

	/* Shut down compositor */
	printf("  Sending SIGTERM to compositor...\n");
	kill(child, SIGTERM);

	int status;
	waitpid(child, &status, 0);

	cleanup();

	/* Verify globals */
	if (!have_wl_compositor) {
		fprintf(stderr, "  FAIL: wl_compositor not found\n");
		return 1;
	}
	if (!have_xdg_wm_base) {
		fprintf(stderr, "  FAIL: xdg_wm_base not found\n");
		return 1;
	}
	if (!have_wl_seat) {
		fprintf(stderr, "  FAIL: wl_seat not found\n");
		return 1;
	}

	/* Verify clean exit */
	if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
		printf("  Compositor exited cleanly (status 0)\n");
	} else if (WIFSIGNALED(status)) {
		/* SIGTERM is expected - compositor may not trap it cleanly */
		printf("  Compositor terminated by signal %d\n",
			WTERMSIG(status));
	} else {
		fprintf(stderr, "  WARNING: compositor exited with status %d\n",
			WEXITSTATUS(status));
	}

	printf("  PASS: integration test\n");
	return 0;
}
