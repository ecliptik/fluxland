# TODO - Future Work

This is the backlog of planned features, improvements, and infrastructure
work for fluxland. Items are grouped by category and roughly ordered
by priority within each section.

## CI/CD

- [ ] GitHub Actions workflow for build + test on push and PR
  - Matrix: Debian stable, Ubuntu LTS, Fedora latest
  - Build with both GCC and Clang
  - Run `meson test` and report failures
- [ ] Automated linting (clang-format check, clang-tidy)

## Window management

- [ ] Per-output workspaces (configurable global vs per-output mode)
- [ ] Window snap zones (quarter/half screen tiling, Windows/macOS style)
  - Snap to screen edges and corners
  - Visual preview overlay during drag
- [ ] Animations
  - Fade in/out on map/unmap
  - Workspace slide transitions
  - Minimize/restore effects

## Protocols

- [ ] DRM lease protocol (`wp-drm-lease-v1`) for VR headset passthrough
- [ ] Transient seat protocol (`ext-transient-seat-v1`) for remote desktop
  temporary seats
- [ ] Explicit sync protocol (`linux-drm-syncobj-v1`) for GPU
  synchronization
- [ ] Input method popup positioning improvements

## Testing

- [ ] Integration test suite expansion
  - Client-side protocol conformance testing
  - Multi-output scenarios
  - Workspace switching with active clients
- [ ] Fuzzing with AFL or libFuzzer on config/keys/style parsers
  - `rcparser.c` (init file parser)
  - `keybind.c` (keys file parser)
  - `style.c` (style file parser)
- [ ] WLCS (Wayland protocol conformance test suite) integration
- [ ] Automated screenshot comparison tests for decoration rendering

## Packaging & distribution

- [ ] Debian `.deb` package
- [ ] Arch Linux `PKGBUILD`
- [ ] Nix flake
- [ ] Fedora `.spec` file
- [ ] AppImage or Flatpak (if feasible for a compositor)
- [ ] `.desktop` session entry for display managers

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

- [ ] Configuration file validation tool (`fluxland --check-config`)
- [ ] Runtime config reload for more options (currently partial)
- [ ] IPC event subscriptions for style/theme changes
- [ ] Mouse button remapping in config
