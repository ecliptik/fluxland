# Wayland Compositor Architecture Research

> Research document for building a stacking/floating window manager compositor.
> Covers protocol fundamentals, compositor libraries, key protocols, input handling,
> rendering pipeline, XWayland, session management, and existing compositor references.

---

## Table of Contents

1. [Wayland Protocol Fundamentals](#1-wayland-protocol-fundamentals)
2. [XDG Shell Protocol](#2-xdg-shell-protocol)
3. [Compositor Libraries](#3-compositor-libraries)
4. [Key Protocols for Window Management](#4-key-protocols-for-window-management)
5. [Input Handling](#5-input-handling)
6. [Rendering Pipeline](#6-rendering-pipeline)
7. [XWayland](#7-xwayland)
8. [Session Management](#8-session-management)
9. [Existing Compositor References](#9-existing-compositor-references)
10. [Implications for Our Project](#10-implications-for-our-project)

---

## 1. Wayland Protocol Fundamentals

### 1.1 Architecture Overview

Wayland is a protocol for a compositor to talk to its clients, as well as a library
implementing that protocol. The compositor can be a standalone display server running on
Linux kernel modesetting and evdev input devices, an X application, or a Wayland client
itself. Clients can be traditional applications, X servers (rootless or fullscreen), or
other display servers.

Key architectural differences from X11:
- **Direct rendering model**: Clients render their own content into buffers shared with
  the compositor (no server-side rendering)
- **No global coordinate space**: Clients have no knowledge of their position on screen
- **No protocol-level window management**: The compositor IS the window manager
- **Object-oriented protocol**: Resources are identified by numeric IDs, communicated over
  a UNIX domain socket (typically named `wayland-0`)

### 1.2 Core Protocol Objects

#### wl_display
The root object. Represents the connection to the compositor. Provides the `wl_registry`
for discovering global objects.

#### wl_registry
Enumerates global objects (interfaces) the compositor offers. Clients bind to globals
they need (e.g., `wl_compositor`, `wl_shm`, `wl_seat`).

#### wl_compositor
The compositor global. Its primary role is creating `wl_surface` objects. The compositor
combines surfaces into the final displayed output. Request: `create_surface()` and
`create_region()`.

#### wl_surface
The most fundamental visual object — a rectangular area that can display pixel content.
Key characteristics:
- Has **pending state** and **applied (current) state** — double-buffered
- State includes: buffer, damage region, opaque region, input region, transform, scale
- `wl_surface.commit()` atomically applies all pending state
- No state at all when first created; must be assigned a **role** (e.g., xdg_toplevel,
  xdg_popup, subsurface, cursor, layer surface)
- Each surface can have at most one role at any time

#### wl_buffer
Represents pixel content. Can be backed by:
- **wl_shm**: CPU-accessible shared memory (simple, portable)
- **linux-dmabuf**: GPU buffer via DMA-BUF file descriptors (zero-copy, hardware-accelerated)
- **wl_drm** (legacy): Older GPU buffer sharing mechanism

#### wl_shm / wl_shm_pool
Shared memory interface for CPU rendering. Client creates a shared memory pool,
allocates buffers from it. Simple but involves a copy to GPU for compositing.

#### wl_region
Describes a set of rectangles. Used for specifying opaque regions (optimization hint)
and input regions (hit testing).

#### wl_callback
Used for frame callbacks — the compositor signals when it's a good time to draw the
next frame, enabling VSync-aligned rendering.

### 1.3 Double-Buffered State Model

All surface state changes are double-buffered:

```
Client                          Compositor
  |                                 |
  |-- attach(buffer) ------------->|  (pending)
  |-- damage(rect) -------------->|  (pending)
  |-- set_opaque_region(region) ->|  (pending)
  |-- commit() ------------------>|  pending -> current (atomic)
  |                                 |
  |<-- frame callback ------------|  (next frame opportunity)
```

The double-buffering ensures no tearing or partial state updates. The compositor only
sees consistent, committed state.

### 1.4 wl_output

Represents a physical display/monitor. Provides:
- Make, model, physical dimensions
- Current mode (resolution, refresh rate)
- Scale factor (for HiDPI)
- Transform (rotation, flipping)
- Position within the output layout

Clients use this to adapt their rendering for the displays they're shown on.

### 1.5 Subsurfaces (wl_subsurface)

Allow building complex surface trees:
- Child surface positioned relative to parent
- Z-ordered relative to siblings and parent
- Two modes: **synchronized** (commit with parent) or **desynchronized** (independent)
- Used for things like video overlays, embedded content, complex widget hierarchies
- NOT for popup menus or tooltips (use xdg_popup for those)

---

## 2. XDG Shell Protocol

### 2.1 Overview

`xdg-shell` (protocol name: `xdg_wm_base`) is the standard protocol extension for
application windows. It replaces the deprecated `wl_shell` and defines semantics for
desktop application windows.

### 2.2 Object Hierarchy

```
xdg_wm_base (global)
  └── xdg_surface (role assignment for wl_surface)
        ├── xdg_toplevel (application window)
        └── xdg_popup (dropdown/context menu/tooltip)
```

### 2.3 xdg_wm_base

Global singleton. Responsibilities:
- Creating `xdg_surface` objects from `wl_surface`
- Ping/pong protocol for detecting unresponsive clients
- If a client doesn't respond to ping within a timeout, compositor can show "not
  responding" decoration or offer to kill

### 2.4 xdg_surface

Wrapper around `wl_surface` that adds desktop window semantics:
- Must be assigned a role via `get_toplevel()` or `get_popup()`
- Handles the **configure/ack_configure lifecycle**
- Provides window geometry (the "visible" bounds, excluding shadows/decorations)

**Mapping conditions** (all must be met for the surface to appear):
1. An xdg role has been assigned (toplevel or popup)
2. The client has acked a configure event
3. A buffer has been committed

### 2.5 xdg_toplevel

The primary application window role. Features:

**Requests (client -> compositor):**
- `set_title(string)` — window title
- `set_app_id(string)` — application identifier (like X11 WM_CLASS)
- `set_parent(xdg_toplevel)` — parent window relationship (for dialogs)
- `move(seat, serial)` — initiate interactive move
- `resize(seat, serial, edges)` — initiate interactive resize
- `set_min_size(w, h)` / `set_max_size(w, h)` — size constraints
- `set_maximized()` / `unset_maximized()`
- `set_fullscreen(output)` / `unset_fullscreen()`
- `set_minimized()` — request minimize (no unset; compositor manages)
- `show_window_menu(seat, serial, x, y)` — request right-click title bar menu

**Events (compositor -> client):**
- `configure(width, height, states)` — suggested size and state
- `close()` — request to close (client may show save dialog)
- `configure_bounds(width, height)` — maximum useful size hint
- `wm_capabilities(capabilities)` — what the WM supports (maximize, minimize, fullscreen, window_menu)

**Window States** (sent in configure):
- `maximized` — fills available area minus panels
- `fullscreen` — covers entire output
- `activated` — has keyboard focus
- `resizing` — currently being interactively resized
- `tiled_left`, `tiled_right`, `tiled_top`, `tiled_bottom` — tiled to edges
- `suspended` — not visible, should reduce resource usage

### 2.6 xdg_popup

For transient surfaces like menus, tooltips, combo boxes:
- Positioned relative to a parent (xdg_surface or xdg_toplevel)
- Uses an `xdg_positioner` for placement rules and constraint adjustment
- Can be repositioned if it would overflow screen edges
- Supports `grab` for exclusive pointer/keyboard input (e.g., dropdown menus)
- Dismissed when clicking outside or pressing Escape

**xdg_positioner** allows specifying:
- Anchor rectangle on parent
- Anchor edge/corner
- Gravity (which direction popup extends)
- Constraint adjustment (flip, slide, resize) when popup would go off-screen
- Offset from anchor point

### 2.7 Configure/Ack Lifecycle

This is the central coordination mechanism between compositor and client:

```
Compositor                        Client
  |                                  |
  |-- xdg_toplevel.configure ------>|  (width, height, states)
  |-- xdg_surface.configure ------->|  (serial)
  |                                  |
  |                                  |  (client adjusts to suggested config)
  |                                  |
  |<-- xdg_surface.ack_configure ---|  (serial)
  |<-- wl_surface.commit -----------|  (new buffer reflecting config)
  |                                  |
```

**Important rules:**
- Compositor MAY send configure with 0x0 size (client chooses its own size)
- Client MUST ack_configure before or alongside the next commit
- Client can ack a configure and commit with a different size (within constraints)
- Multiple configures can arrive before client acks; client only needs to ack the last one

---

## 3. Compositor Libraries

### 3.1 wlroots (C)

**Repository:** https://github.com/swaywm/wlroots
**Language:** C (with a Meson build system)
**License:** MIT
**Current version:** 0.19.x (as of early 2026)

#### Architecture

wlroots provides ~60,000 lines of code implementing common Wayland compositor
functionality. It is designed as "pluggable, composable, unopinionated modules."

**Core subsystems:**

1. **Backend System** — Hardware abstraction layer
   - **DRM/KMS backend**: Direct rendering on TTY (production use)
   - **libinput backend**: Input device handling
   - **Wayland backend**: Run compositor as a Wayland client (development)
   - **X11 backend**: Run compositor as an X11 client (development)
   - **Headless backend**: No display, for testing
   - Backend auto-detection based on environment

2. **Renderer Abstraction**
   - **GLES2 renderer**: OpenGL ES 2.0, the default and most mature
   - **Vulkan renderer**: Alternative renderer, gaining maturity
   - **Pixman renderer**: Software renderer, useful for headless/testing
   - Renderer selection at runtime via `WLR_RENDERER` environment variable

3. **Scene Graph API** (`wlr_scene`)
   The recommended high-level API for compositors:
   - Tree of nodes representing on-screen objects
   - Node types: `WLR_SCENE_NODE_TREE`, `WLR_SCENE_NODE_SURFACE`,
     `WLR_SCENE_NODE_RECT`, `WLR_SCENE_NODE_BUFFER`
   - Handles damage tracking automatically
   - Integrates with `wlr_output_layout` for multi-monitor
   - Integrates with `wlr_presentation` for latency tracking
   - Handles surface-to-output mapping, presentation feedback
   - Limited to basic 2D operations (positioning, stacking, visibility)
   - For advanced effects, compositors must implement custom rendering

4. **Output Management** (`wlr_output`, `wlr_output_layout`)
   - Output enumeration, mode setting, VRR
   - Output layout: spatial arrangement of outputs
   - Frame scheduling, damage tracking per output

5. **Protocol Implementations**
   - Implements many Wayland protocols as wlr_* types
   - Each type has signal-based event notification
   - Compositor listens to signals and responds

#### Signal-based Event Model

wlroots uses a signal/listener pattern (from libwayland):

```c
// Compositor code
struct wlr_xdg_toplevel *toplevel;
wl_signal_add(&toplevel->events.request_move, &my_move_listener);

// Listener callback
static void handle_request_move(struct wl_listener *listener, void *data) {
    // Handle the move request
}
```

#### TinyWL

Reference compositor shipped with wlroots. Minimal but complete enough to show:
- Surface creation and management
- Keyboard and pointer input
- Interactive move and resize
- Output management
- XDG shell handling

**Recommended starting point** for new compositor development.

### 3.2 Smithay (Rust)

**Repository:** https://github.com/Smithay/smithay
**Language:** Rust
**License:** MIT
**Current version:** 0.3.x

#### Architecture

Smithay provides building blocks for Wayland compositors in Rust. Unlike wlroots,
it is explicitly "not a framework" — more a collection of helpers.

**Core components:**

1. **Event Loop** — Built on `calloop`, a callback-oriented event loop
   - Natural fit for Wayland's event-driven model
   - Compositors register event sources and handlers

2. **Backend Module** (`smithay::backend`)
   - **Winit backend**: Run as a windowed application (dev/testing)
   - **DRM backend**: Direct rendering on TTY
   - **libinput integration**: Input device handling
   - **Session management**: logind integration

3. **Graphics Infrastructure**
   - `allocator`: Buffer allocation traits + GBM implementation
   - `renderer`: Rendering traits + GLES2 implementation
   - DMA-BUF support for zero-copy buffer sharing

4. **Wayland Module** (`smithay::wayland`)
   - Protocol handler implementations
   - Shell handling (xdg-shell, layer-shell)
   - Seat management (keyboard, pointer, touch)
   - Output management

#### Key Differences from wlroots

| Aspect | wlroots | Smithay |
|--------|---------|---------|
| Language | C | Rust |
| Memory safety | Manual | Compile-time guaranteed |
| Abstraction level | Higher (scene graph) | Lower (more DIY) |
| Maturity | Very mature, many compositors | Growing, used by COSMIC |
| Community | Large | Smaller but active |
| API stability | Breaking changes between versions | Breaking changes between versions |
| Documentation | Sparse, learn from code | Docs.rs + examples |

### 3.3 Library Recommendation for Our Project

**wlroots is the recommended choice** for a Fluxbox-like stacking WM because:
- Most mature and battle-tested library
- Scene graph API handles damage tracking and basic 2D composition perfectly for a stacking WM
- labwc proves the viability of a wlroots-based stacking compositor
- Large community means more resources, examples, protocol implementations
- C is appropriate for a window manager (performance-critical, system-level)
- TinyWL provides an excellent starting scaffold

**Smithay would be appropriate if:**
- The project mandates Rust
- Deeper control over the graphics pipeline is needed
- The team is comfortable with less ecosystem maturity
- COSMIC DE's success validates Smithay for large projects

---

## 4. Key Protocols for Window Management

### 4.1 xdg-decoration (Server-Side vs Client-Side Decorations)

**Protocol:** `zxdg_decoration_manager_v1` (unstable)

Wayland's default is **client-side decorations (CSD)** — each application draws its own
title bar, borders, and window controls. The xdg-decoration protocol allows negotiation:

- Compositor advertises decoration support via the global
- Client creates a `zxdg_toplevel_decoration_v1` for its toplevel
- Client requests preferred mode: `client_side` or `server_side`
- Compositor responds with the mode it will use

**For a stacking WM (like Fluxbox), server-side decorations are essential:**
- Consistent look and feel across all windows
- Compositor draws title bars with buttons (close, maximize, minimize, shade)
- Custom themes and tab styling
- wlroots supports this protocol; compositors handle drawing

**Fallback:** Clients that don't support xdg-decoration will draw their own decorations.
The compositor should handle both gracefully.

**GNOME/Mutter does NOT support this protocol** — they require CSD. Our compositor
should support it since we want consistent SSD theming.

### 4.2 Layer Shell (wlr-layer-shell)

**Protocol:** `zwlr_layer_shell_v1`

Critical protocol for desktop infrastructure. Provides surfaces that are not regular
application windows but desktop components:

**Four layers** (rendered in order, bottom to top):
1. **Background** — wallpaper clients
2. **Bottom** — desktop widgets, bottom panels
3. *(Application windows are rendered here)*
4. **Top** — top panels, notifications, overlays
5. **Overlay** — screen lockers, critical overlays

**Key features:**
- **Anchoring**: Surface can anchor to edges/corners of output
- **Exclusive zones**: Reserve space that windows shouldn't overlap (e.g., taskbar)
- **Keyboard interactivity**: Layer surfaces can request keyboard focus (e.g., app launcher)
  or decline it (e.g., notification, wallpaper)
- **Output binding**: Can target specific output or appear on all

**Essential for our WM:**
- Panels/taskbar (like Fluxbox toolbar) — either built-in or external via layer-shell
- Screen lock integration
- Application launchers
- Notification overlays

### 4.3 Foreign Toplevel Management

**Protocols:**
- `zwlr_foreign_toplevel_manager_v1` (wlroots, more features)
- `ext_foreign_toplevel_list_v1` (standardized, read-only list)

Enables external taskbars and docks:
- Provides a list of open toplevel windows
- Events: title change, app_id change, state changes (maximized, minimized, fullscreen, activated)
- wlr version allows requesting actions: maximize, minimize, close, set_fullscreen, activate
- ext version is read-only (just listing)

**For our WM:**
- If the taskbar is a separate process, it uses foreign-toplevel to show window list
- If built into the compositor, direct access to window state instead

### 4.4 Output Management

**Protocol:** `zwlr_output_management_v1`

Allows clients (like `wlr-randr` or a settings GUI) to:
- List outputs and their properties
- Set output modes, position, scale, transform
- Enable/disable outputs
- Apply configuration atomically

**For our WM:** Needed for display configuration tools.

### 4.5 Idle Management

**Protocols:**
- `zwp_idle_inhibit_manager_v1` — clients prevent idle (e.g., video players)
- `ext_idle_notification_v1` — clients get notified of idle state (e.g., screen lockers)

**For our WM:** Must honor idle inhibition; integrate with screen locking.

### 4.6 Screencopy / Screen Capture

**Protocols:**
- `zwlr_screencopy_v1` (deprecated)
- `ext_image_copy_capture_v1` (replacement, standardized)

Allows clients to capture screen content for screenshots, screen recording.

### 4.7 Virtual Keyboard / Input Method

**Protocols:**
- `zwp_virtual_keyboard_v1` — inject keyboard events
- `zwp_input_method_v2` — input method framework (IME)
- `zwp_text_input_v3` — text input (complementary to input method)

**For our WM:** Required for on-screen keyboards, input methods (CJK, etc.).

### 4.8 Session Lock

**Protocol:** `ext_session_lock_v1`

Standardized screen locking:
- Lock client requests a lock on the session
- Compositor sends locked event when lock is active
- Lock client creates surfaces on each output
- Must cover all outputs to prevent information leakage
- On unlock, compositor resumes normal operation

### 4.9 Cursor Shape

**Protocol:** `wp_cursor_shape_v1`

Allows clients to set cursor shape by name rather than providing a bitmap. Compositor
can use its own cursor theme.

### 4.10 Content Type / Tearing Control

**Protocols:**
- `wp_content_type_v1` — client declares content type (none, photo, video, game)
- `wp_tearing_control_v1` — client requests async page flips (for games)

### 4.11 Fractional Scale

**Protocol:** `wp_fractional_scale_v1`

Allows the compositor to suggest non-integer scale factors (e.g., 1.25, 1.5) to clients,
improving HiDPI support.

### 4.12 Summary: Protocol Priority for Our WM

| Priority | Protocol | Reason |
|----------|----------|--------|
| **Must have** | xdg-shell | Application windows |
| **Must have** | xdg-decoration | Server-side decorations (Fluxbox-style) |
| **Must have** | wlr-layer-shell | Panels, overlays, lock screens |
| **Must have** | ext-session-lock | Screen locking |
| **Must have** | idle-inhibit | Honor video players etc. |
| **Should have** | foreign-toplevel | Taskbar support |
| **Should have** | output-management | Display configuration |
| **Should have** | pointer-constraints | Gaming, CAD |
| **Should have** | relative-pointer | Gaming, first-person apps |
| **Should have** | cursor-shape | Efficient cursor theming |
| **Should have** | fractional-scale | HiDPI |
| **Nice to have** | screencopy/capture | Screenshots, recording |
| **Nice to have** | virtual-keyboard | Accessibility |
| **Nice to have** | input-method | IME support |
| **Nice to have** | content-type | Adaptive behavior |
| **Nice to have** | tearing-control | Gaming |

---

## 5. Input Handling

### 5.1 libinput

libinput is the standard input device handling library for Wayland compositors:
- Abstracts mice, keyboards, touchpads, touchscreens, tablets
- Handles device-specific quirks
- Provides pointer acceleration, scroll methods, tap-to-click
- Gesture recognition (pinch, swipe)
- Used by both wlroots and Smithay

wlroots wraps libinput through its backend system. New input devices appear as signals
that the compositor handles.

### 5.2 Keyboard Handling

**xkbcommon** is the keymap handling library:
- Converts raw scancodes to keysyms and UTF-8 characters
- Supports XKB keymap format (same as X11)
- Handles modifiers (Shift, Ctrl, Alt, Super/Meta)
- Supports keymap switching (multiple layouts)
- Handles compose sequences (dead keys)

**Key repeat:**
- Compositor sends `wl_keyboard.repeat_info(rate, delay)` to clients
- Clients implement their own key repeat based on these values
- For compositor keybindings, the compositor handles repeat itself

**Keyboard focus model** (wl_keyboard):
- `enter` event: surface gains keyboard focus
- `leave` event: surface loses keyboard focus
- `key` event: key press/release
- `modifiers` event: modifier state change
- `keymap` event: sends the XKB keymap to clients

### 5.3 Pointer Handling

**Pointer events** (wl_pointer):
- `enter(surface, x, y)` — pointer enters a surface
- `leave(surface)` — pointer leaves
- `motion(time, x, y)` — movement within surface (surface-local coords)
- `button(serial, time, button, state)` — click/release
- `axis(time, axis, value)` — scrolling
- `frame()` — groups related events into a single logical event

**Cursor management:**
- Client sets cursor with `wl_pointer.set_cursor(surface, hotspot_x, hotspot_y)`
- Or uses `wp_cursor_shape` protocol for named cursors
- Compositor renders cursor (hardware cursor if possible, software fallback)
- Hardware cursor avoids recompositing the entire screen on every mouse move

**Pointer constraints:**
- `zwp_pointer_constraints_v1` — confine pointer to region or lock to position
- `zwp_relative_pointer_v1` — relative motion events (for locked pointers in games)
- Important for gaming and 3D applications

### 5.4 Touch Handling

Touch events (wl_touch):
- `down`, `up`, `motion`, `frame`, `cancel`
- Multi-touch tracking via touch point IDs
- Touch surfaces receive events based on the initial touch-down position

### 5.5 Input for a Stacking WM

**Compositor-level keybindings:**
- Intercept keys before delivering to clients
- Need a keybinding system (key + modifiers -> action)
- Actions: move window, resize, switch workspace, launch app, close window, etc.
- Consider Fluxbox `keys` file format compatibility

**Window interaction model:**
- Click-to-focus or focus-follows-mouse (configurable)
- Click on title bar: move, double-click for shade/maximize
- Click on border: resize
- Alt+click anywhere on window: move (Alt+drag) or resize (Alt+right-drag)
- Scroll on title bar: shade/unshade or cycle windows

---

## 6. Rendering Pipeline

### 6.1 DRM/KMS (Kernel Mode Setting)

The Linux kernel interface for display output:

- **DRM (Direct Rendering Manager)**: Manages GPU access from userspace
- **KMS (Kernel Mode Setting)**: Configures display outputs

**Key objects:**
- **Connector**: Physical display port (HDMI, DP, eDP, etc.)
- **Encoder**: Converts pixel stream for the connector
- **CRTC (CRT Controller)**: Scanout hardware — reads from a framebuffer and drives output
- **Plane**: Overlay, primary, or cursor — hardware compositing layers
  - **Primary plane**: Main display framebuffer
  - **Cursor plane**: Hardware cursor (avoids full recomposition)
  - **Overlay planes**: Hardware compositing (can avoid GPU composition for some surfaces)

**Atomic modesetting**: Modern API that applies all display changes atomically in a single
commit, preventing tearing and partial state.

### 6.2 GBM (Generic Buffer Management)

Mesa library for GPU buffer allocation:
- Allocates buffers that can be used for both rendering (EGL/Vulkan) and scanout (KMS)
- `gbm_device` created from DRM file descriptor
- `gbm_surface` for swapchain management
- `gbm_bo` (buffer object) is the actual buffer
- Integrates with EGL via `EGL_MESA_platform_gbm`

### 6.3 EGL / OpenGL ES

**EGL**: Platform interface between OpenGL and native windowing (GBM for DRM compositors):
- Creates rendering context
- Manages surfaces (on-screen and off-screen)
- Buffer import: `eglCreateImageKHR` to import client buffers as textures
- **Key extensions**: `EGL_MESA_platform_gbm`, `EGL_WL_bind_wayland_display`,
  `EGL_EXT_image_dma_buf_import`

**OpenGL ES 2.0** is the most common rendering API for Wayland compositors:
- Texture client buffers
- Compose them on screen with transforms
- Draw decorations (title bars, borders)
- Apply effects (shadows, transparency)

### 6.4 Vulkan Rendering

Growing alternative to OpenGL ES:
- More explicit control over GPU
- Better performance potential
- wlroots has a Vulkan renderer
- Wayfire 0.10 added Vulkan support
- More complex to implement than GLES2

### 6.5 Buffer Management

**Client buffer lifecycle:**
1. Client allocates buffer (SHM or DMA-BUF)
2. Client attaches buffer to surface
3. Client commits surface
4. Compositor textures the buffer
5. Compositor sends `wl_buffer.release` when done
6. Client can reuse or destroy the buffer

**DMA-BUF (Direct Memory Access Buffer):**
- Zero-copy buffer sharing between client GPU and compositor GPU
- `linux-dmabuf-v1` protocol
- Buffer described by: file descriptor(s), size, format, modifier
- Modifiers describe tiling/compression layout

### 6.6 Damage Tracking

Critical for performance — only redraw what changed:
- **Client damage**: `wl_surface.damage_buffer(x, y, w, h)` — tells compositor which
  part of the buffer changed
- **Compositor damage**: Track which parts of each output need repainting
- **Frame scheduling**: Only request a page flip if there's actually damage
- wlroots scene graph handles damage tracking automatically
- Without damage tracking, laptop battery life suffers enormously

### 6.7 Frame Scheduling

```
Compositor                  KMS
  |                           |
  |-- atomic commit -------->|  (submit new frame)
  |                           |
  |<-- page flip event ------|  (previous frame done)
  |                           |
  |  (now safe to render)     |
  |  (collect damage)         |
  |  (render damaged regions) |
  |-- atomic commit -------->|  (next frame)
```

**VSync**: Frame submission synchronized to display refresh rate.
**VRR (Variable Refresh Rate)**: Adaptive sync, frame displayed when ready.

### 6.8 Rendering for a Stacking WM

For a Fluxbox-style WM, rendering needs are straightforward:
- Compose client surfaces in Z-order (stacking order)
- Draw server-side decorations (title bars, borders, buttons)
- Handle window transparency/opacity if supported
- Draw shadows under windows (optional)
- wlroots scene graph handles 90% of this; custom rendering only for decorations

---

## 7. XWayland

### 7.1 Architecture

XWayland is a complete X11 server that runs as a Wayland client:

```
┌──────────────────────────────────────────┐
│ Wayland Compositor                        │
│  ┌──────────┐    ┌─────────────────────┐ │
│  │ XWM      │<-->│ Wayland clients     │ │
│  │ (X11     │    └─────────────────────┘ │
│  │  client) │                             │
│  └──────┬───┘                             │
│         │ X11 protocol                    │
│  ┌──────┴───┐                             │
│  │ XWayland │ (Wayland client)            │
│  │ (X server)│                            │
│  └──────┬───┘                             │
│         │ X11 protocol                    │
│  ┌──────┴───┐                             │
│  │ X11 apps │                             │
│  └──────────┘                             │
└──────────────────────────────────────────┘
```

**Two communication channels** between XWayland and the compositor:
1. **Wayland protocol**: XWayland is a Wayland client (surfaces, input)
2. **X11 protocol**: XWM (X Window Manager) is an X11 client of XWayland

### 7.2 Surface Mapping

When XWayland creates an X11 window that needs to be displayed:
1. XWayland creates a `wl_surface` in Wayland
2. XWayland sends an X11 `ClientMessage` of type `WL_SURFACE_ID` (or uses the newer
   `xwayland-shell-v1` protocol) to associate the X11 window with the `wl_surface`
3. The XWM (part of compositor) sees this and creates the mapping
4. Compositor can now manage the window like any other

### 7.3 Window Management in XWayland

The XWM (X Window Manager) inside the compositor:
- Uses standard EWMH/ICCCM protocols to manage X11 windows
- Sets itself as the window manager for the XWayland X server
- Translates between X11 window properties and Wayland window management:
  - `_NET_WM_NAME` -> xdg_toplevel title
  - `WM_CLASS` -> app_id
  - `_NET_WM_STATE` -> window states
  - `_NET_WM_WINDOW_TYPE` -> determines if toplevel, popup, etc.

### 7.4 Positioning Challenges

X11 uses a global coordinate space; Wayland does not:
- X11 apps expect to know their absolute position
- Compositor must translate coordinates for X11 apps
- Override-redirect windows (menus, tooltips) need special handling
- Drag and drop between X11 and Wayland apps requires coordination

### 7.5 Clipboard Interop

X11 and Wayland have different clipboard mechanisms:
- X11: Selection atoms (`CLIPBOARD`, `PRIMARY`)
- Wayland: `wl_data_device`, `wp_primary_selection`
- XWayland acts as bridge, proxying clipboard content between the two
- wlroots handles most of this automatically

### 7.6 XWayland Considerations for Our WM

- **Must support XWayland** — many applications still require X11
- wlroots provides XWayland integration (`wlr_xwayland`)
- Need to handle both Wayland-native and X11 windows in the stacking order
- X11 window decorations should match Wayland window decorations
- Performance: XWayland adds a copy for buffer sharing (no direct scanout for X11 surfaces)

---

## 8. Session Management

### 8.1 logind Integration

`systemd-logind` (or `elogind` on non-systemd systems) provides:

**Device access:**
- Compositor calls `RequestDevice()` on logind to get file descriptors for:
  - DRM device (`/dev/dri/card*`)
  - Input devices (`/dev/input/event*`)
- No need for root privileges or suid binaries
- logind ensures only the foreground session can access devices

**Session lifecycle:**
- Compositor calls `RequestControl()` to become session controller
- logind manages session activation/deactivation
- On VT switch: logind revokes device access, signals compositor

### 8.2 VT (Virtual Terminal) Switching

When user switches VT (Ctrl+Alt+F1-F12):
1. logind sends `PauseDevice("force")` for all devices
2. Compositor releases GPU resources, drops DRM master
3. logind activates new session on target VT
4. When switching back: logind sends `ResumeDevice` signals
5. Compositor re-acquires DRM master, re-initializes outputs

**Implementation in wlroots:**
- `wlr_session` handles logind integration
- Backend automatically pauses/resumes on VT switch
- Compositor receives `session.active` signal

### 8.3 Multi-Seat

logind supports multiple seats (multiple independent desktops on one machine):
- Each seat has its own set of input and output devices
- Useful for kiosk setups, shared workstations
- wl_seat maps to a logical seat in Wayland

### 8.4 Privilege Model

**No root required:**
- logind opens devices on behalf of the compositor
- Only logind runs with elevated privileges
- Compositor runs as regular user
- This is a major security improvement over X11

### 8.5 Session Management for Our WM

- Use wlroots `wlr_session` for logind integration (handled automatically)
- Handle VT switching gracefully (wlroots does most of the work)
- Support session lock protocol
- Consider integration with:
  - Display managers (GDM, SDDM, greetd)
  - Autostart specifications (XDG autostart)
  - Environment setup (dbus session bus, systemd user session)

---

## 9. Existing Compositor References

### 9.1 Sway

**Type:** Tiling WM (i3 clone)
**Base:** wlroots
**Language:** C
**Repository:** https://github.com/swaywm/sway

**Architecture:**
- i3-compatible configuration and IPC
- Tree-based window layout (workspaces -> outputs -> containers -> windows)
- Uses wlroots scene graph for rendering
- Supports all major Wayland protocols
- Built-in bar (swaybar) and lock (swaylock)
- IPC for external tools (same protocol as i3)

**Lessons for our project:**
- Excellent reference for wlroots integration patterns
- Shows how to implement comprehensive keybinding system
- Demonstrates IPC design for external tool integration
- Well-tested, production-quality code

### 9.2 Wayfire

**Type:** Stacking/floating + effects
**Base:** wlroots
**Language:** C++
**Repository:** https://github.com/WayfireWM/wayfire

**Architecture:**
- **Plugin system**: Core is minimal; functionality via plugins
- Plugins for: window management, animations (wobbly, fire, cube), effects, IPC
- Custom rendering pipeline for 3D effects
- Does NOT use wlroots scene graph (needs custom rendering for effects)
- INI-based configuration (via wayfire.ini)
- IPC for external tools

**Lessons for our project:**
- Plugin architecture reference if we want extensibility
- Shows how to implement window animations
- Example of stacking/floating WM on wlroots
- Custom rendering approach for effects beyond scene graph

### 9.3 Hyprland

**Type:** Dynamic tiling + effects
**Base:** Independent (formerly wlroots, now Aquamarine backend)
**Language:** C++
**Repository:** https://github.com/hyprwm/Hyprland

**Architecture:**
- **Aquamarine**: Custom low-level backend library (replaces wlroots backend)
  - NOT a compositor library like wlroots — only handles KMS/DRM/input/etc.
  - Hyprland implements Wayland protocols itself
- `CCompositor` as central coordinator
- Three-stage initialization pattern for dependency management
- Custom rendering with animations and effects
- Hyprlang configuration language
- IPC via UNIX socket

**Lessons for our project:**
- Shows full independence is possible but much more work
- Aquamarine as a lightweight backend is interesting but immature
- Validates that wlroots was a reasonable starting point
- Extensive protocol support reference

### 9.4 labwc

**Type:** Stacking WM (Openbox-like)
**Base:** wlroots
**Language:** C
**Repository:** https://github.com/labwc/labwc

**Architecture:**
- **Most relevant reference for our project** — stacking WM on wlroots
- Inspired by Openbox (similar to our Fluxbox inspiration)
- Uses Openbox-compatible configuration format (rc.xml, menu.xml, themerc)
- Server-side decorations with theming
- Uses wlroots scene graph for rendering
- No IPC — relies on external clients via protocols
- Supports: layer-shell, xdg-decoration, foreign-toplevel, output-management, session-lock
- Lightweight and focused

**Configuration:**
- `rc.xml`: Keybindings, mouse bindings, window rules, focus behavior, workspaces
- `menu.xml`: Right-click desktop menu
- `themerc`: Visual theme (title bar, borders, colors, fonts)
- `autostart`: Session startup commands
- `environment`: Environment variables

**Design philosophy:**
- Small, independent components rather than integrated ecosystem
- Use external tools for: panels (waybar), launchers (fuzzel/wofi), screenshots (grim),
  screen lock (swaylock), wallpaper (swaybg), notifications (mako)
- Focus on stability and correctness over features

**Lessons for our project:**
- **Primary architectural reference**
- Proves wlroots scene graph works well for stacking WM
- Shows SSD (server-side decoration) implementation approach
- Openbox config compatibility approach (relevant for Fluxbox config)
- Clean, readable C codebase
- Good example of keeping scope manageable

### 9.5 Other Notable Compositors

| Compositor | Type | Base | Notes |
|-----------|------|------|-------|
| **Niri** | Scrolling tiling | Smithay (Rust) | Validates Smithay for production |
| **COSMIC** | Full DE | Smithay (Rust) | System76's desktop environment |
| **Cage** | Kiosk | wlroots | Single-app compositor, minimal |
| **Weston** | Reference | Custom | Wayland reference compositor, not for production |
| **KWin** | Full DE | Custom | KDE's compositor, comprehensive but huge |
| **Mutter** | Full DE | Custom | GNOME's compositor, CSD-focused |

---

## 10. Implications for Our Project

### 10.1 Recommended Technology Stack

```
┌─────────────────────────────────────────────┐
│           Our Stacking WM Compositor         │
│                                               │
│  ┌──────────────────────────────────────┐    │
│  │ Window Management Logic               │    │
│  │  - Stacking order                     │    │
│  │  - Focus policy                       │    │
│  │  - Workspaces                         │    │
│  │  - Window decorations (SSD)           │    │
│  │  - Keybindings/mouse bindings         │    │
│  │  - Window snapping/placement          │    │
│  │  - Tab groups (Fluxbox feature)       │    │
│  └──────────────────────────────────────┘    │
│                                               │
│  ┌──────────────────────────────────────┐    │
│  │ wlroots                               │    │
│  │  - Scene graph (rendering + damage)   │    │
│  │  - Protocol implementations           │    │
│  │  - Backend (DRM/KMS, libinput)        │    │
│  │  - XWayland integration               │    │
│  │  - Session management (logind)        │    │
│  └──────────────────────────────────────┘    │
│                                               │
│  ┌──────────────────────────────────────┐    │
│  │ Linux Kernel                          │    │
│  │  - DRM/KMS, evdev, udev              │    │
│  └──────────────────────────────────────┘    │
└─────────────────────────────────────────────┘
```

### 10.2 Key Architectural Decisions

1. **Use wlroots** as the compositor library (C language)
2. **Use wlroots scene graph** for rendering (handles damage tracking, output management)
3. **Implement SSD** via xdg-decoration protocol + custom decoration drawing
4. **Support layer-shell** for panels and desktop components
5. **Support XWayland** for X11 app compatibility
6. **Model architecture after labwc** but with Fluxbox-specific features:
   - Tab groups (multiple windows sharing one frame)
   - Fluxbox-style toolbar (built-in or via layer-shell)
   - Fluxbox-compatible keybinding syntax
   - Slit (dock for windowmaker dockapps) equivalent

### 10.3 Development Approach

1. **Start from TinyWL** or a similar minimal wlroots compositor
2. **Add incrementally:**
   - Basic window management (stacking, focus, move, resize)
   - Server-side decorations
   - Keybindings
   - Workspaces
   - XWayland support
   - Layer-shell support
   - Advanced features (tabs, toolbar, themes)
3. **Use labwc and Sway source code as references** for wlroots API usage
4. **Test with real applications** throughout development

### 10.4 Challenges to Anticipate

- **Server-side decoration rendering**: Must draw title bars, buttons, borders; consider
  using Cairo or direct OpenGL for decoration rendering
- **Tab groups**: No standard Wayland protocol for this; entirely compositor-side logic
- **Configuration format**: Design Fluxbox-compatible or new format
- **XWayland edge cases**: Position mapping, override-redirect windows, clipboard
- **Multi-monitor**: Output layout, window placement, workspace-per-output vs shared
- **HiDPI/fractional scaling**: Different scale factors per output
- **Protocol evolution**: Wayland protocols are still evolving; need to track updates

---

## Sources

### Protocol Documentation
- [The Wayland Protocol Book](https://wayland-book.com/) — Comprehensive protocol guide
- [Wayland Explorer](https://wayland.app/protocols/) — Interactive protocol reference
- [Wayland Official Documentation](https://wayland.freedesktop.org/docs/html/)
- [XDG Shell Protocol](https://wayland.app/protocols/xdg-shell)
- [Layer Shell Protocol](https://wayland.app/protocols/wlr-layer-shell-unstable-v1)
- [XDG Decoration Protocol](https://wayland.app/protocols/xdg-decoration-unstable-v1)
- [Pointer Constraints Protocol](https://wayland.app/protocols/pointer-constraints-unstable-v1)
- [Foreign Toplevel List](https://wayland.app/protocols/ext-foreign-toplevel-list-v1)

### Compositor Libraries
- [wlroots GitHub](https://github.com/swaywm/wlroots) — Modular Wayland compositor library
- [wlroots DeepWiki](https://deepwiki.com/swaywm/wlroots) — Architecture documentation
- [Smithay GitHub](https://github.com/Smithay/smithay) — Rust Wayland compositor library
- [Smithay Documentation](https://docs.rs/smithay)

### Compositor References
- [labwc GitHub](https://github.com/labwc/labwc) — Stacking WM on wlroots
- [labwc Website](https://labwc.github.io/)
- [Sway GitHub](https://github.com/swaywm/sway) — i3-compatible Wayland compositor
- [Wayfire GitHub](https://github.com/WayfireWM/wayfire) — 3D Wayland compositor
- [Hyprland GitHub](https://github.com/hyprwm/Hyprland) — Dynamic tiling compositor
- [Hyprland Architecture](https://deepwiki.com/hyprwm/Hyprland)

### Tutorials and Articles
- [Introduction to Wayland](https://drewdevault.com/2017/06/10/Introduction-to-Wayland.html) — Drew DeVault
- [Writing a Wayland Compositor with wlroots](https://drewdevault.com/2018/02/17/Writing-a-Wayland-compositor-1.html) — Tutorial series
- [Input Handling in wlroots](https://drewdevault.com/2018/07/17/Input-handling-in-wlroots.html)
- [Wayland Architecture](https://wayland.freedesktop.org/architecture.html)
- [wlroots Scene Graph Introduction](https://emersion.fr/blog/2021/status-update-33/)
- [How to Make a Wayland Compositor in Rust](http://way-cooler.org/book/print.html)

### System Integration
- [libinput Documentation](https://wayland.freedesktop.org/libinput/doc/latest/what-is-libinput.html)
- [logind Device Management](https://github.com/KillingSpark/HowLogindWorks)
- [Wayland ArchWiki](https://wiki.archlinux.org/title/Wayland)
