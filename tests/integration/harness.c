/*
 * harness.c - Integration test harness for fluxland
 *
 * Forks a headless compositor, manages Wayland client connections,
 * and provides helpers for creating xdg_shell surfaces.
 */

#define _GNU_SOURCE

#include "harness.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define STARTUP_TIMEOUT_US  3000000  /* 3 seconds */
#define POLL_INTERVAL_US    50000    /* 50 ms */

/* ------------------------------------------------------------------ */
/* Path management                                                     */
/* ------------------------------------------------------------------ */

#define PATH_MAX_BUF 1024

static char test_base[PATH_MAX_BUF];
static char test_runtime[PATH_MAX_BUF];
static char test_config_home[PATH_MAX_BUF];
static char test_config_dir[PATH_MAX_BUF];
static char test_init_file[PATH_MAX_BUF];

static void
init_paths(void)
{
	snprintf(test_base, sizeof(test_base),
		 "/tmp/fluxland-test/harness-%d", getpid());
	snprintf(test_runtime, sizeof(test_runtime),
		 "%s/runtime", test_base);
	snprintf(test_config_home, sizeof(test_config_home),
		 "%s/config", test_base);
	snprintf(test_config_dir, sizeof(test_config_dir),
		 "%s/config/fluxland", test_base);
	snprintf(test_init_file, sizeof(test_init_file),
		 "%s/config/fluxland/init", test_base);
}

void
harness_setup(void)
{
	init_paths();

	mkdir("/tmp/fluxland-test", 0755);
	mkdir(test_base, 0755);
	mkdir(test_runtime, 0700);
	mkdir(test_config_home, 0755);
	mkdir(test_config_dir, 0755);

	FILE *f = fopen(test_init_file, "w");
	if (!f) {
		perror("harness_setup: fopen config");
		return;
	}
	fprintf(f, "session.screen0.workspaces: 1\n");
	fclose(f);
}

const char *
harness_get_runtime_dir(void)
{
	return test_runtime;
}

void
harness_cleanup(void)
{
	/* Remove socket files */
	for (int i = 0; i < 10; i++) {
		char path[PATH_MAX_BUF];
		snprintf(path, sizeof(path), "%s/wayland-%d",
			 test_runtime, i);
		unlink(path);
		snprintf(path, sizeof(path), "%s/wayland-%d.lock",
			 test_runtime, i);
		unlink(path);
	}

	/* Remove IPC socket */
	char ipc_path[PATH_MAX_BUF];
	snprintf(ipc_path, sizeof(ipc_path), "%s/fluxland-ipc.sock",
		 test_runtime);
	unlink(ipc_path);

	unlink(test_init_file);
	rmdir(test_config_dir);
	rmdir(test_config_home);
	rmdir(test_runtime);
	rmdir(test_base);
}

/* ------------------------------------------------------------------ */
/* Compositor lifecycle                                                */
/* ------------------------------------------------------------------ */

static const char *
wait_for_socket(int timeout_us)
{
	static char socket_name[256];
	int waited = 0;

	while (waited < timeout_us) {
		for (int i = 0; i < 10; i++) {
			char path[PATH_MAX_BUF];
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

pid_t
harness_start_compositor(const char **out_socket)
{
	/* Find the compositor binary */
	const char *compositor_bin = NULL;
	if (access("/tmp/fluxland-test/wm-build/fluxland", X_OK) == 0)
		compositor_bin = "/tmp/fluxland-test/wm-build/fluxland";
	else if (access("../fluxland", X_OK) == 0)
		compositor_bin = "../fluxland";
	else
		compositor_bin = "fluxland";

	pid_t child = fork();
	if (child < 0) {
		perror("harness: fork");
		return -1;
	}

	if (child == 0) {
		/* Child: exec compositor with headless backend */
		setenv("WLR_BACKENDS", "headless", 1);
		setenv("WLR_RENDERER", "pixman", 1);
		setenv("XDG_RUNTIME_DIR", test_runtime, 1);
		setenv("XDG_CONFIG_HOME", test_config_home, 1);
		setenv("HOME", test_base, 1);

		freopen("/dev/null", "w", stdout);
		freopen("/dev/null", "w", stderr);

		execl(compositor_bin, "fluxland", (char *)NULL);
		_exit(127);
	}

	/* Parent: set runtime dir and wait for socket */
	setenv("XDG_RUNTIME_DIR", test_runtime, 1);

	const char *socket = wait_for_socket(STARTUP_TIMEOUT_US);
	if (!socket) {
		int status;
		pid_t w = waitpid(child, &status, WNOHANG);
		if (w > 0) {
			fprintf(stderr,
				"harness: compositor exited early (status %d)\n",
				WEXITSTATUS(status));
			*out_socket = NULL;
			return 0; /* signal caller to skip */
		}
		/* Still running but no socket */
		kill(child, SIGTERM);
		waitpid(child, NULL, 0);
		*out_socket = NULL;
		return 0;
	}

	*out_socket = socket;
	return child;
}

void
harness_stop_compositor(pid_t pid)
{
	if (pid <= 0)
		return;
	kill(pid, SIGTERM);
	waitpid(pid, NULL, 0);
}

/* ------------------------------------------------------------------ */
/* Wayland client connection                                           */
/* ------------------------------------------------------------------ */

/* xdg_wm_base listener: respond to pings */
static void
xdg_wm_base_ping(void *data, struct xdg_wm_base *wm_base, uint32_t serial)
{
	struct test_client *client = data;
	xdg_wm_base_pong(wm_base, serial);
	client->got_ping = true;
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
	.ping = xdg_wm_base_ping,
};

/* Registry listener: bind globals */
static void
registry_global(void *data, struct wl_registry *registry, uint32_t name,
		const char *interface, uint32_t version)
{
	struct test_client *client = data;

	if (strcmp(interface, "wl_compositor") == 0) {
		client->compositor = wl_registry_bind(
			registry, name, &wl_compositor_interface,
			version < 6 ? version : 6);
	} else if (strcmp(interface, "xdg_wm_base") == 0) {
		client->xdg_wm_base = wl_registry_bind(
			registry, name, &xdg_wm_base_interface,
			version < 6 ? version : 6);
		xdg_wm_base_add_listener(client->xdg_wm_base,
					 &xdg_wm_base_listener, client);
	} else if (strcmp(interface, "wl_seat") == 0) {
		client->seat = wl_registry_bind(
			registry, name, &wl_seat_interface,
			version < 9 ? version : 9);
	} else if (strcmp(interface, "wl_shm") == 0) {
		client->shm = wl_registry_bind(
			registry, name, &wl_shm_interface,
			version < 1 ? version : 1);
	}
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

int
harness_connect(struct test_client *client, const char *socket)
{
	memset(client, 0, sizeof(*client));

	client->display = wl_display_connect(socket);
	if (!client->display) {
		fprintf(stderr, "harness: failed to connect to %s: %s\n",
			socket, strerror(errno));
		return -1;
	}

	client->registry = wl_display_get_registry(client->display);
	wl_registry_add_listener(client->registry, &registry_listener, client);
	wl_display_roundtrip(client->display);

	if (!client->compositor || !client->xdg_wm_base) {
		fprintf(stderr,
			"harness: missing required globals "
			"(compositor=%p xdg_wm_base=%p)\n",
			(void *)client->compositor,
			(void *)client->xdg_wm_base);
		harness_disconnect(client);
		return -1;
	}

	return 0;
}

int
harness_roundtrip(struct test_client *client)
{
	if (wl_display_roundtrip(client->display) < 0) {
		fprintf(stderr, "harness: roundtrip failed: %s\n",
			strerror(errno));
		return -1;
	}
	return 0;
}

int
harness_dispatch(struct test_client *client)
{
	if (wl_display_flush(client->display) < 0)
		return -1;
	return wl_display_dispatch_pending(client->display) < 0 ? -1 : 0;
}

void
harness_disconnect(struct test_client *client)
{
	if (!client)
		return;
	if (client->seat)
		wl_seat_destroy(client->seat);
	if (client->shm)
		wl_shm_destroy(client->shm);
	if (client->xdg_wm_base)
		xdg_wm_base_destroy(client->xdg_wm_base);
	if (client->compositor)
		wl_compositor_destroy(client->compositor);
	if (client->registry)
		wl_registry_destroy(client->registry);
	if (client->display)
		wl_display_disconnect(client->display);
	memset(client, 0, sizeof(*client));
}

/* ------------------------------------------------------------------ */
/* XDG toplevel creation                                               */
/* ------------------------------------------------------------------ */

/* xdg_toplevel configure listener */
static void
toplevel_configure(void *data, struct xdg_toplevel *toplevel,
		   int32_t width, int32_t height, struct wl_array *states)
{
	struct test_toplevel *tl = data;

	tl->configure_width = width;
	tl->configure_height = height;
	tl->maximized = false;
	tl->fullscreen = false;
	tl->activated = false;

	uint32_t *state;
	wl_array_for_each(state, states) {
		switch (*state) {
		case XDG_TOPLEVEL_STATE_MAXIMIZED:
			tl->maximized = true;
			break;
		case XDG_TOPLEVEL_STATE_FULLSCREEN:
			tl->fullscreen = true;
			break;
		case XDG_TOPLEVEL_STATE_ACTIVATED:
			tl->activated = true;
			break;
		}
	}
}

static void
toplevel_close(void *data, struct xdg_toplevel *toplevel)
{
	(void)data;
	(void)toplevel;
}

static void
toplevel_configure_bounds(void *data, struct xdg_toplevel *toplevel,
			  int32_t width, int32_t height)
{
	(void)data;
	(void)toplevel;
	(void)width;
	(void)height;
}

static void
toplevel_wm_capabilities(void *data, struct xdg_toplevel *toplevel,
			 struct wl_array *caps)
{
	(void)data;
	(void)toplevel;
	(void)caps;
}

static const struct xdg_toplevel_listener toplevel_listener = {
	.configure = toplevel_configure,
	.close = toplevel_close,
	.configure_bounds = toplevel_configure_bounds,
	.wm_capabilities = toplevel_wm_capabilities,
};

/* xdg_surface configure listener */
static void
xdg_surface_configure(void *data, struct xdg_surface *xdg_surface,
		       uint32_t serial)
{
	struct test_toplevel *tl = data;
	tl->configured = true;
	tl->configure_serial = serial;
	xdg_surface_ack_configure(xdg_surface, serial);
}

static const struct xdg_surface_listener xdg_surface_listener = {
	.configure = xdg_surface_configure,
};

/*
 * Create a minimal shared-memory buffer for the surface.
 * Uses an anonymous file via memfd_create or shm_open.
 */
static struct wl_buffer *
create_shm_buffer(struct test_client *client, int width, int height)
{
	int stride = width * 4;
	int size = stride * height;

	int fd = -1;
#ifdef __linux__
	fd = memfd_create("test-buffer", MFD_CLOEXEC);
#endif
	if (fd < 0) {
		char name[64];
		snprintf(name, sizeof(name), "/fluxland-test-%d", getpid());
		fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
		shm_unlink(name);
	}
	if (fd < 0)
		return NULL;

	if (ftruncate(fd, size) < 0) {
		close(fd);
		return NULL;
	}

	struct wl_shm_pool *pool = wl_shm_create_pool(client->shm, fd, size);
	close(fd);
	if (!pool)
		return NULL;

	struct wl_buffer *buffer = wl_shm_pool_create_buffer(
		pool, 0, width, height, stride, WL_SHM_FORMAT_XRGB8888);
	wl_shm_pool_destroy(pool);

	return buffer;
}

int
harness_create_toplevel(struct test_client *client,
			struct test_toplevel *tl,
			int width, int height)
{
	memset(tl, 0, sizeof(*tl));

	tl->surface = wl_compositor_create_surface(client->compositor);
	if (!tl->surface)
		return -1;

	tl->xdg_surface = xdg_wm_base_get_xdg_surface(
		client->xdg_wm_base, tl->surface);
	if (!tl->xdg_surface)
		return -1;
	xdg_surface_add_listener(tl->xdg_surface, &xdg_surface_listener, tl);

	tl->xdg_toplevel = xdg_surface_get_toplevel(tl->xdg_surface);
	if (!tl->xdg_toplevel)
		return -1;
	xdg_toplevel_add_listener(tl->xdg_toplevel, &toplevel_listener, tl);

	/* Initial commit to trigger the first configure event */
	wl_surface_commit(tl->surface);

	/* Roundtrip to get the initial configure */
	harness_roundtrip(client);

	/* Now ack the configure and attach a real buffer */
	if (tl->configured && client->shm) {
		struct wl_buffer *buffer =
			create_shm_buffer(client, width, height);
		if (buffer) {
			wl_surface_attach(tl->surface, buffer, 0, 0);
			wl_surface_commit(tl->surface);
		}
	}

	return 0;
}

void
harness_destroy_toplevel(struct test_client *client,
			 struct test_toplevel *tl)
{
	(void)client;

	if (tl->xdg_toplevel)
		xdg_toplevel_destroy(tl->xdg_toplevel);
	if (tl->xdg_surface)
		xdg_surface_destroy(tl->xdg_surface);
	if (tl->surface)
		wl_surface_destroy(tl->surface);
	memset(tl, 0, sizeof(*tl));
}
