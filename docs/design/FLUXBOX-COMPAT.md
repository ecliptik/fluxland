# Fluxbox Compatibility Matrix

This document details which Fluxbox features and configuration formats fluxland will support, the migration path for Fluxbox users, and known incompatibilities.

## Overview

fluxland aims to be the spiritual successor to Fluxbox in the Wayland world. While 100% compatibility is not possible (due to fundamental differences between X11 and Wayland), we target a high degree of configuration and behavioral compatibility so that Fluxbox users feel immediately at home.

**Compatibility approach**: Import and translate Fluxbox configuration files, mapping X11-specific concepts to their Wayland equivalents where possible, and warning clearly about unsupported features.

## Feature Compatibility Matrix

### Window Management

| Fluxbox Feature | fluxland Support | Notes |
|---|---|---|
| Window tabbing/grouping | Full | Core feature - first-class support |
| Tab drag between windows | Full | Drag tab to group/ungroup windows |
| Window shading (roll up) | Full | Collapse window to titlebar only |
| Window snapping to edges | Full | Snap-to-edge and snap-to-window |
| Window resistance | Full | Edge resistance when moving windows |
| Click-to-focus | Full | Default focus mode |
| Sloppy focus (focus-follows-mouse) | Full | With configurable delay |
| Strict mouse focus | Full | Focus only under pointer |
| Auto-raise | Full | With configurable delay |
| Window stacking layers | Full | Above Normal, Normal, Below Normal, Desktop |
| Remember window positions | Full | Via window rules (apps file) |
| Window maximize (full, horizontal, vertical) | Full | All maximize modes supported |
| Window minimize/iconify | Full | Minimize to toolbar icon bar |
| Window stick (all workspaces) | Full | Pin window across workspaces |
| Resize grips | Full | Corner and edge resize handles |
| Move/resize key bindings | Full | Keyboard-driven window manipulation |

### Workspace Management

| Fluxbox Feature | fluxland Support | Notes |
|---|---|---|
| Multiple workspaces | Full | Named virtual desktops |
| Workspace names | Full | Configurable workspace names |
| Workspace wrapping | Full | Cycle past last to first workspace |
| Send window to workspace | Full | Move window to specific workspace |
| Workspace switching | Full | Via keybinding, toolbar, or mouse wheel |
| Per-workspace wallpaper | Partial | Delegated to external tool (e.g., swaybg) |

### Toolbar

| Fluxbox Feature | fluxland Support | Notes |
|---|---|---|
| Workspace name display | Full | |
| Window list / icon bar | Full | Show icons of windows on current workspace |
| System clock | Full | Configurable format (strftime) |
| System tray | Partial | Via wlr-foreign-toplevel or StatusNotifierItem; legacy XEmbed tray via XWayland |
| Toolbar placement (top/bottom) | Full | Configurable placement and per-monitor |
| Toolbar auto-hide | Full | Auto-hide with configurable delay |
| Toolbar width percentage | Full | Configurable width |
| Toolbar layer | Full | Above/below windows |
| Custom toolbar tools | Partial | Subset of Fluxbox tools; extensible |

### Slit

| Fluxbox Feature | fluxland Support | Notes |
|---|---|---|
| Dock window maker dockapps | Partial | Via XWayland compatibility; native dockapp protocol TBD |
| Slit placement | Full | Configurable screen position |
| Slit direction | Full | Horizontal or vertical |
| Slit auto-hide | Full | With configurable delay |
| Slit ordering | Partial | Best-effort ordering; X11 dockapp protocol differences |

### Window Decorations / Styles

| Fluxbox Feature | fluxland Support | Notes |
|---|---|---|
| Titlebar | Full | Configurable titlebar with text and buttons |
| Titlebar buttons (close, max, min, shade, stick) | Full | All standard buttons |
| Button placement (left/right) | Full | Configurable button order |
| Window borders | Full | Configurable width and color |
| Focused/unfocused styles | Full | Different styles for active/inactive windows |
| Gradient textures (diagonal, horizontal, vertical, etc.) | Full | All Fluxbox gradient types |
| Solid color textures | Full | |
| Pixmap textures | Full | Custom images for decorations |
| Bevel styles (flat, raised, sunken) | Full | |
| Font configuration | Full | Via fontconfig/Pango |
| Rounded corners | Extended | Not in Fluxbox but added as extension |
| Style hot-reload | Full | Change styles without restart |

### Menu System

| Fluxbox Feature | fluxland Support | Notes |
|---|---|---|
| Root menu (right-click desktop) | Full | Customizable desktop context menu |
| Window menu (titlebar right-click) | Full | Window operations menu |
| Nested submenus | Full | Arbitrary nesting depth |
| Menu separators | Full | |
| Exec commands in menu | Full | Execute shell commands from menu entries |
| Pipe menus (dynamic menus) | Full | Shell scripts generating menu content |
| Menu icons | Full | Icons alongside menu text |
| Menu transparency | Full | Wayland-native alpha compositing |
| Workspaces submenu | Full | |
| Styles submenu | Full | Browse and apply styles |

### Key Bindings

| Fluxbox Feature | fluxland Support | Notes |
|---|---|---|
| Mod1 (Alt) | Full | Mapped to Alt |
| Mod4 (Super) | Full | Mapped to Super/Meta |
| Control, Shift | Full | |
| Key chains | Full | Multi-step key sequences |
| Mouse bindings | Full | Click, double-click, scroll |
| OnWindow, OnTitlebar, OnDesktop contexts | Full | Context-specific bindings |
| Workspace commands | Full | NextWorkspace, PrevWorkspace, Workspace N |
| Window commands | Full | Maximize, Minimize, Close, Shade, etc. |
| Exec command | Full | Run arbitrary shell commands |
| MacroCmd (multiple actions) | Full | Chain multiple actions on one binding |
| If/Then conditional bindings | Partial | Basic conditions supported |
| ToggleCmd | Full | Toggle between two actions |

## Configuration File Compatibility

### Supported Config Files

| File | Fluxbox Path | fluxland Path | Compatibility |
|---|---|---|---|
| `keys` | `~/.fluxbox/keys` | `~/.config/fluxland/keys` | High - most bindings translate directly |
| `init` | `~/.fluxbox/init` | `~/.config/fluxland/init` | High - resource format supported, X11-specific resources ignored |
| `menu` | `~/.fluxbox/menu` | `~/.config/fluxland/menu` | Full - menu format is display-server agnostic |
| `apps` | `~/.fluxbox/apps` | `~/.config/fluxland/apps` | High - window matching uses app-id instead of WM_CLASS where appropriate |
| `startup` | `~/.fluxbox/startup` | `~/.config/fluxland/startup` | Full - shell script format, directly compatible |
| `windowmenu` | `~/.fluxbox/windowmenu` | `~/.config/fluxland/windowmenu` | Full |
| `overlay` | `~/.fluxbox/overlay` | N/A | Not supported (X11-specific style overlays) |

### Import Tool

fluxland ships with `fluxland-import`, a migration utility:

```sh
# Import all Fluxbox configuration
fluxland-import --from ~/.fluxbox --to ~/.config/fluxland

# Import only key bindings
fluxland-import --keys ~/.fluxbox/keys

# Dry-run showing what would be imported and any warnings
fluxland-import --dry-run --from ~/.fluxbox
```

The import tool:
- Copies and translates configuration files
- Warns about unsupported features or X11-specific settings
- Generates a migration report listing what was translated, skipped, or needs manual attention
- Preserves the original files (never modifies `~/.fluxbox/`)

## Key Binding Translation

### Modifier Mapping

| Fluxbox Modifier | fluxland Equivalent | Notes |
|---|---|---|
| `Mod1` | `Alt` | Standard Alt key |
| `Mod4` | `Super` | Super/Windows/Meta key |
| `Control` | `Control` | Direct mapping |
| `Shift` | `Shift` | Direct mapping |
| `Mod2` (NumLock) | Ignored | NumLock state not used for bindings |
| `Mod3` | `Mod3` | Rarely used; preserved if hardware supports |
| `Mod5` | `Mod5` | Rarely used; preserved if hardware supports |
| `None` | `None` | No modifier required |

### Action Mapping

| Fluxbox Action | fluxland Action | Notes |
|---|---|---|
| `Exec {command}` | `exec {command}` | Direct mapping |
| `Maximize` | `maximize` | |
| `MaximizeHorizontal` | `maximize horizontal` | |
| `MaximizeVertical` | `maximize vertical` | |
| `Minimize` / `Iconify` | `minimize` | |
| `Close` | `close` | |
| `Kill` / `KillWindow` | `kill` | Send SIGKILL to client |
| `Shade` / `ShadeWindow` | `shade` | Roll up to titlebar |
| `Stick` / `StickWindow` | `sticky toggle` | Pin across workspaces |
| `Fullscreen` | `fullscreen` | |
| `Raise` | `raise` | |
| `Lower` | `lower` | |
| `RaiseLayer` | `layer raise` | |
| `LowerLayer` | `layer lower` | |
| `NextWindow` | `focus next` | |
| `PrevWindow` | `focus prev` | |
| `NextWorkspace` | `workspace next` | |
| `PrevWorkspace` | `workspace prev` | |
| `Workspace {N}` | `workspace {N}` | |
| `SendToWorkspace {N}` | `send-to-workspace {N}` | |
| `Move` | `move` | Interactive move |
| `Resize` | `resize` | Interactive resize |
| `MoveLeft/Right/Up/Down {N}` | `move-relative {dir} {N}` | Pixel-based movement |
| `ResizeTo {W} {H}` | `resize-to {W} {H}` | |
| `MoveTo {X} {Y}` | `move-to {X} {Y}` | |
| `SetDecor {mode}` | `decoration {mode}` | |
| `MacroCmd` | `macro` | Chain multiple commands |
| `ToggleCmd` | `toggle` | Toggle between states |
| `If` | `if` | Conditional execution (limited) |
| `Reconfigure` | `reload-config` | Reload configuration |
| `Restart` | `restart` | Restart compositor |
| `Exit` / `Quit` | `exit` | Exit compositor |
| `RootMenu` | `menu root` | Show root menu |
| `WindowMenu` | `menu window` | Show window menu |
| `WorkspaceMenu` | `menu workspace` | Show workspace menu |
| `SetHead {N}` | `output focus {N}` | Focus specific monitor |
| `Deiconify` | `deiconify` | Restore minimized window |
| `ArrangeWindows` | `arrange` | Tile/arrange visible windows |
| `ShowDesktop` | `show-desktop` | Toggle desktop visibility |

### Unsupported Fluxbox Actions

| Fluxbox Action | Reason | Alternative |
|---|---|---|
| `SetXProp` | X11-specific | Use window rules by app-id |
| `ChangeXDisplay` | X11-specific | N/A (Wayland handles outputs differently) |
| `StartMoving` / `StartResizing` | X11 event model | Use `move` / `resize` |

## Known Incompatibilities

### Fundamental (X11 vs Wayland)

1. **Window positioning by clients**: Wayland does not allow clients to set their own position. Windows that relied on X11 positioning APIs will be placed by the compositor.
2. **Global keyboard shortcuts while focused on client**: Wayland's security model means some key combinations may be intercepted by clients. fluxland handles this through the compositor's privileged key binding mechanism.
3. **Screen capture**: X11's open screen access is replaced by Wayland's screencopy/PipeWire protocols requiring explicit user consent.
4. **Pointer warping**: X11 allows programs to move the mouse cursor; Wayland restricts this for security.
5. **Root window drawing**: X11 root window concepts (wallpaper, root menu triggers) are replaced by layer-shell protocol surfaces.
6. **X11 system tray (XEmbed)**: Legacy tray icons only work via XWayland. Native Wayland apps should use StatusNotifierItem/D-Bus tray.

### Behavioral Differences

1. **Window geometry**: Wayland windows are positioned by the compositor, not the client. Apps cannot request specific screen positions.
2. **Slit/dockapps**: Traditional X11 dockapps must run under XWayland. Native Wayland dockapp protocol is not yet standard.
3. **Focus stealing**: Wayland's security model prevents unexpected focus changes. Some Fluxbox focus behaviors may behave slightly differently.
4. **Overlay style file**: Fluxbox's `overlay` file for per-style patches is not supported. Use the native style system instead.

### Migration Notes

1. **WM_CLASS → app-id**: Window matching in Fluxbox uses X11 WM_CLASS. In Wayland, the equivalent is app-id. The import tool will note where WM_CLASS values need to be updated.
2. **xterm/urxvt → Wayland terminals**: Config files referencing X11 terminal emulators should be updated to Wayland-native terminals (foot, alacritty, kitty).
3. **xdotool/wmctrl → fluxland IPC**: X11 scripting tools don't work in Wayland. Use `fluxland-msg` IPC client instead.
4. **feh/nitrogen → swaybg/swww**: X11 wallpaper tools are replaced by Wayland wallpaper tools using the layer-shell protocol.
5. **xrandr → wlr-randr/kanshi**: Monitor configuration uses Wayland-native tools.

## Compatibility Tiers

To set clear expectations, features are categorized into tiers:

### Tier 1: Full Compatibility
Features that work identically to Fluxbox:
- Window tabbing and grouping
- Key/mouse binding syntax and actions
- Workspace management
- Window shading, sticking, layering
- Menu system (root, window, pipe menus)
- Style system (colors, gradients, fonts)
- Window rules (apps file)

### Tier 2: Near Compatibility
Features that work with minor differences:
- Focus policies (security model differences)
- Toolbar (layout compatible, some tools differ)
- Startup file (compatible, but environment differs)
- Window snapping (enhanced with Wayland features)

### Tier 3: Partial Compatibility
Features that work but require adaptation:
- Slit (X11 dockapps need XWayland)
- System tray (protocol differences)
- Window position remembering (Wayland positioning model)
- Some style pixmaps (rendering engine differences)

### Tier 4: Not Supported
Features that cannot be implemented in Wayland:
- X11-specific properties and atoms
- Direct X11 protocol interactions
- Root window operations
- Overlay style file
- Client-driven window positioning
