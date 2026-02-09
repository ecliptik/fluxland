/*
 * wm-wayland - A Fluxbox-inspired Wayland compositor
 * server.c - Core compositor initialization and teardown
 */

#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/render/allocator.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>

#include "server.h"
#include "config.h"
#include "keybind.h"
#include "output.h"
#include "input.h"
#include "view.h"
#include "workspace.h"

#define WM_WLR_COMPOSITOR_VERSION 6

static int
handle_signal(int signal_number, void *data)
{
	struct wl_display *display = data;
	wl_display_terminate(display);
	return 0;
}

static int
handle_sighup(int signal_number, void *data)
{
	struct wm_server *server = data;

	wlr_log(WLR_INFO, "SIGHUP received, reloading configuration");

	if (server->config) {
		config_reload(server->config);
		server->focus_policy = server->config->focus_policy;
	}

	/* Reload keymodes and keybindings */
	keybind_destroy_all(&server->keymodes);
	wl_list_init(&server->keymodes);
	keybind_destroy_list(&server->keybindings);
	wl_list_init(&server->keybindings);
	if (server->config && server->config->keys_file) {
		keybind_load(&server->keymodes,
			server->config->keys_file);
	}
	/* Reset chain and keymode */
	server->chain_state.in_chain = false;
	server->chain_state.current_level = NULL;
	if (server->chain_state.timeout) {
		wl_event_source_remove(server->chain_state.timeout);
		server->chain_state.timeout = NULL;
	}
	free(server->current_keymode);
	server->current_keymode = strdup("default");

	return 0;
}

bool
wm_server_init(struct wm_server *server)
{
	server->wl_display = wl_display_create();
	if (!server->wl_display) {
		wlr_log(WLR_ERROR, "cannot create wayland display");
		return false;
	}

	server->wl_event_loop = wl_display_get_event_loop(server->wl_display);

	/* Catch SIGINT and SIGTERM for clean shutdown */
	server->sigint_source = wl_event_loop_add_signal(
		server->wl_event_loop, SIGINT, handle_signal,
		server->wl_display);
	server->sigterm_source = wl_event_loop_add_signal(
		server->wl_event_loop, SIGTERM, handle_signal,
		server->wl_display);
	server->sighup_source = wl_event_loop_add_signal(
		server->wl_event_loop, SIGHUP, handle_sighup, server);
	signal(SIGPIPE, SIG_IGN);

	/*
	 * The backend abstracts the underlying input and output hardware.
	 * Autocreate selects the appropriate backend based on the
	 * environment (DRM/KMS on TTY, Wayland or X11 when nested).
	 */
	server->backend = wlr_backend_autocreate(
		server->wl_event_loop, &server->session);
	if (!server->backend) {
		wlr_log(WLR_ERROR, "unable to create wlroots backend");
		return false;
	}

	/* Create the renderer (GLES2, Vulkan, or Pixman) */
	server->renderer = wlr_renderer_autocreate(server->backend);
	if (!server->renderer) {
		wlr_log(WLR_ERROR, "unable to create renderer");
		return false;
	}
	wlr_renderer_init_wl_shm(server->renderer, server->wl_display);

	/* Create the allocator (bridge between renderer and backend) */
	server->allocator = wlr_allocator_autocreate(
		server->backend, server->renderer);
	if (!server->allocator) {
		wlr_log(WLR_ERROR, "unable to create allocator");
		return false;
	}

	/*
	 * Create the wlroots compositor and subcompositor.
	 * These are needed for clients to allocate surfaces.
	 */
	server->compositor = wlr_compositor_create(
		server->wl_display, WM_WLR_COMPOSITOR_VERSION,
		server->renderer);
	if (!server->compositor) {
		wlr_log(WLR_ERROR, "unable to create compositor");
		return false;
	}
	wlr_subcompositor_create(server->wl_display);

	/* Data device manager handles clipboard */
	wlr_data_device_manager_create(server->wl_display);

	/*
	 * Create scene graph. This manages rendering, damage tracking,
	 * and surface-to-output mapping automatically.
	 */
	server->scene = wlr_scene_create();
	if (!server->scene) {
		wlr_log(WLR_ERROR, "unable to create scene");
		return false;
	}

	/* Scene trees for z-ordering (bottom to top) */
	server->view_tree = wlr_scene_tree_create(&server->scene->tree);
	server->xdg_popup_tree = wlr_scene_tree_create(&server->scene->tree);

	/* Output layout and scene integration */
	wm_output_init(server);

	/* XDG shell for application windows */
	server->xdg_shell = wlr_xdg_shell_create(server->wl_display, 6);
	if (!server->xdg_shell) {
		wlr_log(WLR_ERROR, "unable to create xdg shell");
		return false;
	}
	/* Wire up XDG toplevel/popup listeners (view lifecycle) */
	wm_view_init(server);

	/* Seat for input (keyboard/pointer) */
	server->seat = wlr_seat_create(server->wl_display, "seat0");
	if (!server->seat) {
		wlr_log(WLR_ERROR, "unable to create seat");
		return false;
	}

	/* Cursor */
	server->cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(server->cursor,
		server->output_layout);
	server->cursor_mgr = wlr_xcursor_manager_create(
		WM_XCURSOR_DEFAULT, WM_XCURSOR_SIZE);

	/* Init lists */
	wl_list_init(&server->views);
	server->cursor_mode = WM_CURSOR_PASSTHROUGH;
	server->grabbed_view = NULL;

	/* Input handling (keyboard, pointer, seat requests) */
	wm_input_init(server);

	/* Load configuration */
	server->config = config_create();
	if (server->config) {
		config_load(server->config, NULL);
		server->focus_policy = server->config->focus_policy;
		wlr_log(WLR_INFO, "config dir: %s",
			server->config->config_dir ?
			server->config->config_dir : "(none)");
	}

	/* Load keybindings into keymode system */
	wl_list_init(&server->keybindings);
	wl_list_init(&server->keymodes);
	server->current_keymode = strdup("default");
	memset(&server->chain_state, 0, sizeof(server->chain_state));
	if (server->config && server->config->keys_file) {
		if (keybind_load(&server->keymodes,
				server->config->keys_file) == 0) {
			wlr_log(WLR_INFO, "loaded keybindings from %s",
				server->config->keys_file);
		} else {
			wlr_log(WLR_INFO, "no keys file at %s, using defaults",
				server->config->keys_file);
		}
	}

	/* Initialize workspaces */
	int ws_count = server->config ?
		server->config->workspace_count : WM_DEFAULT_WORKSPACE_COUNT;
	wm_workspaces_init(server, ws_count);

	return true;
}

bool
wm_server_start(struct wm_server *server)
{
	/* Add a Unix socket to the Wayland display */
	server->socket = wl_display_add_socket_auto(server->wl_display);
	if (!server->socket) {
		wlr_log(WLR_ERROR, "unable to open wayland socket");
		return false;
	}

	/* Start the backend (enumerate outputs, become DRM master, etc.) */
	if (!wlr_backend_start(server->backend)) {
		wlr_log(WLR_ERROR, "unable to start wlroots backend");
		return false;
	}

	return true;
}

void
wm_server_run(struct wm_server *server)
{
	wl_display_run(server->wl_display);
}

void
wm_server_destroy(struct wm_server *server)
{
	wl_display_destroy_clients(server->wl_display);

	wm_workspaces_finish(server);

	keybind_destroy_all(&server->keymodes);
	keybind_destroy_list(&server->keybindings);
	free(server->current_keymode);
	if (server->chain_state.timeout)
		wl_event_source_remove(server->chain_state.timeout);
	config_destroy(server->config);

	wm_input_finish(server);

	wlr_xcursor_manager_destroy(server->cursor_mgr);
	wlr_cursor_destroy(server->cursor);

	wm_output_finish(server);

	wl_event_source_remove(server->sigint_source);
	wl_event_source_remove(server->sigterm_source);
	wl_event_source_remove(server->sighup_source);

	wlr_scene_node_destroy(&server->scene->tree.node);
	wlr_backend_destroy(server->backend);
	wlr_allocator_destroy(server->allocator);
	wlr_renderer_destroy(server->renderer);

	wl_display_destroy(server->wl_display);
}
