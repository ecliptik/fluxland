/*
 * fluxland - A Fluxbox-inspired Wayland compositor
 * Copyright (c) 2025 fluxland contributors
 * SPDX-License-Identifier: MIT
 * server.h - Core compositor state
 */

#ifndef WM_SERVER_H
#define WM_SERVER_H

#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/box.h>

#include "keybind.h"
#include "mousebind.h"
#include "ipc.h"
#include "rules.h"
#include "idle.h"
#include "output_management.h"
#include "protocols.h"
#include "fractional_scale.h"
#include "gamma_control.h"
#include "screencopy.h"
#include "session_lock.h"
#include "drm_lease.h"
#include "drm_syncobj.h"
#include "focus_nav.h"
#include "text_input.h"
#include "transient_seat.h"
#include "viewporter.h"
#include "xwayland.h"

/* Forward declarations for types defined by other modules */
struct wm_view;
struct wm_workspace;
struct wm_config;
struct wm_style;
struct wm_menu;
struct wm_toolbar;
struct wm_slit;
struct wm_systray;
struct wlr_foreign_toplevel_manager_v1;
struct wlr_primary_selection_v1_device_manager;
struct wlr_pointer_constraints_v1;
struct wlr_pointer_constraint_v1;
struct wlr_relative_pointer_manager_v1;
struct wlr_fractional_scale_manager_v1;
struct wlr_cursor_shape_manager_v1;
struct wlr_viewporter;
struct wlr_single_pixel_buffer_manager_v1;
struct wlr_gamma_control_manager_v1;
struct wlr_data_control_manager_v1;
struct wlr_presentation;
struct wlr_virtual_keyboard_manager_v1;
struct wlr_virtual_pointer_manager_v1;
struct wlr_keyboard_shortcuts_inhibit_manager_v1;
struct wlr_keyboard_shortcuts_inhibitor_v1;
struct wlr_pointer_gestures_v1;
struct wlr_xdg_activation_v1;
struct wlr_tearing_control_manager_v1;
struct wlr_output_power_manager_v1;
struct wlr_tablet_manager_v2;
struct wlr_content_type_manager_v1;
struct wlr_alpha_modifier_v1;
struct wlr_security_context_manager_v1;
struct wlr_linux_dmabuf_v1;
struct wlr_xdg_output_manager_v1;
struct wlr_xdg_foreign_registry;
struct wlr_xdg_foreign_v1;
struct wlr_xdg_foreign_v2;
struct wlr_ext_foreign_toplevel_list_v1;
struct wlr_drm_lease_v1_manager;
struct wlr_transient_seat_manager_v1;
struct wlr_linux_drm_syncobj_manager_v1;

#define WM_XCURSOR_DEFAULT "left_ptr"
#define WM_XCURSOR_SIZE 24

enum wm_cursor_mode {
	WM_CURSOR_PASSTHROUGH = 0,
	WM_CURSOR_MOVE,
	WM_CURSOR_RESIZE,
	WM_CURSOR_TABBING, /* dragging a tab onto another window */
};

struct wm_server {
	struct wl_display *wl_display;
	struct wl_event_loop *wl_event_loop;

	struct wlr_backend *backend;
	struct wlr_session *session;
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wlr_compositor *compositor;

	struct wlr_scene *scene;
	struct wlr_scene_output_layout *scene_layout;

	/* Scene tree layers (bottom to top) */
	struct wlr_scene_tree *layer_background;
	struct wlr_scene_tree *layer_bottom;
	struct wlr_scene_tree *view_tree;
	/* Layer sub-trees within view_tree (bottom to top) */
	struct wlr_scene_tree *view_layer_desktop;
	struct wlr_scene_tree *view_layer_below;
	struct wlr_scene_tree *view_layer_normal;
	struct wlr_scene_tree *view_layer_above;
	struct wlr_scene_tree *xdg_popup_tree;
	struct wlr_scene_tree *layer_top;
	struct wlr_scene_tree *layer_overlay;

	struct wlr_xdg_shell *xdg_shell;
	struct wl_listener new_xdg_toplevel;
	struct wl_listener new_xdg_popup;

	/* xdg-decoration protocol (SSD negotiation) */
	struct wlr_xdg_decoration_manager_v1 *xdg_decoration_mgr;
	struct wl_listener new_xdg_decoration;

	/* Layer shell protocol (panels, wallpaper, lock screens) */
	struct wlr_layer_shell_v1 *layer_shell;
	struct wl_listener new_layer_surface;
	struct wl_list layer_surfaces; /* wm_layer_surface.link */

	/* Theme/style for decorations */
	struct wm_style *style;

	struct wl_list outputs; /* wm_output.link */
	struct wl_listener new_output;
	struct wlr_output_layout *output_layout;

	/* Input / seat */
	struct wlr_seat *seat;
	struct wl_listener new_input;
	struct wl_listener request_cursor;
	struct wl_listener request_set_selection;

	/* Drag and drop */
	struct wl_listener request_start_drag;
	struct wl_listener start_drag;
	struct wlr_scene_tree *drag_icon_tree;

	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *cursor_mgr;
	struct wl_listener cursor_motion;
	struct wl_listener cursor_motion_absolute;
	struct wl_listener cursor_button;
	struct wl_listener cursor_axis;
	struct wl_listener cursor_frame;

	/* Touch input */
	struct wl_listener cursor_touch_down;
	struct wl_listener cursor_touch_up;
	struct wl_listener cursor_touch_motion;
	struct wl_listener cursor_touch_cancel;
	struct wl_listener cursor_touch_frame;

	/* Tablet input (tablet-v2 protocol for stylus/pad devices) */
	struct wlr_tablet_manager_v2 *tablet_manager;
	struct wl_list tablets;     /* wm_tablet.link */
	struct wl_list tablet_pads; /* wm_tablet_pad.link */

	/* Interactive move/resize state */
	enum wm_cursor_mode cursor_mode;
	struct wm_view *grabbed_view;
	double grab_x, grab_y;
	struct wlr_box grab_geobox;
	uint32_t resize_edges;

	/* Show window position/size overlay during move/resize */
	bool show_position;
	struct wlr_scene_buffer *position_overlay;

	/* Window snap zones (edge/corner snapping preview) */
	int snap_zone; /* enum wm_snap_zone from config.h */
	struct wlr_box snap_preview_box;
	struct wlr_scene_buffer *snap_preview;

	/* Wireframe move/resize (opaqueMove: false / opaqueResize: false) */
	struct wlr_scene_rect *wireframe_rects[4]; /* top, right, bottom, left */
	bool wireframe_active;
	int wireframe_x, wireframe_y, wireframe_w, wireframe_h;

	/* Focus policy (click-to-focus, sloppy) */
	int focus_policy; /* enum wm_focus_policy from view.h */
	struct wm_view *focused_view;
	bool focus_user_initiated; /* true for click/keybind focus changes */

	/* Auto-raise timer for focus-follows-mouse modes */
	struct wl_event_source *auto_raise_timer;
	struct wm_view *auto_raise_view;

	/* Managed windows */
	struct wl_list views; /* wm_view.link */

	/* Workspaces */
	struct wl_list workspaces; /* wm_workspace.link */
	struct wm_workspace *current_workspace;
	int workspace_count;
	struct wlr_scene_tree *sticky_tree; /* always-visible views */
	struct wm_ws_transition *ws_transition; /* active slide/fade, or NULL */

	/* Input devices */
	struct wl_list keyboards; /* wm_keyboard.link */

	/* Configuration and keybindings */
	struct wm_config *config;
	struct wl_list keybindings; /* wm_keybind.link (legacy flat list) */
	struct wl_list keymodes;    /* wm_keymode.link */
	char *current_keymode;      /* active keymode name ("default") */
	struct wm_chain_state chain_state; /* multi-key chain state */

	/* Mouse bindings */
	struct wl_list mousebindings; /* wm_mousebind.link */
	struct wm_mouse_state mouse_state;

	/* Per-window rules (Fluxbox apps file) */
	struct wm_rules rules;

	/* IPC server */
	struct wm_ipc_server ipc;

	/* Keyboard focus navigation (accessibility) */
	struct wm_focus_nav focus_nav;

	/* Foreign toplevel management (taskbar support) */
	struct wlr_foreign_toplevel_manager_v1 *foreign_toplevel_manager;

	/* Output management protocol (wlr-randr, kanshi) */
	struct wm_output_management output_mgmt;

	/* Idle notification and inhibit (ext-idle-notify-v1, idle-inhibit-v1) */
	struct wm_idle idle;

	/* Primary selection (middle-click paste) */
	struct wlr_primary_selection_v1_device_manager *primary_selection_mgr;
	struct wl_listener request_set_primary_selection;

	/* Data control (clipboard managers: cliphist, wl-clipboard, etc.) */
	struct wlr_data_control_manager_v1 *data_control_mgr;

	/* Pointer constraints (lock/confine for games) */
	struct wlr_pointer_constraints_v1 *pointer_constraints;
	struct wlr_pointer_constraint_v1 *active_constraint;
	struct wl_listener new_pointer_constraint;
	struct wl_listener pointer_constraint_destroy;

	/* Relative pointer (unaccelerated motion for games) */
	struct wlr_relative_pointer_manager_v1 *relative_pointer_mgr;

	/* Pointer gestures (touchpad swipe/pinch/hold) */
	struct wlr_pointer_gestures_v1 *pointer_gestures;
	struct wl_listener cursor_swipe_begin;
	struct wl_listener cursor_swipe_update;
	struct wl_listener cursor_swipe_end;
	struct wl_listener cursor_pinch_begin;
	struct wl_listener cursor_pinch_update;
	struct wl_listener cursor_pinch_end;
	struct wl_listener cursor_hold_begin;
	struct wl_listener cursor_hold_end;

	/* Fractional scale (wp-fractional-scale-v1 for HiDPI) */
	struct wlr_fractional_scale_manager_v1 *fractional_scale_mgr;

	/* Cursor shape (wp-cursor-shape-v1) */
	struct wlr_cursor_shape_manager_v1 *cursor_shape_mgr;
	struct wl_listener cursor_shape_request;

	/* Viewporter and single-pixel-buffer */
	struct wlr_viewporter *viewporter;
	struct wlr_single_pixel_buffer_manager_v1 *single_pixel_buffer_mgr;

	/* Gamma control (wlr-gamma-control-v1 for gammastep, wlsunset) */
	struct wlr_gamma_control_manager_v1 *gamma_control_mgr;
	struct wl_listener gamma_set_gamma;

	/* Presentation time (wp-presentation-time for frame timing) */
	struct wlr_presentation *presentation;

	/* Text input / input method relay (IME support) */
	struct wm_text_input_relay text_input_relay;

	/* Virtual keyboard (virtual-keyboard-v1 for on-screen keyboards) */
	struct wlr_virtual_keyboard_manager_v1 *virtual_keyboard_mgr;
	struct wl_listener new_virtual_keyboard;

	/* Virtual pointer (virtual-pointer-v1 for remote input/automation) */
	struct wlr_virtual_pointer_manager_v1 *virtual_pointer_mgr;
	struct wl_listener new_virtual_pointer;

	/* Keyboard shortcuts inhibit (for games, remote desktops) */
	struct wlr_keyboard_shortcuts_inhibit_manager_v1 *kb_shortcuts_inhibit_mgr;
	struct wl_listener new_kb_shortcuts_inhibitor;
	struct wlr_keyboard_shortcuts_inhibitor_v1 *active_kb_inhibitor;
	struct wl_listener kb_inhibitor_destroy;

	/* XDG activation (app launch focus tokens) */
	struct wlr_xdg_activation_v1 *xdg_activation;
	struct wl_listener xdg_activation_request;

	/* Tearing control (variable refresh rate for games) */
	struct wlr_tearing_control_manager_v1 *tearing_control_mgr;
	struct wl_listener tearing_new_object;

	/* Output power management (display on/off for swayidle) */
	struct wlr_output_power_manager_v1 *output_power_mgr;
	struct wl_listener output_power_set_mode;

	/* Content type hints (wp-content-type-v1) */
	struct wlr_content_type_manager_v1 *content_type_mgr;

	/* Alpha modifier (wp-alpha-modifier-v1 for per-surface opacity) */
	struct wlr_alpha_modifier_v1 *alpha_modifier;

	/* Security context (wp-security-context-v1 for sandboxed clients) */
	struct wlr_security_context_manager_v1 *security_context_mgr;

	/* Linux DMA-BUF (linux-dmabuf-v1 for GPU buffer sharing) */
	struct wlr_linux_dmabuf_v1 *linux_dmabuf;

	/* XDG output (xdg-output-v1 for logical output info) */
	struct wlr_xdg_output_manager_v1 *xdg_output_mgr;

	/* XDG foreign (xdg-foreign-v1/v2 for cross-app surface sharing) */
	struct wlr_xdg_foreign_registry *xdg_foreign_registry;
	struct wlr_xdg_foreign_v1 *xdg_foreign_v1;
	struct wlr_xdg_foreign_v2 *xdg_foreign_v2;

	/* Ext foreign toplevel list (ext-foreign-toplevel-list-v1) */
	struct wlr_ext_foreign_toplevel_list_v1 *ext_foreign_toplevel_list;

	/* Session lock (ext-session-lock-v1) */
	struct wm_session_lock session_lock;

	/* DRM lease (wp-drm-lease-v1 for VR headsets) */
	struct wlr_drm_lease_v1_manager *drm_lease_mgr;
	struct wl_listener drm_lease_request;

	/* Transient seat (ext-transient-seat-v1 for remote desktop) */
	struct wlr_transient_seat_manager_v1 *transient_seat_mgr;
	struct wl_listener transient_seat_create;

	/* Explicit sync (linux-drm-syncobj-v1) */
	struct wlr_linux_drm_syncobj_manager_v1 *syncobj_mgr;

	/* Toolbar */
	struct wm_toolbar *toolbar;

	/* Slit (dockapp container) */
	struct wm_slit *slit;

#ifdef WM_HAS_SYSTRAY
	/* System tray (StatusNotifierItem) */
	struct wm_systray *systray;
#endif

	/* Menus */
	struct wm_menu *root_menu;
	struct wm_menu *window_menu;
	struct wm_menu *window_list_menu;
	struct wm_menu *workspace_menu;
	struct wm_menu *client_menu;
	struct wm_menu *custom_menu;

#ifdef WM_HAS_XWAYLAND
	/* XWayland bridge */
	struct wlr_xwayland *xwayland;
	struct wl_listener xwayland_new_surface;
	struct wl_listener xwayland_ready;
	struct wl_list xwayland_views; /* wm_xwayland_view.link */
	struct wm_xwayland_atoms xwayland_atoms;
#endif

	/* Signal handlers */
	struct wl_event_source *sigint_source;
	struct wl_event_source *sigterm_source;
	struct wl_event_source *sighup_source;
	struct wl_event_source *sigchld_source;

	const char *socket;

	/* Security: --ipc-no-exec disables Exec/BindKey/SetStyle via IPC */
	bool ipc_no_exec;
};

/* Initialize the server (create display, backend, renderer, scene, etc.) */
bool wm_server_init(struct wm_server *server);

/* Start the backend and bind socket */
bool wm_server_start(struct wm_server *server);

/* Run the event loop */
void wm_server_run(struct wm_server *server);

/* Reload all configuration (style, menus, rules, keys, XKB, toolbar) */
void wm_server_reconfigure(struct wm_server *server);

/* Tear down and free resources */
void wm_server_destroy(struct wm_server *server);

#endif /* WM_SERVER_H */
