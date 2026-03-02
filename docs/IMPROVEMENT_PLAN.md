# Fluxland Improvement Plan

*Generated from comprehensive codebase review — 2026-03-02*
*Last updated: 2026-03-02 — 6 items completed in first sprint*

## Executive Summary

Fluxland is in strong shape for a 1.0.0 release. The 35K-line codebase demonstrates consistent coding conventions, thoughtful architecture (scene graph rendering, double-fork exec, server-side decorations), and solid security fundamentals (safe_atoi, fopen_nofollow, SO_PEERCRED, full RELRO, stack protector). The 38 C unit tests and 137 Python UI tests provide 81.2% line coverage — excellent for a compositor of this complexity.

The primary areas for improvement fall into three themes: **test coverage breadth** (22 of 47 source modules lack dedicated test files, with several large modules like `view.c`, `decoration.c`, and `cursor.c` untested), **function complexity** (several functions exceed 200–900 lines and would benefit from decomposition), and **missing infrastructure** (no CI/CD pipeline, incomplete i18n, missing man pages for `init` and `startup` configs). There are also several malloc calls without NULL checks and duplicated double-fork boilerplate across 5 files that could be consolidated.

The plan below is ordered by priority: critical fixes first, then high-ROI improvements, followed by longer-term enhancements. Each item includes affected files, effort estimates, and enough context to be immediately actionable.

---

## 1. Critical Fixes

Items that should be addressed before new feature work.

### 1.1 ~~Missing NULL checks after malloc/calloc~~ DONE
**Severity**: High | **Effort**: S | **Category**: Security/Robustness

Fixed in commit `eebb940`. Added NULL check in `strbuf_init()` (`ipc_commands.c`) — sets `cap = 0` on malloc failure. Full audit of all malloc/calloc/realloc calls in `src/` confirmed the other two cited locations (`toolbar.c:533`, `rules.c:429`) already had proper NULL checks.

### ~~1.2 Missing wlroots scene node error checks~~ DONE
**Severity**: Medium | **Effort**: S | **Category**: Robustness

Fixed in `7dc510c`. Added NULL checks with graceful cleanup in `wm_decoration_create()` (border rects, titlebar tree, scene buffers), `layout_and_render()` (button buffers), and `wm_toolbar_create()` (bg buffer).

### 1.3 XWayland XKB keymap compilation failure
**Severity**: Medium | **Effort**: M | **Category**: Bug

Known issue: XWayland XKB keymap compilation fails in QEMU VM environment. This affects X11 application keyboard input.

**Action**: Investigate whether this is a VM-specific issue or a general bug. Check XKB keymap generation path in `src/xwayland.c` and `src/keyboard.c`.

### ~~1.4 ShowDesktop may need 2 presses~~ DONE
**Severity**: Low | **Effort**: S | **Category**: Bug

Fixed in `f363998`. Root cause: `request_minimize.notify()` triggered focus changes that reordered the views list during `wl_list_for_each` iteration, causing skipped windows. Fix: directly set scene node disabled + foreign_toplevel minimized without triggering focus changes. Applied to both keyboard_actions.c and ipc_commands.c paths.

---

## 2. High-Value Improvements

Best ROI — biggest impact for effort spent.

### 2.1 Set up CI/CD pipeline
**Effort**: M | **Category**: Infrastructure

No CI/CD exists. The `.github/` directory has only a PR template — no workflows. The project has CLAUDE.md listing a Forgejo Actions need.

**Action**:
- Create `.github/workflows/build-test.yml` (or Forgejo equivalent) with: Meson build, `meson test` (C unit tests), `pytest tests/ui/` (if headless mode works in CI)
- Add compiler warning check (ensure `-Werror` in CI)
- Add ASAN/UBSAN build variant
- Consider `clang-format` check for style enforcement

### ~~2.2 Consolidate double-fork boilerplate into shared helper~~ DONE
**Effort**: S | **Category**: Code Quality

Fixed in `f363998`. Created `wm_spawn_command()` in new `src/util.c` with double-fork + setsid + closefrom + LD_* env sanitization. Replaced inline fork/exec in keyboard_actions.c, menu.c, cursor.c, ipc_commands.c (4 of 5 paths). autostart.c retained its callback-based pattern. Also deduplicated `is_blocked_env_var()` → `wm_is_blocked_env_var()` in util.h.

### 2.3 Decompose mega-functions
**Effort**: L | **Category**: Code Quality

Several functions are extremely large, making them hard to review and maintain:

| Function | File | Lines | Suggested decomposition |
|----------|------|-------|------------------------|
| IPC `command` handler | `ipc_commands.c:462` | 927 | Split into per-command handlers (one function per IPC command) with a dispatch table |
| `wm_execute_action()` | `keyboard_actions.c:358` | 925 | Already a switch on action type — extract each case to a named handler function |
| Decoration render | `decoration.c:747` | 580 | Split titlebar, borders, and button rendering into separate functions |
| Config apply | `config.c:468` | 437 | Group by config category (window, toolbar, workspace, etc.) |
| Menu parse | `menu.c:629` | 353 | Extract submenu parsing, pipe-menu handling, separator handling |

**Action**: Start with `ipc_commands.c` and `keyboard_actions.c` as they are the largest and most frequently modified. Use a function pointer dispatch table for IPC commands.

### ~~2.4 Add environment variable sanitization to exec paths~~ DONE
**Effort**: S | **Category**: Security

Fixed in `f363998` (combined with 2.2). `wm_spawn_command()` strips LD_PRELOAD, LD_LIBRARY_PATH, LD_AUDIT, LD_DEBUG, LD_PROFILE before exec.

---

## 3. Code Quality & Technical Debt

### ~~3.1 IPC sprintf usage~~ DONE
**Effort**: S | **Category**: Security

Fixed in `6f1abc3`. Replaced `sprintf` with `snprintf` using proper remaining buffer size calculation.

### 3.2 Inconsistent error handling in config parsing
**Effort**: M | **Category**: Robustness

Multiple config file parsers (`rcparser.c`, `keybind.c`, `mousebind.c`, `rules.c`, `slit.c`, `validate.c`) each implement their own `fgets()` loops with slightly different error handling. Some check `fopen` return, others don't handle malformed lines gracefully.

**Action**: Consider a shared config line iterator utility, or at minimum audit all `fgets()` loops for consistent error handling (file open failure, line length overflow, unterminated quotes).

### 3.3 Menu.c complexity
**Effort**: L | **Category**: Code Quality

At 2,920 lines, `menu.c` is the largest source file and contains 26 `wl_list_for_each` iterations, multiple nested parsing functions, and rendering logic. It combines parsing, state management, rendering, and input handling in one file.

**Action**: Consider splitting into `menu_parse.c` (config parsing, pipe menus), `menu_render.c` (Cairo/Pango rendering), and `menu.c` (state + input handling). This would make each file ~1000 lines and improve maintainability.

### 3.4 View.c and cursor.c complexity
**Effort**: M per file | **Category**: Code Quality

`view.c` (1,931 lines) mixes lifecycle management, focus logic, XDG toplevel handling, and geometry calculations. `cursor.c` (1,796 lines) handles pointer input, move/resize, snap zones, and touch events.

**Action**: Extract snap zone logic from cursor.c into existing pattern (it's already partially in cursor_snap test module). Extract focus management from view.c into a dedicated module.

---

## 4. Test Coverage & Quality

### 4.1 Add test files for untested modules
**Effort**: XL (total) | **Category**: Testing

22 of 47 source modules have no dedicated test file. Priority order by module size and criticality:

| Module | Lines | Priority | Effort |
|--------|-------|----------|--------|
| `view.c` | 1,931 | Critical | L |
| `decoration.c` | 1,821 | Critical | L |
| `cursor.c` | 1,796 | High | L |
| `ipc_commands.c` | 1,664 | High | L |
| `toolbar.c` | 1,523 | High | M |
| `keyboard_actions.c` | 1,282 | High | L |
| `render.c` | 770 | Medium | M |
| `rcparser.c` | varies | Medium | M |
| `input.c` | varies | Medium | M |
| `xwayland.c` | 869 | Medium | L |
| `tablet.c` | 551 | Low | M |
| `screencopy.c` | varies | Low | M |
| `output_management.c` | varies | Low | M |
| `text_input.c` | varies | Medium | M |

**Note**: Some modules (like `rcparser.c`) are already indirectly tested through other tests or fuzz targets. Focus on modules with complex logic and no indirect coverage first.

### 4.2 Expand fuzz targets
**Effort**: M | **Category**: Testing

Current fuzz targets: `fuzz_rcparser`, `fuzz_keybind`, `fuzz_style`, `fuzz_menu`. Additional fuzzing candidates:
- **IPC command parser** — fuzzing the JSON command parsing in `ipc_commands.c` would cover a network-facing attack surface
- **Window rules parser** — `rules.c` `fgets()` parsing of apps file
- **Mousebind parser** — `mousebind.c` config parsing
- **Validate** — `validate.c` handles arbitrary user config input

**Action**: Add `fuzz_ipc_command`, `fuzz_rules`, `fuzz_mousebind` targets. IPC fuzzing is highest priority as it processes external input.

### 4.3 XWayland test coverage
**Effort**: L | **Category**: Testing

`xwayland.c` is at 14.5% coverage — the lowest in the project. While XWayland testing is inherently difficult (requires X11 infrastructure), the view lifecycle and event handling logic can be unit tested with stubs.

**Action**: Create `tests/test_xwayland.c` (note: `test_xwayland_logic.c` exists — check what it covers and extend it). Focus on the `wm_xwayland_view` lifecycle, unmap/destroy handlers, and geometry management.

### 4.4 Integration test coverage
**Effort**: M | **Category**: Testing

The UI tests are comprehensive (137 tests, 19 files covering all major features), but some areas could use more edge-case coverage:
- Multi-monitor workspace switching
- Rapid window create/destroy cycles (stress testing)
- IPC command injection attempts
- Config live-reload during active window operations

---

## 5. Documentation & Developer Experience

### 5.1 Missing man pages
**Effort**: M | **Category**: Documentation

Two config file formats lack man pages:
- `fluxland-init.5` — the main settings file (`~/.config/fluxland/init`)
- `fluxland-startup.5` — the autostart config (`~/.config/fluxland/startup`)

Existing man pages: `fluxland.1`, `fluxland-keys.5`, `fluxland-apps.5`, `fluxland-menu.5`, `fluxland-style.5`.

**Action**: Create both man pages following the format of existing ones. The init file has the most settings and is the most complex; reference `src/config.c` and `src/rcparser.c` for all supported keys.

### 5.2 Internationalization infrastructure incomplete
**Effort**: M | **Category**: i18n

Only 4 of 47 source files include `i18n.h` and use `_()` macros. No `.po` translation files exist yet — only the `.pot` template. Many user-visible strings (error messages, toolbar labels, menu items) are not wrapped in `_()`.

**Files using i18n**: `menu.c` (28 strings), `ipc_commands.c` (9), `main.c` (4), `wlcs_shim.c` (1)
**Files needing i18n**: `validate.c`, `keyboard_actions.c`, `config.c`, `toolbar.c`, `rules.c`, `view.c`

**Action**:
1. Wrap all user-visible strings in remaining source files with `_()`
2. Update `po/POTFILES.in` to include all source files with translatable strings
3. Regenerate `po/fluxland.pot`
4. Create at least one `.po` file (e.g., `po/es.po`) as a template for translators

### 5.3 Empty docs/user/ and docs/dev/ directories
**Effort**: S | **Category**: Documentation

Both `docs/user/` and `docs/dev/` directories exist but are empty. They should either contain content or be removed.

**Action**: Either populate with content (user guide, developer guide) or remove the empty directories if the existing docs (QUICKSTART.md, ARCHITECTURE.md, CONTRIBUTING.md, man pages) are sufficient.

### 5.4 CONTRIBUTING.md improvements
**Effort**: S | **Category**: Developer Experience

At 344 lines, the CONTRIBUTING.md is thorough. Minor gaps:
- No mention of the WLCS (Wayland Conformance Test Suite) integration
- No guidance on writing fuzz targets
- Could reference the `--check-config` CLI flag for testing config changes

### 5.5 Packaging improvements
**Effort**: M | **Category**: Packaging

- **Flatpak**: Verify the sha256 hash was properly fixed (was FIXME pre-1.0.0)
- **Nix flake**: Ensure `flake.nix` builds correctly and version matches 1.0.0
- **Debian**: `packaging/debian/` is in `.gitignore` — document why and how to work with it
- **CI integration**: Add package build verification to CI pipeline (see 2.1)

---

## 6. Feature Roadmap

### 6.1 Workspace slide transitions
**Effort**: L | **Category**: Feature

Known gap: only fade animations are implemented. Workspace slide transitions (horizontal slide between desktops) are not yet available.

**Action**: Implement slide animation in `src/animation.c` using the existing timer-based animation framework. Add a config option for transition type (`fade`, `slide`, `none`).

### 6.2 ext-image-capture-source / ext-image-copy-capture protocols
**Effort**: L | **Category**: Feature/Protocol

These are the modern replacements for `wlr-screencopy`. Currently Fluxland supports `wlr-screencopy` but not the ext- protocol equivalents being standardized.

**Action**: Implement when wlroots 0.19 provides helpers, or implement directly against the protocol XML.

### 6.3 Color management protocol
**Effort**: XL | **Category**: Feature/Protocol

No color management support (`ext-color-management-v1`). Increasingly important for HDR displays and color-accurate workflows.

### 6.4 Native dockapp protocol for Slit
**Effort**: L | **Category**: Feature

The Fluxbox compatibility matrix notes "native dockapp protocol TBD" for the slit. Currently relies on XWayland compatibility for Window Maker dockapps.

**Action**: Design a Wayland-native protocol or use an existing one (e.g., layer-shell) for native dockapp support.

### 6.5 Conditional keybindings (full support)
**Effort**: M | **Category**: Feature

FLUXBOX-COMPAT.md lists "If/Then conditional bindings" as "Partial — basic conditions supported." Full Fluxbox-compatible conditional bindings would allow more complex keybinding configurations.

### 6.6 Per-workspace wallpaper (native)
**Effort**: M | **Category**: Feature

Currently delegated to external tools like `swaybg`. Native per-workspace wallpaper support would improve the out-of-box experience for Fluxbox migrants who are used to `fbsetroot` or `fbsetbg`.

---

## 7. Long-Term / Aspirational

### 7.1 Plugin/extension system
**Effort**: XL | **Category**: Architecture

Allow users to extend the compositor with plugins (Lua, shared libraries, or IPC-based extensions). Would enable community-contributed features without core changes.

### 7.2 Wayland Conformance Test Suite (WLCS) integration
**Effort**: L | **Category**: Testing

The build system already has a `wlcs` option and `src/wlcs_shim.c` exists. Full WLCS integration would validate protocol compliance and catch edge cases in XDG shell, layer shell, and other protocol implementations.

### 7.3 Screen recording / pipewire integration
**Effort**: L | **Category**: Feature

PipeWire-based screen capture for screen recording and streaming (e.g., for OBS, xdg-desktop-portal integration).

### 7.4 Touch and gesture support improvements
**Effort**: M | **Category**: Feature

While pointer-gestures-v1 is initialized, full touch gesture support (pinch-to-zoom, three-finger swipe for workspace switching) would improve tablet/touchscreen usability.

### 7.5 Accessibility improvements
**Effort**: L | **Category**: Accessibility

The IPC has accessibility meta-subscription support, but deeper accessibility integration (screen reader support via AT-SPI, keyboard-only navigation for all UI elements, high-contrast theme improvements) would broaden the user base.

### 7.6 Performance profiling and optimization
**Effort**: M | **Category**: Performance

No known performance issues, but systematic profiling of:
- Decoration rendering hot path (called on every frame with damage)
- Menu rendering with many items
- IPC command response time under load
- Scene graph operations with many windows (50+ views)

---

## Appendix: Effort Estimates

| # | Item | Effort | Priority | Category | Status |
|---|------|--------|----------|----------|--------|
| 1.1 | Missing NULL checks after malloc | S | Critical | Security | **DONE** |
| 1.2 | Missing wlroots scene node checks | S | Critical | Robustness | **DONE** |
| 1.3 | XWayland XKB keymap failure | M | Critical | Bug | Open |
| 1.4 | ShowDesktop double-press | S | Medium | Bug | **DONE** |
| 2.1 | CI/CD pipeline | M | High | Infrastructure | Open |
| 2.2 | Consolidate double-fork helper | S | High | Code Quality | **DONE** |
| 2.3 | Decompose mega-functions | L | High | Code Quality | Open |
| 2.4 | Environment variable sanitization | S | High | Security | **DONE** |
| 3.1 | IPC sprintf → snprintf | S | Medium | Security | **DONE** |
| 3.2 | Consistent config parser error handling | M | Medium | Robustness |
| 3.3 | Split menu.c | L | Medium | Code Quality |
| 3.4 | Split view.c and cursor.c | M each | Medium | Code Quality |
| 4.1 | Test files for 22 untested modules | XL | High | Testing |
| 4.2 | Expand fuzz targets | M | High | Testing |
| 4.3 | XWayland test coverage | L | Medium | Testing |
| 4.4 | Integration test edge cases | M | Medium | Testing |
| 5.1 | Missing man pages (init, startup) | M | High | Docs |
| 5.2 | i18n infrastructure completion | M | Medium | i18n |
| 5.3 | Empty docs directories | S | Low | Docs |
| 5.4 | CONTRIBUTING.md improvements | S | Low | DX |
| 5.5 | Packaging improvements | M | Medium | Packaging |
| 6.1 | Workspace slide transitions | L | Medium | Feature |
| 6.2 | ext-image-capture protocols | L | Medium | Feature |
| 6.3 | Color management protocol | XL | Low | Feature |
| 6.4 | Native dockapp protocol | L | Low | Feature |
| 6.5 | Full conditional keybindings | M | Low | Feature |
| 6.6 | Native per-workspace wallpaper | M | Low | Feature |
| 7.1 | Plugin/extension system | XL | Low | Architecture |
| 7.2 | WLCS integration | L | Medium | Testing |
| 7.3 | PipeWire screen recording | L | Low | Feature |
| 7.4 | Touch/gesture improvements | M | Low | Feature |
| 7.5 | Accessibility improvements | L | Medium | Accessibility |
| 7.6 | Performance profiling | M | Low | Performance |

**Effort key**: S = < 2 hours | M = 2–8 hours | L = 1–3 days | XL = 1+ week

### Recommended Execution Order

1. ~~**Quick wins** (1–2 days): Items 1.1, 1.2, 2.2, 2.4, 3.1~~ **DONE** (+ 1.4 ShowDesktop fix)
2. **Infrastructure** (1 week): Item 2.1 (CI/CD) — unlocks automated quality gates
3. **Test coverage sprint** (2 weeks): Items 4.1 (priority modules first), 4.2, 4.3
4. **Code quality** (2 weeks): Items 2.3, 3.3, 3.4 — decompose large functions/files
5. **Documentation** (1 week): Items 5.1, 5.2, 5.3, 5.4, 5.5
6. **Features** (ongoing): Items from section 6 based on user demand
