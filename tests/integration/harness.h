/*
 * harness.h - Integration test harness for fluxland
 *
 * Provides helpers to fork a headless compositor, connect as a
 * Wayland client, create xdg_shell surfaces, and tear everything down.
 */

#ifndef FLUXLAND_TEST_HARNESS_H
#define FLUXLAND_TEST_HARNESS_H

#include <stdbool.h>
#include <sys/types.h>
#include <wayland-client.h>

/* Generated xdg-shell client protocol */
#include "xdg-shell-client-protocol.h"

/*
 * State for a single test client connection.
 */
struct test_client {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct wl_shm *shm;
	struct xdg_wm_base *xdg_wm_base;
	struct wl_seat *seat;

	/* Set by xdg_wm_base ping handler */
	bool got_ping;
};

/*
 * State for a single xdg toplevel surface.
 */
struct test_toplevel {
	struct wl_surface *surface;
	struct xdg_surface *xdg_surface;
	struct xdg_toplevel *xdg_toplevel;

	/* Updated by configure events */
	bool configured;
	int configure_width;
	int configure_height;
	uint32_t configure_serial;
	bool maximized;
	bool fullscreen;
	bool activated;
};

/*
 * Set up the temporary directories and minimal config files needed
 * to run the compositor in test mode.  Must be called before
 * harness_start_compositor().
 */
void harness_setup(void);

/*
 * Fork the compositor with WLR_BACKENDS=headless WLR_RENDERER=pixman.
 * Blocks until the Wayland socket appears or a timeout expires.
 *
 * Returns the compositor PID on success, or -1 on failure.
 * The socket name (e.g. "wayland-0") is written to *out_socket
 * (caller must not free — points to static storage).
 *
 * Returns 0 and sets *out_socket to NULL if the headless backend
 * is unavailable (caller should skip the test with exit code 77).
 */
pid_t harness_start_compositor(const char **out_socket);

/*
 * Connect a test client to the compositor on the given socket name.
 * Performs a roundtrip and binds core globals (wl_compositor,
 * xdg_wm_base, wl_seat, wl_shm).
 *
 * Returns 0 on success, -1 on failure.
 */
int harness_connect(struct test_client *client, const char *socket);

/*
 * Wrapper around wl_display_roundtrip.
 * Returns 0 on success, -1 on failure.
 */
int harness_roundtrip(struct test_client *client);

/*
 * Dispatch events without blocking — flushes and reads available events.
 * Returns 0 on success, -1 on failure.
 */
int harness_dispatch(struct test_client *client);

/*
 * Create an xdg_toplevel surface of the given size.
 * Attaches a minimal wl_shm buffer and commits.  Does NOT roundtrip;
 * the caller should call harness_roundtrip() to receive the initial
 * configure event.
 *
 * Returns 0 on success, -1 on failure.
 */
int harness_create_toplevel(struct test_client *client,
			    struct test_toplevel *tl,
			    int width, int height);

/*
 * Destroy a toplevel and its associated surfaces.
 */
void harness_destroy_toplevel(struct test_client *client,
			      struct test_toplevel *tl);

/*
 * Disconnect a test client and release all Wayland resources.
 */
void harness_disconnect(struct test_client *client);

/*
 * Send SIGTERM to the compositor and wait for it to exit.
 */
void harness_stop_compositor(pid_t pid);

/*
 * Clean up temporary directories and socket files.
 */
void harness_cleanup(void);

/*
 * Return the XDG_RUNTIME_DIR used by the test harness.
 */
const char *harness_get_runtime_dir(void);

#endif /* FLUXLAND_TEST_HARNESS_H */
