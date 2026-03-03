List of outstanding bugs and items to address

Menu
- [RESOLVED] Only Foot launches from menu, other applications are listed in menus but do not launch. Install ones it lists to test.
  - Fixed: Environmental — apps were not installed. Installed alacritty, kitty, thunar, nautilus, pcmanfm, mousepad, gedit, mpv, vlc, gimp, inkscape, chromium. Fixed chromium binary name in data/menu (chromium-browser → chromium). All Wayland-native apps verified launching via UI test harness (16 passed, 3 skipped for XWayland-only apps).
- [RESOLVED] Configuration menu shows options, but not a active toggle when enabled
  - Fixed: Added bool checked field to menu items with filled diamond indicator. Config toggles now show active state and update in-place on click.
- [RESOLVED] When enabling configuration like "Opaque Move" applications no longer launch from the menu
  - Fixed: Critical use-after-free — handle_config_command() called wm_server_reconfigure() which freed menu tree while click handler still held dangling pointer. Also, config file re-read from disk undid in-memory toggle. Fix: replaced reconfigure with targeted toolbar relayout, hide menus before executing actions.

Windows
- [RESOLVED] In fluxbox when a window is "Sticky", "Maximized", "Iconify" a dot appears next to it to show when it is active.
  - Fixed: Window menu items now show a filled diamond indicator reflecting focused view's Shade/Stick/Maximize/Iconify state, refreshed each time the menu opens.
- [RESOLVED] Remember does not not remember the window dimensions or placement
  - Fixed: Layer name parsing now supports string names (Desktop/Below/Normal/Above) in addition to integers. Layer and minimized rules are now applied in wm_rules_apply(). Shaded state is now saved in format_remember_block(). Position and dimensions were already saving/restoring correctly.

Toolbar

Styles
- [RESOLVED] Switching a theme only applies to newly opened windows, not existing windows or the menu and toolbar. Switching theme should apply to all new and existing UI elements controlled by a theme. Reloading configuraiton or restarting fluxland from the menu does not update the theme.
  - Fixed: WM_MENU_STYLE handler now updates config->style_file (preserves choice across reconfigure), calls wm_toolbar_relayout(), updates all view decorations, and broadcasts IPC style_changed event. Matches existing handle_set_style() pattern.

Rendering
- Actions cause multiple artifacts and rendering issues, blocked areas, black areas, leftover artifacts due to improper damage notifcations. This occurs on an X forwarded session with virt-manager
  - INVESTIGATED: Likely environmental (SPICE/VNC compression through virt-manager). The view_at() type confusion fix (Bug #8) may have been contributing to scene graph corruption. Remaining artifacts need testing on direct display to confirm if environmental.

Crashes
- [RESOLVED] Launching some applications like "Area Screenshot" cause Fluxland to crash
  - Fixed: Type confusion in view_at() — when layer surfaces (e.g. slurp overlay) were present, node.data was returned as wm_view* but was actually wlr_scene_layer_surface_v1*, causing segfault on dereference. Fix: added validate_view_data() that checks candidate pointer against server->views list.
