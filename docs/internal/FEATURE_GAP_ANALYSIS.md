# Fluxland vs Fluxbox — Feature Gap Analysis & Implementation Plan

## Status Legend
- **IMPLEMENTED** — Feature exists and works in fluxland
- **PARTIAL** — Feature exists but incomplete
- **MISSING** — Feature not implemented
- **N/A** — Not applicable to Wayland compositor (X11-only concept)

## Priority Legend
- **P0** — Bug / broken functionality that needs immediate fix
- **P1** — Important for Fluxbox parity, core WM features
- **P2** — Nice-to-have, advanced/niche features
- **N/A** — Not applicable to Wayland

---

## 1. WINDOW MANAGEMENT

| Feature | Status | Priority | Notes |
|---------|--------|----------|-------|
| ClickToFocus | IMPLEMENTED | — | |
| MouseFocus / Sloppy | IMPLEMENTED | — | |
| StrictMouseFocus | IMPLEMENTED | — | |
| ClickTabFocus / MouseTabFocus | MISSING | P2 | Tab-specific focus models |
| Auto-raise with delay | IMPLEMENTED | — | |
| Click-raises | IMPLEMENTED | — | |
| Focus new windows | IMPLEMENTED | — | |
| FocusProtection (Gain/Refuse/Deny/Lock) | MISSING | P2 | Per-window focus protection in apps file |
| RowSmartPlacement | IMPLEMENTED | — | |
| ColSmartPlacement | IMPLEMENTED | — | |
| CascadePlacement | IMPLEMENTED | — | |
| UnderMousePlacement | IMPLEMENTED | — | |
| RowMinOverlapPlacement | MISSING | P2 | |
| ColMinOverlapPlacement | MISSING | P2 | |
| AutotabPlacement | MISSING | P2 | Auto-tab new windows with existing |
| Placement direction (L→R/R→L, T→B/B→T) | MISSING | P2 | |
| Move (interactive) | IMPLEMENTED | — | |
| Resize (interactive) | IMPLEMENTED | — | |
| Shade / ShadeOn / ShadeOff | IMPLEMENTED | — | |
| Maximize (full) | IMPLEMENTED | — | |
| MaximizeVertical | IMPLEMENTED | — | |
| MaximizeHorizontal | IMPLEMENTED | — | |
| **LHalf / RHalf (half-screen tiling)** | **MISSING** | **P1** | Tile to left/right half of screen |
| Fullscreen | IMPLEMENTED | — | |
| Minimize / Iconify | IMPLEMENTED | — | |
| Close | IMPLEMENTED | — | |
| Kill | IMPLEMENTED | — | |
| Raise / Lower | IMPLEMENTED | — | |
| RaiseLayer / LowerLayer / SetLayer | IMPLEMENTED | — | |
| Stick / StickWindow | IMPLEMENTED | — | |
| ToggleDecor / SetDecor | IMPLEMENTED | — | |
| SetAlpha (relative +/- adjustment) | PARTIAL | P2 | Exists but no relative +/- syntax |
| Move delta-x delta-y (relative) | IMPLEMENTED | — | MoveLeft/Right/Up/Down |
| MoveTo with % coords and * wildcard | PARTIAL | P1 | Need to verify % and * support |
| **ResizeTo** | IMPLEMENTED | — | |
| **Resize (relative delta-w delta-h)** | **MISSING** | **P1** | Only ResizeTo (absolute) exists |
| **ResizeHorizontal / ResizeVertical** | **MISSING** | **P1** | Single-axis relative resize |
| StartResizing corner specification | PARTIAL | P2 | Works by cursor pos, no explicit corner arg |
| fullMaximization (over toolbar/slit) | MISSING | P1 | Maximize ignoring toolbar/slit |
| opaqueMove / opaqueResize | PARTIAL | P2 | Config parsed but always opaque (no wireframe) |
| showwindowposition overlay | MISSING | P2 | Position display during move |
| **FocusPrev (reverse cycle)** | **PARTIAL (BUG)** | **P0** | Maps to same function as FocusNext |
| **Directional focus (FocusLeft/Right/Up/Down)** | **MISSING** | **P1** | Spatial focus navigation |
| NextWindow/PrevWindow with pattern filtering | PARTIAL | P2 | Cycling works, no pattern arg |
| NextGroup / PrevGroup | MISSING | P2 | Cycle between tab groups |
| GotoWindow N | MISSING | P2 | Focus window by stack position |
| **Deiconify modes (All/AllWorkspace/Last)** | **PARTIAL** | **P1** | Only "last" mode exists |
| **SetHead / SendToNextHead / SendToPrevHead** | **MISSING** | **P1** | Multi-monitor window movement |
| edgeResizeSnapThreshold | MISSING | P2 | Snap during resize |
| **Workspace warping (edge flipping)** | **PARTIAL** | **P1** | Config parsed but not implemented in cursor.c |
| ArrangeWindows | IMPLEMENTED | — | |
| ArrangeWindowsVertical | IMPLEMENTED | — | |
| ArrangeWindowsHorizontal | IMPLEMENTED | — | |
| CascadeWindows | IMPLEMENTED | — | |
| **ArrangeWindowsStackLeft/Right/Top/Bottom** | **MISSING** | **P1** | Master-stack tiling layouts |
| Unclutter | MISSING | P2 | Reduce overlap without resize |
| **CloseAllWindows** | **MISSING** | **P1** | Close every window |
| Tab grouping (in titlebar) | IMPLEMENTED | — | |
| Interactive tabbing (drag) | IMPLEMENTED | — | |
| NextTab / PrevTab / MoveTabLeft/Right | IMPLEMENTED | — | |
| DetachClient | IMPLEMENTED | — | |
| Auto-grouping (apps file) | IMPLEMENTED | — | |
| External tabs (outside window) | MISSING | P2 | Tabs positioned outside window |
| Tab configuration (padding, attach area) | MISSING | P2 | session.tabPadding, tabsAttachArea |
| **Remember window properties (runtime save)** | **MISSING** | **P1** | "Remember..." menu to save to apps file |
| Attach pattern (programmatic grouping) | MISSING | P2 | |

## 2. WORKSPACE MANAGEMENT

| Feature | Status | Priority | Notes |
|---------|--------|----------|-------|
| Configurable workspace count | IMPLEMENTED | — | |
| Named workspaces | IMPLEMENTED | — | |
| NextWorkspace / PrevWorkspace | IMPLEMENTED | — | |
| Workspace N (direct switch) | IMPLEMENTED | — | |
| SendToWorkspace | IMPLEMENTED | — | |
| Sticky windows | IMPLEMENTED | — | |
| RightWorkspace / LeftWorkspace (no wrap) | MISSING | P2 | Like Next/Prev but don't wrap around |
| **TakeToWorkspace** | **MISSING** | **P1** | Move window AND switch to workspace |
| **SendToNextWorkspace / SendToPrevWorkspace** | **MISSING** | **P1** | Relative send |
| **TakeToNextWorkspace / TakeToPrevWorkspace** | **MISSING** | **P1** | Relative take |
| **AddWorkspace** | **MISSING** | **P1** | Dynamically add workspace |
| **RemoveLastWorkspace** | **MISSING** | **P1** | Dynamically remove workspace |
| SetWorkspaceName / Dialog | MISSING | P2 | Rename workspace at runtime |

## 3. TOOLBAR

| Feature | Status | Priority | Notes |
|---------|--------|----------|-------|
| Toolbar visibility toggle | IMPLEMENTED | — | |
| Toolbar placement (6 positions) | IMPLEMENTED | — | |
| Toolbar auto-hide | IMPLEMENTED | — | |
| Toolbar width percent | IMPLEMENTED | — | |
| Workspace display | IMPLEMENTED | — | |
| Iconbar / window list | IMPLEMENTED | — | |
| Clock display | IMPLEMENTED | — | |
| Iconbar modes (Workspace/All/Icons/etc.) | PARTIAL | P1 | Modes exist in code but not configurable from init file |
| **Clock format (strftimeFormat)** | **MISSING** | **P1** | Hardcoded "%H:%M" |
| **Toolbar height config** | **MISSING** | **P1** | Not in init file |
| **Toolbar layer config** | **MISSING** | **P1** | Not in init file |
| **Toolbar alpha/transparency** | **MISSING** | **P1** | Not in init file |
| Toolbar onhead (multi-monitor) | MISSING | P1 | Which monitor toolbar appears on |
| Toolbar autoRaise | MISSING | P2 | |
| Toolbar maxOver | MISSING | P1 | Related to fullMaximization |
| **Configurable toolbar tools order** | **MISSING** | **P2** | session.screen0.toolbar.tools |
| Iconbar alignment | MISSING | P2 | Left/Relative/Right |
| Iconbar icon width | MISSING | P2 | |
| Iconbar usePixmap (window icons) | MISSING | P2 | Show app icons in iconbar |
| Iconbar wheel mode (scroll behavior) | MISSING | P2 | |
| Prev/Next workspace buttons | MISSING | P2 | |
| **System tray** | **MISSING** | **P2** | Complex on Wayland; use status-notifier-watcher |
| Custom toolbar buttons | MISSING | P2 | |

## 4. SLIT

| Feature | Status | Priority | Notes |
|---------|--------|----------|-------|
| Slit container for dock apps | IMPLEMENTED | — | |
| Slit placement | IMPLEMENTED | — | In code but not loaded from init |
| Slit direction | IMPLEMENTED | — | In code but not loaded from init |
| Slit auto-hide | IMPLEMENTED | — | In code but not loaded from init |
| **Slit config from init file** | **MISSING** | **P1** | slit.autoHide, slit.placement, slit.direction, slit.layer, slit.onhead |
| Slit alpha/transparency | MISSING | P2 | |
| Slit layer config | MISSING | P2 | |
| Slit maxOver | MISSING | P2 | |
| Slitlist file (dockapp ordering) | MISSING | P2 | |
| ToggleSlitAbove / ToggleSlitHidden commands | MISSING | P2 | |

## 5. MENU SYSTEM

| Feature | Status | Priority | Notes |
|---------|--------|----------|-------|
| Root menu | IMPLEMENTED | — | |
| Window menu (context) | IMPLEMENTED | — | |
| [exec] | IMPLEMENTED | — | |
| [submenu] | IMPLEMENTED | — | |
| [separator] | IMPLEMENTED | — | |
| [nop] | IMPLEMENTED | — | |
| [include] | IMPLEMENTED | — | |
| [config] | IMPLEMENTED | — | |
| [workspaces] | IMPLEMENTED | — | |
| [reconfig] | IMPLEMENTED | — | |
| [restart] | IMPLEMENTED | — | |
| [exit] | IMPLEMENTED | — | |
| [style] | IMPLEMENTED | — | |
| [stylesdir] | IMPLEMENTED | — | |
| [command] | IMPLEMENTED | — | |
| Keyboard navigation | IMPLEMENTED | — | |
| Mouse navigation | IMPLEMENTED | — | |
| **[wallpapers] tag** | **MISSING** | **P2** | Wallpaper directory browser |
| [encoding]/[endencoding] | MISSING | P2 | Character encoding |
| [stylesmenu] | MISSING | P2 | Auto-generated style list |
| **ClientMenu command** | **MISSING** | **P1** | Window list filtered by pattern |
| **CustomMenu command** | **MISSING** | **P1** | Open menu from arbitrary file |
| **WorkspaceMenu command** | **MISSING** | **P1** | Middle-click workspace menu |
| Configurable window menu file | MISSING | P2 | session.screen0.windowMenu |
| **Menu icons** | **MISSING** | **P2** | Icon support on menu items |
| Menu type-ahead search | MISSING | P2 | session.menuSearch |
| Menu delay config | MISSING | P2 | session.screen0.menuDelay |
| "Remember..." submenu (extramenus) | MISSING | P1 | Save window props to apps file |
| [alpha] submenu in window menu | MISSING | P2 | |
| [settitledialog] | MISSING | P2 | |

## 6. KEYBINDINGS

| Feature | Status | Priority | Notes |
|---------|--------|----------|-------|
| Simple key bindings | IMPLEMENTED | — | |
| Chain key bindings | IMPLEMENTED | — | |
| KeyMode (modal bindings) | IMPLEMENTED | — | |
| MacroCmd | IMPLEMENTED | — | |
| ToggleCmd | IMPLEMENTED | — | |
| All window operation commands | IMPLEMENTED | — | |
| **If/Cond conditional execution** | **MISSING** | **P2** | If {condition} {then} {else} |
| **ForEach/Map** | **MISSING** | **P2** | Apply command to matching windows |
| **Delay** | **MISSING** | **P2** | Deferred execution |
| **BindKey (dynamic add)** | **MISSING** | **P2** | Add keybindings at runtime |
| **SetStyle / ReloadStyle** | **MISSING** | **P1** | Change/reload style via keybinding |
| **CommandDialog** | **MISSING** | **P2** | Interactive command entry |
| **SetEnv/Export** | **MISSING** | **P2** | Set environment variables |
| **SetResourceValue** | **MISSING** | **P2** | Change init resource at runtime |
| Context modifiers (OnDesktop, OnWindow, etc.) | PARTIAL | P2 | Supported in mouse bindings, not key bindings |

## 7. MOUSE BINDINGS

| Feature | Status | Priority | Notes |
|---------|--------|----------|-------|
| OnDesktop / OnToolbar / OnTitlebar / OnTab | IMPLEMENTED | — | |
| OnWindow / OnWindowBorder | IMPLEMENTED | — | |
| OnLeftGrip / OnRightGrip | IMPLEMENTED | — | |
| Mouse / Click / Move events | IMPLEMENTED | — | |
| Double-click events | IMPLEMENTED | — | |
| StartMoving / StartResizing / StartTabbing | IMPLEMENTED | — | |
| MacroCmd / ToggleCmd in mouse bindings | IMPLEMENTED | — | |
| OnSlit context | MISSING | P2 | |

## 8. THEMING / STYLING

| Feature | Status | Priority | Notes |
|---------|--------|----------|-------|
| Flat / Raised / Sunken textures | IMPLEMENTED | — | |
| Solid / Gradient fills | IMPLEMENTED | — | |
| All 8 gradient types | IMPLEMENTED | — | |
| Interlaced effect | IMPLEMENTED | — | |
| Bevel1 / Bevel2 | IMPLEMENTED | — | |
| ParentRelative | IMPLEMENTED | — | |
| Pixmap textures | IMPLEMENTED | — | |
| Window title/label/button/handle/grip styling | IMPLEMENTED | — | |
| Menu title/frame/hilite styling | IMPLEMENTED | — | |
| Toolbar styling | IMPLEMENTED | — | |
| Button pixmaps (all states) | IMPLEMENTED | — | |
| Font with shadow effects | IMPLEMENTED | — | |
| Text justification | IMPLEMENTED | — | |
| Round corners (parse) | PARTIAL | P2 | Parsed but not rendered |
| **Slit styling** | **MISSING** | **P2** | slit.*, slit.borderWidth, etc. |
| **Background/wallpaper style resources** | **MISSING** | **P2** | background.*, background.pixmap |
| LHalf / RHalf button pixmaps | MISSING | P1 | Depends on LHalf/RHalf feature |
| Menu bullet style config | MISSING | P2 | |
| Menu item height config | MISSING | P2 | |
| Toolbar shaped | N/A | — | X11 shape extension |
| Halo text effect | MISSING | P2 | |

## 9. CONFIGURATION

| Feature | Status | Priority | Notes |
|---------|--------|----------|-------|
| Init file (X resource format) | IMPLEMENTED | — | |
| Keys file | IMPLEMENTED | — | |
| Apps file (window rules) | IMPLEMENTED | — | |
| Menu file | IMPLEMENTED | — | |
| Style file | IMPLEMENTED | — | |
| Startup file | IMPLEMENTED | — | |
| Fluxbox compat fallback (~/.fluxbox) | IMPLEMENTED | — | |
| Runtime reconfigure | IMPLEMENTED | — | |
| **session.titlebar.left/right** | **MISSING** | **P1** | Button layout config in init file |
| **session.screen0.strftimeFormat** | **MISSING** | **P1** | Clock format |
| **session.screen0.iconbar.mode** | **MISSING** | **P1** | Iconbar mode in init file |
| **session.screen0.toolbar.height** | **MISSING** | **P1** | |
| **session.screen0.toolbar.layer** | **MISSING** | **P1** | |
| **session.screen0.toolbar.alpha** | **MISSING** | **P1** | |
| session.screen0.toolbar.onhead | MISSING | P1 | |
| **session.screen0.slit.*** | **MISSING** | **P1** | All slit init options |
| session.screen0.fullMaximization | MISSING | P1 | |
| session.screen0.defaultDeco | MISSING | P2 | |
| session.doubleClickInterval | MISSING | P2 | |
| session.screen0.menuDelay | MISSING | P2 | |
| session.menuSearch | MISSING | P2 | |
| session.screen0.allowRemoteActions | MISSING | P2 | |
| session.screen0.rootCommand | MISSING | P2 | |
| session.screen0.showwindowposition | MISSING | P2 | |
| session.tabPadding | MISSING | P2 | |
| session.screen0.window.focus/unfocus.alpha | MISSING | P1 | Global default window alpha |
| session.screen0.menu.alpha | MISSING | P2 | |
| session.styleOverlay | MISSING | P2 | |
| session.screen0.struts | MISSING | P2 | |
| FocusProtection in apps file | MISSING | P2 | |
| IgnoreSizeHints in apps file | MISSING | P2 | |

## 10. IPC / REMOTE CONTROL

| Feature | Status | Priority | Notes |
|---------|--------|----------|-------|
| IPC socket | IMPLEMENTED | — | JSON over Unix socket (better than X11) |
| get_workspaces | IMPLEMENTED | — | |
| get_windows | IMPLEMENTED | — | |
| get_outputs | IMPLEMENTED | — | |
| get_config | IMPLEMENTED | — | |
| command execution | IMPLEMENTED | — | |
| Event subscriptions | IMPLEMENTED | — | |
| --list-commands CLI | MISSING | P2 | |

## 11. NOT APPLICABLE TO WAYLAND

These Fluxbox features are X11-specific and don't apply:
- `SetXProp` / `@XPROP` pattern matching (X11 atoms)
- `session.forcePseudoTransparency` (X11 compositing)
- `session.colorsPerChannel` (8-bit X11 displays)
- `session.cacheLife` / `session.cacheMax` (X11 pixmap cache)
- `toolbar.shaped` (X11 shape extension)
- `fbsetbg` (use `swaybg` on Wayland)
- `-display`, `-screen`, `-sync` CLI options (X11)
- fluxbox-remote via X11 protocol (fluxland has better Unix socket IPC)
- SIGUSR1/SIGUSR2 signal handling (fluxland uses SIGHUP for reconfig)

---

# IMPLEMENTATION PLAN

## Phase 1: Bug Fixes & Quick Wins (P0 + Easy P1)

### 1.1 Fix FocusPrev (P0 — BUG)
- **Files**: `src/view.c`, `src/view.h`, `src/keyboard.c`
- **What**: Implement `wm_focus_prev_view()` that cycles in reverse order (currently FocusPrev calls the same function as FocusNext)
- **Complexity**: S
- **Test**: Unit test + manual test cycling forward then backward

### 1.2 Add Missing Init File Config Options (P1)
- **Files**: `src/config.c`, `src/config.h`
- **What**: Parse these from init file:
  - `session.titlebar.left` / `session.titlebar.right` (button layout)
  - `session.screen0.strftimeFormat` (clock format)
  - `session.screen0.iconbar.mode` (iconbar mode)
  - `session.screen0.toolbar.height`
  - `session.screen0.toolbar.layer`
  - `session.screen0.toolbar.alpha`
  - `session.screen0.toolbar.onhead`
  - `session.screen0.slit.autoHide`, `slit.placement`, `slit.direction`, `slit.layer`, `slit.onhead`
  - `session.screen0.fullMaximization`
  - `session.screen0.window.focus.alpha` / `unfocus.alpha` (global defaults)
- **Propagation files**: `src/toolbar.c`, `src/slit.c`, `src/decoration.c`, `src/view.c`
- **Complexity**: M
- **Test**: Add tests to `test_config.c`, manual verify each option

### 1.3 Clock Format Support (P1)
- **Files**: `src/toolbar.c`, `src/config.h`
- **What**: Use config's strftime format string instead of hardcoded "%H:%M"
- **Complexity**: S
- **Test**: Set format in init, verify clock display

### 1.4 Iconbar Mode Configuration (P1)
- **Files**: `src/toolbar.c`, `src/config.c`
- **What**: Read iconbar.mode from init file and apply to toolbar
- **Complexity**: S
- **Test**: Set different modes in init file, verify behavior

## Phase 2: Window Management Commands (P1)

### 2.1 Half-Screen Tiling (LHalf / RHalf) (P1)
- **Files**: `src/view.c`, `src/view.h`, `src/keybind.h`, `src/keybind.c`, `src/keyboard.c`, `src/ipc_commands.c`
- **What**: Implement LHalf (left 50% of output) and RHalf (right 50% of output), toggling like maximize. Save/restore geometry. Also add button pixmap support.
- **Complexity**: M
- **Test**: Keybinding test, verify geometry, toggle behavior

### 2.2 Relative Resize Commands (P1)
- **Files**: `src/view.c`, `src/view.h`, `src/keybind.h`, `src/keybind.c`, `src/keyboard.c`, `src/ipc_commands.c`
- **What**: Add `Resize delta-w delta-h`, `ResizeHorizontal delta-w`, `ResizeVertical delta-h` (support px and %)
- **Complexity**: M
- **Test**: Unit test resize deltas, boundary checks

### 2.3 Directional Focus (P1)
- **Files**: `src/view.c`, `src/view.h`, `src/keybind.h`, `src/keybind.c`, `src/keyboard.c`, `src/ipc_commands.c`
- **What**: Implement FocusLeft/Right/Up/Down using geometric proximity (find nearest view in that direction from current focused view center)
- **Complexity**: M
- **Test**: Unit test with mock view geometries

### 2.4 Multi-Monitor Window Movement (P1)
- **Files**: `src/view.c`, `src/view.h`, `src/keybind.h`, `src/keybind.c`, `src/keyboard.c`, `src/ipc_commands.c`
- **What**: SetHead N, SendToNextHead, SendToPrevHead — move window to specified output, adjusting position
- **Complexity**: M
- **Test**: Manual test with multi-monitor setup

### 2.5 Master-Stack Tiling Layouts (P1)
- **Files**: `src/view.c`, `src/view.h`, `src/keybind.h`, `src/keybind.c`, `src/keyboard.c`, `src/ipc_commands.c`
- **What**: ArrangeWindowsStackLeft/Right/Top/Bottom — first window gets half the screen, remaining stack in the other half
- **Complexity**: M
- **Test**: Unit test layout calculations

### 2.6 CloseAllWindows (P1)
- **Files**: `src/view.c`, `src/keybind.h`, `src/keybind.c`, `src/keyboard.c`, `src/ipc_commands.c`
- **What**: Close all windows on current workspace (send close to each)
- **Complexity**: S
- **Test**: Manual test

## Phase 3: Workspace Commands (P1)

### 3.1 TakeToWorkspace (P1)
- **Files**: `src/workspace.c`, `src/workspace.h`, `src/keybind.h`, `src/keybind.c`, `src/keyboard.c`, `src/ipc_commands.c`
- **What**: Move focused window to workspace N AND switch to that workspace
- **Complexity**: S
- **Test**: Keybinding test, verify window moves and workspace switches

### 3.2 Relative Send/Take Commands (P1)
- **Files**: `src/workspace.c`, `src/workspace.h`, `src/keybind.h`, `src/keybind.c`, `src/keyboard.c`, `src/ipc_commands.c`
- **What**: SendToNextWorkspace, SendToPrevWorkspace, TakeToNextWorkspace, TakeToPrevWorkspace
- **Complexity**: S
- **Test**: Keybinding tests

### 3.3 Dynamic Workspaces (P1)
- **Files**: `src/workspace.c`, `src/workspace.h`, `src/keybind.h`, `src/keybind.c`, `src/keyboard.c`, `src/ipc_commands.c`, `src/toolbar.c`
- **What**: AddWorkspace (create new workspace, update toolbar), RemoveLastWorkspace (move windows from last workspace, remove, update toolbar)
- **Complexity**: M
- **Test**: Add workspace, verify toolbar updates; remove workspace, verify windows moved

## Phase 4: Menu & Toolbar Enhancements (P1)

### 4.1 WorkspaceMenu Command (P1)
- **Files**: `src/menu.c`, `src/menu.h`, `src/keybind.h`, `src/keybind.c`, `src/keyboard.c`
- **What**: Open a standalone workspace management menu (switch, send-to, rename)
- **Complexity**: M
- **Test**: Trigger via keybinding/mouse, verify workspace operations

### 4.2 ClientMenu Command (P1)
- **Files**: `src/menu.c`, `src/menu.h`, `src/keybind.h`, `src/keybind.c`, `src/keyboard.c`
- **What**: Open a filterable window list menu. Optional pattern argument.
- **Complexity**: M
- **Test**: Trigger with/without pattern, verify window list

### 4.3 CustomMenu Command (P1)
- **Files**: `src/menu.c`, `src/menu.h`, `src/keybind.h`, `src/keybind.c`, `src/keyboard.c`
- **What**: Open a menu from an arbitrary menu file path
- **Complexity**: S
- **Test**: Create temp menu file, open via keybinding

### 4.4 Remember Window Properties (P1)
- **Files**: `src/rules.c`, `src/rules.h`, `src/menu.c`, `src/view.c`
- **What**: "Remember..." submenu in window menu that saves current window's properties (workspace, position, size, decorations, layer, alpha, sticky, shaded, maximized) to the apps file
- **Complexity**: L
- **Test**: Remember properties, restart, verify window restores

### 4.5 Workspace Warping Implementation (P1)
- **Files**: `src/cursor.c`, `src/workspace.c`
- **What**: When dragging a window to a screen edge, if workspace_warping is enabled, switch to adjacent workspace and continue the drag
- **Complexity**: M
- **Test**: Manual test — drag window to edge, verify workspace switch

### 4.6 Deiconify Modes (P1)
- **Files**: `src/view.c`, `src/view.h`, `src/keybind.h`, `src/keybind.c`, `src/keyboard.c`, `src/ipc_commands.c`
- **What**: Extend Deiconify with argument: All (all workspaces), AllWorkspace (current workspace), Last (default), LastWorkspace. Also destination: Current (deiconify to current workspace) or OriginQuiet (deiconify to original workspace)
- **Complexity**: M
- **Test**: Minimize several windows, test each deiconify mode

### 4.7 SetStyle / ReloadStyle Commands (P1)
- **Files**: `src/keybind.h`, `src/keybind.c`, `src/keyboard.c`, `src/config.c`, `src/style.c`
- **What**: SetStyle changes the style file path and reloads; ReloadStyle reloads current style
- **Complexity**: S
- **Test**: Change style via keybinding, verify visual change

## Phase 5: Advanced Features (P2)

### 5.1 Conditional Commands (If/Cond, ForEach/Map)
- **Files**: `src/keybind.c`, `src/keybind.h`, `src/keyboard.c`
- **What**: Add If/Cond with pattern matching conditions and ForEach to apply actions to matching windows
- **Complexity**: L
- **Test**: Complex keybinding parsing tests

### 5.2 Menu Enhancements
- Menu icons, type-ahead search, menu delay config, configurable window menu file
- **Complexity**: L (total across all sub-features)

### 5.3 External Tabs
- Tab positioning outside the window (Top/Bottom/Left/Right)
- **Complexity**: XL

### 5.4 Round Corners Rendering
- Actually render the round corners that are already parsed from style files
- **Complexity**: M

### 5.5 System Tray (status-notifier-watcher)
- Implement StatusNotifierWatcher protocol for Wayland system tray
- **Complexity**: XL

### 5.6 Style Overlay
- session.styleOverlay support to override style properties
- **Complexity**: S

### 5.7 Toolbar Tools Configuration
- session.screen0.toolbar.tools for configurable component order
- **Complexity**: L

### 5.8 Slit Enhancements
- Slitlist file, slit alpha, slit layer, toggle commands
- **Complexity**: M

### 5.9 Remaining Placement Strategies
- MinOverlap variants, AutotabPlacement, placement direction
- **Complexity**: M

### 5.10 Additional Config Options
- All remaining P2 config options (menuSearch, doubleClickInterval, rootCommand, struts, FocusProtection, IgnoreSizeHints, etc.)
- **Complexity**: M (total)

### 5.11 No-Wrap Workspace Navigation
- RightWorkspace / LeftWorkspace (like Next/Prev but don't wrap)
- **Complexity**: S

### 5.12 SetWorkspaceName
- Runtime workspace renaming
- **Complexity**: S

---

# IMPLEMENTATION SUMMARY

| Phase | Items | Estimated Effort | Priority |
|-------|-------|-----------------|----------|
| Phase 1: Bug Fixes & Config | 4 items | S-M each | P0-P1 |
| Phase 2: Window Commands | 6 items | S-M each | P1 |
| Phase 3: Workspace Commands | 3 items | S-M each | P1 |
| Phase 4: Menu & Misc | 7 items | S-L each | P1 |
| Phase 5: Advanced | 12 items | S-XL each | P2 |

**Total P0 items**: 1 (FocusPrev bug)
**Total P1 items**: ~20 features across Phases 1-4
**Total P2 items**: ~30 features in Phase 5

### Dependency Graph
```
Phase 1.2 (config options) ──→ Phase 1.3 (clock format)
                           ──→ Phase 1.4 (iconbar mode)
                           ──→ Phase 4.5 (workspace warping)

Phase 2.1 (LHalf/RHalf)   ──→ Style: LHalf/RHalf button pixmaps

Phase 3.3 (dynamic workspaces) ──→ Toolbar update logic

Phase 4.4 (remember)      ──→ Rules file write-back logic

Everything else is independent and can be parallelized.
```

### Test Strategy
- **Unit tests**: Extend existing test files (test_config.c, test_keybind.c, test_placement.c)
- **New test file**: `test_workspace.c` for workspace command tests
- **New test file**: `test_view_commands.c` for view operation tests
- **Manual testing**: Use VM setup described in `vm-testing.md` for visual verification
- **IPC testing**: Test new commands via IPC socket
