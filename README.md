# wm-wayland

A lightweight, highly configurable Wayland compositor inspired by [Fluxbox](http://fluxbox.org/).

## Vision

wm-wayland aims to bring the Fluxbox experience to the Wayland display protocol. For years, Fluxbox has been the window manager of choice for users who value simplicity, speed, and deep configurability. As the Linux desktop ecosystem transitions from X11 to Wayland, wm-wayland provides a familiar home for Fluxbox users while embracing modern display server capabilities.

## Goals

- **Lightweight**: Minimal resource footprint, fast startup, responsive window management
- **Fluxbox-compatible configuration**: Support Fluxbox config file formats where possible, providing a smooth migration path for existing Fluxbox users
- **Highly configurable**: Extensive key binding, window rules, theming, and menu customization
- **Wayland-native**: Built on wlroots, supporting modern Wayland protocols including fractional scaling, HDR, and explicit sync
- **Tabbed windows**: First-class window tabbing and grouping, a hallmark Fluxbox feature
- **XWayland support**: Run legacy X11 applications seamlessly alongside Wayland-native clients

## Planned Features

- **Window tabbing and grouping** - Combine multiple windows into tabbed groups
- **Configurable toolbar** - Workspace switcher, system tray, clock, and custom widgets
- **Slit** - Dock area for Window Maker dockapps and other dockable applications
- **Flexible key bindings** - Comprehensive, Fluxbox-compatible keybinding system
- **Window rules** - Per-application window placement, decoration, and behavior rules
- **Root menu** - Customizable right-click desktop menu
- **Workspace management** - Multiple virtual desktops with configurable behavior
- **Theming** - Fluxbox-compatible style/theme system for window decorations
- **Layer shell support** - For panels, wallpapers, overlays, and lock screens
- **XWayland** - Full X11 backward compatibility

## Current Status

**Phase: Research & Design**

The project is currently in the research and design phase. We are:
- Analyzing Fluxbox's feature set and configuration system
- Studying Wayland compositor architecture patterns (wlroots-based)
- Designing the component architecture and data flow
- Planning the Fluxbox configuration compatibility layer
- Defining the QA and testing strategy

## Building

> Build instructions will be added once the implementation phase begins.

```sh
# Placeholder - exact build system TBD
meson setup build
ninja -C build
```

### Dependencies (Planned)

- wlroots (>= 0.18)
- wayland
- wayland-protocols
- libinput
- pixman
- xkbcommon
- XWayland (optional, for X11 compatibility)

## Configuration

wm-wayland will read configuration from `~/.config/wm-wayland/` with support for importing Fluxbox configuration files from `~/.fluxbox/`.

Key configuration files (planned):
- `keys` - Key and mouse bindings
- `init` - General settings and behavior
- `menu` - Root menu and window menu definitions
- `apps` - Per-application window rules
- `styles/` - Window decoration themes
- `startup` - Autostart applications

See [FLUXBOX-COMPAT.md](docs/design/FLUXBOX-COMPAT.md) for details on Fluxbox configuration compatibility.

## Documentation

- [Architecture Overview](docs/design/ARCHITECTURE.md)
- [Fluxbox Compatibility Matrix](docs/design/FLUXBOX-COMPAT.md)
- [Research Summary](docs/research/SUMMARY.md)

## Contributing

Contribution guidelines will be established as the project matures. For now, check the documentation and research notes in the `docs/` directory.

## License

License TBD.

## Acknowledgments

- [Fluxbox](http://fluxbox.org/) - The window manager that inspires this project
- [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots) - The Wayland compositor library
- The broader Wayland compositor community (Sway, Wayfire, Hyprland, labwc) for pioneering patterns we build upon
