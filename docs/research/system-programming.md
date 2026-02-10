# Linux System Programming Research for fluxland

## Overview

This document provides a comprehensive analysis of the Linux system programming requirements
for building a Wayland compositor/window manager. It covers language selection, build systems,
kernel interfaces, event loop architecture, memory management, IPC, process management,
security, performance, and dependency management.

---

## 1. Language Choice Analysis

### C (wlroots, labwc, Sway)

**Pros:**
- Direct alignment with the Wayland ecosystem. wlroots, libwayland, and nearly all core
  Wayland infrastructure are written in C
- Minimal abstraction overhead; direct access to kernel APIs (ioctl, mmap, etc.)
- Widest portability and simplest FFI for binding to other languages
- labwc (Openbox-inspired, stacking compositor) demonstrates a lean C codebase built on wlroots

**Cons:**
- No memory safety guarantees; manual lifetime management for buffers, surfaces, and clients
- Error-prone resource cleanup (fd leaks, double-free, use-after-free)
- Limited type system; no namespaces, templates, or RAII

**Ecosystem examples:** wlroots, Sway, labwc, Weston

### C++ (Wayfire, KWin, Louvre)

**Pros:**
- RAII for deterministic resource cleanup (fds, GPU buffers, wl_resources)
- Strong type system with templates, std::variant, std::optional
- Direct use of wlroots C API without FFI overhead (C++ is a superset of C for linking)
- Plugin architectures are natural with virtual dispatch (Wayfire's plugin system)
- Fluxbox was written in C++, so a C++ successor preserves familiarity for contributors
- Louvre provides a native C++ compositor framework with Factory Method patterns

**Cons:**
- Still allows unsafe operations (raw pointers, manual delete, undefined behavior)
- Build times increase with heavy template usage
- ABI stability concerns for plugin interfaces across compiler versions

**Ecosystem examples:** Wayfire, KWin, Mutter (C/C++ mix), Louvre

### Rust (Smithay-based compositors)

**Pros:**
- Memory safety enforced at compile time; eliminates use-after-free, data races
- Ownership model maps well to Wayland resource lifecycles
- Smithay provides comprehensive building blocks: backend abstractions (DRM, libinput, udev),
  calloop event loop, and protocol handling
- Xfce's new Wayland compositor (Xfwl4) chose Smithay/Rust specifically for safety
- Growing ecosystem: calloop, wayland-rs, smithay, nix crate for syscalls

**Cons:**
- Ecosystem is younger; fewer reference implementations to study
- wlroots interop requires unsafe FFI if mixing
- Longer compile times than C
- Steeper learning curve for contributors unfamiliar with ownership/borrowing

**Ecosystem examples:** Smithay, COSMIC (System76), Niri, Xfwl4

### Recommendation for fluxland

**C++ is the recommended choice** for this project:

1. **Fluxbox heritage**: Fluxbox was C++; maintaining language continuity helps port concepts
2. **wlroots interop**: Direct C linkage with wlroots without FFI overhead
3. **RAII for resource safety**: Wrapping DRM fds, GBM buffers, wl_resources in RAII types
   provides most of Rust's safety benefits for the resource-heavy compositor domain
4. **Plugin potential**: Virtual dispatch and shared libraries enable a Wayfire-style plugin
   architecture for extensibility
5. **Modern C++ (C++20/23)**: std::expected, std::span, concepts, coroutines provide
   expressive, safe abstractions

Use C++20 as the minimum standard. Apply `-Wall -Wextra -Werror` and
`-fsanitize=address,undefined` during development.

---

## 2. Build System

### Meson (Recommended)

Meson is the de facto standard for the Wayland ecosystem:
- wlroots, Sway, Wayfire, labwc, Weston all use Meson
- Native pkg-config integration for discovering wayland-server, libdrm, xkbcommon, etc.
- Built-in wayland-scanner support via `wayland_scanner = find_program('wayland-scanner')`
- Fast configuration and Ninja-backed builds
- Subproject/wrap support for vendoring wlroots or other deps

**Example structure:**
```
meson.build              # Root build definition
meson_options.txt        # Feature flags (xwayland, vulkan-renderer, etc.)
src/meson.build          # Source compilation
protocols/meson.build    # Wayland protocol generation
tests/meson.build        # Test suite
```

**Key patterns from wlroots meson.build:**
```meson
project('fluxland', 'cpp',
  version: '0.1.0',
  default_options: [
    'cpp_std=c++20',
    'warning_level=3',
    'werror=true',
  ],
  meson_version: '>=0.59.0',
)

# Core dependencies
wayland_server = dependency('wayland-server', version: '>=1.19')
wayland_client = dependency('wayland-client')
wlroots = dependency('wlroots', version: '>=0.18')
libdrm = dependency('libdrm', version: '>=2.4.105')
gbm = dependency('gbm', version: '>=17.1.0')
xkbcommon = dependency('xkbcommon')
libudev = dependency('libudev')
pixman = dependency('pixman-1')
libinput = dependency('libinput', version: '>=1.7.0')
libseat = dependency('libseat')
```

### CMake (Alternative)

- Better IDE integration (CLion, VS Code CMake Tools)
- More familiar to C++ developers
- pkg-config integration requires `find_package(PkgConfig)` + `pkg_check_modules`
- Not standard in the Wayland ecosystem; would be swimming upstream

### Cargo (Rust only)

- Excellent dependency management via crates.io
- `smithay`, `calloop`, `wayland-server` crates available
- Not applicable if choosing C++

---

## 3. Core System Interfaces

### 3.1 DRM/KMS (Direct Rendering Manager / Kernel Mode Setting)

DRM/KMS is the kernel subsystem for display output control. The compositor interacts with
`/dev/dri/card*` devices.

**Key concepts:**

| Object | Purpose |
|--------|---------|
| **Connector** | Physical display output (HDMI, DP, eDP) |
| **Encoder** | Converts CRTC pixel stream to connector signal |
| **CRTC** | Scanout engine that reads from a framebuffer |
| **Plane** | Hardware composition layer (primary, cursor, overlay) |
| **Framebuffer** | Pixel buffer registered for scanout |

**Atomic Modesetting API (required):**
- Replaces legacy `drmModeSetCrtc` / `drmModePageFlip` with atomic commits
- `drmModeAtomicAddProperty()` builds a property set for CRTCs, planes, connectors
- `drmModeAtomicCommit()` applies all changes atomically in a single vblank
- `DRM_MODE_ATOMIC_TEST_ONLY` flag allows test-before-commit (validate configurations
  without applying)
- `DRM_MODE_ATOMIC_NONBLOCK` for async page flips with fence signaling
- Enables multi-plane composition: overlay planes reduce GPU composition work

**Practical workflow:**
1. Enumerate connectors/CRTCs/planes via `drmModeGetResources()`
2. Build atomic request with properties (FB_ID, CRTC_ID, SRC_*, CRTC_*)
3. Test with `DRM_MODE_ATOMIC_TEST_ONLY`
4. Commit on vblank with `DRM_MODE_ATOMIC_NONBLOCK`
5. Handle `DRM_EVENT_FLIP_COMPLETE` via fd readable event

**wlroots abstraction:** `wlr_output` and `wlr_output_state` encapsulate atomic commits.
The compositor sets output state (mode, enabled, buffer) and calls `wlr_output_commit()`.

### 3.2 libinput

libinput is the standard input device handling library for Wayland compositors.

**Key features:**
- Unified API for keyboards, mice, touchpads, tablets, touchscreens, switches
- Device capability detection and hotplug via udev
- Touchpad gesture recognition (pinch, swipe)
- Pointer acceleration profiles (flat, adaptive)
- Tap-to-click, natural scrolling, click methods

**Integration pattern:**
```
libinput_udev_create_context()  → Create context with udev
libinput_udev_assign_seat()     → Assign seat (usually "seat0")
libinput_get_fd()               → Get epoll-able fd
libinput_dispatch()             → Process pending events
libinput_get_event()            → Dequeue events
```

Events are typed: `LIBINPUT_EVENT_KEYBOARD_KEY`, `LIBINPUT_EVENT_POINTER_MOTION`,
`LIBINPUT_EVENT_TOUCH_DOWN`, etc.

**wlroots abstraction:** `wlr_libinput_backend` wraps libinput integration. Events are
forwarded through `wlr_keyboard`, `wlr_pointer`, `wlr_touch` interfaces with signals.

### 3.3 udev (Device Hotplug)

udev provides device enumeration and hotplug notifications:

- `udev_enumerate` for initial device discovery (DRM cards, input devices)
- `udev_monitor` for runtime hotplug/unplug events
- Monitor fd is epoll-able for event loop integration
- Key properties: `SUBSYSTEM` (drm, input), `DEVTYPE`, `ID_INPUT_*`

**Hotplug scenarios:**
- Monitor connect/disconnect: DRM connector status change
- Input device add/remove: libinput handles via its udev integration
- GPU add/remove (multi-GPU): Rare but supported

### 3.4 D-Bus

D-Bus is used for desktop integration:

| Service | Purpose |
|---------|---------|
| **org.freedesktop.login1** (logind) | Session management, device access |
| **org.freedesktop.Notifications** | Desktop notification display |
| **org.freedesktop.ScreenSaver** | Screen lock/idle inhibit |
| **org.freedesktop.portal.*** | XDG desktop portals (file picker, screenshots) |

**Libraries:** sd-bus (systemd), libdbus, GDBus, or zbus (Rust)

For a C++ compositor, **sd-bus** is recommended:
- Already linked via libsystemd for logind
- Minimal, low-level, efficient
- Async dispatch integrates with epoll

### 3.5 timerfd / signalfd / eventfd

These Linux-specific APIs create file descriptors for non-I/O event sources, enabling
unified event loop integration:

| API | Purpose in Compositor |
|-----|----------------------|
| **timerfd** | Frame scheduling, animation timers, idle timeout, repeat key delay |
| **signalfd** | SIGCHLD (child exit), SIGTERM (clean shutdown), SIGHUP (reload config) |
| **eventfd** | Inter-thread wakeup (render thread → main thread notification) |

All three produce file descriptors that can be polled with epoll/io_uring alongside
Wayland client sockets, DRM fds, and libinput fds.

---

## 4. Event Loop Design

### Architecture Overview

A Wayland compositor's event loop is the central dispatch mechanism. All I/O sources
are multiplexed through a single loop:

```
┌─────────────────────────────────────────────────────┐
│                    Event Loop (epoll)                │
├──────────┬──────────┬──────────┬──────────┬─────────┤
│ Wayland  │  DRM     │ libinput │ Timers   │  IPC    │
│ clients  │  vblank  │  events  │ (timerfd)│ sockets │
│ (socket) │  (fd)    │  (fd)    │          │         │
└──────────┴──────────┴──────────┴──────────┴─────────┘
```

### epoll (Standard approach)

The proven approach used by Weston, Sway, and all wlroots-based compositors:

```c
// Simplified event loop structure
int epoll_fd = epoll_create1(EPOLL_CLOEXEC);

// Add Wayland display fd
epoll_ctl(epoll_fd, EPOLL_CTL_ADD, wl_display_fd, &ev);
// Add DRM fd (for page flip events)
epoll_ctl(epoll_fd, EPOLL_CTL_ADD, drm_fd, &ev);
// Add libinput fd
epoll_ctl(epoll_fd, EPOLL_CTL_ADD, libinput_fd, &ev);
// Add signalfd (SIGCHLD, SIGTERM)
epoll_ctl(epoll_fd, EPOLL_CTL_ADD, signal_fd, &ev);
// Add timerfd (frame scheduling)
epoll_ctl(epoll_fd, EPOLL_CTL_ADD, timer_fd, &ev);
// Add IPC socket
epoll_ctl(epoll_fd, EPOLL_CTL_ADD, ipc_fd, &ev);

while (running) {
    int n = epoll_wait(epoll_fd, events, MAX_EVENTS, timeout);
    for (int i = 0; i < n; i++) {
        dispatch(events[i]);  // Route to appropriate handler
    }
    wl_display_flush_clients(display);
}
```

**wlroots provides `wl_event_loop`:**
- `wl_event_loop_add_fd()` — Add file descriptor source
- `wl_event_loop_add_timer()` — Add timer source
- `wl_event_loop_add_signal()` — Add signal source
- `wl_event_loop_dispatch()` — Run one iteration

### io_uring (Future consideration)

io_uring offers potential advantages over epoll:

- **Batched syscalls**: Submit multiple I/O operations in a single syscall via
  shared ring buffers between userspace and kernel
- **Completion-based**: Kernel performs the I/O and notifies on completion, vs.
  epoll's readiness-based model requiring a follow-up read/write
- **~25% throughput improvement** and ~1ms p99 latency reduction in benchmarks
- **Multishot operations**: Persistent event monitors without re-arming

**Current status for compositors:**
- No Wayland compositor currently uses io_uring for its main event loop
- libwayland's `wl_event_loop` is epoll-based; replacing it would require a
  custom event loop
- Practical benefit for a compositor is minimal: the bottleneck is GPU work,
  not I/O dispatch overhead
- **Recommendation**: Start with wl_event_loop (epoll). Consider io_uring only
  if profiling shows event dispatch as a bottleneck.

### Calloop (Smithay/Rust)

For Rust-based compositors, Smithay uses calloop:
- Callback-oriented event loop
- Type-safe event sources
- Centralized mutable state passed to callbacks (avoids Rc/Arc)
- Generic over event source types

---

## 5. Memory Management

### 5.1 GPU Buffer Allocation (GBM + DMA-buf)

**GBM (Generic Buffer Management):**
- Part of Mesa; allocates GPU-accessible buffers
- `gbm_bo_create()` — Allocate with format, width, height, usage flags
- `gbm_bo_create_with_modifiers()` — Allocate with explicit format modifiers
  (tiling, compression hints negotiated between allocator and display hardware)
- Returns a `gbm_bo` that can be exported as a DMA-buf fd

**DMA-buf:**
- Kernel framework for sharing buffers between devices via file descriptors
- Zero-copy buffer sharing: client renders to a DMA-buf, passes fd to compositor,
  compositor textures from same buffer without copying pixel data
- `dma_buf_export()` creates a shareable fd from a GBM buffer object
- Compositor imports DMA-buf fds from clients as EGL images or Vulkan memory

**Buffer lifecycle in a compositor:**
```
Client                          Compositor
  |                                |
  |-- gbm_bo_create() ----------->|
  |-- render to buffer ---------->|
  |-- wl_surface.attach(dmabuf) ->|
  |-- wl_surface.commit() ------->|
  |                                |-- import dmabuf fd as texture
  |                                |-- composite with other surfaces
  |                                |-- submit to DRM plane / scanout
  |                                |-- page flip complete
  |<- wl_buffer.release() --------|
  |-- reuse or destroy buffer     |
```

### 5.2 Shared Memory (wl_shm)

For CPU-rendered clients (toolkits without GPU access):

- Client creates anonymous shared memory (`memfd_create` or `shm_open`)
- Passes fd to compositor via `wl_shm_create_pool()`
- Compositor calls `mmap()` with `MAP_SHARED` on the fd
- Multiple `wl_buffer` objects can be carved from a single pool
- Supported formats: `ARGB8888` and `XRGB8888` (required); others optional

**Pool management:**
- Pools can be resized via `wl_shm_pool.resize()` (grow only)
- Memory is released when all buffers from the pool are destroyed
- Double-buffering: client should use 2+ buffers to avoid writing to a
  buffer the compositor is still reading

### 5.3 C++ Memory Safety Patterns

For a C++ compositor, apply these patterns:

```cpp
// RAII wrapper for file descriptors
class UniqueFd {
    int fd_ = -1;
public:
    explicit UniqueFd(int fd) : fd_(fd) {}
    ~UniqueFd() { if (fd_ >= 0) close(fd_); }
    UniqueFd(UniqueFd&& o) noexcept : fd_(std::exchange(o.fd_, -1)) {}
    UniqueFd& operator=(UniqueFd&& o) noexcept;
    int get() const { return fd_; }
    // No copy
    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;
};

// RAII wrapper for GBM buffer objects
class GbmBuffer {
    gbm_bo* bo_ = nullptr;
public:
    explicit GbmBuffer(gbm_bo* bo) : bo_(bo) {}
    ~GbmBuffer() { if (bo_) gbm_bo_destroy(bo_); }
    // Move semantics...
};

// RAII wrapper for wlr resources
class WlrOutputState {
    wlr_output_state state_;
public:
    WlrOutputState() { wlr_output_state_init(&state_); }
    ~WlrOutputState() { wlr_output_state_finish(&state_); }
};
```

**Additional strategies:**
- Use `std::unique_ptr` with custom deleters for C library objects
- Use `wl_signal`/`wl_listener` patterns wrapped in C++ classes
- Run CI with AddressSanitizer (`-fsanitize=address`) and UBSan (`-fsanitize=undefined`)
- Use Valgrind for leak detection in integration tests

---

## 6. IPC Mechanisms

### 6.1 Wayland Protocol (Client Communication)

Wayland's native IPC is a UNIX domain stream socket:

- Server socket at `$XDG_RUNTIME_DIR/wayland-0` (configurable via `WAYLAND_DISPLAY`)
- Wire protocol: object-oriented message passing with marshalling
- Ancillary data (fd passing) via `SCM_RIGHTS` for buffer sharing
- libwayland handles serialization, deserialization, and dispatch

### 6.2 Compositor Control IPC (Fluxbox-compatible)

Fluxbox used X11 client messages (`XSendEvent`) for `fluxbox-remote`. Since Wayland
has no equivalent, we need a custom IPC channel.

**Recommended approach: Hyprland-style dual socket**

```
$XDG_RUNTIME_DIR/fluxland/$INSTANCE/
├── command.sock    # Synchronous command/response (like hyprctl)
└── event.sock      # Asynchronous event broadcast (like hyprland events)
```

**Command socket:**
- UNIX domain stream socket
- Text-based or JSON protocol for scriptability
- Commands: `workspace <n>`, `focus <direction>`, `move <direction>`,
  `exec <command>`, `reload-config`, `set-style <property> <value>`, etc.
- Response: JSON object with status and optional data

**Event socket:**
- UNIX domain stream socket (one-to-many via accept)
- Push-based: compositor sends events as they occur
- Events: `workspace-changed`, `window-opened`, `window-closed`,
  `window-focused`, `config-reloaded`, etc.
- Enables status bars, scripts, and automation tools

**CLI tool (fluxland-ctl):**
- Equivalent to `fluxbox-remote` and `hyprctl`
- Connects to command socket, sends command, prints response
- Example: `fluxland-ctl workspace 3`

**Fluxbox compatibility mapping:**

| Fluxbox | fluxland equivalent |
|---------|-----------------------|
| `fluxbox-remote` | `fluxland-ctl` |
| `fbrun` | `fluxland-run` (or use external launcher like rofi/fuzzel) |
| X11 ClientMessage | UNIX domain socket command |
| `~/.fluxbox/startup` | `~/.config/fluxland/autostart` |

### 6.3 D-Bus Interface

Expose a D-Bus interface for desktop environment integration:

```
org.wmwayland.Compositor
├── Methods:
│   ├── ListWindows() → a(ssuub)   # [title, app_id, workspace, focused, minimized]
│   ├── FocusWindow(id: u)
│   ├── CloseWindow(id: u)
│   ├── SwitchWorkspace(n: u)
│   └── ReloadConfig()
├── Signals:
│   ├── WindowOpened(id: u, app_id: s)
│   ├── WindowClosed(id: u)
│   ├── WorkspaceChanged(n: u)
│   └── ConfigReloaded()
└── Properties:
    ├── ActiveWorkspace: u
    ├── WindowCount: u
    └── Version: s
```

This enables:
- Desktop portal integration
- Notification daemon coordination
- External panel/taskbar communication
- Accessibility tools

---

## 7. Process Management

### 7.1 Application Launching

**Basic process creation:**
```cpp
void launch_application(const std::string& command) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        setsid();                    // New session leader
        // Close compositor fds (DRM, Wayland, etc.)
        // Set up environment (WAYLAND_DISPLAY, etc.)
        execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);
        _exit(127);
    }
    // Parent: track pid for SIGCHLD handling
}
```

**Environment setup for children:**
- `WAYLAND_DISPLAY` — Wayland socket name
- `XDG_CURRENT_DESKTOP=fluxland`
- `XDG_SESSION_TYPE=wayland`
- Clear `DISPLAY` unless Xwayland is running (then set it)

**Best practices:**
- Use `posix_spawn()` where possible (more efficient than fork+exec)
- Set `CLOEXEC` on all compositor-internal fds to prevent leaking to children
- Use `setsid()` in children so they don't receive compositor signals
- Close stdin or redirect to `/dev/null` in spawned processes

### 7.2 SIGCHLD Handling

Use `signalfd` for clean integration with the event loop:

```cpp
sigset_t mask;
sigemptyset(&mask);
sigaddset(&mask, SIGCHLD);
sigaddset(&mask, SIGTERM);
sigaddset(&mask, SIGHUP);
sigprocmask(SIG_BLOCK, &mask, nullptr);  // Block signals from default handling

int sfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
// Add sfd to event loop

// Handler:
void handle_signal(int sfd) {
    signalfd_siginfo info;
    read(sfd, &info, sizeof(info));
    switch (info.ssi_signo) {
        case SIGCHLD:
            // Reap zombies
            while (waitpid(-1, nullptr, WNOHANG) > 0) {}
            break;
        case SIGTERM:
            initiate_shutdown();
            break;
        case SIGHUP:
            reload_config();
            break;
    }
}
```

### 7.3 Session Startup

**Autostart mechanism (Fluxbox-compatible):**
```
~/.config/fluxland/autostart
```

Contents executed line-by-line at compositor startup:
```bash
# Panel
waybar &
# Wallpaper
swaybg -i ~/wallpaper.jpg &
# Notification daemon
mako &
# Polkit agent
/usr/lib/polkit-gnome/polkit-gnome-authentication-agent-1 &
```

**systemd integration (optional):**
- Start compositor as a systemd user service
- Use `systemd-run --user` to launch children in proper cgroups
- UWSM (Universal Wayland Session Manager) pattern for clean session lifecycle

---

## 8. Security Considerations

### 8.1 Wayland Security Model vs X11

**X11 security flaws:**
- Any connected client can keylog all other clients (via `XRecord`, `XInput2`)
- Any client can capture the screen (`XGetImage`, `XShm`)
- Any client can inject input events (`XSendEvent`, `XTest`)
- 32-bit GEM buffer handles can be guessed/enumerated for GPU memory access
- Three critical X.Org vulnerabilities disclosed in 2025 (CVE-2025-62229, CVE-2025-62230,
  CVE-2025-62231) — use-after-free and overflow bugs dating back 20+ years

**Wayland security improvements:**
- **Process isolation**: Each client has its own connection; no shared event queue
- **No input snooping**: Clients only receive input events for their own surfaces
- **No screen capture by default**: Screenshots/screencasting require explicit
  compositor permission via protocols (wlr-screencopy, xdg-desktop-portal)
- **Buffer isolation**: DMA-buf file descriptors are per-client; kernel enforces access
- **No input injection**: Clients cannot send fake input to other clients
- Privileged protocols (like virtual-keyboard, input-method) require explicit
  compositor authorization

**Trade-offs:**
- Global hotkeys require compositor support (no client-side global grab)
- Screen sharing needs portal integration
- Automation tools need compositor IPC (no equivalent of `xdotool`)

### 8.2 Running Without Root (logind)

Modern compositors run as unprivileged users via systemd-logind:

**Device access flow:**
1. Compositor calls `org.freedesktop.login1.Session.TakeDevice(major, minor)`
2. logind opens the device (DRM card, input device) with root privileges
3. logind returns a file descriptor to the compositor via D-Bus fd passing
4. logind manages device pause/resume on VT switch:
   - `PauseDevice(major, minor, "pause")` → compositor must release DRM master
   - `ResumeDevice(major, minor, fd)` → compositor re-acquires DRM master

**libseat abstraction:**
- `libseat` provides a backend-agnostic API: works with logind, seatd, or direct
- wlroots uses libseat internally via `wlr_session`
- Required capabilities: none (all privileged operations delegated to seat manager)

**Alternative: seatd**
- Minimal seat management daemon (no systemd dependency)
- Used on non-systemd distributions (Void, Alpine, Gentoo)
- libseat auto-detects available backend

### 8.3 Additional Security Measures

- **CLOEXEC on all fds**: Prevent leaking compositor fds to child processes
- **Sandboxing awareness**: Support Flatpak/Snap portals for sandboxed app access
  to screenshots, screen recording, file choosers
- **Seccomp (optional)**: Filter syscalls for the compositor process itself
- **Capabilities**: No `CAP_SYS_ADMIN` required with logind; only standard user perms

---

## 9. Performance

### 9.1 Frame Scheduling

**Goal:** Deliver frames at display refresh rate (typically 60 Hz = 16.67ms per frame)
with minimal input-to-display latency.

**Scheduling strategy:**
```
Input event → Process → Composite → Submit → VBlank → Scanout
  ↑                                                       ↓
  └───────── Total latency budget: ~16.67ms ──────────────┘
```

**Key techniques:**
- **Render on demand**: Only composite when surfaces have new content (damage tracking)
- **Damage tracking**: `wlr_damage_ring` tracks regions that changed; only re-render
  those areas
- **Direct scanout**: If a fullscreen client's buffer matches the output format,
  skip composition entirely and scanout the client buffer directly
- **Adaptive sync / VRR**: Variable refresh rate to match actual content framerate
- **Frame callbacks**: Only send `wl_surface.frame` to visible, non-occluded surfaces

### 9.2 GPU Synchronization

**Implicit sync (legacy):**
- Kernel driver infers sync from command buffer dependencies
- Works but can cause issues with some drivers (notably NVIDIA proprietary)

**Explicit sync (modern, recommended):**
- `wp_linux_drm_syncobj` protocol (standardized in 2024)
- Applications signal render completion via DRM sync objects
- Compositor waits on sync objects before compositing or scanout
- Eliminates flickering on NVIDIA drivers
- Reduces unnecessary driver overhead by removing implicit dependency tracking

**Fence FDs:**
- DRM sync objects export fence fds
- Compositor polls fence fds or integrates with event loop
- `dma_fence_wait()` in kernel; `sync_file` in userspace

### 9.3 Multi-Plane Composition

Using DRM overlay planes reduces GPU composition work:
- **Primary plane**: Main composited output
- **Cursor plane**: Hardware cursor (avoids re-compositing on cursor move)
- **Overlay planes**: Direct scanout of client surfaces (video players, games)

**libliftoff** can automatically assign surfaces to planes based on hardware capabilities.

### 9.4 Profiling Tools

| Tool | Purpose |
|------|---------|
| **perf** | System-wide CPU profiling, flame graphs |
| **tracy** | Real-time frame profiler (~50ns overhead per zone); ideal for frame-based apps |
| **gpuvis** | GPU trace visualization (AMD gpuvis_trace_utils, ftrace integration) |
| **sysprof** | GNOME system profiler with Wayland integration |
| **valgrind/massif** | Heap profiling, memory leak detection |
| **renderdoc** | GPU frame capture and analysis (OpenGL ES / Vulkan) |
| **perf + ftrace** | Frame latency analysis with DRM/KMS tracepoints |
| **wayland-tracker** | Protocol message logging for debugging client/server interaction |

**Performance targets:**
- Frame time < 8ms (leaves headroom for scheduling jitter)
- Input-to-display latency < 1 frame (16.67ms at 60Hz)
- Zero frame drops under normal desktop usage
- Memory usage < 100MB RSS for idle compositor (no clients)

---

## 10. Dependencies and Packaging

### 10.1 Required Dependencies

| Library | Minimum Version | Purpose |
|---------|----------------|---------|
| wayland-server | 1.19+ | Wayland protocol server |
| wayland-protocols | 1.32+ | Standard protocol extensions |
| wlroots | 0.18+ | Compositor building blocks |
| libdrm | 2.4.105+ | DRM/KMS userspace API |
| gbm | 17.1.0+ | GPU buffer allocation |
| EGL + GLESv2 | — | OpenGL ES rendering |
| xkbcommon | 1.0+ | Keyboard keymap handling |
| libinput | 1.7.0+ | Input device handling |
| libudev | — | Device enumeration/hotplug |
| pixman | 0.40+ | Software pixel manipulation |
| libseat | 0.6+ | Seat management (logind/seatd) |
| cairo (optional) | 1.16+ | 2D rendering for decorations |
| pango (optional) | 1.44+ | Text rendering for titlebars |

### 10.2 Optional Dependencies

| Library | Purpose |
|---------|---------|
| Xwayland | X11 application compatibility |
| libxcb + xcb-utils | Xwayland integration |
| Vulkan loader + headers | Vulkan renderer backend |
| libdisplay-info | EDID parsing for monitor info |
| libliftoff | Automatic DRM plane assignment |
| nlohmann-json (C++) | JSON config/IPC parsing |
| sd-bus (libsystemd) | D-Bus interface, logind |

### 10.3 Build-time Dependencies

| Tool | Purpose |
|------|---------|
| meson (>=0.59) | Build system |
| ninja | Build executor |
| wayland-scanner | Protocol code generator |
| pkg-config | Dependency discovery |
| glslang (optional) | GLSL shader compilation for Vulkan |
| scdoc (optional) | Man page generation |
| clang-tidy | Static analysis (development) |

### 10.4 Distribution Packaging

**Debian/Ubuntu (.deb):**
```
Build-Depends: meson, ninja-build, pkg-config,
  libwayland-dev, wayland-protocols, libwlroots-dev (>= 0.18),
  libdrm-dev, libgbm-dev, libgles-dev, libegl-dev,
  libxkbcommon-dev, libinput-dev, libudev-dev,
  libpixman-1-dev, libseat-dev,
  libcairo2-dev, libpango1.0-dev
```

**Fedora/RHEL (.rpm):**
```
BuildRequires: meson, ninja-build, pkgconfig,
  wayland-devel, wayland-protocols-devel, wlroots-devel >= 0.18,
  libdrm-devel, mesa-libgbm-devel, mesa-libGLES-devel, mesa-libEGL-devel,
  libxkbcommon-devel, libinput-devel, systemd-devel,
  pixman-devel, libseat-devel,
  cairo-devel, pango-devel
```

**Arch Linux (PKGBUILD):**
```
depends=(wayland wlroots libdrm libgbm libglvnd
         libxkbcommon libinput systemd pixman libseat
         cairo pango)
makedepends=(meson ninja wayland-protocols)
```

**Flatpak considerations:**
- Compositors are **not** packaged as Flatpaks (they are system components)
- However, the compositor should support portals for Flatpak apps:
  - `xdg-desktop-portal-wlr` or custom portal backend
  - `org.freedesktop.impl.portal.ScreenCast`
  - `org.freedesktop.impl.portal.Screenshot`
  - `org.freedesktop.impl.portal.FileChooser` (via external file picker)

### 10.5 Version Compatibility Strategy

- **Pin wlroots major version**: wlroots breaks API between major versions (0.17 → 0.18).
  Support one major version at a time.
- **Minimum wayland-protocols**: Track wlroots' requirements
- **Feature flags**: Use meson options to make Xwayland, Vulkan renderer, etc. optional:
  ```meson
  option('xwayland', type: 'feature', value: 'auto', description: 'Xwayland support')
  option('vulkan-renderer', type: 'feature', value: 'auto', description: 'Vulkan renderer')
  ```

---

## Summary of Key Decisions

| Aspect | Recommendation | Rationale |
|--------|---------------|-----------|
| Language | C++20 | Fluxbox heritage, wlroots interop, RAII safety |
| Build system | Meson | Ecosystem standard, pkg-config native |
| Display backend | wlroots (DRM/KMS) | Proven abstraction over atomic modesetting |
| Input | wlroots (libinput) | Standard for all Wayland compositors |
| Event loop | wl_event_loop (epoll) | Provided by libwayland, sufficient for compositor |
| Buffer mgmt | GBM + DMA-buf + wl_shm | Standard Wayland buffer sharing |
| Compositor IPC | UNIX socket + D-Bus | Scriptable control + desktop integration |
| Session mgmt | libseat (logind/seatd) | Non-root operation, VT switching |
| Rendering | OpenGL ES 2.0 (primary) | Universal GPU support; Vulkan optional |
| Frame sync | Explicit sync preferred | Modern, avoids NVIDIA issues |

---

## References

- [wlroots repository](https://github.com/swaywm/wlroots)
- [Smithay repository](https://github.com/Smithay/smithay)
- [Louvre C++ compositor library](https://github.com/CuarzoSoftware/Louvre)
- [labwc (Openbox-inspired compositor)](https://github.com/labwc/labwc)
- [Wayfire (C++ modular compositor)](https://github.com/WayfireWM/wayfire)
- [Hyprland IPC documentation](https://wiki.hypr.land/IPC/)
- [Linux DRM/KMS documentation](https://www.kernel.org/doc/html/v4.15/gpu/drm-kms.html)
- [Atomic modesetting overview (LWN)](https://lwn.net/Articles/653071/)
- [Wayland protocol book](https://wayland-book.com/)
- [Wayland shared memory buffers](https://wayland-book.com/surfaces/shared-memory.html)
- [Explicit sync in Wayland](https://zamundaaa.github.io/wayland/2024/04/05/explicit-sync.html)
- [DMA-buf kernel documentation](https://docs.kernel.org/userspace-api/dma-buf-alloc-exchange.html)
- [Wayland vs X11 security comparison](https://www.glukhov.org/post/2026/01/wayland-vs-x11-comparison/)
- [Privileged Wayland clients](https://blog.ce9e.org/posts/2025-10-03-wayland-security/)
- [Wayland sessions and multi-seat support](https://openlib.io/wayland-sessions-and-multi-seat-support-in-linux/)
- [KDE blog: Wayland vs D-Bus IPC](https://blogs.kde.org/2020/10/11/linux-desktop-shell-ipc-wayland-vs-d-bus-and-lack-agreement-when-use-them/)
- [Fluxbox remote manual](https://fluxbox.org/help/man-fluxbox-remote/)
- [io_uring vs epoll](https://ryanseipp.com/post/iouring-vs-epoll/)
- [Swapchains and frame pacing](https://raphlinus.github.io/ui/graphics/gpu/2021/10/22/swapchain-frame-pacing.html)
- [wlroots-0.19.2 build guide](https://glfs-book.github.io/slfs/graph/wlroots.html)
