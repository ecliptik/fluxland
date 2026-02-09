# QA Testing Strategy and Validation Plan

## Overview

This document outlines a comprehensive testing strategy for a Wayland compositor project inspired by Fluxbox. It covers testing frameworks, methodologies, tooling, CI/CD pipeline configuration, and manual testing procedures needed to ensure compositor correctness, performance, and compatibility.

The strategy is informed by how existing Wayland compositors (Weston, Sway/wlroots, Mir, Wayfire) approach testing, adapted to the specific needs of a Fluxbox-inspired compositor.

---

## Table of Contents

1. [Testing Frameworks](#1-testing-frameworks)
2. [Unit Testing](#2-unit-testing)
3. [Integration Testing](#3-integration-testing)
4. [Visual and Rendering Testing](#4-visual-and-rendering-testing)
5. [Input Testing](#5-input-testing)
6. [Configuration Testing](#6-configuration-testing)
7. [XWayland Testing](#7-xwayland-testing)
8. [Performance Testing](#8-performance-testing)
9. [CI/CD Pipeline](#9-cicd-pipeline)
10. [Manual Testing Checklist](#10-manual-testing-checklist)
11. [Fuzzing](#11-fuzzing)
12. [Accessibility Testing](#12-accessibility-testing)
13. [Test Organization and Infrastructure](#13-test-organization-and-infrastructure)
14. [Recommended Implementation Order](#14-recommended-implementation-order)

---

## 1. Testing Frameworks

### 1.1 Existing Compositor Testing Approaches

#### Weston Test Suite
Weston provides the most mature and well-documented compositor test suite. Its architecture defines three test types:

- **Standalone tests**: Do not launch the compositor; test pure logic in isolation (e.g., math utilities, data structures, config parsing).
- **Plugin tests**: Launch the Weston compositor and execute from an idle callback within the compositor context. The test blocks the compositor and has direct access to `weston_compositor` internals.
- **Client tests**: Launch the compositor and execute tests in a separate thread, allowing the compositor to run independently. Tests act as normal Wayland clients.

Key design principles from Weston:
- Each test program is a standalone executable runnable with standard debugging tools (GDB, Valgrind)
- Test programs are designed for parallel execution across different test programs
- The compositor fixture sets up the environment and calls `wet_main()` directly
- DRM-backend tests run specifically on VKMS (Virtual KMS)

Reference: [Weston test suite documentation](https://wayland.pages.freedesktop.org/weston/toc/test-suite.html)

#### WLCS (Wayland Conformance Test Suite)
WLCS (by Canonical) is a protocol-conformance-verifying test suite designed for any Wayland compositor. Its key differentiator is the integration approach:

- Tests and compositor share the same address space (in-process testing)
- No need for special Wayland protocol extensions to interrogate compositor state
- Consistent global clock for timing
- Standard debugging tools can trace control flow between test and compositor

To integrate, the compositor must provide a shared library implementing hooks defined in `include/wlcs/`:
- Starting and stopping the mainloop
- Providing file descriptors for client connection
- Window manipulation and compositor control
- Must export a symbol `wlcs_server_integration` of type `struct WlcsServerIntegration const`

Run tests with: `wlcs my_compositor_wlcs_integration.so`

Reference: [WLCS GitHub](https://github.com/canonical/wlcs)

#### wlroots Headless Backend
wlroots provides a headless backend ideal for testing:
- No physical outputs or inputs by default
- Can create "fake" virtual inputs and outputs programmatically
- Virtual input devices allow manual raising of event signals to simulate input
- Virtual outputs can be created for rendering tests without a display

### 1.2 Recommended Framework Stack

| Layer | Tool | Purpose |
|-------|------|---------|
| Unit test framework | **Google Test** (gtest) | C/C++ unit tests, well-supported in Meson |
| Protocol conformance | **WLCS** | Wayland protocol correctness verification |
| Build/test runner | **Meson** (`meson test`) | Native test discovery, parallel execution, TAP output |
| Coverage | **gcovr** / **lcov** | Code coverage reporting with `-Db_coverage=true` |
| Mocking | **Custom test doubles** | Compositor-specific mocks for wlroots interfaces |
| Assertions | **gtest matchers** + **custom** | Protocol-specific assertion helpers |

**Why Google Test over Catch2**: Google Test is more established in the C/C++ Wayland ecosystem, has better Meson integration with `gtest_dep = dependency('gtest')`, and provides richer mocking support via Google Mock. However, for a C-only project, a lighter framework or custom test harness (similar to Weston's approach) may be more appropriate.

**Alternative for C-only**: If the compositor is pure C, consider using Weston's test fixture approach as a model — a custom lightweight test harness with macros like `TEST()`, `TEST_P()`, `PLUGIN_TEST()`, integrated directly with Meson's `test()` function.

---

## 2. Unit Testing

### 2.1 What to Unit Test

Unit tests should cover all compositor logic that can be tested in isolation without launching a full compositor:

#### Window Management Logic
- **Window placement algorithms**: Cascade placement, smart placement, centered placement — verify coordinates for various screen sizes and existing window layouts
- **Focus management**: Focus-follows-mouse, click-to-focus, focus ordering, focus stack behavior
- **Workspace switching**: Workspace creation, deletion, window assignment to workspaces, workspace cycling
- **Window snapping**: Edge snapping, corner snapping, snap distance calculations
- **Window grouping**: Fluxbox tab groups — adding/removing windows from groups, group focus behavior
- **Window state transitions**: Normal → maximized → fullscreen → minimized, restore state

#### Configuration Parsing
- **Fluxbox-compatible config files**: `init` file key-value parsing, default values, type validation
- **Key binding parsing**: Modifier + key combinations, chained key bindings, action mapping
- **Menu file parsing**: `[begin]`/`[end]` tags, `[exec]`, `[submenu]`, nested menus, comment handling (`#` and `!` prefixes)
- **Style/theme parsing**: Style property parsing, color values, font specifications, texture definitions
- **Error handling**: Malformed configs, missing required values, unknown keys, encoding issues

#### Geometry and Layout
- **Rectangle operations**: Intersection, union, containment, edge detection
- **Strut/panel calculations**: Reserved screen space for docks/panels, multi-monitor strut accumulation
- **Multi-monitor layout**: Output arrangement, coordinate mapping between outputs, window placement across monitors

#### Protocol Helpers
- **Surface state management**: Buffer attachment, damage tracking, commit semantics
- **Subsurface ordering**: Z-order management for subsurfaces

### 2.2 Unit Test Structure

```
tests/
├── unit/
│   ├── test_window_placement.c    # Window placement algorithms
│   ├── test_focus_management.c    # Focus policy logic
│   ├── test_workspace.c           # Workspace operations
│   ├── test_config_parser.c       # Configuration file parsing
│   ├── test_keybinding_parser.c   # Key binding specification parsing
│   ├── test_menu_parser.c         # Menu file parsing
│   ├── test_style_parser.c        # Style/theme parsing
│   ├── test_geometry.c            # Rectangle/geometry utilities
│   ├── test_strut.c               # Panel strut calculations
│   └── meson.build
```

### 2.3 Meson Integration

```meson
# tests/unit/meson.build
test_deps = [dependency('gtest', main: true), project_dep]

test('window_placement',
  executable('test_window_placement',
    'test_window_placement.c',
    dependencies: test_deps))

test('config_parser',
  executable('test_config_parser',
    'test_config_parser.c',
    dependencies: test_deps))
```

### 2.4 Design for Testability

To enable effective unit testing, the compositor architecture should:

- **Separate pure logic from Wayland/wlroots dependencies**: Window placement calculations should take parameters (screen geometry, existing windows) and return coordinates, not directly access wlroots structs.
- **Use dependency injection**: Pass interfaces for output management, input handling, etc., so tests can provide mocks.
- **Keep state explicit**: Prefer functions that take state as parameters over functions that access global state.
- **Create clear module boundaries**: Config parsing, window management, layout, and rendering should be separate modules with defined interfaces.

---

## 3. Integration Testing

### 3.1 Protocol Conformance with WLCS

WLCS tests verify correct implementation of core Wayland protocols:

- `wl_compositor` — surface creation, regions
- `wl_shell` / `xdg_shell` — toplevel, popup, positioning
- `wl_seat` — input device capabilities, keyboard/pointer/touch
- `wl_output` — output geometry, modes, transforms
- `wl_data_device` — clipboard, drag-and-drop
- `wl_subsurface` — subsurface positioning, sync/desync

**Integration steps:**
1. Create a shared library (`fluxway_wlcs_integration.so`)
2. Implement `WlcsServerIntegration` hooks:
   - `start_server()` / `stop_server()` — compositor lifecycle
   - `create_client_socket()` — provide fd for client connection
   - `position_window()` — move windows for position-dependent tests
3. Run: `wlcs fluxway_wlcs_integration.so`

### 3.2 Client-Compositor Integration Tests

Following Weston's client test model, write tests that act as Wayland clients:

```
tests/
├── integration/
│   ├── test_xdg_toplevel.c        # XDG toplevel lifecycle
│   ├── test_xdg_popup.c           # Popup positioning and dismiss
│   ├── test_layer_shell.c         # Layer shell surfaces (panels, overlays)
│   ├── test_output_management.c   # Output configuration protocol
│   ├── test_foreign_toplevel.c    # Foreign toplevel management
│   ├── test_clipboard.c           # Copy/paste between clients
│   ├── test_drag_and_drop.c       # DnD between clients
│   ├── test_idle_inhibit.c        # Idle inhibitor protocol
│   └── meson.build
```

Each test:
1. Starts the compositor with the headless backend
2. Connects as a Wayland client
3. Creates surfaces and performs protocol operations
4. Asserts expected state changes (surface mapped, focus received, etc.)
5. Cleans up and disconnects

### 3.3 Compositor-Internal Integration Tests

Following Weston's plugin test model, tests can run inside the compositor context:

- Test that creating an xdg_toplevel surface results in correct internal window state
- Test that focus-follows-mouse correctly updates compositor focus tracking when pointer moves between surfaces
- Test that workspace switching correctly maps/unmaps the expected surfaces

### 3.4 Diagnostic Tools

- **`wayland-info`**: Lists all global interfaces advertised by the compositor. Useful for verifying protocol advertisement. Comes with `wayland-utils`.
- **`WAYLAND_DEBUG=1`**: Environment variable that logs all Wayland protocol messages between client and compositor. Essential for debugging protocol issues.
- **`wlr-randr`**: For wlroots-based compositors, queries and configures outputs.
- **`wlr-which-key`**: Debug key binding resolution.

---

## 4. Visual and Rendering Testing

### 4.1 Screenshot-Based Comparison Testing

Visual regression testing captures compositor output and compares against reference images:

**Capture tools:**
- **grim**: Command-line screenshot tool for Wayland compositors (PNG output)
- **Compositor-internal capture**: Direct framebuffer readback using the headless backend with Pixman renderer — write pixel data to files for comparison
- **weston-screenshooter**: Weston's built-in screenshot mechanism

**Comparison approach:**
1. Run compositor with headless backend + Pixman (software) renderer for deterministic output
2. Launch test client applications that render known content
3. Capture compositor output (full screen or per-surface)
4. Compare against reference images using perceptual diff (e.g., `perceptualdiff`, `pixelmatch`, or custom SSIM)
5. Allow configurable threshold for pixel differences (anti-aliasing, font rendering)

### 4.2 Rendering Backend Testing

Test with multiple rendering backends:

| Backend | Purpose |
|---------|---------|
| **Pixman (software)** | Deterministic reference rendering, CI-friendly |
| **GLES2 (llvmpipe)** | GPU rendering path with software fallback |
| **Vulkan (lavapipe)** | Vulkan rendering path with software fallback |

Using Mesa's software renderers (llvmpipe for OpenGL, lavapipe for Vulkan) enables GPU code path testing in headless CI environments without physical GPU hardware.

### 4.3 What to Visually Test

- **Window decorations**: Title bar rendering, button placement, border thickness, active/inactive states
- **Fluxbox-style tabs**: Tab bar rendering, tab switching visual feedback
- **Toolbar**: Fluxbox toolbar appearance, workspace names, clock, systray
- **Menus**: Right-click menu rendering, submenu positioning, highlight states
- **Slit**: Dock app container rendering and positioning
- **Theme/style application**: Verify that style files produce correct visual output
- **Window state indicators**: Maximized, minimized icon, shade (window rolled up), stuck (visible on all workspaces)
- **Multi-monitor**: Correct rendering when outputs have different scales, transforms, or resolutions

### 4.4 Frame Timing Validation

- Verify frame callbacks are delivered at the correct rate
- Test that damage tracking correctly limits redraws to damaged regions
- Verify presentation timestamps via the `wp_presentation` protocol

---

## 5. Input Testing

### 5.1 Virtual Input Devices

Using wlroots headless backend, create virtual input devices:

- **Virtual pointer**: Simulate motion, button press/release, scroll
- **Virtual keyboard**: Simulate key press/release with specific keycodes
- **Virtual touch**: Simulate touch down/motion/up for touch screen testing

The headless backend creates input devices where the test harness is responsible for raising event signals manually, giving complete control over input simulation.

### 5.2 Input Simulation Tools

| Tool | Capabilities | Use Case |
|------|-------------|----------|
| **ydotool** | Mouse + keyboard automation | End-to-end testing |
| **wtype** | Keyboard input simulation | Key binding testing |
| **wlrctl** | Wayland window/seat control | Window management testing |
| **Virtual input protocol** | `zwp_virtual_keyboard_v1`, `wlr_virtual_pointer_v1` | Programmatic input injection |

### 5.3 Input Test Scenarios

#### Keyboard
- Key binding resolution: modifier combinations (Mod1+Tab, Mod4+1, etc.)
- Key binding conflicts: overlapping bindings, shadowed bindings
- Key repeat: correct repeat rate and delay
- Keyboard layout switching: multiple layouts, per-window layout
- Keyboard grab: exclusive key bindings during fullscreen

#### Mouse/Pointer
- Focus-follows-mouse: focus changes correctly as pointer moves between surfaces
- Click-to-focus: focus only changes on click
- Sloppy focus: Fluxbox's sloppy focus variant
- Resize handles: edge and corner resize with correct cursors
- Window move: drag titlebar, snap to edges
- Mouse button bindings: configurable mouse button actions on titlebar, frame, client area

#### Edge Cases
- Input during workspace switch animation
- Input to partially obscured windows
- Input to subsurface vs parent surface
- Keyboard input during pointer grab (resize/move)
- Rapid focus changes (focus storm prevention)

---

## 6. Configuration Testing

### 6.1 Fluxbox Configuration Files

Fluxbox uses several configuration files that our compositor should support (or provide compatible alternatives):

| File | Purpose | Test Focus |
|------|---------|------------|
| `init` | Main configuration (key-value pairs) | Type validation, defaults, unknown keys |
| `keys` | Key bindings | Binding syntax, modifier parsing, chaining |
| `menu` | Root menu definition | Tag parsing, nesting, exec commands |
| `apps` | Per-application settings | Pattern matching, window matching, remembered state |
| `styles/*` | Visual themes | Property parsing, color formats, texture types |
| `startup` | Startup script | Command execution, environment setup |
| `overlay` | Keybinding overlay | Display formatting |

### 6.2 Configuration Test Strategy

#### Parser Unit Tests
For each config file type, test:
- **Valid input**: All documented syntax variants parse correctly
- **Comments**: Lines starting with `#` or `!` are ignored
- **Whitespace**: Leading/trailing whitespace, blank lines, tabs vs spaces
- **Edge cases**: Empty files, very long lines, Unicode content, binary garbage
- **Error recovery**: Malformed lines produce warnings but don't crash; parsing continues
- **Defaults**: Missing values fall back to documented defaults
- **Type validation**: Integer values reject strings, color values validate format

#### Round-Trip Tests
- Parse a config file → serialize back → parse again → compare internal state
- Ensures no information is lost during config reload

#### Regression Tests
- Maintain a corpus of real-world Fluxbox config files (with permission) as regression tests
- Test that config files from different Fluxbox versions parse correctly
- Test `[reconfig]` — re-reading config files at runtime applies changes correctly

#### Hot-Reload Tests (Integration)
- Modify a config file while the compositor is running
- Send reconfigure signal or trigger `[reconfig]`
- Verify changes take effect without restart
- Verify that invalid changes don't break the running compositor

### 6.3 Configuration Compatibility Matrix

Test a matrix of configuration options against expected behavior:

```
# Example test matrix for focus model
session.screen0.focusModel: ClickToFocus   → click on unfocused window → window gains focus
session.screen0.focusModel: MouseFocus      → hover over unfocused window → window gains focus
session.screen0.focusModel: StrictMouseFocus → move mouse outside all windows → no window focused
```

---

## 7. XWayland Testing

### 7.1 X11 Application Compatibility

XWayland enables running legacy X11 applications within the Wayland compositor. Key test areas:

#### Window Management
- X11 window mapping: `MapRequest` → visible Wayland surface
- Window hints: `WM_HINTS`, `WM_NORMAL_HINTS`, `WM_CLASS`, `_NET_WM_NAME`
- Window types: `_NET_WM_WINDOW_TYPE_NORMAL`, `_DIALOG`, `_SPLASH`, `_DOCK`, etc.
- Transient windows: `WM_TRANSIENT_FOR` correctly creates parent-child relationship
- ICCCM and EWMH compliance: `_NET_SUPPORTED`, `_NET_ACTIVE_WINDOW`, etc.
- Override-redirect windows: Menus, tooltips, popups from X11 apps

#### Clipboard and Selection
- Copy text from X11 app → paste in Wayland app (and vice versa)
- Copy images/rich content between X11 and Wayland apps
- PRIMARY selection (middle-click paste) synchronization
- CLIPBOARD selection synchronization
- Large data transfers (images, files)

#### Drag and Drop
- X11 XDND → Wayland `wl_data_device` translation
- Drag from X11 window to Wayland window
- Drag from Wayland window to X11 window
- Drag between two X11 windows via XWayland
- Drag cancel and failure handling

### 7.2 XWayland Test Applications

Use representative X11 applications for testing:

| Application | Tests |
|-------------|-------|
| **xterm** | Basic window, keyboard input, clipboard |
| **xeyes** | Override-redirect, shaped windows |
| **xclock** | Shaped windows, timer updates |
| **Firefox (X11 mode)** | Complex app, popups, clipboard, DnD |
| **GIMP** | Multi-window, dock windows, DnD |
| **xdpyinfo** | X11 display information verification |
| **xprop** | Window property verification |
| **xclipboard** | Clipboard operations |
| **xdotool** | X11 input simulation for testing |

### 7.3 Automated XWayland Tests

```
tests/
├── xwayland/
│   ├── test_x11_window_mapping.c     # Window lifecycle
│   ├── test_x11_window_types.c       # NET_WM_WINDOW_TYPE handling
│   ├── test_x11_clipboard.c          # Selection synchronization
│   ├── test_x11_dnd.c                # Drag and drop bridge
│   ├── test_x11_input.c              # Input event forwarding
│   ├── test_x11_fullscreen.c         # Fullscreen transition
│   └── meson.build
```

---

## 8. Performance Testing

### 8.1 Key Metrics

| Metric | Target | Tool |
|--------|--------|------|
| **Frame time** | < 16.6ms (60fps) | `wp_presentation` timestamps, internal profiling |
| **Frame time variance** | < 2ms jitter | Statistical analysis of frame times |
| **Input-to-display latency** | < 20ms | Timestamp correlation |
| **Startup time** | < 500ms to first frame | Wall clock measurement |
| **Memory usage (idle)** | < 50MB RSS | `/proc/self/status`, `VmRSS` |
| **Memory growth per window** | < 5MB per window | Monitor RSS as windows are created |
| **CPU usage (idle)** | < 1% | `top` / `perf stat` |
| **Config reload time** | < 100ms | Internal timing |

### 8.2 Performance Testing Approaches

#### Frame Time Profiling
- Use `wp_presentation` protocol to get hardware-level presentation timestamps
- Record per-frame timing over sustained periods (10+ seconds)
- Calculate mean, P50, P95, P99, max frame times
- Detect dropped frames (frame time > 2x target)
- Test under load: many windows, complex decorations, animations

#### Memory Profiling
- Track RSS at compositor startup
- Create N windows, measure memory growth
- Destroy all windows, verify memory returns close to baseline (no leaks)
- Long-running soak test: create and destroy windows repeatedly for hours
- Use Valgrind/memcheck for leak detection in CI

#### CPU Profiling
- Measure CPU usage when idle (no window activity)
- Measure CPU usage during continuous window operations (resize, move)
- Profile with `perf` to identify hot paths
- Ensure damage tracking limits redraws (CPU should drop when nothing changes)

#### Latency Measurement
- Use `WAYLAND_DEBUG=1` to trace request-event timing
- Measure time from input event to frame presentation
- Compare with other compositors as baseline
- Test latency under different load conditions

### 8.3 Benchmarking Tools

- **`weston-simple-egl`**: EGL rendering benchmark for Wayland
- **`glmark2-wayland`**: OpenGL benchmark suite with Wayland backend
- **`perf`**: Linux performance profiling
- **`heaptrack`**: Memory allocation profiling
- **`Wayland Rendering Analysis Tool`** (Tizen): FPS performance charts for Wayland rendering analysis
- **Custom benchmark clients**: Purpose-built clients that stress specific compositor paths

### 8.4 Performance Regression Detection

- Run benchmarks in CI on every merge request
- Store results in a time-series database or flat files
- Alert when metrics regress beyond threshold (e.g., >10% frame time increase)
- Compare results across branches and commits

---

## 9. CI/CD Pipeline

### 9.1 Environment Setup

#### Headless Testing Requirements

Wayland compositors can be tested in CI without physical displays using:

1. **wlroots headless backend**: No display server needed; creates virtual outputs and inputs in-memory
2. **Mesa llvmpipe**: Software OpenGL renderer — no GPU hardware needed
   - Set `LIBGL_ALWAYS_SOFTWARE=1` or `GALLIUM_DRIVER=llvmpipe`
3. **Mesa lavapipe**: Software Vulkan renderer — no GPU hardware needed
   - Set `VK_ICD_FILENAMES` to point to lavapipe ICD
4. **VKMS (Virtual KMS)**: Kernel module that pretends a display is connected, enabling KMS/DRM-backend testing on headless machines

#### Docker/Container Image

```dockerfile
# CI base image
FROM debian:bookworm

RUN apt-get update && apt-get install -y \
    # Build dependencies
    meson ninja-build gcc g++ pkg-config \
    # wlroots dependencies
    libwlroots-dev libwayland-dev libxkbcommon-dev \
    libpixman-1-dev libegl-dev libgles2-mesa-dev \
    # Software rendering
    mesa-utils libegl1-mesa libgl1-mesa-dri \
    # Testing tools
    wayland-utils wlcs \
    # XWayland
    xwayland xdotool xterm \
    # Coverage
    gcovr lcov \
    # Fuzzing
    libfuzzer-* afl++ \
    # Debugging
    gdb valgrind
```

### 9.2 Pipeline Stages

```yaml
# GitLab CI example (.gitlab-ci.yml)
stages:
  - build
  - test
  - analyze
  - benchmark

build:
  stage: build
  script:
    - meson setup build -Db_coverage=true -Db_sanitize=address,undefined
    - ninja -C build
  artifacts:
    paths: [build/]

unit-tests:
  stage: test
  script:
    - meson test -C build --suite unit --print-errorlogs
  artifacts:
    when: always
    reports:
      junit: build/meson-logs/testlog.junit.xml

integration-tests:
  stage: test
  variables:
    LIBGL_ALWAYS_SOFTWARE: "1"
    WLR_BACKENDS: "headless"
    WLR_RENDERER: "pixman"
  script:
    - meson test -C build --suite integration --print-errorlogs

wlcs-conformance:
  stage: test
  variables:
    LIBGL_ALWAYS_SOFTWARE: "1"
  script:
    - wlcs build/fluxway_wlcs_integration.so

visual-regression:
  stage: test
  variables:
    WLR_BACKENDS: "headless"
    WLR_RENDERER: "pixman"
  script:
    - meson test -C build --suite visual --print-errorlogs
    - python3 scripts/compare_screenshots.py
  artifacts:
    when: on_failure
    paths: [build/test-outputs/]

xwayland-tests:
  stage: test
  variables:
    WLR_BACKENDS: "headless"
    WLR_RENDERER: "pixman"
  script:
    - meson test -C build --suite xwayland --print-errorlogs

coverage:
  stage: analyze
  script:
    - ninja -C build coverage-html
  artifacts:
    paths: [build/meson-logs/coveragereport/]
  coverage: '/lines\.\.\.\.\.\.: (\d+\.\d+%)/'

static-analysis:
  stage: analyze
  script:
    - ninja -C build scan-build
    # or: scan-build meson compile -C build

sanitizer-tests:
  stage: analyze
  script:
    - meson setup build-asan -Db_sanitize=address,undefined
    - ninja -C build-asan
    - meson test -C build-asan --print-errorlogs

valgrind-memcheck:
  stage: analyze
  script:
    - meson test -C build --wrap='valgrind --error-exitcode=1 --leak-check=full' --suite unit

performance:
  stage: benchmark
  script:
    - meson test -C build --suite benchmark --print-errorlogs
    - python3 scripts/check_perf_regression.py
  only:
    - merge_requests
```

### 9.3 GitHub Actions Alternative

```yaml
# .github/workflows/ci.yml
name: CI
on: [push, pull_request]

jobs:
  build-and-test:
    runs-on: ubuntu-latest
    container: ghcr.io/fluxway/ci-base:latest
    env:
      LIBGL_ALWAYS_SOFTWARE: "1"
      WLR_BACKENDS: headless
      WLR_RENDERER: pixman
    steps:
      - uses: actions/checkout@v4
      - name: Build
        run: |
          meson setup build -Db_coverage=true
          ninja -C build
      - name: Unit Tests
        run: meson test -C build --suite unit --print-errorlogs
      - name: Integration Tests
        run: meson test -C build --suite integration --print-errorlogs
      - name: WLCS Conformance
        run: wlcs build/fluxway_wlcs_integration.so
      - name: Coverage Report
        run: ninja -C build coverage-html
      - name: Upload Coverage
        uses: actions/upload-artifact@v4
        with:
          name: coverage-report
          path: build/meson-logs/coveragereport/
```

### 9.4 Test Suites Organization in Meson

```meson
# Organize tests into suites for selective CI execution
test('window_placement', test_window_placement, suite: 'unit')
test('config_parser', test_config_parser, suite: 'unit')
test('xdg_toplevel', test_xdg_toplevel, suite: 'integration')
test('wlcs', wlcs_test, suite: 'conformance')
test('visual_decorations', test_visual_decorations, suite: 'visual')
test('xwayland_clipboard', test_xwayland_clipboard, suite: 'xwayland')
test('frame_time_benchmark', test_frame_time, suite: 'benchmark')
```

Run specific suites: `meson test -C build --suite unit`

---

## 10. Manual Testing Checklist

### 10.1 Critical User Workflows

These workflows require manual verification on real hardware with real display servers:

#### Window Lifecycle
- [ ] Open an application → window appears with correct decorations
- [ ] Close window via titlebar button → window destroyed, focus moves correctly
- [ ] Close window via keyboard shortcut → same as above
- [ ] Force-kill unresponsive application → compositor remains stable

#### Window Operations
- [ ] Move window by dragging titlebar → smooth, responsive
- [ ] Resize window by dragging edges/corners → correct minimum size enforcement
- [ ] Maximize window → fills available screen (respects panels/docks)
- [ ] Restore from maximize → returns to previous size and position
- [ ] Minimize window → removed from view, accessible from taskbar
- [ ] Shade window (roll up) → only titlebar visible
- [ ] Fullscreen → covers entire output including panels
- [ ] Stick window (all workspaces) → visible when switching workspaces

#### Fluxbox-Specific Features
- [ ] Tab grouping: drag titlebar onto another window → windows grouped
- [ ] Tab switching: click tab → correct window shown
- [ ] Tab ungrouping: drag tab away → window separated
- [ ] Toolbar: workspace name, window list, clock display correctly
- [ ] Slit: dock apps display and interact correctly
- [ ] Root menu: right-click desktop → menu appears, items work
- [ ] Window menu: right-click titlebar → context menu with all options
- [ ] Toolbar clock: correct time, updates

#### Focus Management
- [ ] Focus-follows-mouse: focus changes as mouse moves between windows
- [ ] Click-to-focus: focus only changes on mouse click
- [ ] Alt+Tab: cycle through windows in correct order
- [ ] Focus new windows: newly opened windows receive focus appropriately
- [ ] Focus returns correctly after closing focused window
- [ ] Focus follows mouse doesn't steal focus during typing

#### Workspace Management
- [ ] Switch workspace via keyboard shortcut
- [ ] Switch workspace via toolbar click
- [ ] Send window to another workspace
- [ ] Follow window to another workspace
- [ ] Workspace names display correctly in toolbar

#### Multi-Monitor
- [ ] Windows open on correct monitor
- [ ] Moving windows between monitors works smoothly
- [ ] Monitor hotplug: adding/removing monitors doesn't crash
- [ ] Different resolutions and scales per monitor
- [ ] Panel/toolbar placement on each monitor

#### Key Bindings
- [ ] All default key bindings work
- [ ] Custom key bindings from config file work
- [ ] Key bindings with multiple modifiers
- [ ] Key chains (multi-step key bindings)
- [ ] Key bindings don't interfere with application shortcuts

### 10.2 Edge Cases and Stress Tests

- [ ] Open 50+ windows simultaneously → compositor remains responsive
- [ ] Rapidly open and close windows → no crashes or leaks
- [ ] Unplug monitor while window is fullscreened on it → graceful fallback
- [ ] Suspend and resume laptop → compositor recovers correctly
- [ ] GPU reset/hang recovery → compositor handles or restarts gracefully
- [ ] Application requests extremely large window → handled correctly
- [ ] Application provides invalid protocol requests → error, not crash
- [ ] Compositor config file deleted while running → continues with defaults
- [ ] SIGTERM → clean shutdown, no orphan processes

### 10.3 Application Compatibility

Test with representative applications:

| Category | Applications |
|----------|-------------|
| **Terminal** | foot, alacritty, kitty, xterm (XWayland) |
| **Browser** | Firefox (Wayland), Chromium (Wayland), Firefox (XWayland) |
| **File manager** | Nautilus, Thunar, pcmanfm |
| **Text editor** | gedit, kate, vim (in terminal) |
| **IDE** | VS Code, IntelliJ (XWayland) |
| **Media** | mpv, VLC, GIMP |
| **Games** | Steam (XWayland), native Wayland games |
| **Office** | LibreOffice |
| **System** | System monitor, settings panels |

---

## 11. Fuzzing

### 11.1 Protocol Fuzzing

The Wayland protocol is the primary attack surface for a compositor. Fuzz the protocol parsing:

#### Target: Wayland Message Parsing
```c
// Fuzz target for Wayland protocol messages
// Compile: clang -fsanitize=fuzzer,address protocol_fuzz.c -o protocol_fuzz
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // Create a mock client connection
    // Feed raw bytes as Wayland protocol messages
    // Verify no crashes, memory errors, or undefined behavior
    return 0;
}
```

#### Target: Individual Protocol Handlers
Create fuzz targets for each protocol request handler:
- `wl_surface.attach` / `commit` / `damage`
- `xdg_toplevel.set_title` / `set_app_id` / `set_max_size`
- `wl_keyboard.key` / `wl_pointer.motion`

### 11.2 Configuration File Fuzzing

Fuzz all configuration file parsers:

```c
// Fuzz target for Fluxbox-style config file parsing
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // Null-terminate the input
    char *config = strndup((const char *)data, size);
    if (!config) return 0;

    // Parse config — should never crash regardless of input
    struct config *cfg = config_parse_string(config);
    config_destroy(cfg);
    free(config);
    return 0;
}
```

Fuzz targets for:
- `init` file parser
- `keys` file parser
- Menu file parser
- Style file parser
- `apps` file parser

### 11.3 Input Fuzzing

- Fuzz libinput event processing with random event sequences
- Fuzz keyboard layout/keymap handling with malformed XKB files
- Fuzz touch event sequences (invalid finger IDs, overlapping touches)

### 11.4 Fuzzing Infrastructure

| Tool | Use Case | Integration |
|------|----------|-------------|
| **libFuzzer** | In-process, coverage-guided | Compile with `-fsanitize=fuzzer,address` |
| **AFL++** | Fork-based, long campaigns | Compile with `afl-clang-fast` |
| **Honggfuzz** | Alternative coverage-guided fuzzer | Compile with `hfuzz-clang` |
| **OSS-Fuzz** | Continuous fuzzing service | Submit project for inclusion |

### 11.5 Sanitizers

Always run fuzzing with sanitizers enabled:

- **AddressSanitizer** (ASan): Detects buffer overflows, use-after-free, double-free
- **UndefinedBehaviorSanitizer** (UBSan): Detects undefined behavior (integer overflow, null deref)
- **MemorySanitizer** (MSan): Detects uninitialized memory reads
- **ThreadSanitizer** (TSan): Detects data races (if using threads)

Meson integration: `meson setup build -Db_sanitize=address,undefined`

### 11.6 Corpus Management

- Maintain a seed corpus of valid protocol messages, config files, and input sequences
- Store crash-reproducing inputs in the repository
- Run regression tests with previous crash inputs to prevent regressions
- Use coverage-guided fuzzing to discover new code paths

---

## 12. Accessibility Testing

### 12.1 Overview

Wayland accessibility is an evolving landscape. The primary framework is AT-SPI2 (Assistive Technology Service Provider Interface) over D-Bus, which allows toolkit widgets to expose their content to screen readers like Orca.

### 12.2 Compositor Responsibilities

A compositor has specific accessibility responsibilities:

- **Keyboard navigation**: All compositor UI (menus, toolbar, window list) must be keyboard-navigable
- **Focus indication**: Clear visual focus indicators for keyboard users
- **High contrast**: Support high-contrast themes for visually impaired users
- **Reduced motion**: Option to disable animations
- **Screen reader support**: Compositor notifications (workspace switch, focus change) should be announced

### 12.3 AT-SPI2 Integration

The compositor should integrate with AT-SPI2 to:
- Announce window focus changes
- Announce workspace switches
- Make menus and toolbar accessible
- Provide window titles and state to screen readers

**Newton project** (GNOME): A Wayland-native accessibility approach where Orca can work with Wayland applications. The compositor should support this as the ecosystem matures.

Reference: [Newton project update](https://blogs.gnome.org/a11y/2024/06/18/update-on-newton-the-wayland-native-accessibility-project/)

### 12.4 Accessibility Test Plan

#### Keyboard-Only Navigation
- [ ] All window operations accessible via keyboard (move, resize, maximize, minimize, close)
- [ ] Menu navigation via keyboard (arrow keys, Enter, Escape)
- [ ] Workspace switching via keyboard
- [ ] Window list/Alt+Tab fully keyboard accessible
- [ ] Toolbar interaction via keyboard
- [ ] No keyboard traps (can always Tab/Escape out of any state)

#### Screen Reader Compatibility
- [ ] Install and configure Orca
- [ ] Focus changes announced by screen reader
- [ ] Workspace changes announced
- [ ] Window titles read on focus
- [ ] Menu items read when navigated
- [ ] Error/notification messages announced

#### Visual Accessibility
- [ ] High-contrast theme renders correctly
- [ ] All text meets WCAG AA contrast ratio (4.5:1)
- [ ] Focus indicators are visible in all themes
- [ ] No information conveyed solely by color
- [ ] UI scales correctly at 1.5x and 2x DPI

#### Reduced Motion
- [ ] Animation disable option works
- [ ] No flashing content (avoid seizure triggers)
- [ ] Workspace switch works without animation

### 12.5 Automated Accessibility Testing

- Use `accerciser` (AT-SPI2 inspector) to verify accessible tree structure
- Script Orca to verify announcements are made for key compositor events
- Use `pyatspi2` for programmatic accessibility testing:

```python
# Example: verify window title announcement
import pyatspi

def on_focus(event):
    assert event.source.name != ""  # Focused element has a name
    assert event.source.getRole() != pyatspi.ROLE_UNKNOWN

pyatspi.Registry.registerEventListener(on_focus, "focus:")
pyatspi.Registry.start()
```

---

## 13. Test Organization and Infrastructure

### 13.1 Directory Structure

```
tests/
├── meson.build                    # Top-level test configuration
├── common/
│   ├── test_helpers.h             # Shared test utilities
│   ├── test_fixtures.h            # Common test fixtures
│   ├── mock_wlroots.h             # Mock wlroots interfaces
│   └── wayland_test_client.h      # Test Wayland client helpers
├── unit/
│   ├── meson.build
│   ├── test_window_placement.c
│   ├── test_focus_management.c
│   ├── test_workspace.c
│   ├── test_config_parser.c
│   ├── test_keybinding_parser.c
│   ├── test_menu_parser.c
│   ├── test_style_parser.c
│   └── test_geometry.c
├── integration/
│   ├── meson.build
│   ├── test_xdg_toplevel.c
│   ├── test_xdg_popup.c
│   ├── test_layer_shell.c
│   ├── test_clipboard.c
│   └── test_drag_and_drop.c
├── visual/
│   ├── meson.build
│   ├── test_decorations.c
│   ├── test_tabs.c
│   ├── test_menus.c
│   ├── reference/                 # Reference screenshots
│   │   ├── decorations_active.png
│   │   ├── decorations_inactive.png
│   │   └── ...
│   └── compare.py                 # Screenshot comparison script
├── xwayland/
│   ├── meson.build
│   ├── test_x11_window_mapping.c
│   ├── test_x11_clipboard.c
│   └── test_x11_dnd.c
├── performance/
│   ├── meson.build
│   ├── bench_frame_time.c
│   ├── bench_startup.c
│   ├── bench_memory.c
│   └── baselines/                 # Performance baselines
├── fuzz/
│   ├── meson.build
│   ├── fuzz_protocol.c
│   ├── fuzz_config_init.c
│   ├── fuzz_config_keys.c
│   ├── fuzz_config_menu.c
│   ├── fuzz_config_style.c
│   └── corpus/                    # Seed corpus for fuzzers
│       ├── protocol/
│       ├── config/
│       └── style/
├── wlcs/
│   ├── meson.build
│   └── fluxway_wlcs_integration.c
└── data/
    ├── test_configs/              # Test configuration files
    │   ├── valid/
    │   ├── invalid/
    │   └── edge_cases/
    ├── test_styles/               # Test style/theme files
    └── test_menus/                # Test menu files
```

### 13.2 Test Data Management

- **Golden files**: Reference screenshots and expected output stored in `tests/visual/reference/` and `tests/data/`
- **Test configurations**: Known-good and intentionally-broken config files in `tests/data/test_configs/`
- **Fuzz corpus**: Seed inputs for fuzzers in `tests/fuzz/corpus/`, crash reproductions tracked in git
- **Performance baselines**: Stored in `tests/performance/baselines/`, updated manually after confirmed improvements

### 13.3 Test Naming Conventions

- Test files: `test_<module>.c`
- Fuzz targets: `fuzz_<target>.c`
- Benchmarks: `bench_<metric>.c`
- Test functions: `test_<module>_<scenario>` (e.g., `test_config_parser_empty_file`)

---

## 14. Recommended Implementation Order

### Phase 1: Foundation (Before First Feature)
1. Set up Meson test infrastructure with test harness
2. Write unit tests for geometry/math utilities
3. Write unit tests for config file parsers
4. Set up CI pipeline with headless testing (llvmpipe)
5. Enable ASan/UBSan in CI builds

### Phase 2: Core Compositor (With First Working Compositor)
6. Implement WLCS integration module
7. Write client integration tests for basic surface lifecycle
8. Write unit tests for window placement and focus management
9. Add Valgrind memcheck to CI
10. Begin configuration fuzzing

### Phase 3: Feature Testing (As Features Land)
11. Add visual regression tests for window decorations
12. Write input simulation tests for key bindings and mouse operations
13. Add XWayland integration tests
14. Begin performance benchmarking baseline
15. Write workspace management tests

### Phase 4: Polish and Hardening
16. Full protocol fuzzing campaign
17. Accessibility audit and testing
18. Performance regression detection in CI
19. Manual testing with real applications on real hardware
20. Submit to OSS-Fuzz for continuous fuzzing

### Phase 5: Ongoing
- Expand test coverage with each new feature
- Update visual regression baselines when UI changes
- Maintain and update fuzz corpus
- Regular manual testing with latest application versions
- Monitor CI reliability and fix flaky tests promptly

---

## References

- [Weston Test Suite Documentation](https://wayland.pages.freedesktop.org/weston/toc/test-suite.html)
- [WLCS - Wayland Conformance Test Suite](https://github.com/canonical/wlcs)
- [wlroots - Modular Wayland Compositor Library](https://github.com/swaywm/wlroots)
- [Wayland Debugging Extras](https://wayland.freedesktop.org/extras.html)
- [Weston Test Protocol](https://wayland.app/protocols/weston-test)
- [Testing Weston DRM/KMS Backends with VKMS](https://www.collabora.com/news-and-blog/blog/2020/08/07/testing-weston-drm-kms-backends-with-virtme-and-vkms/)
- [Newton - Wayland-native Accessibility](https://blogs.gnome.org/a11y/2024/06/18/update-on-newton-the-wayland-native-accessibility-project/)
- [Wayland Accessibility Notes](https://github.com/splondike/wayland-accessibility-notes)
- [AT-SPI2 - freedesktop.org](https://www.freedesktop.org/wiki/Accessibility/AT-SPI2/)
- [Meson Build System - Unit Tests](https://mesonbuild.com/Unit-tests.html)
- [Mesa LLVMpipe Documentation](https://docs.mesa3d.org/drivers/llvmpipe.html)
- [Wayland Rendering Analysis Tool](https://wiki.tizen.org/Wayland_Rendering_Analysis_Tool)
- [Wayland Clipboard and Drag-and-Drop](https://emersion.fr/blog/2020/wayland-clipboard-drag-and-drop/)
- [libFuzzer Documentation](https://llvm.org/docs/LibFuzzer.html)
- [wlr-test Protocol Discussion](https://github.com/swaywm/wlroots/issues/494)
- [Wayland-fits - Functional Integration Test Suite](https://github.com/tizenorg/test.generic.wayland-fits)
