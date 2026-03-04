# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.0] - 2026-03-03

### Added
- Fluxbox style compatibility with 37 bundled styles (10 community from box-look.org, 27 upstream from Fluxbox)
- GdkPixbuf support for XPM, JPEG, and BMP pixmaps in styles
- Style gallery documentation with screenshots for all bundled styles
- PipeWire screen cast module with IPC control
- Touchpad gesture interception for workspace switching
- AT-SPI accessibility bridge and IPC enrichment
- Native dockapp protocol for slit
- Conditional keybindings
- Per-workspace wallpaper support
- Workspace slide transitions
- Mouse drag-to-tab and tab click activation
- Fluxbox-style toolbar layout with workspace name tool
- Checked state indicators for menu items
- XWayland views in IPC get_windows response
- Default keybindings for LHalf/RHalf half-tiling
- Shaded field in IPC get_windows response
- Great Wave as default style for new installs
- Config validation entries for workspace, animation, and snap zone settings
- SECURITY.md with vulnerability reporting and audit history
- Man pages for fluxland-init(5), fluxland-startup(5), and fluxland-ctl(1)
- 11 new C unit test files and 3 new fuzz targets
- App launch tests for all default menu entries

### Changed
- Deduplicated example configs to overlay format
- Renamed "theme" terminology to "style" throughout codebase for Fluxbox consistency
- Decomposed mega-functions in ipc_commands.c, keyboard_actions.c, decoration.c, and config.c
- Decomposed view.c and cursor.c into focused modules
- Extracted menu_render.c/h and menu_parse.c/h from menu.c
- Extracted shared pixel_buffer module
- Consolidated double-fork into wm_spawn_command() with environment sanitization
- Cleaned up screenshot history and renamed screenshots to descriptive names

### Fixed
- Critical UAF in menu config toggle and type confusion crash in view_at()
- Style rendering: XPM support, greyNN colors, rgb: color scaling, pixmap subdirectory resolution
- Style rendering: per-button pixmaps, shade button ParentRelative fallback, implicit pixmap fill
- Style rendering: wildcard keys in parser, per-component toolbar styles
- Toolbar text contrast and workspace white wash in wave style
- ShowDesktop focus restore and xdg_surface configure timing guards
- Double-centering of text in titlebars and toolbar
- Toolbar workspace button sizing and toggle visibility
- Theme switch propagation, remember save/restore, and layer parsing
- XDG toplevel request handler guards against pre-initialization
- Config parser hardening against long lines and symlink attacks
- NULL checks after malloc and wlr_scene_*_create() calls
- Buffer overflow: replaced sprintf with snprintf in json_escape
- Standalone style installation (wave, default, hc-dark, hc-light)

### Performance
- Iconbar per-entry dirty tracking to skip redundant re-renders
- Position-change guards on decoration scene node updates
- Config reload mtime diffing to skip unchanged subsystems
- Text rendering pipeline and toolbar allocation churn optimization
- WLCS source fix and compile-time performance profiling

## [1.0.0] - 2026-02-27

### Added

**Core compositor:**
- Wayland compositor built on wlroots 0.18, written in C17
- Server-side window decorations with full Fluxbox theme/style compatibility (gradients, textures, fonts)
- Window tabbing with drag-to-tab from titlebars
- Window shading, sticky windows, and per-window opacity/alpha
- Smart window placement (row-smart, column-smart, cascade, under-mouse)
- Window arrangement actions (grid, vertical columns, horizontal rows, cascade)
- Edge snapping with visual preview overlay
- Multiple focus policies (click-to-focus, sloppy mouse focus, strict mouse focus)
- Up to 32 named virtual desktops with edge warping
- Per-output workspaces

**Keybindings and input:**
- Key chains (multi-key sequences, e.g. `Mod4+x Mod4+t`)
- Keymodes (modal keybinding sets with named modes)
- MacroCmd and ToggleCmd compound actions
- Mouse bindings with per-context dispatch (titlebar, desktop, border, grip)
- Touch input support
- Tablet/stylus input via tablet-v2 protocol
- Full XKB keyboard configuration with multi-layout support and runtime switching

**UI:**
- Configurable toolbar with workspace switcher, icon bar, and clock
- Toolbar auto-hide and configurable placement (top/bottom)
- Root menu and window menu with Fluxbox-compatible format
- Window list menu

**Configuration:**
- Fluxbox-compatible config files (init, keys, apps, menu, style, startup)
- Per-window rules with regex matching on app_id, title, and class
- Auto-grouping rules for window tabbing
- Live reconfiguration via `Mod4+r` or `SIGHUP`
- Config validation with `--check-config` flag

**IPC:**
- JSON-over-Unix-socket IPC protocol
- `fluxland-ctl` command-line client
- Event subscriptions (window, workspace, output events)
- Accessibility meta-subscription

**Protocols (30+):**
- xdg-shell, xdg-decoration, layer-shell, session-lock
- fractional-scale, tearing-control, content-type, presentation-time
- pointer-constraints, relative-pointer, pointer-gestures
- virtual-keyboard, virtual-pointer, text-input, input-method
- gamma-control, output-management, output-power, screencopy
- data-control, xdg-activation, xdg-foreign, tablet-v2
- cursor-shape, viewporter, single-pixel-buffer, alpha-modifier
- security-context, linux-dmabuf, idle-notify, idle-inhibit
- DRM lease, DRM syncobj, transient-seat

**Accessibility:**
- High-contrast dark and light themes (WCAG AAA compliant)
- Configurable focus border width and color for focused windows
- Keyboard-only toolbar navigation (FocusToolbar, FocusNextElement, FocusPrevElement, FocusActivate)
- RTL text support with automatic detection via Pango

**Other:**
- XWayland support for X11 applications
- Slit (dockapp container) implemented via layer-shell
- Animation system with fade-in/fade-out for window transitions
- i18n infrastructure with gettext (`_()` / `N_()` macros, `.pot` template)
- Man pages: `fluxland(1)`, `fluxland-keys(5)`, `fluxland-apps(5)`, `fluxland-menu(5)`, `fluxland-style(5)`
- Example configurations and themes (default, Great Wave, high-contrast dark/light)
- Packaging support for Debian, Arch Linux (PKGBUILD), and Fedora (RPM spec)
- Comprehensive test suite: 81% line coverage, 35 unit test files, 4 fuzz targets, automated UI tests

### Security
- Hardened build flags: `FORTIFY_SOURCE=2`, stack protector, stack-clash-protection
- Full RELRO and `-z now` link flags
- IPC rate limiting to prevent abuse
- Double-fork pattern for child process execution to avoid SIGCHLD races
