/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * server.c - Core compositor initialization and teardown
 */

#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <wlr/render/allocator.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>

#include "server.h"
#include "atspi_bridge.h"
#include "screen_cast.h"
#include "config.h"
#include "decoration.h"
#include "keyboard.h"
#include "foreign_toplevel.h"
#include "ipc.h"
#include "keybind.h"
#include "layer_shell.h"
#include "menu.h"
#include "output.h"
#include "input.h"
#include "rules.h"
#include "idle.h"
#include "output_management.h"
#include "protocols.h"
#include "gamma_control.h"
#include "presentation.h"
#include "screencopy.h"
#include "session_lock.h"
#include "slit.h"
#include "wallpaper.h"
#include "style.h"
#ifdef WM_HAS_SYSTRAY
#include "systray.h"
#endif
#include "drm_lease.h"
#include "drm_syncobj.h"
#include "text_input.h"
#include "toolbar.h"
#include "transient_seat.h"
#include "view.h"
#include "viewporter.h"
#include "workspace.h"
#include "tablet.h"
#include "xwayland.h"

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
	wlr_log(WLR_INFO, "%s", "SIGHUP received, reloading configuration");
	wm_server_reconfigure(server);
	return 0;
}

static int
handle_sigchld(int signal_number, void *data)
{
	(void)data;
	/*
	 * Do NOT reap children with waitpid(-1) here. wlroots manages
	 * its own children (Xwayland) and needs to waitpid() them.
	 * A blanket waitpid(-1) races with wlroots, causing ECHILD.
	 *
	 * Launched apps use double-fork so their immediate children
	 * exit instantly and are reaped inline. The actual processes
	 * run as grandchildren adopted by init.
	 */
	return 0;
}

void
wm_server_reconfigure(struct wm_server *server)
{
	/* 1. Reload config file */
	if (server->config) {
		config_reload(server->config);
		server->focus_policy = server->config->focus_policy;
		server->show_position = server->config->show_window_position;
	}

	/* Broadcast config_reloaded event to IPC subscribers */
	wm_ipc_broadcast_event(&server->ipc,
		WM_IPC_EVENT_CONFIG_RELOADED,
		"{\"event\":\"config_reloaded\"}");

	/* 2. Reload keymodes and keybindings */
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
	if (!server->current_keymode)
		return;

	/* 3. Reload mouse bindings */
	mousebind_destroy_list(&server->mousebindings);
	wl_list_init(&server->mousebindings);
	if (server->config && server->config->keys_file) {
		mousebind_load(&server->mousebindings,
			server->config->keys_file,
			server->config->mouse_button_map);
	}

	/* 4. Reload style */
	if (server->style && server->config && server->config->style_file) {
		style_load(server->style, server->config->style_file);
		if (server->config->style_overlay) {
			style_load_overlay(server->style,
				server->config->style_overlay);
		}

		/* Broadcast style_changed event to IPC subscribers */
		wm_ipc_broadcast_event(&server->ipc,
			WM_IPC_EVENT_STYLE_CHANGED,
			"{\"event\":\"style_changed\"}");
	}

	/* 5. Update workspace names from config */
	if (server->config) {
		struct wm_workspace *ws;
		wl_list_for_each(ws, &server->workspaces, link) {
			const char *name = NULL;
			if (ws->index < server->config->workspace_name_count &&
			    server->config->workspace_names[ws->index]) {
				name = server->config->workspace_names[ws->index];
			}
			if (name && (!ws->name || strcmp(ws->name, name) != 0)) {
				free(ws->name);
				ws->name = strdup(name);
			}
		}
	}

	/* 5b. Handle workspace mode change */
	if (server->config) {
		bool per_output =
			server->config->workspace_mode == WM_WORKSPACE_PER_OUTPUT;
		struct wm_workspace *ws;
		if (per_output) {
			/* Enable all workspace trees */
			wl_list_for_each(ws, &server->workspaces, link) {
				wlr_scene_node_set_enabled(
					&ws->tree->node, true);
			}
		} else {
			/* Disable all except current */
			wl_list_for_each(ws, &server->workspaces, link) {
				wlr_scene_node_set_enabled(
					&ws->tree->node,
					ws == server->current_workspace);
			}
		}
	}

	/* 6. Reload menus (after workspace names so submenus reflect changes) */
	wm_menu_hide_all(server);
	if (server->root_menu) {
		wm_menu_destroy(server->root_menu);
		server->root_menu = NULL;
	}
	if (server->window_menu) {
		wm_menu_destroy(server->window_menu);
		server->window_menu = NULL;
	}
	if (server->config && server->config->menu_file) {
		server->root_menu = wm_menu_load(server,
			server->config->menu_file);
	}
	server->window_menu = wm_menu_create_window_menu(server);

	/* 7. Reload rules */
	wm_rules_finish(&server->rules);
	wm_rules_init(&server->rules);
	if (server->config && server->config->apps_file) {
		wm_rules_load(&server->rules,
			server->config->apps_file);
	}

	/* 8. Reapply XKB keyboard layout from config */
	wm_keyboard_apply_config(server);

	/* 9. Refresh toolbar with new config and style */
	if (server->toolbar) {
		wm_toolbar_relayout(server->toolbar);
	}

	/* 10. Refresh slit with new config and style */
	if (server->slit) {
		wm_slit_relayout(server->slit);
	}

	/* 10b. Reload wallpapers from updated config */
	if (server->wallpaper) {
		wm_wallpaper_reload(server->wallpaper);
	} else {
		/* Maybe wallpaper was newly configured */
		server->wallpaper = wm_wallpaper_create(server);
	}

	/* 11. Refresh all view decorations with new style */
	if (server->style) {
		struct wm_view *view;
		wl_list_for_each(view, &server->views, link) {
			if (view->decoration) {
				wm_decoration_update(view->decoration,
					server->style);
			}
		}
	}

	wlr_log(WLR_INFO, "%s", "configuration reloaded");
}

bool
wm_server_init(struct wm_server *server)
{
	server->wl_display = wl_display_create();
	if (!server->wl_display) {
		wlr_log(WLR_ERROR, "%s", "cannot create wayland display");
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
	server->sigchld_source = wl_event_loop_add_signal(
		server->wl_event_loop, SIGCHLD, handle_sigchld, server);
	struct sigaction sa_pipe = {0};
	sa_pipe.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sa_pipe, NULL);

	/*
	 * The backend abstracts the underlying input and output hardware.
	 * Autocreate selects the appropriate backend based on the
	 * environment (DRM/KMS on TTY, Wayland or X11 when nested).
	 */
	server->backend = wlr_backend_autocreate(
		server->wl_event_loop, &server->session);
	if (!server->backend) {
		wlr_log(WLR_ERROR, "%s", "unable to create wlroots backend");
		return false;
	}

	/* Create the renderer (GLES2, Vulkan, or Pixman) */
	server->renderer = wlr_renderer_autocreate(server->backend);
	if (!server->renderer) {
		wlr_log(WLR_ERROR, "%s", "unable to create renderer");
		return false;
	}
	wlr_renderer_init_wl_shm(server->renderer, server->wl_display);

	/* Create the allocator (bridge between renderer and backend) */
	server->allocator = wlr_allocator_autocreate(
		server->backend, server->renderer);
	if (!server->allocator) {
		wlr_log(WLR_ERROR, "%s", "unable to create allocator");
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
		wlr_log(WLR_ERROR, "%s", "unable to create compositor");
		return false;
	}
	wlr_subcompositor_create(server->wl_display);

	/* Data device manager handles clipboard */
	wlr_data_device_manager_create(server->wl_display);

	/* Data control for clipboard managers (cliphist, wl-clipboard) */
	server->data_control_mgr = wlr_data_control_manager_v1_create(
		server->wl_display);

	/*
	 * Create scene graph. This manages rendering, damage tracking,
	 * and surface-to-output mapping automatically.
	 */
	server->scene = wlr_scene_create();
	if (!server->scene) {
		wlr_log(WLR_ERROR, "%s", "unable to create scene");
		return false;
	}

	/* Scene trees for z-ordering (bottom to top) */
	server->layer_background = wlr_scene_tree_create(&server->scene->tree);
	server->layer_bottom = wlr_scene_tree_create(&server->scene->tree);
	server->view_tree = wlr_scene_tree_create(&server->scene->tree);
	/* Layer sub-trees within view_tree (bottom to top) */
	server->view_layer_desktop = wlr_scene_tree_create(server->view_tree);
	server->view_layer_below = wlr_scene_tree_create(server->view_tree);
	server->view_layer_normal = wlr_scene_tree_create(server->view_tree);
	server->view_layer_above = wlr_scene_tree_create(server->view_tree);
	server->xdg_popup_tree = wlr_scene_tree_create(&server->scene->tree);
	server->layer_top = wlr_scene_tree_create(&server->scene->tree);
	server->layer_overlay = wlr_scene_tree_create(&server->scene->tree);

	/* Output layout and scene integration */
	wm_output_init(server);

	/* XDG shell for application windows */
	server->xdg_shell = wlr_xdg_shell_create(server->wl_display, 6);
	if (!server->xdg_shell) {
		wlr_log(WLR_ERROR, "%s", "unable to create xdg shell");
		return false;
	}
	/* Wire up XDG toplevel/popup listeners (view lifecycle) */
	wm_view_init(server);

	/* xdg-decoration: negotiate server-side decorations */
	wm_xdg_decoration_init(server);

	/* Layer shell: panels, wallpapers, lock screens, notifications */
	wm_layer_shell_init(server);

	/* Session lock: ext-session-lock-v1 for screen lockers */
	wm_session_lock_init(server);

	/* Idle notification and inhibit protocols */
	wm_idle_init(server);

	/* Output management protocol (wlr-randr, kanshi) */
	wm_output_management_init(server);

	/* Screencopy and DMA-BUF export for screenshot/recording tools */
	wm_screencopy_init(server);

	/* Gamma control for color temperature tools (gammastep, wlsunset) */
	wm_gamma_control_init(server);

	/* Fractional scale for HiDPI displays */
	wm_fractional_scale_init(server);

	/* Foreign toplevel management for external taskbars (waybar) */
	wm_foreign_toplevel_init(server);

	/* Viewporter and single-pixel-buffer for client buffer flexibility */
	wm_viewporter_init(server);

	/* Presentation time for frame timing feedback */
	wm_presentation_init(server);

	/* Seat for input (keyboard/pointer) */
	server->seat = wlr_seat_create(server->wl_display, "seat0");
	if (!server->seat) {
		wlr_log(WLR_ERROR, "%s", "unable to create seat");
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

	/* Primary selection, pointer constraints, relative pointer */
	wm_protocols_init(server);

	/* Text input / input method relay for IME support */
	wm_text_input_init(server);

	/* Tablet input (tablet-v2 protocol for stylus/pad devices) */
	wm_tablet_init(server);

	/* DRM lease (wp-drm-lease-v1 for VR headsets) */
	wm_drm_lease_init(server);

	/* Transient seat (ext-transient-seat-v1 for remote desktop) */
	wm_transient_seat_init(server);

	/* Explicit sync (linux-drm-syncobj-v1) */
	wm_drm_syncobj_init(server);

	/* Load configuration */
	server->config = config_create();
	if (server->config) {
		config_load(server->config, NULL);
		server->focus_policy = server->config->focus_policy;
		server->show_position = server->config->show_window_position;
		wlr_log(WLR_INFO, "config dir: %s",
			server->config->config_dir ?
			server->config->config_dir : "(none)");
	}

	/* Load style/theme */
	server->style = style_create();
	if (server->style && server->config && server->config->style_file) {
		if (style_load(server->style,
				server->config->style_file) == 0) {
			wlr_log(WLR_INFO, "loaded style from %s",
				server->config->style_file);
		}
		if (server->config->style_overlay) {
			style_load_overlay(server->style,
				server->config->style_overlay);
		}
	}

	/* Load keybindings into keymode system */
	wl_list_init(&server->keybindings);
	wl_list_init(&server->keymodes);
	server->current_keymode = strdup("default");
	if (!server->current_keymode)
		return false;
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

	/* Load mouse bindings from same keys file */
	wl_list_init(&server->mousebindings);
	memset(&server->mouse_state, 0, sizeof(server->mouse_state));
	if (server->config && server->config->keys_file) {
		mousebind_load(&server->mousebindings,
			server->config->keys_file,
			server->config->mouse_button_map);
	}

	/* Initialize workspaces */
	int ws_count = server->config ?
		server->config->workspace_count : WM_DEFAULT_WORKSPACE_COUNT;
	wm_workspaces_init(server, ws_count);

	/* Initialize keyboard focus navigation (accessibility) */
	wm_focus_nav_init(&server->focus_nav);

	/* Initialize AT-SPI accessibility bridge (NULL when disabled) */
	server->atspi_bridge = wm_atspi_bridge_create(server);

	/* Initialize PipeWire screen recording (NULL if not available) */
	server->screen_cast = wm_screen_cast_init(server);

	/* Create toolbar (after workspaces so it can display them) */
	server->toolbar = wm_toolbar_create(server);

	/* Create per-workspace wallpapers (if configured) */
	server->wallpaper = wm_wallpaper_create(server);

	/* Create slit (dockapp container) */
	server->slit = wm_slit_create(server);

#ifdef WM_HAS_SYSTRAY
	/* Create system tray (StatusNotifierItem D-Bus watcher) */
	server->systray = wm_systray_create(server);
#endif

	/* Load per-window rules (Fluxbox apps file) */
	wm_rules_init(&server->rules);
	if (server->config && server->config->apps_file) {
		if (wm_rules_load(&server->rules,
				server->config->apps_file)) {
			wlr_log(WLR_INFO, "loaded window rules from %s",
				server->config->apps_file);
		}
	}

	/* Load menus */
	server->root_menu = NULL;
	server->window_menu = NULL;
	if (server->config && server->config->menu_file) {
		server->root_menu = wm_menu_load(server,
			server->config->menu_file);
		if (server->root_menu) {
			wlr_log(WLR_INFO, "loaded root menu from %s",
				server->config->menu_file);
		}
	}
	server->window_menu = wm_menu_create_window_menu(server);

	/* IPC server */
	wm_ipc_init(&server->ipc, server);

	/* XWayland: X11 application support (compile-time optional) */
	wm_xwayland_init(server);

	return true;
}

bool
wm_server_start(struct wm_server *server)
{
	/* Add a Unix socket to the Wayland display */
	server->socket = wl_display_add_socket_auto(server->wl_display);
	if (!server->socket) {
		wlr_log(WLR_ERROR, "%s", "unable to open wayland socket");
		return false;
	}

	/* Start the backend (enumerate outputs, become DRM master, etc.) */
	if (!wlr_backend_start(server->backend)) {
		wlr_log(WLR_ERROR, "%s", "unable to start wlroots backend");
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
	wm_xwayland_finish(server);
	wl_display_destroy_clients(server->wl_display);

#ifdef WM_HAS_SYSTRAY
	wm_systray_destroy(server->systray);
#endif
	wm_screen_cast_destroy(server->screen_cast);
	wm_atspi_bridge_destroy(server->atspi_bridge);
	wm_slit_destroy(server->slit);
	wm_wallpaper_destroy(server->wallpaper);
	wm_toolbar_destroy(server->toolbar);
	wm_ipc_destroy(&server->ipc);
	wm_menu_destroy(server->root_menu);
	wm_menu_destroy(server->window_menu);
	wm_rules_finish(&server->rules);

	wm_drm_syncobj_finish(server);
	wm_transient_seat_finish(server);
	wm_drm_lease_finish(server);
	wm_tablet_finish(server);
	wm_text_input_finish(server);
	wm_protocols_finish(server);
	wm_output_management_finish(server);
	wm_idle_finish(server);
	wm_session_lock_finish(server);
	wm_layer_shell_finish(server);
	wm_workspaces_finish(server);

	keybind_destroy_all(&server->keymodes);
	keybind_destroy_list(&server->keybindings);
	mousebind_destroy_list(&server->mousebindings);
	free(server->current_keymode);
	if (server->chain_state.timeout)
		wl_event_source_remove(server->chain_state.timeout);
	if (server->auto_raise_timer)
		wl_event_source_remove(server->auto_raise_timer);
	style_destroy(server->style);
	config_destroy(server->config);

	wm_input_finish(server);

	wlr_xcursor_manager_destroy(server->cursor_mgr);
	wlr_cursor_destroy(server->cursor);

	wm_output_finish(server);

	wl_event_source_remove(server->sigint_source);
	wl_event_source_remove(server->sigterm_source);
	wl_event_source_remove(server->sighup_source);
	wl_event_source_remove(server->sigchld_source);

	wlr_scene_node_destroy(&server->scene->tree.node);
	wlr_backend_destroy(server->backend);
	wlr_allocator_destroy(server->allocator);
	wlr_renderer_destroy(server->renderer);

	wl_display_destroy(server->wl_display);
}
