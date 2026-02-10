# Research Summary: fluxland

> Synthesized findings from all research tracks for the fluxland project —
> a Fluxbox-inspired Wayland compositor.

---

## 1. Executive Overview

This document consolidates findings from four parallel research efforts:

1. **Fluxbox UI/UX Research** — Comprehensive analysis of Fluxbox's feature set, configuration system, and user experience patterns
2. **Wayland Compositor Architecture** — Study of Wayland protocols, compositor libraries (wlroots, Smithay), and reference compositors (labwc, Sway, Wayfire, Hyprland)
3. **Linux System Programming** — Language selection, build systems, kernel interfaces, event loop design, memory management, IPC, security, and performance
4. **QA Testing Strategy** — Testing frameworks, CI/CD pipeline, fuzzing, accessibility, and validation approaches

The research establishes a clear consensus on technology choices and identifies the critical path for building a viable Fluxbox successor on Wayland.

---

## 2. Consensus Recommendations

### 2.1 Compositor Library: wlroots

All research tracks converge on **wlroots** as the compositor library:

- **Most mature** Wayland compositor library (~60,000 lines of battle-tested C)
- **Scene graph API** handles damage tracking and 2D compositing — ideal for a stacking WM
- **labwc validates** the approach: a stacking WM (Openbox-inspired) successfully built on wlroots
- **Comprehensive protocol support**: xdg-shell, layer-shell, xdg-decoration, foreign-toplevel, output-management, session-lock, and more
- **XWayland integration** provided out of the box via `wlr_xwayland`
- **Session management** via `wlr_session` (logind/seatd) for non-root operation
- **Headless backend** enables automated testing without physical displays

Alternative considered: **Smithay** (Rust) — validated by COSMIC DE and Niri, but younger ecosystem and steeper learning curve. Could be revisited if Rust is preferred.

### 2.2 Language: C or C++

The research identifies a tension between two viable choices:

| Factor | C | C++ |
|--------|---|-----|
| wlroots interop | Native | Direct (C linkage) |
| Ecosystem alignment | Sway, labwc, Weston | Wayfire, KWin, Fluxbox itself |
| Memory safety | Manual | RAII (partial safety) |
| Resource management | Error-prone cleanup | Deterministic via RAII |
| Community familiarity | Wayland developers | Fluxbox contributors |

**Wayland research recommends C** (aligned with wlroots, labwc, Sway).
**System programming research recommends C++20** (RAII safety, Fluxbox heritage, plugin potential).

**Resolution**: Either is viable. C is lower-risk for wlroots integration. C++ with C++20 features offers better resource safety. The architecture document assumes C, but wrapping wlroots types in C++ RAII wrappers is a valid hybrid approach. The final decision should weigh team expertise.

### 2.3 Build System: Meson + Ninja

Unanimous recommendation. Meson is the de facto standard for the Wayland ecosystem:
- Used by wlroots, Sway, labwc, Weston, Wayfire
- Native pkg-config and wayland-scanner integration
- Built-in test infrastructure with suite support
- Subproject/wrap support for dependency management

### 2.4 Architecture Model: labwc

**labwc** is identified as the closest architectural reference:
- Stacking WM on wlroots (like our project, but Openbox-inspired vs Fluxbox-inspired)
- Server-side decorations with theming
- Uses wlroots scene graph API
- Clean, focused C codebase
- Supports all essential Wayland protocols

We extend the labwc model with Fluxbox-specific features: window tabbing, Fluxbox config compatibility, chainable keybindings, keymodes, and the slit.

---

## 3. Priority Fluxbox Features

Based on the Fluxbox research, these features are ranked by user impact and differentiation:

### Tier 1: Core Identity (Must Ship in v1.0)

1. **Window Tabbing/Grouping**
   - Fluxbox's signature feature, underserved in the Wayland ecosystem
   - Tabs in titlebar (default) or external tab placement
   - Auto-grouping via window rules
   - Tab operations: drag-to-group, detach, reorder, next/prev tab
   - No standard Wayland protocol exists — entirely compositor-side logic

2. **Chainable Key Bindings**
   - Emacs-style multi-key sequences (e.g., `Mod4 x Mod4 t :Exec xterm`)
   - Escape to abort mid-chain
   - Fluxbox `keys` file format compatibility

3. **Keymodes**
   - Named binding sets (like Vim modes)
   - `KeyMode` command switches active binding set
   - Enables specialized interaction modes (resize mode, launch mode, etc.)

4. **Server-Side Decorations**
   - Fluxbox-style titlebar with configurable buttons (Close, Maximize, Minimize, Shade, Stick)
   - Multi-functional maximize button (left=full, middle=vertical, right=horizontal)
   - Window handle, grips, borders
   - Focused/unfocused states with gradient/solid/pixmap textures
   - Decoration presets (NORMAL, NONE, BORDER, TAB, TINY, TOOL) and bitmask control

5. **Per-Window Rules (apps file)**
   - Pattern matching: name, class, title, role (mapped to Wayland app-id + title)
   - Rich rule set: workspace, dimensions, position, decorations, layer, alpha, sticky, etc.
   - Auto-grouping sections for automatic tabbing
   - Rules rescanned on each new window (no reload needed)

### Tier 2: Complete Experience (Should Ship in v1.0)

6. **Focus Policies**: Click-to-focus, sloppy focus, strict mouse focus with auto-raise
7. **Workspace Management**: Named workspaces, wrap-around, send/take-to-workspace, workspace warping
8. **Menu System**: Root menu (right-click desktop), window menu, nested submenus, pipe menus
9. **Window Shading**: Roll window up to titlebar only (double-click titlebar)
10. **Edge Snapping**: Snap to screen edges and other windows within configurable threshold
11. **Style/Theme System**: Fluxbox-compatible style format (textures, gradients, colors, fonts, pixmaps)
12. **Six-Layer Stacking**: AboveDock, Dock, Top, Normal, Bottom, Desktop

### Tier 3: Polish (Post-v1.0)

13. **Toolbar**: Built-in panel with workspace name, iconbar, system tray, clock
14. **Slit**: Dockapp container (partial — X11 dockapps need XWayland)
15. **Style Overlay**: Global theme overrides
16. **Window Transparency**: Per-window alpha (focused/unfocused)
17. **Placement Strategies**: RowSmart, ColSmart, Cascade, UnderMouse, MinOverlap
18. **ArrangeWindows**: Auto-layout commands (vertical, horizontal, stacked)

---

## 4. Technology Stack Summary

| Component | Choice | Rationale |
|-----------|--------|-----------|
| Display protocol | Wayland | Industry direction, security model |
| Compositor library | wlroots >= 0.18 | Most mature, scene graph API, labwc validates |
| Language | C (C17) or C++20 | Direct wlroots interop; team decides |
| Build system | Meson + Ninja | Ecosystem standard |
| Rendering | wlroots scene graph (GLES2/Vulkan/Pixman) | Handles damage tracking automatically |
| Input | libinput (via wlroots) | Standard for Wayland |
| Keyboard handling | libxkbcommon | XKB keymap format |
| Session management | libseat (logind/seatd) | Non-root operation, VT switching |
| Text rendering | Pango + Cairo | For titlebar text, menus |
| Image loading | libpng, libjpeg | For theme pixmaps |
| IPC | Unix domain socket (JSON) | Scriptable control (like Hyprland/Sway) |
| Desktop integration | D-Bus (sd-bus) | Portals, notifications, logind |
| X11 compatibility | XWayland (optional) | Legacy app support |
| Testing framework | Google Test + WLCS + Meson test | Unit + protocol conformance |
| CI rendering | Mesa llvmpipe/lavapipe | Software rendering for headless CI |

---

## 5. Key Architectural Decisions

### 5.1 Scene Graph for Rendering

Use the wlroots scene graph API (`wlr_scene`) rather than implementing custom rendering:
- Automatic damage tracking
- Efficient output management
- Handles surface-to-output mapping
- Sufficient for a stacking WM's needs (no 3D effects needed)
- Custom rendering only for decorations (title bars, menus)

### 5.2 Server-Side Decorations as Primary Mode

Fluxbox's identity is its window decorations. SSD gives full control:
- Use `xdg-decoration` protocol to negotiate SSD preference
- Fallback gracefully when clients insist on CSD
- Draw decorations using Cairo/Pango or direct OpenGL

### 5.3 Fluxbox Config via Translation Layer

Support Fluxbox config files by parsing and translating to internal format:
- X11 modifier names (Mod1, Mod4) mapped to Wayland equivalents
- X11 WM_CLASS mapped to Wayland app-id
- X11-specific actions warned/skipped
- Users can choose native or Fluxbox-compatible config
- Ship a `fluxland-import` migration utility

### 5.4 IPC: Unix Socket + D-Bus

Two complementary IPC channels:
- **Command socket**: Synchronous request/response for scripting (`fluxland-ctl`)
- **Event socket**: Async event broadcast for status bars and automation
- **D-Bus**: Desktop integration (portals, notifications, screen lock)

### 5.5 Testing from Day One

The QA research emphasizes test infrastructure before features:
- Headless backend + Pixman renderer for deterministic CI testing
- WLCS integration for protocol conformance
- Config parser fuzzing from the start
- ASan/UBSan in all CI builds
- Visual regression testing for decorations

---

## 6. Suggested Implementation Roadmap

### Phase 1: Skeleton Compositor
- Start from TinyWL (wlroots reference compositor)
- Basic window management: create, destroy, focus, stack
- Headless backend working, CI pipeline running
- Meson build system, test infrastructure

### Phase 2: Core Window Management
- Server-side decorations (basic titlebar with buttons)
- Click-to-focus and sloppy focus
- Window move and resize (mouse and keyboard)
- Window states: maximize, minimize, fullscreen, shade
- Workspaces (switch, send-to, named)

### Phase 3: Fluxbox Identity Features
- Window tabbing and grouping
- Configurable keybindings (Fluxbox `keys` file format)
- Key chains and keymodes
- Per-window rules (Fluxbox `apps` file format)
- Basic menu system (root menu, window menu)
- Edge snapping

### Phase 4: Theming and Configuration
- Style/theme system (Fluxbox-compatible style format)
- Init file parser (Fluxbox resource format)
- Titlebar button layout configuration
- Six-layer stacking model
- Focus policies (all three modes)
- XWayland support

### Phase 5: Desktop Integration
- Layer-shell support (for external panels, launchers, wallpaper, lock screen)
- IPC (command socket + event socket)
- Toolbar (built-in or external via layer-shell)
- Session lock protocol
- `fluxland-import` migration tool
- D-Bus interface

### Phase 6: Polish and Release
- Slit (dockapp container)
- Window transparency
- Placement strategies
- ArrangeWindows commands
- Performance optimization (direct scanout, multi-plane)
- Comprehensive manual testing
- Documentation and man pages
- v1.0 release

---

## 7. Risks and Open Questions

### 7.1 Technical Risks

| Risk | Impact | Mitigation |
|------|--------|------------|
| wlroots API breakage between versions | Build breaks, feature gaps | Pin wlroots version, track upstream closely |
| Tab grouping complexity | High development effort | Start with basic tabbing, iterate; study Wayfire's grouping |
| SSD rendering performance | Sluggish decorations | Profile early, use GPU-accelerated rendering for decorations |
| Fluxbox config edge cases | Incomplete compatibility | Focus on common patterns first; document known gaps |
| XWayland positioning | X11 apps mispositioned | Study labwc/Sway XWayland implementation |
| Multi-monitor HiDPI | Per-output scaling issues | Use wlroots fractional-scale protocol support |

### 7.2 Open Questions

1. **C vs C++**: Final language decision impacts contributor base and code patterns. Need team input.
2. **Built-in vs external toolbar**: Fluxbox has a built-in toolbar. Should we build one in, or rely on external panels (waybar) via layer-shell? Trade-off: control vs ecosystem compatibility.
3. **Slit implementation**: Traditional X11 dockapps require XWayland. Should we define a native Wayland dockapp protocol?
4. **Plugin system**: Should we support a Wayfire-style plugin architecture for extensibility? Adds complexity but enables community extensions.
5. **Configuration format**: Support only Fluxbox format, or also define a native format? Having both adds maintenance burden but eases migration.
6. **Explicit sync**: Required for NVIDIA GPU support. wlroots 0.18+ supports `wp_linux_drm_syncobj`. Should we mandate wlroots >= 0.18?
7. **Wayland protocol evolution**: Several protocols are still "unstable" (xdg-decoration, layer-shell). How do we handle protocol version updates?

### 7.3 Areas Needing Further Investigation

- **Tab group rendering**: How to efficiently render tab bars within the scene graph. Need to prototype.
- **Fluxbox gradient rendering**: Fluxbox supports 8 gradient types + interlacing + bevels. Need to evaluate GPU shader approach vs Cairo rendering.
- **Accessibility**: Wayland accessibility (AT-SPI2, Newton project) is still maturing. Should track ecosystem progress.
- **Gamepad/tablet input**: Fluxbox didn't handle these. Need to decide on scope for Wayland.
- **Screen recording/sharing**: Portal integration for PipeWire-based screen sharing.

---

## 8. Key References

### Primary Architectural References
- [labwc](https://github.com/labwc/labwc) — Closest existing compositor to our goals
- [wlroots](https://github.com/swaywm/wlroots) — Compositor library
- [TinyWL](https://gitlab.freedesktop.org/wlroots/wlroots/-/tree/master/tinywl) — Minimal starting point

### Fluxbox Documentation
- [Fluxbox Man Pages](https://fluxbox.org/help/man-fluxbox/) — Authoritative feature reference
- [Fluxbox Keys](https://fluxbox.org/help/man-fluxbox-keys/) — Key binding system
- [Fluxbox Apps](https://fluxbox.org/help/man-fluxbox-apps/) — Per-window rules
- [Fluxbox Style](https://fluxbox.org/help/man-fluxbox-style/) — Theme system

### Wayland Protocol Resources
- [The Wayland Protocol Book](https://wayland-book.com/) — Comprehensive protocol guide
- [Wayland Explorer](https://wayland.app/protocols/) — Interactive protocol reference
- [Writing a Wayland Compositor](https://drewdevault.com/2018/02/17/Writing-a-Wayland-compositor-1.html) — Tutorial series

### Testing
- [WLCS](https://github.com/canonical/wlcs) — Protocol conformance suite
- [Weston Test Suite](https://wayland.pages.freedesktop.org/weston/toc/test-suite.html) — Test architecture reference

---

*This summary was generated from research conducted in February 2026 for the fluxland project.*
