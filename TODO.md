# TODO - Future Work

This is the backlog of planned features, improvements, and infrastructure
work for fluxland. Items are grouped by category and roughly ordered
by priority within each section.

See `FEATURE_GAP_ANALYSIS.md` for the full Fluxbox feature parity audit.

---

## Fluxbox Feature Parity — Remaining Items

These were identified in the Fluxbox feature audit but not yet implemented.
Phases 1-5 are complete; these are the remaining gaps.

### High Complexity (XL) — Need Dedicated Sessions

- [x] **External Tabs** — Tab positioning outside the window (Top/Bottom/Left/Right)
  - Config: `session.screen0.tabs.intitlebar`, `tab.placement`, `tab.width`,
    `tabPadding`, `tabsAttachArea`
  - Files: `decoration.c/h`, `tabgroup.c/h`, `config.c/h`
- [x] **System Tray** — StatusNotifierWatcher/StatusNotifierItem D-Bus protocol
  - Alternative to X11 system tray for Wayland
  - Files: new `systray.c/h`, `toolbar.c`, D-Bus dependency
- [x] **Conditional Commands** — `If {condition} {then} {else}`, `ForEach/Map`,
  `Delay {cmd} [us]`
  - Conditions: Matches, Some, Every, Not, And, Or, Xor
  - Files: `keybind.c/h`, `keyboard.c`
- [x] **Toolbar Tools Configuration** — `session.screen0.toolbar.tools` for
  configurable component order (clock, workspacename, iconbar, buttons, etc.)
  - Files: `toolbar.c/h`, `config.c/h`

### Medium Complexity (M)

- [x] **Menu Icons** — `<icon_path>` support on menu items
  - Files: `menu.c/h`, `render.c`
- [x] **FocusProtection** — Per-window `[FocusProtection] {Gain,Refuse,Deny,Lock}`
  in apps file
  - Files: `rules.c/h`, `view.c`
- [x] **IgnoreSizeHints** — Per-window `[IgnoreSizeHints] {yes}` in apps file
  - Files: `rules.c/h`, `view.c`
- [x] **Slitlist File** — `session.slitlistFile` to persist dockapp ordering
  - Files: `slit.c/h`, `config.c`
- [x] **Slit Alpha/Layer/MaxOver** — `slit.alpha`, `slit.layer`, `slit.maxOver`
  config options
  - Files: `slit.c`, `config.c/h`
- [x] **Halo Text Effect** — `effect: halo` with `halo.color` in style files
  - Files: `render.c`
- [x] **Wireframe Move/Resize** — Honor `opaqueMove: false` / `opaqueResize: false`
  with outline rendering instead of opaque
  - Files: `cursor.c`, `render.c`

### Low Complexity (S)

- [x] **BindKey** — Add keybindings at runtime via `BindKey` command
- [x] **SetEnv/Export** — Set environment variables from keys file
- [x] **ShowWindowPosition** — Coordinate overlay during move/resize
- [x] **Slit Styling** — `slit.*` style properties (borderWidth, color, etc.)
- [x] **Menu Bullet Style** — `menu.bullet`, `menu.bullet.position` style props
- [x] **Menu Item/Title Height** — `menu.itemHeight`, `menu.titleHeight` style props
- [x] **SetAlpha Relative** — `SetAlpha +10` / `SetAlpha -10` relative adjustment
- [x] **NextWindow/PrevWindow Pattern** — Filter cycling by pattern argument
- [x] **NextGroup/PrevGroup** — Cycle between tab groups
- [x] **GotoWindow N** — Focus window at stack position N
- [x] **Unclutter** — Reduce window overlap without resizing
- [x] **AutotabPlacement** — Auto-tab new windows into existing groups
- [x] **--list-commands CLI** — Print all available commands and exit
- [x] **[encoding]/[endencoding]** — Menu character encoding tags
- [x] **[wallpapers] Menu Tag** — Browse wallpaper directory
- [x] **[stylesmenu] Menu Tag** — Auto-generated style list
- [x] **Iconbar Enhancements** — alignment, iconWidth, usePixmap, wheelMode,
  iconifiedPattern config options
- [x] **Tab-Specific Focus Models** — ClickTabFocus / MouseTabFocus

---

## CI/CD

- [ ] GitHub Actions workflow for build + test on push and PR
  - Matrix: Debian stable, Ubuntu LTS, Fedora latest
  - Build with both GCC and Clang
  - Run `meson test` and report failures
- [ ] Automated linting (clang-format check, clang-tidy)

## Window management

- [x] Per-output workspaces (configurable global vs per-output mode)
- [x] Window snap zones (quarter/half screen tiling, Windows/macOS style)
  - Snap to screen edges and corners
  - Visual preview overlay during drag
- [x] Animations
  - Fade in/out on map/unmap
  - ~~Workspace slide transitions~~ (deferred)
  - Minimize/restore effects

## Protocols

- [ ] DRM lease protocol (`wp-drm-lease-v1`) for VR headset passthrough
- [ ] Transient seat protocol (`ext-transient-seat-v1`) for remote desktop
  temporary seats
- [ ] Explicit sync protocol (`linux-drm-syncobj-v1`) for GPU
  synchronization
- [ ] Input method popup positioning improvements

## Testing

- [x] Integration test suite expansion
  - Client-side protocol conformance testing
  - Multi-output scenarios
  - Workspace switching with active clients
- [x] Fuzzing with AFL or libFuzzer on config/keys/style parsers
  - `rcparser.c` (init file parser)
  - `keybind.c` (keys file parser)
  - `style.c` (style file parser)
- [x] WLCS (Wayland protocol conformance test suite) integration
- [x] Automated screenshot comparison tests for decoration rendering

## Packaging & distribution

- [x] Debian `.deb` package
- [x] Arch Linux `PKGBUILD`
- [x] Nix flake
- [x] Fedora `.spec` file
- [x] Flatpak manifest (experimental — compositors in Flatpak are unusual)
- [x] `.desktop` session entry for display managers

## Accessibility

- [ ] Screen reader integration (AT-SPI bridge or similar)
- [ ] High-contrast theme support
- [ ] Keyboard-only navigation mode for all UI elements (menus, toolbar)
- [ ] Focus indication improvements for low-vision users

## Localization

- [ ] i18n support for log messages and IPC error strings
- [ ] Translatable menu labels
- [ ] Right-to-left layout support for decorations

## Quality of life

- [x] Configuration file validation tool (`fluxland --check-config`)
- [x] Runtime config reload for more options (currently partial)
- [x] IPC event subscriptions for style/theme changes
- [x] Mouse button remapping in config
