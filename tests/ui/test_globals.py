"""The Wayland protocols fluxland advertises.

Clients decide what they can do from the globals list, so a protocol
silently going missing (a refactor, a wlroots upgrade) breaks apps
without any crash. dmabuf, DRM lease and syncobj need the DRM backend and
are not expected headless.
"""

import os
import re
import shutil
import subprocess

import pytest

EXPECTED = {
    # Core and shells
    "wl_compositor", "wl_subcompositor", "wl_shm", "wl_seat", "wl_output",
    "wl_data_device_manager", "xdg_wm_base", "zwlr_layer_shell_v1",
    # Decorations: xdg-decoration and KDE's (GTK3, Qt)
    "zxdg_decoration_manager_v1", "org_kde_kwin_server_decoration_manager",
    # Taskbars, docks, launchers
    "zwlr_foreign_toplevel_manager_v1", "ext_foreign_toplevel_list_v1",
    "xdg_activation_v1",
    # Screens: capture, configuration, power, gamma
    "zwlr_screencopy_manager_v1", "zwlr_export_dmabuf_manager_v1",
    "zwlr_output_manager_v1", "zwlr_output_power_manager_v1",
    "zwlr_gamma_control_manager_v1", "zxdg_output_manager_v1",
    # Session: lock, idle
    "ext_session_lock_manager_v1", "ext_idle_notifier_v1",
    "zwp_idle_inhibit_manager_v1",
    # Input
    "zwp_text_input_manager_v3", "zwp_input_method_manager_v2",
    "zwp_virtual_keyboard_manager_v1", "zwlr_virtual_pointer_manager_v1",
    "zwp_pointer_constraints_v1", "zwp_relative_pointer_manager_v1",
    "zwp_pointer_gestures_v1", "zwp_tablet_manager_v2",
    "zwp_keyboard_shortcuts_inhibit_manager_v1", "wp_cursor_shape_manager_v1",
    # Clipboard
    "zwp_primary_selection_device_manager_v1", "zwlr_data_control_manager_v1",
    "ext_data_control_manager_v1",
    # Screen capture for portals and recorders (wlroots 0.19)
    "ext_image_copy_capture_manager_v1",
    "ext_output_image_capture_source_manager_v1",
    # Dialogs and bells (wlroots 0.19)
    "xdg_wm_dialog_v1", "xdg_system_bell_v1",
    # Window icons for the iconbar (wlroots 0.19)
    "xdg_toplevel_icon_manager_v1",
    # Presentation
    "wp_viewporter", "wp_fractional_scale_manager_v1", "wp_presentation",
    "wp_single_pixel_buffer_manager_v1", "wp_tearing_control_manager_v1",
    "wp_content_type_manager_v1", "wp_alpha_modifier_v1",
    # Sandboxing, transient seats, X11, cross-client embedding
    "wp_security_context_manager_v1", "ext_transient_seat_manager_v1",
    "xwayland_shell_v1", "zxdg_exporter_v2", "zxdg_importer_v2",
}


def advertised(compositor):
    exe = shutil.which("wayland-info")
    if not exe:
        pytest.skip("wayland-info (wayland-utils) is required")
    env = dict(os.environ, XDG_RUNTIME_DIR=compositor.runtime_dir,
               WAYLAND_DISPLAY=compositor.wayland_display)
    out = subprocess.run([exe], env=env, capture_output=True, text=True,
                         timeout=10).stdout
    return set(re.findall(r"interface: '([a-z0-9_]+)'", out))


def test_expected_globals(compositor):
    if compositor.is_live_session:
        pytest.skip("headless only (a live session may add DRM globals)")
    missing = EXPECTED - advertised(compositor)
    assert not missing, f"missing globals: {sorted(missing)}"


def test_data_control_advertised_once(compositor):
    """It used to be created twice."""
    exe = shutil.which("wayland-info")
    if not exe:
        pytest.skip("wayland-info (wayland-utils) is required")
    env = dict(os.environ, XDG_RUNTIME_DIR=compositor.runtime_dir,
               WAYLAND_DISPLAY=compositor.wayland_display)
    out = subprocess.run([exe], env=env, capture_output=True, text=True,
                         timeout=10).stdout
    assert out.count("'zwlr_data_control_manager_v1'") == 1
