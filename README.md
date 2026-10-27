# fluxland

A lightweight, highly configurable Wayland compositor inspired by [Fluxbox](http://fluxbox.org/).

[![Build](https://img.shields.io/github/actions/workflow/status/ecliptik/fluxland/ci.yml?branch=main)](https://github.com/ecliptik/fluxland/actions)
[![License](https://img.shields.io/github/license/ecliptik/fluxland)](LICENSE)
[![Version](https://img.shields.io/github/v/release/ecliptik/fluxland?include_prereleases)](https://github.com/ecliptik/fluxland/releases)

---

[About](#about) | [Screenshots](#screenshots) | [Install](#install) | [Quick Start](#quick-start) | [Features](#features) | [Dependencies](#dependencies) | [Building](#building) | [Configuration](#configuration) | [Migration from Fluxbox](#migration-from-fluxbox) | [IPC](#ipc) | [Man Pages](#man-pages) | [Contributing](#contributing) | [License](#license)

---

## About

fluxland brings the Fluxbox experience to the [Wayland](https://wayland.freedesktop.org/) display protocol.

This project is built agentically with [Claude Code](https://docs.anthropic.com/en/docs/claude-code).
[Read about how it was built](docs/DEBRIEF.md) — agentic development, team orchestration, and
what I learned building a 35,000-line compositor without writing a single line of code.

Built on [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots) 0.20, fluxland implements 30+ Wayland protocols and delivers
first-class support for server-side decorations with Fluxbox style
compatibility, key chains and modal keybindings, window tabbing, a configurable
toolbar with icon bar, the slit (dockapp container), and per-window rules. It
reads Fluxbox configuration files directly, making migration straightforward.

fluxland targets users who want a fast, keyboard-driven stacking compositor
with the depth of configuration that Fluxbox is known for, without sacrificing
the benefits of a modern Wayland session.

## Screenshots

*[Great Wave](https://en.wikipedia.org/wiki/The_Great_Wave_off_Kanagawa) style with ocean blue decorations.*

<table>
<tr>
<td width="50%" align="center"><img src="docs/screenshots/desktop.png" alt="Desktop" width="100%"></td>
<td width="50%" align="center"><img src="docs/screenshots/window_slit.png" alt="Tabs" width="100%"></td>
</tr>
<tr>
<td width="50%" valign="top">Desktop with wallpaper</td>
<td width="50%" valign="top">Three terminals tabbed in one frame (an apps file <code>[group]</code>), <a href="http://gkrellm.srcbox.net/">gkrellm</a> in the slit (XWayland)</td>
</tr>
<tr>
<td width="50%" align="center"><img src="docs/screenshots/menu.png" alt="Menu" width="100%"></td>
<td width="50%" align="center"><img src="docs/screenshots/grid.png" alt="Grid" width="100%"></td>
</tr>
<tr>
<td width="50%" valign="top">Root menu</td>
<td width="50%" valign="top"><code>ArrangeWindows</code> (4 windows)</td>
</tr>
</table>

### Fluxbox desktops, recreated

Four of the author's Fluxbox desktops from 2002 to 2005 (left, from
[Screenshots Over the Years](https://www.ecliptik.com/blog/2008/Screenshots-Over-the-Years/))
and fluxland running the same Fluxbox styles today (right). Programs
that don't run on Wayland have modern stand-ins: foot for Eterm and
aterm, Audacious with the same Winamp skins for XMMS.

<table>
<tr>
<td width="50%" align="center"><img src="docs/screenshots/fluxbox-desktops/2002-fluxarnation.png" alt="2002, Fluxarnation" width="100%"></td>
<td width="50%" align="center"><img src="docs/screenshots/fluxbox-desktops/2002-eterm.png" alt="2002, Eterm" width="100%"></td>
</tr>
<tr>
<td width="50%" valign="top">2002: <a href="https://web.archive.org/web/2003id_/http://fluxbox.sourceforge.net:80/download/themes/sid/aleczapka_fluxcarnation.tar.bz2"><em>Fluxarnation</em></a> by aLEczapKA, <a href="https://web.archive.org/web/2003id_/http://fluxbox.sourceforge.net:80/download/themes/sid/aleczapka_fluxcarnation.tar.bz2"><em>sid_fluxbox</em></a> by aLEczapKA, gkrellm's <a href="https://web.archive.org/web/20060113185908id_/http://www.muhri.net:80/gkrellm/CoplandOS.gkrellm.tar.gz"><em>CoplandOS</em></a> by an unknown author</td>
<td width="50%" valign="top">2002: style unknown, <a href="https://web.archive.org/web/20090131104840/http://comp.dpics.org:80/12__Kernel_Destruction.htm"><em>Kernel Destruction</em></a> by an unknown artist, <a href="https://skins.webamp.org/skin/c04758885478077f72708a5d6ef1940f"><em>LCD screen: titanium</em></a> by Philipp RW Urech</td>
</tr>
<tr>
<td width="50%" align="center"><img src="docs/screenshots/fluxbox-desktops/2004-magraxx.png" alt="2004, Magraxx" width="100%"></td>
<td width="50%" align="center"><img src="docs/screenshots/fluxbox-desktops/2005-meta.png" alt="2005, Meta" width="100%"></td>
</tr>
<tr>
<td width="50%" valign="top">2004: <a href="https://web.archive.org/web/20221025152713/http://tenr.de/styles/archives/fluxmod-styles-pkg.tar.bz2"><em>Magraxx</em></a> by ikaro, <a href="https://web.archive.org/web/2008id_/http://peoples.mandriva.com/~ayo/distant_file/dessins/wallpaper/03_debian_expo_1280_1024.png"><em>Debian Style</em></a> by ayo</td>
<td width="50%" valign="top">2005: <a href="https://github.com/fluxbox/fluxbox/tree/master/data/styles/Meta"><em>Meta</em></a> from Fluxbox, <em>Winter's Eve</em> by lisyria over <a href="https://visualparadox.com/wallpapers/wintereve.htm"><em>Winter Eve</em></a> by Visual Paradox, <a href="https://skins.webamp.org/skin/9c7804f8c6b3ec79e1f4757832c487d9"><em>ArcticAMP</em></a> by Zarko Jovic</td>
</tr>
</table>

See [`examples/`](examples/) for styles and complete configuration examples, and the [style gallery](docs/STYLES.md) for Fluxbox style compatibility screenshots.

## Install

Packages for each [release](https://github.com/ecliptik/fluxland/releases/latest) bundle wlroots 0.20 and other dependencies:

| Distribution | Install |
|---|---|
| Debian 13 (trixie) | `sudo apt install ./fluxland_2.0.0-1_amd64_debian-13.deb` |
| Ubuntu 24.04, Linux Mint 22 | `sudo apt install ./fluxland_2.0.0-1_amd64_ubuntu-24.04.deb` |
| Arch Linux | `sudo pacman -U fluxland-2.0.0-1-x86_64.pkg.tar.zst` |
| Fedora 44 | `sudo dnf install ./fluxland-2.0.0-1.fc44.x86_64.rpm` |
| Anything else | build the source tarball, or [from a clone](#building) |

Choose **Fluxland** in your display manager, or run `fluxland` from a text console. `SHA256SUMS` on the release lists each file's checksum.

## Quick Start

1. **Build from source** (or [install a package](#install)): install the dependencies (see [Dependencies](#dependencies) below), then:

   ```sh
   meson setup build && ninja -C build
   sudo meson install -C build --skip-subprojects
   ```

   For distro-specific instructions, see [`docs/QUICKSTART.md`](docs/QUICKSTART.md).

2. **Configuration.** On its first start fluxland copies its default
   configuration to `~/.config/fluxland/` (if you have a `~/.fluxbox`
   and no `~/.config/fluxland`, it uses `~/.fluxbox` as it is; see
   [Migration from Fluxbox](#migration-from-fluxbox)). A file you delete
   comes back as the default next time.

   Or start from a themed example (see [`examples/`](examples/)):

   ```sh
   mkdir -p ~/.config/fluxland
   cp /path/to/fluxland/examples/wave-desktop/* ~/.config/fluxland/
   ```

3. **Start the compositor** from a TTY or from within another compositor:

   ```sh
   fluxland
   ```

4. **Essential keybindings:**

   | Key | Action |
   |---|---|
   | `Mod4+Return` | Open terminal ([foot](https://codeberg.org/dnkl/foot)) |
   | `Mod4+d` | Application launcher ([wofi](https://hg.sr.ht/~scoopta/wofi)) |
   | `Mod4+Shift+q` | Close focused window |
   | `Mod4+f` | Toggle fullscreen |
   | `Mod4+m` | Toggle maximize |
   | `Mod4+1`..`9` | Switch to workspace |
   | `Mod4+Shift+1`..`9` | Send window to workspace |
   | `Alt+Tab` | Cycle windows |
   | `Mod4+r` | Reload configuration |
   | `Mod4+Shift+e` | Exit compositor |

   `Mod4` is the Super/Windows key.

## Features

- **Fluxbox compatible** -- an existing `~/.fluxbox` works unchanged: every Fluxbox command (132), init resource (90), apps attribute (25) and menu tag (36), checked by a parity test; see [Migration from Fluxbox](#migration-from-fluxbox)
- **Server-side decorations** with full [Fluxbox style support](docs/STYLES.md) (gradients, textures, fonts, colors)
- **Key chains** -- multi-key sequences (e.g. `Mod4+x Mod4+t` to launch a terminal)
- **Keymodes** -- modal keybinding sets (e.g. a resize mode with vim-style movement keys)
- **Window tabbing** -- group windows into tabbed containers, drag-to-tab from titlebars
- **Toolbar** with workspace switcher, icon bar, and clock; configurable placement and auto-hide
- **Slit** -- dock area for [Window Maker](https://www.windowmaker.org/) dockapps and other dockable applications (e.g. gkrellm via XWayland)
- **Root menu and window menu** -- Fluxbox-compatible menu definitions with submenus, separators, and built-in entries
- **Per-window rules** -- match by app_id, title, or class with regex; set workspace, dimensions, position, decorations, layer, transparency, and more
- **Auto-grouping** -- automatically tab windows matching specified patterns
- **MacroCmd / ToggleCmd** -- combine multiple actions in a single binding or cycle between them
- **Mouse bindings** -- per-context (titlebar, desktop, border, grip) with click, drag, and double-click support
- **Multiple focus policies** -- click-to-focus, sloppy (mouse focus), strict mouse focus
- **Smart window placement** -- row-smart, column-smart, cascade, and under-mouse placement policies
- **Workspace management** -- up to 32 named virtual desktops with edge warping
- **XKB keyboard layout** -- full XKB configuration with multi-layout support and runtime switching
- **IPC** via Unix socket with JSON protocol; includes `fluxland-ctl` command-line client
- **Event subscriptions** -- subscribe to window, workspace, and output events for scripting
- **XWayland support** -- run X11 applications alongside native Wayland clients
- **Live reconfiguration** -- reload all config files at runtime (keys, apps, menu, style, init) via `Mod4+r` or `SIGHUP`
- **Lightweight and efficient** -- runs comfortably on a Raspberry Pi 3 or 2010-era Intel Atom. The compositor uses ~16-32 MB of RAM, with an optimized rendering pipeline that caches text measurement contexts, pre-allocates buffers, and skips redundant scene graph updates. The only real hardware requirement is a GPU with OpenGL ES 2.0 and DRM/KMS kernel driver support.
- **30+ Wayland protocols** including:
  - xdg-shell, xdg-decoration, layer-shell, session-lock
  - fractional-scale, tearing-control, content-type, presentation-time
  - pointer-constraints, relative-pointer, pointer-gestures
  - virtual-keyboard, virtual-pointer, text-input, input-method
  - gamma-control, output-management, output-power, screencopy
  - data-control, xdg-activation, xdg-foreign, tablet-v2
  - cursor-shape, viewporter, single-pixel-buffer, alpha-modifier
  - security-context, linux-dmabuf, idle-notify, idle-inhibit

## Dependencies

### Build dependencies

| Dependency | Debian/Ubuntu | Arch |
|---|---|---|
| meson (>= 1.3) | `meson` | `meson` |
| ninja | `ninja-build` | `ninja` |
| C17 compiler | `gcc` or `clang` | `gcc` or `clang` |
| wlroots 0.20 | built from `subprojects/` (or a system `wlroots-0.20`) | `wlroots0.20` |
| wayland | `libwayland-dev` | `wayland` |
| wayland-protocols (>= 1.32) | `wayland-protocols` | `wayland-protocols` |
| xkbcommon | `libxkbcommon-dev` | `libxkbcommon` |
| libinput (>= 1.14) | `libinput-dev` | `libinput` |
| pixman | `libpixman-1-dev` | `pixman` |
| pangocairo | `libpango1.0-dev` | `pango` |
| libdrm | `libdrm-dev` | `libdrm` |

### Optional dependencies

| Dependency | Debian/Ubuntu | Arch | Purpose |
|---|---|---|---|
| XWayland | `xwayland` | `xorg-xwayland` | X11 application support |
| xcb, xcb-ewmh, xcb-icccm | `libxcb1-dev libxcb-ewmh-dev libxcb-icccm4-dev` | `libxcb xcb-util-wm` | XWayland EWMH/ICCCM |

### Runtime companions (recommended)

- **foot** or another Wayland terminal emulator
- **wofi**, **[bemenu](https://github.com/Cloudef/bemenu)**, or **[fuzzel](https://codeberg.org/dnkl/fuzzel)** for application launching
- **[swaybg](https://github.com/swaywm/swaybg)** for wallpaper
- **[waybar](https://github.com/Alexays/Waybar)** for a status bar
- **[mako](https://github.com/emersion/mako)** or **[dunst](https://dunst-project.org/)** for notifications
- **[swayidle](https://github.com/swaywm/swayidle)** + **[swaylock](https://github.com/swaywm/swaylock)** for idle/lock management
- **[grim](https://sr.ht/~emersion/grim/)** + **[slurp](https://github.com/emersion/slurp)** for screenshots

## Building

```sh
meson setup build
ninja -C build
```

To install system-wide:

```sh
sudo meson install -C build --skip-subprojects
```

### Build options

| Option | Type | Default | Description |
|---|---|---|---|
| `xwayland` | feature | auto | Enable XWayland support |
| `asan` | boolean | false | Enable address sanitizer |
| `ubsan` | boolean | false | Enable undefined behavior sanitizer |

Example with options:

```sh
meson setup build -Dxwayland=enabled -Dasan=true
ninja -C build
```

## Configuration

fluxland reads configuration from the first directory found:

1. `$FLUXLAND_CONFIG_DIR/`
2. `$XDG_CONFIG_HOME/fluxland/`
3. `~/.config/fluxland/`
4. `~/.fluxbox/` (Fluxbox compatibility fallback)

### Config files

| File | Format | Description |
|---|---|---|
| `init` | X resource (key: value) | Compositor settings: workspaces, focus policy, toolbar, placement, XKB layout |
| `keys` | Fluxbox keys format | Keyboard and mouse bindings, key chains, keymodes, macro/toggle commands |
| `apps` | Fluxbox apps format | Per-window rules: workspace, dimensions, position, decorations, layer, transparency, auto-grouping |
| `menu` | Fluxbox menu format | Root menu definition with exec entries, submenus, separators, styles directory, and built-in items |
| `style` | Fluxbox style format | Window decoration style: titlebar, buttons, handle, grip, frame, menu, and toolbar colors/textures/fonts |
| `startup` | Shell script | Autostart script executed once at launch; use for wallpaper, bars, daemons |

All configuration can be reloaded at runtime with `Mod4+r` (Reconfigure action)
or by sending `SIGHUP` to the compositor process. Example configuration files
are provided in the `data/` directory.

See the man pages below for detailed format documentation.

## Migration from Fluxbox

fluxland reads Fluxbox's files the way Fluxbox does: with no
`~/.config/fluxland` it uses `~/.fluxbox` as it is, and
`fluxland --import-fluxbox` makes fluxland's own copy of it, its startup
script turned into one fluxland runs. Every Fluxbox command, init
resource, apps attribute and menu tag is supported (a handful of init
resources that mean nothing on Wayland are accepted and ignored).

- [Migrating from Fluxbox](docs/MIGRATING.md): what works the same, and
  what Wayland changes (the startup script's X programs, `fluxbox-remote`,
  `fbrun`, X11 programs and dockapps, the tray).
- [Compatibility](docs/COMPATIBILITY.md): each Fluxbox name and what
  fluxland does with it, from the parity test.

## IPC

fluxland provides a Unix socket IPC interface with JSON messages. The
`fluxland-ctl` command-line tool handles socket discovery and message
formatting.

### Examples

```sh
# Execute compositor actions
fluxland-ctl action Close
fluxland-ctl action Workspace 3
fluxland-ctl action Exec foot

# Query state
fluxland-ctl get_windows
fluxland-ctl get_workspaces
fluxland-ctl get_outputs
fluxland-ctl get_config

# Subscribe to events (streams JSON to stdout)
fluxland-ctl subscribe window workspace

# List all available actions
fluxland-ctl list-actions

# Test connectivity
fluxland-ctl ping
```

The socket path is auto-detected from `$FLUXLAND_SOCK` or
`$XDG_RUNTIME_DIR/fluxland.$WAYLAND_DISPLAY.sock`. Override with `-s <path>`.

## Man Pages

| Page | Section | Description |
|---|---|---|
| fluxland(1) | 1 | Compositor usage and command-line options |
| fluxland-ctl(1) | 1 | IPC client usage |
| fluxland-keys(5) | 5 | Key and mouse binding configuration |
| fluxland-apps(5) | 5 | Per-window rules and auto-grouping |
| fluxland-init(5) | 5 | Init file reference |
| fluxland-style(5) | 5 | Style file format |
| fluxland-menu(5) | 5 | Menu definition format |
| fluxland-startup(5) | 5 | Startup script reference |

After installation, access with `man fluxland`, `man fluxland-keys`, etc.

## Contributing

Contributions are welcome. See [CONTRIBUTING.md](CONTRIBUTING.md) for
guidelines on code style, commit messages, and the development workflow.

## License

fluxland is released under the [MIT License](LICENSE).

## Acknowledgments

- [Fluxbox](http://fluxbox.org/) -- the window manager that inspires this project
- [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots) -- the Wayland compositor library
- [Window Maker](https://www.windowmaker.org/) -- the dock app ecosystem that the slit supports
- The broader [Wayland](https://wayland.freedesktop.org/) compositor community ([Sway](https://swaywm.org/), [Wayfire](https://wayfire.org/), [Hyprland](https://hyprland.org/), [labwc](https://labwc.github.io/)) for pioneering patterns we build upon
