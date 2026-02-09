# Fluxbox UI/UX Research Document

> Comprehensive analysis of Fluxbox's design, features, and configuration system
> for informing the design of a Wayland compositor inspired by Fluxbox.

## Table of Contents

1. [Overview and History](#1-overview-and-history)
2. [Architecture and Design Philosophy](#2-architecture-and-design-philosophy)
3. [Window Decorations and Theming](#3-window-decorations-and-theming)
4. [Workspace Model](#4-workspace-model)
5. [Window Management Features](#5-window-management-features)
6. [Menus](#6-menus)
7. [Toolbar and Slit](#7-toolbar-and-slit)
8. [Key and Mouse Bindings](#8-key-and-mouse-bindings)
9. [Configuration Files](#9-configuration-files)
10. [IPC and Scripting](#10-ipc-and-scripting)
11. [Compatibility Priorities for Migration](#11-compatibility-priorities-for-migration)

---

## 1. Overview and History

Fluxbox is a lightweight, stacking window manager for X11, forked from Blackbox 0.61.1 in
2001 by Henrik Kinnunen. Written in C++ under the MIT License, it emphasizes minimal resource
usage (~16 MB RAM), high configurability through plain-text files, and a keyboard-driven
workflow. It is widely used in lightweight Linux distributions (GParted Live, Grml, MX Linux,
Slax) and remains popular for resource-constrained environments.

Key differentiators from Blackbox:
- Native window tabbing/grouping
- Chainable key grabber for Emacs-style multi-key sequences
- Extended menu system with pipe menus
- Per-window rules via the apps file
- A more robust toolbar with configurable components

Fluxbox has full EWMH (Extended Window Manager Hints) compliance.

---

## 2. Architecture and Design Philosophy

### Design Principles
- **Minimalism**: Bare-bones UI with just a taskbar, right-click root menu, and window decorations
- **Text-file configuration**: All configuration via plain ASCII files, no XML/JSON
- **User control**: Every aspect of behavior is user-configurable
- **Low overhead**: Event-driven X11 architecture, minimal memory footprint
- **Keyboard-first**: Comprehensive keybinding system with chaining and keymodes

### Stacking Layer System

Fluxbox uses a 6-layer stacking model (highest to lowest):

| Layer       | Numeric Value | Typical Use                     |
|-------------|---------------|----------------------------------|
| AboveDock   | 2             | Always-on-top panels, overlays  |
| Dock        | 4             | Toolbar, slit, dockable panels  |
| Top         | 6             | "Always on top" windows         |
| Normal      | 8             | Regular application windows     |
| Bottom      | 10            | Background utilities            |
| Desktop     | 12            | Desktop widgets, wallpaper      |

Windows on a higher layer always appear above those on a lower one. Within a layer,
windows are ordered by raise/lower operations.

---

## 3. Window Decorations and Theming

### 3.1 Style System Overview

Fluxbox styles (themes) are plain ASCII text files using X resource database syntax.
They control the visual appearance of four main components: **window decorations**,
**toolbar**, **menus**, and **slit**.

Styles are stored in:
- System styles: `/usr/share/fluxbox/styles/`
- User styles: `~/.fluxbox/styles/`

A style can be a single file or a directory containing a `theme.cfg` (or `style.cfg`)
file and a `pixmaps/` subdirectory for image assets.

Styles are 100% compatible with Blackbox themes.

### 3.2 Style File Syntax

```
! This is a comment
resource.name: value

! Example
window.title.focus: Raised Gradient Vertical
window.title.focus.color: #1a1a2e
window.title.focus.colorTo: #16213e
window.label.focus.textColor: #e0e0e0
window.label.focus.font: sans-10:bold
```

Comments are preceded by `!` (X resource style) or `#`.

### 3.3 Texture Types

Textures define the visual appearance of UI elements and consist of up to five fields:

**Appearance**: `Flat`, `Raised`, `Sunken`

**Fill method**: `Gradient`, `Solid`

**Gradient direction** (when using Gradient):
- `Horizontal`, `Vertical`, `Diagonal`, `Crossdiagonal`
- `Pipecross`, `Elliptic`, `Rectangle`, `Pyramid`

**Effects**: `Interlaced` (darkens alternating lines)

**Bevel**: `Bevel1` (edge-placed), `Bevel2` (inset one pixel)

**Special types**:
- `ParentRelative` - inherit parent texture (pseudo-transparency)
- `Pixmap` - use a custom image file

### 3.4 Color Formats

- Hexadecimal: `#RRGGBB` (e.g., `#1a1a2e`)
- RGB notation: `rgb:<0-255>/<0-255>/<0-255>` (e.g., `rgb:26/26/46`)

Each textured element supports:
- `color` - primary/start color
- `colorTo` - end color (for gradients)

### 3.5 Font Specifications

Format: `family-size[:style]`

Examples:
- `sans-10`
- `monospace-12:bold`
- `DejaVu Sans-11:bold:italic`

Font effects:
- Shadow: `shadow.x`, `shadow.y`, `shadow.color`
- Halo: `halo.color`

### 3.6 Window Decoration Resources

#### Title Bar
```
window.title.focus:           <texture>
window.title.focus.color:     <color>
window.title.focus.colorTo:   <color>
window.title.focus.pixmap:    <filename>
window.title.unfocus:         <texture>
window.title.unfocus.color:   <color>
window.title.unfocus.colorTo: <color>
window.title.unfocus.pixmap:  <filename>
window.title.height:          <integer pixels>
```

#### Window Label (title text area)
```
window.label.focus:           <texture>
window.label.focus.color:     <color>
window.label.focus.colorTo:   <color>
window.label.focus.textColor: <color>
window.label.focus.font:      <font>
window.label.focus.pixmap:    <filename>
window.label.unfocus:         <texture>
window.label.unfocus.color:   <color>
window.label.unfocus.colorTo: <color>
window.label.unfocus.textColor: <color>
window.label.unfocus.font:    <font>
window.label.unfocus.pixmap:  <filename>
window.label.active:          <texture>      (active tab)
window.label.active.textColor: <color>
window.justify:               Left|Center|Right
```

#### Title Bar Buttons
Each button type supports focused, unfocused, and pressed states:
```
window.close.pixmap:          <filename>
window.close.pressed.pixmap:  <filename>
window.close.unfocus.pixmap:  <filename>

window.maximize.pixmap:       <filename>
window.maximize.pressed.pixmap: <filename>
window.maximize.unfocus.pixmap: <filename>

window.iconify.pixmap:        <filename>
window.iconify.pressed.pixmap: <filename>
window.iconify.unfocus.pixmap: <filename>

window.shade.pixmap:          <filename>
window.shade.pressed.pixmap:  <filename>
window.shade.unfocus.pixmap:  <filename>

window.stick.pixmap:          <filename>
window.stick.pressed.pixmap:  <filename>
window.stick.unfocus.pixmap:  <filename>

window.lhalf.pixmap:          <filename>    (left-half maximize)
window.rhalf.pixmap:          <filename>    (right-half maximize)
```

Buttons also support color-based rendering:
```
window.button.focus:          <texture>
window.button.focus.color:    <color>
window.button.focus.colorTo:  <color>
window.button.focus.picColor: <color>
window.button.unfocus:        <texture>
window.button.unfocus.color:  <color>
window.button.unfocus.picColor: <color>
window.button.pressed:        <texture>
window.button.pressed.color:  <color>
window.button.pressed.picColor: <color>
```

#### Window Frame
```
window.frame.focusColor:      <color>
window.frame.unfocusColor:    <color>
```

#### Window Handle (bottom resize bar) and Grips
```
window.handle.focus:          <texture>
window.handle.focus.color:    <color>
window.handle.focus.colorTo:  <color>
window.handle.focus.pixmap:   <filename>
window.handle.unfocus:        <texture>
window.handle.unfocus.color:  <color>
window.handle.unfocus.colorTo: <color>
window.handle.unfocus.pixmap: <filename>
window.handleWidth:           <integer pixels>

window.grip.focus:            <texture>
window.grip.focus.color:      <color>
window.grip.focus.colorTo:    <color>
window.grip.focus.pixmap:     <filename>
window.grip.unfocus:          <texture>
window.grip.unfocus.color:    <color>
window.grip.unfocus.colorTo:  <color>
window.grip.unfocus.pixmap:   <filename>
```

#### Borders and Rounding
```
window.bevelWidth:            <integer 0-255>
window.borderColor:           <color>
window.borderWidth:           <integer 0-255>
window.roundCorners:          TopLeft TopRight BottomLeft BottomRight
```

### 3.7 Titlebar Button Layout

Configured in the init file (not the style):
```
session.titlebar.left:  Stick
session.titlebar.right: Shade Minimize Maximize Close
```

Available buttons: `Close`, `Maximize`, `MenuIcon`, `Minimize`, `Shade`, `Stick`,
`LHalf`, `RHalf`

### 3.8 Decoration Presets

The `[Deco]` setting in the apps file supports these presets:

| Preset   | Description                                           |
|----------|-------------------------------------------------------|
| `NORMAL` | Full decorations (title, handle, grips, border, tabs) |
| `NONE`   | No decorations at all                                 |
| `BORDER` | Border only                                           |
| `TAB`    | Titlebar and tabs only                                |
| `TINY`   | Titlebar only, no resize handles                      |
| `TOOL`   | Similar to TINY (tool window style)                   |

A bitmask is also available where bits 0-10 correspond to:
Titlebar, Handle/Grips, Border, Iconify, Maximize, Close, Menu, Sticky, Shade,
External Tabs, Focus Enabled.

### 3.9 Style Overlay

The overlay file (`~/.fluxbox/overlay`, path set by `session.styleOverlay`) lets
users override any style resource globally. Same syntax as style files. Useful for
enforcing consistent fonts or colors across all themes:

```
! Override fonts in all styles
*font: DejaVu Sans-10
toolbar.clock.font: DejaVu Sans Mono-10
```

### 3.10 Wildcard Matching

Styles support wildcard syntax with `*` to apply declarations broadly:
```
*textColor:       rgb:200/200/200    ! all text colors
*font:            sans-10            ! all fonts
toolbar.clock.textColor: #ff0000    ! specific override wins
```

---

## 4. Workspace Model

### 4.1 Virtual Desktops

Fluxbox supports multiple virtual workspaces (desktops), configured via:
```
session.screen0.workspaces:      4
session.screen0.workspaceNames:  Main, Web, Code, Media
```

### 4.2 Workspace Navigation

- **Wrap-around**: `session.screen0.workspacewarping: True` enables cycling
  past the last workspace back to the first
- **Keyboard**: `NextWorkspace`, `PrevWorkspace`, `RightWorkspace`, `LeftWorkspace`,
  `Workspace N` (direct jump)
- **Mouse**: Scroll wheel on desktop changes workspaces (default binding)
- **Dynamic**: `AddWorkspace` and `RemoveLastWorkspace` commands

### 4.3 Window-Workspace Interaction

- **SendToWorkspace N**: Move a window to workspace N without following it
- **TakeToWorkspace N**: Move window and switch to workspace N
- **SendToNextWorkspace / SendToPrevWorkspace**: Move window one workspace over
- **TakeToNextWorkspace / TakeToPrevWorkspace**: Move window and follow
- **Sticky windows**: `Stick` / `StickWindow` makes a window visible on all workspaces

### 4.4 Window Placement Per Workspace

Via the apps file, windows can be forced to open on a specific workspace:
```
[app] (class=Firefox)
  [Workspace] {1}
  [Jump] {yes}        # Switch to that workspace when the window opens
[end]
```

---

## 5. Window Management Features

### 5.1 Focus Models

Fluxbox supports three focus models:

| Model              | Behavior                                                          |
|--------------------|-------------------------------------------------------------------|
| `ClickToFocus`     | Windows gain focus only when clicked                              |
| `MouseFocus`       | Focus follows mouse pointer during movement (sloppy focus)        |
| `StrictMouseFocus` | Focus always follows mouse, including layer/desktop changes       |

Additional focus settings:
- `session.screen0.autoRaise: True` - auto-raise focused windows after delay
- `session.autoRaiseDelay: 250` - milliseconds before auto-raise
- `session.screen0.clickRaises: True` - clicking anywhere on window raises it
- `session.screen0.focusNewWindows: True` - newly opened windows receive focus

Tab focus can independently be click-based or mouse-following.

### 5.2 Window Placement Strategies

| Strategy                  | Description                                         |
|---------------------------|-----------------------------------------------------|
| `RowSmartPlacement`       | Place windows in rows without overlapping (default) |
| `ColSmartPlacement`       | Place windows in columns without overlapping        |
| `CascadePlacement`        | Cascade below previous window's titlebar            |
| `UnderMousePlacement`     | Place new windows centered under mouse cursor       |
| `RowMinOverlapPlacement`  | Rows with minimal overlap when no free space        |
| `ColMinOverlapPlacement`  | Columns with minimal overlap when no free space     |
| `AutotabPlacement`        | Tab into the currently focused window               |

Direction control:
- `session.screen0.rowPlacementDirection`: `LeftToRight` | `RightToLeft`
- `session.screen0.colPlacementDirection`: `TopToBottom` | `BottomToTop`

### 5.3 Edge Snapping

```
session.screen0.edgeSnapThreshold:       10   # pixels for window-edge snapping
session.screen0.edgeResizeSnapThreshold: 0    # pixels for resize-edge snapping
```

When moving a window, it snaps to screen edges and other window edges when within
the threshold distance.

### 5.4 Tiling / Half-Screen Positioning

Fluxbox doesn't have a built-in tiling mode but supports tiling-like behavior through
keybindings:

```
# Half-screen tiling via keybindings
Mod4 Left  :MacroCmd {ResizeTo 50% 100%} {MoveTo 0 0 Left}
Mod4 Right :MacroCmd {ResizeTo 50% 100%} {MoveTo 0 0 Right}
Mod4 Up    :MacroCmd {ResizeTo 100% 50%} {MoveTo 0 0 Top}
Mod4 Down  :MacroCmd {ResizeTo 100% 50%} {MoveTo 0 0 Bottom}

# Quarter-screen
Ctrl Mod4 1 :MacroCmd {ResizeTo 50% 50%} {MoveTo 0 0 TopLeft}
Ctrl Mod4 2 :MacroCmd {ResizeTo 50% 50%} {MoveTo 0 0 TopRight}
Ctrl Mod4 3 :MacroCmd {ResizeTo 50% 50%} {MoveTo 0 0 BottomLeft}
Ctrl Mod4 4 :MacroCmd {ResizeTo 50% 50%} {MoveTo 0 0 BottomRight}
```

Built-in arrangement commands:
- `ArrangeWindows pattern` - minimize overlap for all matching windows
- `ArrangeWindowsVertical pattern` - arrange vertically
- `ArrangeWindowsHorizontal pattern` - arrange horizontally
- `ShowDesktop` - toggle minimize all

Titlebar buttons `LHalf` and `RHalf` provide one-click half-screen placement.

### 5.5 Tabbed Windows (Window Grouping)

One of Fluxbox's signature features. Multiple windows can be combined into a single
frame with tabs.

**Manual tabbing**: Middle-click (or Ctrl+left-click) and drag one window's tab onto
another window to group them.

**Tab display modes**:
- In-titlebar tabs: `session.screen0.tabs.intitlebar: True` (default)
  - Tabs embedded in the title bar, width scales by number of tabs
- External tabs: `session.screen0.tabs.intitlebar: False`
  - Tabs appear outside the window at a configurable position
  - `session.screen0.tab.placement`: TopLeft, TopRight, BottomLeft, BottomRight,
    LeftTop, LeftBottom, RightTop, RightBottom
  - `session.screen0.tab.width: 64` (pixels)

**Tab settings**:
- `session.tabPadding: 0` - spacing between tabs in pixels
- `session.tabsAttachArea: Window` - drop zone for attaching (`Window` or `Titlebar`)

**Tab operations**:
- `NextTab` / `PrevTab` - cycle through tabs
- `Tab N` - jump to tab N
- `MoveTabRight` / `MoveTabLeft` - reorder tabs
- `DetachClient` - remove current tab from group
- `StartTabbing` - mouse action to start tab drag

**Auto-grouping** via the apps file:
```
[group] (workspace)
  [app] (class=urxvt)
  [app] (class=xterm)
[end]
```
All matching windows automatically tab together.

### 5.6 Window Shading

`Shade` / `ShadeWindow` rolls a window up to show only its title bar. Double-clicking
the titlebar toggles shade state (default binding). `ShadeOn` / `ShadeOff` set state
explicitly.

### 5.7 Window Transparency

```
session.screen0.window.focus.alpha:   255    # 0-255 (0=transparent, 255=opaque)
session.screen0.window.unfocus.alpha: 255
session.forcePseudoTransparency:      False
```

Per-window: `SetAlpha [focused-alpha [unfocused-alpha]]`

### 5.8 Window Operations Summary

| Command              | Description                                    |
|----------------------|------------------------------------------------|
| `Minimize`           | Minimize/iconify window                        |
| `Maximize`           | Toggle maximize                                |
| `MaximizeHorizontal` | Maximize width only                            |
| `MaximizeVertical`   | Maximize height only                           |
| `Fullscreen`         | Borderless fullscreen                          |
| `Close`              | Close window                                   |
| `Kill`               | Force-kill window                              |
| `Shade`              | Roll up to titlebar only                       |
| `Stick`              | Toggle visibility on all workspaces            |
| `Raise` / `Lower`    | Reorder within current layer                   |
| `RaiseLayer`         | Move to higher layer                           |
| `LowerLayer`         | Move to lower layer                            |
| `SetLayer <layer>`   | Move to specific layer                         |
| `ToggleDecor`        | Toggle decorations                             |
| `SetDecor <decor>`   | Set specific decoration preset                 |
| `ResizeTo W H`       | Resize to absolute dimensions (pixels or %)    |
| `Resize dW dH`       | Resize relative                                |
| `MoveTo X Y [anchor]`| Move to absolute position                      |
| `Move dX dY`         | Move relative                                  |
| `SetAlpha a1 [a2]`   | Set transparency                               |
| `SetHead N`          | Move to monitor N                              |

### 5.9 Per-Window Rules (apps file)

Comprehensive per-window behavior control:

```
[app] (class=Firefox)
  [Workspace]   {1}
  [Dimensions]  {1024 768}
  [Position]    (Center) {0 0}
  [Deco]        {NORMAL}
  [Sticky]      {no}
  [Layer]       {8}
  [Alpha]       {255 200}
  [Maximized]   {no}
  [Minimized]   {no}
  [Fullscreen]  {no}
  [Shaded]      {no}
  [Tab]         {yes}
  [FocusNewWindow] {yes}
  [FocusHidden] {no}
  [IconHidden]  {no}
  [Close]       {yes}         # Save settings on close
  [Jump]        {no}
  [Head]        {0}
[end]
```

### 5.10 Window Matching Patterns

Pattern syntax: `(propertyname[!=]regexp)`

| Property        | Source                        |
|-----------------|-------------------------------|
| `Name`          | WM_CLASS first field          |
| `Class`         | WM_CLASS second field         |
| `Title`         | WM_NAME / _NET_WM_NAME       |
| `Role`          | WM_WINDOW_ROLE                |
| `Transient`     | yes/no for dialog windows     |
| `Maximized`     | yes/no current state          |
| `Minimized`     | yes/no current state          |
| `Fullscreen`    | yes/no current state          |
| `Shaded`        | yes/no current state          |
| `Stuck`         | yes/no current state          |
| `FocusHidden`   | yes/no current state          |
| `IconHidden`    | yes/no current state          |
| `Urgent`        | yes/no current state          |
| `Workspace`     | Numeric workspace index       |
| `WorkspaceName` | Workspace string name         |
| `Head`          | Monitor number                |
| `Layer`         | Layer name                    |
| `Screen`        | Screen number                 |
| `@XPROP`        | Custom X property             |

Special values:
- `[current]` - matches the focused window's corresponding value
- `[mouse]` - matches the head containing the mouse (for Head property)

Multiple patterns are AND-combined:
```
[app] (class=Firefox) (role=browser)
```

Regular expressions follow POSIX regex syntax.

---

## 6. Menus

### 6.1 Menu Types

- **Root menu**: Right-click on desktop; main application launcher
- **Workspace menu**: Middle-click on desktop; shows workspaces and window list
- **Window menu**: Right-click on titlebar; per-window operations
- **Custom menus**: Arbitrary menus triggered by keybinding (`CustomMenu path`)

### 6.2 Root Menu File Format

Location: `~/.fluxbox/menu` (configurable via `session.menuFile`)

```
[begin] (Fluxbox Menu)
  [exec] (Terminal) {xterm} </path/to/icon.png>
  [exec] (Firefox) {firefox}
  [separator]
  [submenu] (System)
    [exec] (File Manager) {thunar}
    [exec] (Task Manager) {htop -t}
  [end]
  [submenu] (Styles) {Choose a style}
    [stylesdir] (~/.fluxbox/styles)
    [stylesmenu] (/usr/share/fluxbox/styles)
  [end]
  [separator]
  [config] (Configuration)
  [workspaces] (Workspaces)
  [reconfig] (Reload Config)
  [restart] (Restart)
  [exit] (Exit)
[end]
```

### 6.3 Menu Tags Reference

| Tag            | Syntax                                    | Description                           |
|----------------|-------------------------------------------|---------------------------------------|
| `[begin]`      | `(title)`                                 | Menu root (required)                  |
| `[end]`        | -                                         | End menu/submenu (required)           |
| `[exec]`       | `(label) {command} <icon>`                | Run a shell command                   |
| `[submenu]`    | `(label) {title} <icon>`                  | Nested submenu                        |
| `[separator]`  | -                                         | Horizontal dividing line              |
| `[nop]`        | `(label) <icon>`                          | Non-operational label                 |
| `[include]`    | `(path)`                                  | Include another menu file inline      |
| `[config]`     | `(label) <icon>`                          | Built-in config submenu               |
| `[workspaces]` | `(label) <icon>`                          | Workspaces submenu                    |
| `[reconfig]`   | `(label) <icon>`                          | Reload all config                     |
| `[restart]`    | `(label) {command} <icon>`                | Restart fluxbox (or switch WM)        |
| `[exit]`       | `(label) <icon>`                          | Exit fluxbox                          |
| `[style]`      | `(label) {filename} <icon>`               | Apply a specific style                |
| `[stylesdir]`  | `(label) {directory} <icon>`              | Submenu from directory of styles      |
| `[stylesmenu]` | `(directory) <icon>`                      | Inline style items from directory     |
| `[wallpapers]` | `(directory) {command} <icon>`            | Wallpaper picker from directory       |
| `[command]`    | `(label) <icon>`                          | Execute any keys-file command         |
| `[encoding]`   | `{encoding} ... [endencoding]`            | Character encoding for strings        |

### 6.4 Window Menu Tags

```
[shade]
[stick]
[maximize]
[iconify]
[close]
[kill]
[raise]
[lower]
[settitledialog]
[sendto]
[layer]
[alpha]
[extramenus]
[separator]
```

### 6.5 Pipe Menus

Not directly supported as a first-class feature like in Openbox, but `[include]`
can reference script output, and `[exec]` entries combined with `CustomMenu` provide
similar dynamic menu capabilities.

### 6.6 Menu Icons

Icons are optional and specified in angle brackets: `<path/to/icon.png>`
Supported formats: `.xpm`, `.png`

### 6.7 Menu Commands

- `RootMenu` - open the root menu
- `WorkspaceMenu` - open the workspace/window list menu
- `WindowMenu` - open the current window's context menu
- `ClientMenu [pattern]` - open a window list filtered by pattern
- `CustomMenu path` - open a custom menu file
- `HideMenus` - close all open menus

---

## 7. Toolbar and Slit

### 7.1 Toolbar Overview

The toolbar is a configurable panel that displays workspace info, window list, clock,
and system tray. It is composed of modular "tools" that can be rearranged.

### 7.2 Toolbar Configuration

```
session.screen0.toolbar.visible:       True
session.screen0.toolbar.autoHide:      False
session.screen0.toolbar.autoRaise:     False
session.screen0.toolbar.widthPercent:  100        # 1-100
session.screen0.toolbar.height:        0          # 0 = style-controlled
session.screen0.toolbar.layer:         Dock
session.screen0.toolbar.maxOver:       False      # allow windows to maximize over toolbar
session.screen0.toolbar.onhead:        1          # which monitor (xinerama)
session.screen0.toolbar.alpha:         255        # 0-255 transparency
```

### 7.3 Toolbar Placement

12 positions available:
```
TopLeft,    TopCenter,    TopRight
LeftTop,    LeftCenter,   LeftBottom
RightTop,   RightCenter,  RightBottom
BottomLeft, BottomCenter, BottomRight
```

```
session.screen0.toolbar.placement: BottomCenter
```

### 7.4 Toolbar Tools

Configured as a comma-separated list:
```
session.screen0.toolbar.tools: workspacename, prevworkspace, nextworkspace, iconbar, prevwindow, nextwindow, systemtray, clock
```

Available tools:
| Tool              | Description                                      |
|-------------------|--------------------------------------------------|
| `workspacename`   | Displays current workspace name                  |
| `prevworkspace`   | Button to go to previous workspace               |
| `nextworkspace`   | Button to go to next workspace                   |
| `iconbar`         | Task list / window buttons                       |
| `prevwindow`      | Button to cycle to previous window               |
| `nextwindow`      | Button to cycle to next window                   |
| `systemtray`      | System tray for status icons                     |
| `clock`           | Time display                                     |

### 7.5 Iconbar (Taskbar)

The iconbar displays buttons for open windows:

```
session.screen0.iconbar.mode:             {static groups} (workspace)
session.screen0.iconbar.usePixmap:        True
session.screen0.iconbar.iconTextPadding:  10
session.screen0.iconbar.alignment:        Relative     # Left|Relative|RelativeSmart|Right
session.screen0.iconbar.iconifiedPattern: ( %t )       # format for minimized windows
session.screen0.iconbar.iconWidth:        128
```

Iconbar display modes:
| Mode               | Description                          |
|--------------------|--------------------------------------|
| `None`             | No window buttons                    |
| `Icons`            | Only minimized windows               |
| `NoIcons`          | Only non-minimized windows           |
| `WorkspaceIcons`   | Minimized windows on current workspace |
| `WorkspaceNoIcons` | Non-minimized on current workspace   |
| `Workspace`        | All windows on current workspace     |
| `All Windows`      | All windows across all workspaces    |

### 7.6 System Tray Pinning

```
session.screen0.pinLeft:  class1, class2    # pin to left side of tray
session.screen0.pinRight: class3, class4    # pin to right side of tray
```

### 7.7 Clock Format

```
session.screen0.strftimeFormat: %k:%M     # strftime format string
```

### 7.8 Toolbar Styling

Style file resources:
```
toolbar:                          <texture>
toolbar.bevelWidth:               <integer>
toolbar.borderColor:              <color>
toolbar.borderWidth:              <integer>
toolbar.color:                    <color>
toolbar.colorTo:                  <color>
toolbar.height:                   <integer>
toolbar.pixmap:                   <filename>
toolbar.shaped:                   <boolean>

toolbar.clock:                    <texture>
toolbar.clock.color:              <color>
toolbar.clock.colorTo:            <color>
toolbar.clock.textColor:          <color>
toolbar.clock.font:               <font>
toolbar.clock.justify:            Left|Center|Right
toolbar.clock.pixmap:             <filename>
toolbar.clock.borderColor:        <color>
toolbar.clock.borderWidth:        <integer>

toolbar.workspace:                <texture>
toolbar.workspace.color:          <color>
toolbar.workspace.colorTo:        <color>
toolbar.workspace.textColor:      <color>
toolbar.workspace.font:           <font>
toolbar.workspace.justify:        Left|Center|Right
toolbar.workspace.pixmap:         <filename>
toolbar.workspace.borderColor:    <color>
toolbar.workspace.borderWidth:    <integer>

toolbar.iconbar.focused:          <texture>
toolbar.iconbar.focused.color:    <color>
toolbar.iconbar.focused.colorTo:  <color>
toolbar.iconbar.focused.textColor: <color>
toolbar.iconbar.focused.font:     <font>
toolbar.iconbar.focused.justify:  Left|Center|Right
toolbar.iconbar.focused.pixmap:   <filename>

toolbar.iconbar.unfocused:        <texture>
toolbar.iconbar.unfocused.color:  <color>
toolbar.iconbar.unfocused.colorTo: <color>
toolbar.iconbar.unfocused.textColor: <color>
toolbar.iconbar.unfocused.font:   <font>
toolbar.iconbar.unfocused.justify: Left|Center|Right
toolbar.iconbar.unfocused.pixmap: <filename>

toolbar.iconbar.empty:            <texture>
toolbar.iconbar.empty.color:      <color>
toolbar.iconbar.empty.colorTo:    <color>
toolbar.iconbar.empty.pixmap:     <filename>

toolbar.button.scale:             <integer>
```

### 7.9 Slit

The slit is a special dock frame for "dockable" applications (Window Maker dockapps,
bbtools, gkrellm, etc.). Applications started with the `-w` flag dock into the slit.

```
session.screen0.slit.autoHide:    False
session.screen0.slit.autoRaise:   False
session.screen0.slit.layer:       Dock
session.screen0.slit.placement:   RightBottom
session.screen0.slit.maxOver:     False
session.screen0.slit.onhead:      0
session.screen0.slit.alpha:       255
```

The slit client order is preserved in `~/.fluxbox/slitlist` (one app name per line).

Slit styling:
```
slit:                             <texture>
slit.bevelWidth:                  <integer>
slit.borderColor:                 <color>
slit.borderWidth:                 <integer>
slit.color:                       <color>
slit.colorTo:                     <color>
slit.pixmap:                      <filename>
```

When slit style resources aren't set, it inherits toolbar appearance.

### 7.10 Startup Applications

Applications for the slit are launched in `~/.fluxbox/startup`:
```bash
#!/bin/sh

# Dockable apps
bbmail -w &
bbpager -w &
conky &

# Desktop setup
fbsetbg -l &           # restore last wallpaper
xmodmap ~/.Xmodmap &

# Start fluxbox (must be last, must use exec)
exec fluxbox
```

---

## 8. Key and Mouse Bindings

### 8.1 Keys File Location and Syntax

File: `~/.fluxbox/keys` (configurable via `session.keyFile`)

Basic syntax:
```
[modifier1] [modifier2] key :Command [arguments]
```

The space before `:` and the colon are mandatory. Modifiers and commands are
case-insensitive. Arguments may be case-sensitive.

Comments begin with `#` or `!`.

### 8.2 Modifiers

| Modifier    | Typical Key   |
|-------------|---------------|
| `Mod1`      | Alt           |
| `Mod4`      | Super (Win)   |
| `Control`   | Ctrl          |
| `Shift`     | Shift         |
| `None`      | No modifier   |
| `OnDesktop` | Mouse on desktop (special modifier) |
| `OnToolbar` | Mouse on toolbar |
| `OnWindow`  | Mouse on window frame |
| `OnTitlebar`| Mouse on titlebar |
| `OnTab`     | Mouse on tab |
| `OnWindowBorder` | Mouse on window border |
| `OnLeftGrip`  | Mouse on left resize grip |
| `OnRightGrip` | Mouse on right resize grip |

Keys can be specified by name (`a`, `space`, `Return`, `Left`, `F1`) or
by numeric keycode (`38`, `0xf3`).

### 8.3 Chained Keybindings

Emacs-style key sequences:
```
Mod4 x Mod4 t :Exec xterm        # Win+x then Win+t opens terminal
Control a 1   :Workspace 1        # Ctrl+a then 1 goes to workspace 1
Control a 2   :Workspace 2        # Ctrl+a then 2 goes to workspace 2
```

Press `Escape` to abort a chain mid-sequence.

### 8.4 Keymodes

Named keybinding modes (like Vim modes):
```
# Define bindings for "moving" mode
moving: h :MoveLeft 10
moving: l :MoveRight 10
moving: j :MoveDown 10
moving: k :MoveUp 10
moving: Escape :KeyMode default

# Activate "moving" mode
Mod4 m :KeyMode moving Escape
```

When a keymode is activated, only bindings prefixed with that mode name are active.
All other keybindings are temporarily deactivated.

### 8.5 Mouse Bindings

Mouse events:
- `MouseN` - button N pressed and held
- `ClickN` - button N clicked (press + release without movement)
- `DoubleN` - button N double-clicked
- `MoveN` - button N held while moving (fires repeatedly)

Context modifiers for mouse:
- `OnDesktop` - root window
- `OnToolbar` - toolbar
- `OnTitlebar` - window titlebar
- `OnWindow` - anywhere on window frame
- `OnTab` - on a tab
- `OnWindowBorder` - window border
- `OnLeftGrip` / `OnRightGrip` - resize grips

Example default mouse bindings:
```
OnDesktop Mouse1   :HideMenus
OnDesktop Mouse2   :WorkspaceMenu
OnDesktop Mouse3   :RootMenu
OnDesktop Mouse4   :NextWorkspace
OnDesktop Mouse5   :PrevWorkspace

OnTitlebar Mouse1  :MacroCmd {Raise} {Focus} {ActivateTab}
OnTitlebar Move1   :StartMoving
OnTitlebar Double1 :Shade

Mod1 Mouse1        :MacroCmd {Raise} {Focus} {StartMoving}
Mod1 Mouse3        :MacroCmd {Raise} {Focus} {StartResizing NearestCorner}
```

### 8.6 Complete Command Reference

#### Window Commands
```
Minimize / MinimizeWindow / Iconify
Maximize / MaximizeWindow
MaximizeHorizontal / MaximizeVertical
Fullscreen
Raise / Lower
RaiseLayer [offset] / LowerLayer [offset]
SetLayer <layer>
Close
Kill / KillWindow
Shade / ShadeWindow / ShadeOn / ShadeOff
Stick / StickWindow
SetDecor <decor>
ToggleDecor
NextTab / PrevTab
Tab <number>
MoveTabRight / MoveTabLeft
DetachClient
ResizeTo <width[%]> <height[%]>
Resize <delta-width[%]> <delta-height[%]>
ResizeHorizontal <delta> / ResizeVertical <delta>
MoveTo <x[%]> <y[%]> [anchor]
Move <dx> <dy>
MoveRight <d> / MoveLeft <d> / MoveUp <d> / MoveDown <d>
TakeToWorkspace <n> / SendToWorkspace <n>
TakeToNextWorkspace [offset] / TakeToPrevWorkspace [offset]
SendToNextWorkspace [offset] / SendToPrevWorkspace [offset]
SetAlpha [alpha [unfocused-alpha]]
SetHead <number>
SendToNextHead [offset] / SendToPrevHead [offset]
SetXProp <PROP=value>
```

#### Workspace Commands
```
AddWorkspace / RemoveLastWorkspace
NextWorkspace [n] / PrevWorkspace [n]
RightWorkspace [n] / LeftWorkspace [n]
Workspace <number>
NextWindow [{options}] [pattern] / PrevWindow [{options}] [pattern]
NextGroup [{options}] [pattern] / PrevGroup [{options}] [pattern]
GotoWindow <number> [{options}] [pattern]
Activate [pattern] / Focus [pattern]
Attach <pattern>
FocusLeft / FocusRight / FocusUp / FocusDown
ArrangeWindows [pattern]
ArrangeWindowsVertical [pattern] / ArrangeWindowsHorizontal [pattern]
ShowDesktop
Deiconify <mode> <destination>
SetWorkspaceName <name>
CloseAllWindows
```

#### Menu Commands
```
RootMenu
WorkspaceMenu
WindowMenu
ClientMenu [pattern]
CustomMenu <path>
HideMenus
```

#### WM Commands
```
Restart [path]
Quit / Exit
Reconfig / Reconfigure
SetStyle <path>
ReloadStyle
ExecCommand <args> / Exec <args> / Execute <args>
CommandDialog
SetEnv <name> <value> / Export <name=value>
SetResourceValue <resource> <value>
```

#### Meta-Commands
```
MacroCmd {cmd1} {cmd2} ...        # run multiple commands sequentially
Delay {command} [microseconds]     # delay execution
ToggleCmd {cmd1} {cmd2} ...       # alternate between commands on each invocation
BindKey <keybinding>               # add keybinding dynamically
KeyMode <keymode> [return-key]     # switch to named keymode
ForEach {command} [{condition}]    # apply to all matching windows (alias: Map)
If {condition} {then} [{else}]     # conditional command execution
```

#### Conditions (for If/ForEach/NextWindow/etc.)
```
Matches <pattern>       # window matches pattern
Some <condition>        # any window matches
Every <condition>       # all windows match
Not <condition>         # invert
And {cond1} {cond2}     # all must be true
Or {cond1} {cond2}      # any must be true
Xor {cond1} {cond2}     # boolean XOR
```

### 8.7 Default Keybindings

```
# Window management
Mod1 Tab           :NextWindow {groups} (workspace=[current])
Mod1 Shift Tab     :PrevWindow {groups} (workspace=[current])
Mod4 Tab           :NextTab
Mod4 Shift Tab     :PrevTab
Mod1 F4            :Close
Mod1 F5            :Kill
Mod1 F9            :Minimize
Mod1 F10           :Maximize
Mod1 F11           :Fullscreen

# Workspace switching
Control Mod1 Left  :PrevWorkspace
Control Mod1 Right :NextWorkspace

# Window movement
Mod4 Left          :SendToPrevWorkspace
Mod4 Right         :SendToNextWorkspace

# Application launching
Mod1 F1            :Exec xterm
Mod1 F2            :Exec fbrun

# Mouse on desktop
OnDesktop Mouse3   :RootMenu
OnDesktop Mouse2   :WorkspaceMenu
OnDesktop Mouse4   :NextWorkspace
OnDesktop Mouse5   :PrevWorkspace

# Mouse on windows
Mod1 Mouse1        :MacroCmd {Raise} {Focus} {StartMoving}
Mod1 Mouse3        :MacroCmd {Raise} {Focus} {StartResizing NearestCorner}

# System
Control Mod1 Delete :Exit
```

---

## 9. Configuration Files

### 9.1 File Overview

All files live in `~/.fluxbox/` by default:

| File         | Purpose                                    | Format               |
|--------------|--------------------------------------------|----------------------|
| `init`       | Main resource configuration                | X resource database  |
| `keys`       | Keyboard and mouse bindings                | Custom syntax        |
| `menu`       | Root menu definition                       | Custom tag syntax    |
| `apps`       | Per-window rules and auto-grouping         | Custom tag syntax    |
| `windowmenu` | Window context menu definition             | Tag syntax           |
| `startup`    | Startup script (programs to launch)        | Shell script         |
| `overlay`    | Style overrides                            | X resource syntax    |
| `slitlist`   | Slit client ordering                       | Plain list           |
| `styles/`    | Directory of theme files                   | X resource syntax    |

### 9.2 Init File (Resource Database)

The init file uses X resource database format: `resource.name: value`

Complete resource reference:

#### File Paths
```
session.menuFile:         ~/.fluxbox/menu
session.keyFile:          ~/.fluxbox/keys
session.appsFile:         ~/.fluxbox/apps
session.styleFile:        /usr/share/fluxbox/styles/default
session.styleOverlay:     ~/.fluxbox/overlay
session.slitlistFile:     ~/.fluxbox/slitlist
```

#### Titlebar Layout
```
session.titlebar.left:    Stick
session.titlebar.right:   Shade Minimize Maximize Close
```

#### Window Behavior
```
session.screen0.focusModel:                ClickToFocus
session.screen0.autoRaise:                 True
session.screen0.clickRaises:               True
session.screen0.focusNewWindows:           True
session.autoRaiseDelay:                    250
session.screen0.windowPlacement:           RowSmartPlacement
session.screen0.rowPlacementDirection:     LeftToRight
session.screen0.colPlacementDirection:     TopToBottom
session.screen0.edgeSnapThreshold:         10
session.screen0.edgeResizeSnapThreshold:   0
session.screen0.opaqueMove:                True
session.screen0.opaqueResize:              False
session.screen0.opaqueResizeDelay:         40
session.screen0.showwindowposition:        False
session.screen0.defaultDeco:               NORMAL
```

#### Window Transparency
```
session.screen0.window.focus.alpha:        255
session.screen0.window.unfocus.alpha:      255
session.forcePseudoTransparency:           False
```

#### Workspaces
```
session.screen0.workspaces:                4
session.screen0.workspaceNames:            Workspace 1,Workspace 2,Workspace 3,Workspace 4
session.screen0.workspacewarping:          True
session.screen0.struts:                    0,0,0,0
```

#### Toolbar
```
session.screen0.toolbar.visible:           True
session.screen0.toolbar.autoHide:          False
session.screen0.toolbar.autoRaise:         False
session.screen0.toolbar.widthPercent:      100
session.screen0.toolbar.height:            0
session.screen0.toolbar.layer:             Dock
session.screen0.toolbar.placement:         BottomCenter
session.screen0.toolbar.maxOver:           False
session.screen0.toolbar.onhead:            1
session.screen0.toolbar.alpha:             255
session.screen0.toolbar.tools:             workspacename, prevworkspace, nextworkspace, iconbar, prevwindow, nextwindow, systemtray, clock
```

#### Iconbar
```
session.screen0.iconbar.mode:              {static groups} (workspace)
session.screen0.iconbar.usePixmap:         True
session.screen0.iconbar.iconTextPadding:   10
session.screen0.iconbar.alignment:         Relative
session.screen0.iconbar.iconifiedPattern:  ( %t )
session.screen0.iconbar.iconWidth:         128
```

#### Tabs
```
session.screen0.tabs.intitlebar:           True
session.screen0.tab.placement:             TopLeft
session.screen0.tab.width:                 64
session.screen0.tabs.maxOver:              False
session.tabPadding:                        0
session.tabsAttachArea:                    Window
```

#### Slit
```
session.screen0.slit.autoHide:             False
session.screen0.slit.autoRaise:            False
session.screen0.slit.layer:                Dock
session.screen0.slit.placement:            RightBottom
session.screen0.slit.maxOver:              False
session.screen0.slit.onhead:               0
```

#### System Tray
```
session.screen0.pinLeft:                   (comma-separated classes)
session.screen0.pinRight:                  (comma-separated classes)
```

#### Clock
```
session.screen0.strftimeFormat:            %k:%M
```

#### Display
```
session.screen0.menuDelay:                 200
session.cacheLife:                          5
session.cacheMax:                          200
session.colorsPerChannel:                  4
session.doubleClickInterval:               250
session.ignoreBorder:                      False
session.menuSearch:                        itemstart
```

#### Remote Control
```
session.screen0.allowRemoteActions:        False
```

### 9.3 Keys File

See [Section 8](#8-key-and-mouse-bindings) for complete documentation.

### 9.4 Menu File

See [Section 6](#6-menus) for complete documentation.

### 9.5 Apps File

See [Section 5.9](#59-per-window-rules-apps-file) for complete documentation.

### 9.6 Startup File

A shell script (`~/.fluxbox/startup`) that launches applications before fluxbox.
Must be executable. The last line should be `exec fluxbox`.

```bash
#!/bin/sh
# Background applications
nitrogen --restore &
nm-applet &
volumeicon &
conky &

# Start fluxbox
exec fluxbox
```

### 9.7 Overlay File

Uses same syntax as style files. Overrides any style resource globally.
See [Section 3.9](#39-style-overlay).

### 9.8 Window Menu File

`~/.fluxbox/windowmenu` - defines the right-click window menu. Uses the same tag
syntax as the window menu tags documented in [Section 6.4](#64-window-menu-tags).

---

## 10. IPC and Scripting

### 10.1 fluxbox-remote

Command-line tool for controlling Fluxbox programmatically:
```bash
fluxbox-remote "Exec xterm"
fluxbox-remote "Workspace 2"
fluxbox-remote "Maximize"
fluxbox-remote "MacroCmd {ResizeTo 50% 100%} {MoveTo 0 0 Left}"
```

Requires `session.screen0.allowRemoteActions: True` in the init file.

Uses X11 properties for communication (non-standardized protocol). Any user with
X server access can use it, so some dangerous commands are disabled for security.

### 10.2 EWMH Compliance

Fluxbox has high EWMH compliance, meaning standard tools like `wmctrl` and `xdotool`
work well:
```bash
wmctrl -s 2                    # switch to workspace 2
wmctrl -r :ACTIVE: -t 3       # send active window to workspace 3
xdotool key super+Left         # simulate keypress
```

### 10.3 Reconfiguration

- `Reconfig` / `Reconfigure` command reloads all config files without restart
- `ReloadStyle` reloads only the style
- `Restart` fully restarts Fluxbox (preserves window state)
- Config changes to the keys file require a `Reconfigure` to take effect

---

## 11. Compatibility Priorities for Migration

### 11.1 High Priority (Should Support)

These are the core Fluxbox config files that users invest the most time in. Supporting
them enables direct migration from Fluxbox X11 to our Wayland compositor:

1. **Keys file (`~/.fluxbox/keys`)**
   - Most-customized file for power users
   - Syntax is well-defined and parseable
   - All commands map directly to compositor actions
   - Chained keybindings and keymodes are distinctive features worth preserving
   - Mouse binding contexts (OnDesktop, OnTitlebar, etc.) translate well to Wayland

2. **Apps file (`~/.fluxbox/apps`)**
   - Per-window rules are extremely useful
   - Pattern matching syntax (class, name, title, role) maps to Wayland app_id + title
   - Window properties (workspace, dimensions, position, decorations) all apply
   - Group sections (auto-tabbing) are a unique Fluxbox feature

3. **Style/Theme files**
   - Users and communities create extensive theme collections
   - X resource syntax is straightforward to parse
   - Texture rendering (gradients, bevels, etc.) needs reimplementation for GPU
   - Pixmap support needs to map to modern image loading
   - Consider supporting existing Fluxbox themes with a compatibility layer

4. **Init file (`~/.fluxbox/init`)**
   - Resource database format is simple to parse
   - Most settings have direct equivalents in a Wayland compositor
   - Titlebar button layout is easy to support
   - Focus model settings map directly

### 11.2 Medium Priority (Nice to Have)

5. **Menu file (`~/.fluxbox/menu`)**
   - Simple tag-based format, easy to parse
   - Could be supported as-is or with minor extensions
   - Icon support needs updating (add SVG, modern formats)

6. **Startup file (`~/.fluxbox/startup`)**
   - Just a shell script, trivially compatible
   - May need adaptation for Wayland-specific environment variables

### 11.3 Lower Priority (Can Diverge)

7. **Window menu file** - minor customization, can use internal defaults
8. **Overlay file** - simple to support if style files are supported
9. **Slitlist** - slit concept may not translate directly to Wayland

### 11.4 Key Wayland Adaptations Required

| Fluxbox Concept    | Wayland Adaptation                                          |
|--------------------|-------------------------------------------------------------|
| X11 WM_CLASS       | Use `app_id` from xdg-toplevel                             |
| WM_NAME            | Use `title` from xdg-toplevel                              |
| WM_WINDOW_ROLE     | May need application-specific handling                      |
| X property (@XPROP)| No direct equivalent; may need custom protocol             |
| Slit/dockapps      | Use layer-shell protocol for panel docking                  |
| System tray        | Use wlr-foreign-toplevel or status-notifier protocol       |
| fluxbox-remote     | Implement IPC via Unix socket or Wayland protocol           |
| Pixmap rendering   | Use GPU-accelerated texture rendering                       |
| Edge snapping      | Implement in compositor, not X server                       |
| EWMH compliance    | Implement wlr-foreign-toplevel-management + ext-foreign-toplevel |
| Focus stealing     | Use xdg-activation-v1 protocol                             |
| Keyboard grabbing  | Use keyboard-shortcuts-inhibit protocol for key capture     |

### 11.5 Features to Preserve

These Fluxbox features are distinctive and should be core to the Wayland compositor:

1. **Window tabbing/grouping** - signature feature, underserved in Wayland compositors
2. **Chainable keybindings** - Emacs-style multi-key sequences
3. **Keymodes** - Vim-like modal keybinding sets
4. **Per-window rules** - regex-based window matching with rich property control
5. **Configurable titlebar buttons** - user-defined button layout
6. **6-layer stacking model** - fine-grained z-ordering control
7. **Multiple window placement strategies** - smart, cascade, under-mouse, etc.
8. **MacroCmd / ToggleCmd / If** - command composition and conditional logic
9. **Text-file configuration** - no bloated XML/JSON, fast to edit
10. **Lightweight footprint** - stay minimal and fast

---

## Appendix A: Example Configuration Files

### A.1 Example Keys File
```
# Desktop mouse actions
OnDesktop Mouse1 :HideMenus
OnDesktop Mouse2 :WorkspaceMenu
OnDesktop Mouse3 :RootMenu
OnDesktop Mouse4 :NextWorkspace
OnDesktop Mouse5 :PrevWorkspace

# Window mouse actions
OnTitlebar Mouse1 :MacroCmd {Raise} {Focus} {ActivateTab}
OnTitlebar Move1  :StartMoving
OnTitlebar Mouse3 :WindowMenu
OnTitlebar Double1 :Shade

OnWindow Mod1 Mouse1 :MacroCmd {Raise} {Focus} {StartMoving}
OnWindow Mod1 Mouse3 :MacroCmd {Raise} {Focus} {StartResizing NearestCorner}

# Window switching
Mod1 Tab       :NextWindow {groups} (workspace=[current])
Mod1 Shift Tab :PrevWindow {groups} (workspace=[current])
Mod4 Tab       :NextTab
Mod4 Shift Tab :PrevTab
Mod4 1         :Tab 1
Mod4 2         :Tab 2
Mod4 3         :Tab 3

# Window operations
Mod1 F4        :Close
Mod1 F9        :Minimize
Mod1 F10       :Maximize
Mod1 F11       :Fullscreen

# Workspace navigation
Control Mod1 Left  :PrevWorkspace
Control Mod1 Right :NextWorkspace
Mod4 Left          :SendToPrevWorkspace
Mod4 Right         :SendToNextWorkspace

# Half-screen tiling
Mod4 h :MacroCmd {ResizeTo 50% 100%} {MoveTo 0 0 Left}
Mod4 l :MacroCmd {ResizeTo 50% 100%} {MoveTo 0 0 Right}

# Applications
Mod1 F1 :Exec xterm
Mod1 F2 :Exec fbrun

# Chained commands
Mod4 x Mod4 t :Exec xterm
Mod4 x Mod4 f :Exec firefox
Mod4 x Mod4 e :Exec emacs
```

### A.2 Example Apps File
```
[app] (class=Firefox)
  [Workspace] {1}
  [Dimensions] {1200 800}
  [Position] (Center) {0 0}
  [Jump] {yes}
[end]

[app] (class=XTerm)
  [Deco] {NORMAL}
  [Alpha] {240 180}
[end]

[app] (title=.*floating.*)
  [Deco] {BORDER}
  [Sticky] {yes}
  [Layer] {6}
[end]

[group] (workspace)
  [app] (class=URxvt)
  [app] (class=XTerm)
[end]

[app] (class=mpv)
  [Deco] {NONE}
  [Fullscreen] {yes}
[end]
```

### A.3 Example Style File
```
! Fluxbox Style: DarkMinimal

! Window title bar
window.title.focus:             Raised Gradient Vertical
window.title.focus.color:       #2d2d3f
window.title.focus.colorTo:     #1a1a2e
window.title.unfocus:           Flat Solid
window.title.unfocus.color:     #1a1a1a
window.title.height:            22

! Window label (title text)
window.label.focus:             ParentRelative
window.label.focus.textColor:   #e0e0e0
window.label.focus.font:        sans-10:bold
window.label.unfocus:           ParentRelative
window.label.unfocus.textColor: #808080
window.label.unfocus.font:      sans-10

! Window borders
window.borderWidth:             1
window.borderColor:             #0f0f1a
window.bevelWidth:              1
window.roundCorners:            TopLeft TopRight

! Window handle and grips
window.handleWidth:             4
window.handle.focus:            Flat Solid
window.handle.focus.color:      #2d2d3f
window.handle.unfocus:          Flat Solid
window.handle.unfocus.color:    #1a1a1a
window.grip.focus:              Flat Solid
window.grip.focus.color:        #4a4a6a
window.grip.unfocus:            Flat Solid
window.grip.unfocus.color:      #2a2a2a

! Buttons
window.button.focus:            Flat Solid
window.button.focus.color:      #2d2d3f
window.button.focus.picColor:   #e0e0e0
window.button.unfocus:          Flat Solid
window.button.unfocus.color:    #1a1a1a
window.button.unfocus.picColor: #606060
window.button.pressed:          Sunken Solid
window.button.pressed.color:    #1a1a2e

! Toolbar
toolbar:                        Flat Solid
toolbar.color:                  #1a1a2e
toolbar.borderWidth:            0
toolbar.height:                 24

toolbar.clock:                  Flat Solid
toolbar.clock.color:            #1a1a2e
toolbar.clock.textColor:        #a0a0c0
toolbar.clock.font:             monospace-10

toolbar.workspace:              Flat Solid
toolbar.workspace.color:        #2d2d3f
toolbar.workspace.textColor:    #e0e0e0
toolbar.workspace.font:         sans-10:bold

toolbar.iconbar.focused:        Flat Solid
toolbar.iconbar.focused.color:  #2d2d3f
toolbar.iconbar.focused.textColor: #e0e0e0
toolbar.iconbar.focused.font:   sans-10
toolbar.iconbar.unfocused:      Flat Solid
toolbar.iconbar.unfocused.color: #1a1a2e
toolbar.iconbar.unfocused.textColor: #808080
toolbar.iconbar.empty:          Flat Solid
toolbar.iconbar.empty.color:    #1a1a2e

! Menu
menu.title:                     Raised Gradient Vertical
menu.title.color:               #2d2d3f
menu.title.colorTo:             #1a1a2e
menu.title.textColor:           #e0e0e0
menu.title.font:                sans-10:bold
menu.title.justify:             Center

menu.frame:                     Flat Solid
menu.frame.color:               #1a1a2e
menu.frame.textColor:           #c0c0c0
menu.frame.font:                sans-10

menu.hilite:                    Flat Solid
menu.hilite.color:              #3d3d5f
menu.hilite.textColor:          #ffffff

menu.bullet:                    triangle
menu.bullet.position:           right
menu.borderWidth:               1
menu.borderColor:               #0f0f1a
menu.bevelWidth:                2
```

---

## Appendix B: Source References

- [Fluxbox Official Site](https://fluxbox.org/)
- [Fluxbox GitHub Repository](https://github.com/fluxbox/fluxbox)
- [Fluxbox Man Page](https://fluxbox.org/help/man-fluxbox/)
- [Fluxbox Keys Man Page](https://fluxbox.org/help/man-fluxbox-keys/)
- [Fluxbox Style Man Page](https://fluxbox.org/help/man-fluxbox-style/)
- [Fluxbox Menu Man Page](https://fluxbox.org/help/man-fluxbox-menu/)
- [Fluxbox Apps Man Page](https://fluxbox.org/help/man-fluxbox-apps/)
- [Fluxbox Remote Man Page](https://fluxbox.org/help/man-fluxbox-remote/)
- [Fluxbox Features Page](https://fluxbox.org/features/)
- [ArchWiki - Fluxbox](https://wiki.archlinux.org/title/Fluxbox)
- [Fluxbox Wikipedia](https://en.wikipedia.org/wiki/Fluxbox)
- [Ubuntu Fluxbox Man Pages](https://manpages.ubuntu.com/manpages/focal/en/man5/fluxbox-style.5.html)
- [Gentoo Wiki - Fluxbox](https://wiki.gentoo.org/wiki/Fluxbox)
