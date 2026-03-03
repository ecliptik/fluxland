# fluxland Architecture Overview

## Introduction

fluxland is a Wayland compositor built on the wlroots library, designed to replicate and extend the Fluxbox window management experience. This document describes the high-level architecture, component design, data flow, and key technical decisions.

## Architecture Principles

1. **Modularity**: Each subsystem is a self-contained module with clear interfaces
2. **Simplicity**: Prefer straightforward implementations over clever abstractions
3. **Compatibility**: Fluxbox configuration formats are supported through a dedicated parsing/translation layer
4. **Performance**: Minimal overhead in the rendering and input paths
5. **Correctness**: Strict adherence to Wayland protocol semantics

## High-Level Component Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                         fluxland compositor                       │
│                                                                     │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────────────────────┐ │
│  │ Config Engine │  │  Fluxbox     │  │     Style Engine          │ │
│  │              │◄─┤  Compat Layer│  │  (decoration rendering)   │ │
│  └──────┬───────┘  └──────────────┘  └───────────┬───────────────┘ │
│         │                                         │                 │
│  ┌──────▼───────────────────────────────────────────────────────┐  │
│  │                    Window Manager Core                        │  │
│  │                                                               │  │
│  │  ┌─────────────┐ ┌──────────┐ ┌──────────┐ ┌─────────────┐  │  │
│  │  │  Tab/Group   │ │Workspace │ │ Window   │ │  Focus &    │  │  │
│  │  │  Manager     │ │ Manager  │ │ Rules    │ │  Stacking   │  │  │
│  │  └─────────────┘ └──────────┘ └──────────┘ └─────────────┘  │  │
│  └──────────────────────────┬────────────────────────────────────┘  │
│                              │                                      │
│  ┌───────────┐  ┌───────────▼───────────┐  ┌───────────────────┐  │
│  │  Input    │  │   wlroots Interface    │  │   Menu System     │  │
│  │  Handler  │──┤   (scene graph, output │  │   (root, window,  │  │
│  │  (keys,   │  │    management, seat)   │  │    client menus)  │  │
│  │   mouse)  │  └───────────┬───────────┘  └───────────────────┘  │
│  └───────────┘              │                                      │
│                              │                                      │
│  ┌──────────────┐  ┌───────▼───────┐  ┌──────────────────────┐   │
│  │   Toolbar    │  │  Output /     │  │    Slit (Dockapp     │   │
│  │  (taskbar,   │  │  Rendering    │  │     Container)       │   │
│  │   clock,     │  │  Pipeline     │  │                      │   │
│  │   systray)   │  └───────┬───────┘  └──────────────────────┘   │
│  └──────────────┘          │                                      │
│                              │                                      │
│  ┌──────────────┐  ┌───────▼───────┐  ┌──────────────────────┐   │
│  │  IPC / D-Bus │  │   Wayland     │  │    XWayland          │   │
│  │  Interface   │  │   Protocol    │  │    Bridge             │   │
│  │              │  │   Handler     │  │                      │   │
│  └──────────────┘  └───────────────┘  └──────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
         │                    │                      │
         ▼                    ▼                      ▼
   ┌──────────┐      ┌──────────────┐       ┌──────────────┐
   │  Client  │      │   Wayland    │       │   X11        │
   │  Tools   │      │   Clients    │       │   Clients    │
   └──────────┘      └──────────────┘       └──────────────┘
```

## Core Components

### 1. wlroots Interface Layer

The foundation of fluxland. Wraps wlroots to provide:

- **Output management**: Multi-monitor support, mode setting, arrangement, and hotplug
- **Seat / Input**: Keyboard, pointer, touch, and tablet input via libinput
- **Scene graph**: wlroots scene-graph API for efficient rendering and damage tracking
- **Protocol implementation**: Handles Wayland protocol objects (surfaces, buffers, etc.)

**Key wlroots components used:**
- `wlr_backend` - Hardware abstraction (DRM/KMS, libinput, headless)
- `wlr_renderer` - GPU rendering (Vulkan, GLES2, Pixman)
- `wlr_scene` - Scene graph for compositing
- `wlr_output` - Display output management
- `wlr_seat` - Input device management
- `wlr_xdg_shell` - XDG shell protocol (window management)
- `wlr_layer_shell` - Layer shell protocol (panels, overlays)

### 2. Window Manager Core

The central orchestrator that implements Fluxbox-style window management:

- **Tab/Group Manager**: Groups windows into tabbed containers (a signature Fluxbox feature). Each tab group shares a single frame/decoration. Tabs can be created by drag-and-drop or key bindings.
- **Workspace Manager**: Virtual desktop management with named workspaces. Windows can be sent to, or pinned across, workspaces. Supports workspace wrapping and per-monitor workspaces.
- **Window Rules Engine**: Matches windows by app-id, title, or other properties and applies rules (workspace assignment, geometry, decoration style, layer, focus behavior). Compatible with Fluxbox `apps` file format.
- **Focus & Stacking Manager**: Controls focus policy (click-to-focus, sloppy focus, strict mouse focus) and window stacking order. Supports raise-on-focus, focus-follows-mouse with optional delay, and focus history.

### 3. Config Engine

Parses and manages all configuration:

- Reads configuration from `~/.config/fluxland/`
- Supports live reloading via signal or IPC command
- Validates configuration and provides clear error messages
- Falls back to sensible defaults for missing values

### 4. Fluxbox Compatibility Layer

A dedicated subsystem for Fluxbox config format support:

- **Key file parser**: Reads Fluxbox `keys` file format (Mod1/Mod4/Control/Shift + key → action mapping)
- **Init file parser**: Reads Fluxbox `init` resource-style configuration
- **Menu file parser**: Reads Fluxbox menu definition format
- **Apps file parser**: Reads Fluxbox `apps` window rules format
- **Style translator**: Converts Fluxbox style definitions to fluxland style format
- **Action mapper**: Maps Fluxbox actions to fluxland equivalents, warning on unsupported actions

### 5. Style Engine

Renders window decorations (server-side decoration):

- Titlebar with configurable buttons (close, maximize, minimize, shade, stick)
- Titlebar tabs for grouped windows
- Window borders with configurable width and color
- Fluxbox-compatible style format support (textures, colors, fonts, button styles)
- Pixmap and gradient rendering for decoration textures

### 6. Input Handler

Processes all input events:

- Key binding dispatch with support for key chains (e.g., `Mod4 a Mod4 b`)
- Mouse bindings for titlebar, border, root window, and client window
- Configurable mouse wheel actions
- Resize and move grips
- Multi-modifier support matching Fluxbox syntax

### 7. Menu System

Implements Fluxbox-style menus:

- **Root menu**: Right-click desktop menu with nested submenus, separators, and commands
- **Window menu**: Context menu for window operations (send to workspace, layer, decorations, etc.)
- **Client menu**: Customizable per-application menus
- Menu pipe commands (dynamic menus generated by shell scripts)
- Support for icons in menu entries

### 8. Toolbar

A configurable panel providing:

- Workspace name / switcher
- Window list / icon bar
- System tray (via wlr-foreign-toplevel-management or custom protocol)
- Clock with configurable format
- Customizable tool layout (left/center/right regions)
- Auto-hide and placement options (top, bottom, per-monitor)

### 9. Slit

A docking area modeled after Fluxbox's slit:

- Hosts small dockapp-style applications
- Configurable placement and orientation
- Auto-hide support
- May use layer-shell protocol for positioning

### 10. XWayland Bridge

Handles X11 application compatibility:

- Manages the XWayland server lifecycle
- Maps X11 window types to appropriate Wayland behaviors
- Handles X11 selections (clipboard, primary) bridging
- Supports X11 system tray protocol for legacy tray icons

### 11. IPC Interface

External control and scripting:

- Unix socket-based IPC (similar to sway/i3 IPC)
- Commands for window manipulation, workspace control, configuration changes
- Event subscription for window, workspace, and output events
- Enables scripting and integration with external tools (bars, launchers, etc.)

## Data Flow

### Input Event Flow

```
Hardware Input
    │
    ▼
libinput (via wlr_backend)
    │
    ▼
wlr_seat (keyboard/pointer routing)
    │
    ├──► Input Handler (check key/mouse bindings)
    │       │
    │       ├──► Match found → Execute action (WM Core, Menu, etc.)
    │       │
    │       └──► No match → Forward to focused client
    │
    └──► Direct to client (when no binding matches)
```

### Window Lifecycle

```
New Surface Created (xdg_toplevel / xwayland)
    │
    ▼
Window Manager Core: Create managed window
    │
    ▼
Window Rules Engine: Apply matching rules
    │
    ├──► Assign workspace
    ├──► Set geometry / position
    ├──► Set decoration style
    ├──► Set layer / stacking
    │
    ▼
Tab/Group Manager: Add to group if rules dictate
    │
    ▼
Style Engine: Create window decoration
    │
    ▼
Scene Graph: Add to render tree
    │
    ▼
Output: Render frame with new window
```

### Configuration Reload Flow

```
Reload Signal (SIGHUP / IPC command)
    │
    ▼
Config Engine: Re-read all config files
    │
    ├──► Fluxbox Compat Layer (if using Fluxbox format)
    │       │
    │       └──► Translate to internal config
    │
    ▼
Distribute updated config to subsystems
    │
    ├──► Input Handler: Rebind keys/mouse
    ├──► Style Engine: Reload decorations
    ├──► Menu System: Rebuild menus
    ├──► Toolbar: Reconfigure layout
    ├──► Window Rules: Update rule set
    └──► Workspace Manager: Update workspace config
```

## Key Design Decisions

### 1. wlroots as the compositor library

**Decision**: Build on wlroots rather than implementing Wayland protocols from scratch.

**Rationale**: wlroots provides battle-tested implementations of Wayland protocols, hardware abstraction, and rendering. It is the foundation for Sway, Wayfire, Hyprland, and many other compositors. Using wlroots lets us focus on the window management and Fluxbox compatibility layers rather than low-level protocol work.

### 2. C as the implementation language

**Decision**: Implement in C (C23 standard, C17 minimum).

**Rationale**: wlroots is a C library with a C API. Using C avoids FFI overhead and complexity, provides direct access to all wlroots internals, and aligns with the broader Wayland ecosystem (labwc, Sway, Weston). The Wayland ecosystem tooling (wayland-scanner, protocol XML) generates C code. C keeps the codebase accessible to contributors from the wlroots/Wayland community. Follows the labwc model as our closest architectural reference.

### 3. Server-Side Decorations (SSD)

**Decision**: Use server-side window decorations as the primary mode, with client-side decoration (CSD) support as fallback.

**Rationale**: Fluxbox is fundamentally an SSD window manager - its identity is defined by its window decorations (tabs, titlebar, borders). SSD gives us full control over the Fluxbox look and feel. The xdg-decoration protocol is used to negotiate SSD preference with clients.

### 4. Scene Graph API

**Decision**: Use the wlroots scene graph API (`wlr_scene`) for rendering.

**Rationale**: The scene graph API handles damage tracking, buffer management, and compositing automatically. This simplifies the rendering path significantly compared to manual rendering. It is the recommended approach for new wlroots compositors.

### 5. Fluxbox Config Compatibility via Translation Layer

**Decision**: Support Fluxbox config files by parsing them and translating to internal configuration, rather than using Fluxbox's config format as the native format.

**Rationale**: Fluxbox's config formats have X11-specific concepts (e.g., X11 modifier names, X11-specific actions). A translation layer lets us support the familiar format while internally using Wayland-appropriate concepts. It also allows us to extend the configuration beyond what Fluxbox supports. Users can choose to use either native fluxland config or Fluxbox-compatible config.

### 6. IPC Protocol

**Decision**: Implement an IPC protocol inspired by sway/i3 IPC.

**Rationale**: The sway/i3 IPC protocol is well-documented, widely supported by tools (waybar, rofi, etc.), and familiar to the Wayland/tiling WM community. Adopting a compatible subset ensures fluxland works with existing ecosystem tools out of the box.

## Technology Stack

| Component | Technology |
|-----------|-----------|
| Display protocol | Wayland |
| Compositor library | wlroots (>= 0.18) |
| Language | C (C17) |
| Build system | Meson + Ninja |
| Input handling | libinput (via wlroots) |
| Rendering | wlroots scene graph (Vulkan/GLES2/Pixman backends) |
| XKB | libxkbcommon (keyboard handling) |
| Image loading | libpng, libjpeg (for styles/pixmaps) |
| Font rendering | Pango + Cairo (for titlebar text, menus) |
| X11 compat | XWayland (optional) |
| IPC | Unix domain socket (JSON messages) |
| Documentation | Man pages + Markdown |

## Module Dependency Graph

```
                    ┌──────────┐
                    │  main()  │
                    └────┬─────┘
                         │
                    ┌────▼─────┐
                    │  server  │ (top-level compositor state)
                    └────┬─────┘
                         │
          ┌──────────────┼──────────────┐
          │              │              │
    ┌─────▼─────┐ ┌─────▼─────┐ ┌─────▼─────┐
    │  config   │ │  wlroots  │ │   ipc     │
    │  engine   │ │  backend  │ │  server   │
    └─────┬─────┘ └─────┬─────┘ └───────────┘
          │              │
    ┌─────▼─────┐       │
    │  fluxbox  │       │
    │  compat   │       │
    └───────────┘       │
                         │
          ┌──────────────┼──────────────┬───────────────┐
          │              │              │               │
    ┌─────▼─────┐ ┌─────▼─────┐ ┌─────▼─────┐  ┌─────▼─────┐
    │  window   │ │  input    │ │  output   │  │  xwayland │
    │  manager  │ │  handler  │ │  manager  │  │  bridge   │
    └─────┬─────┘ └───────────┘ └───────────┘  └───────────┘
          │
    ┌─────┼──────────┬──────────┬──────────┐
    │     │          │          │          │
  ┌─▼──┐┌─▼───┐┌────▼──┐┌─────▼──┐┌──────▼──┐
  │tab ││work- ││window ││ focus  ││ menu    │
  │grp ││space ││rules  ││ stack  ││ system  │
  └────┘└──────┘└───────┘└────────┘└─────────┘
```

## Future Considerations

- **Plugin system**: Lua or other scripting for extensibility
- **Animation framework**: Smooth window transitions and effects
- **Touchpad gestures**: Workspace switching and window management via gestures
- **Fractional scaling**: Per-monitor fractional scale support
- **HDR support**: When wlroots and Wayland protocols mature
- **Session management**: Integration with systemd and login managers
