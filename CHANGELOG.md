# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.1.0] - 2026-02-27

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
