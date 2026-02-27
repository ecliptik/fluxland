# Quick Start Guide

Test-drive fluxland on **Debian Trixie** (13) or newer.

> **⚠️ Linux Mint / Ubuntu 24.04 users:** These distributions ship wlroots 0.17,
> but fluxland requires wlroots 0.18. See the [wlroots 0.18 installation section](#installing-wlroots-018-on-ubuntu-2404linux-mint-223)
> below for workarounds.

---

## 1. Install build dependencies

### On Debian Trixie (13)

```sh
sudo apt update
sudo apt install -y \
  meson ninja-build pkg-config gcc \
  libwlroots-0.18-dev libwayland-dev wayland-protocols \
  libxkbcommon-dev libinput-dev libpixman-1-dev \
  libpango1.0-dev libdrm-dev
```

### Installing wlroots 0.18 on Ubuntu 24.04/Linux Mint 22.3

Ubuntu 24.04 and Linux Mint 22.3 ship wlroots 0.17, but fluxland requires 0.18.

**Option A: Build wlroots 0.18 from source** (recommended)

```sh
# Install dependencies
sudo apt update
sudo apt install -y \
  meson ninja-build pkg-config gcc \
  libwayland-dev wayland-protocols \
  libxkbcommon-dev libinput-dev libpixman-1-dev \
  libdrm-dev libgbm-dev libseat-dev libudev-dev \
  libvulkan-dev libvulkan-volk-dev glslang-tools \
  libxcb1-dev libxcb-composite0-dev libxcb-render0-dev \
  libxcb-xfixes0-dev hwdata

# Clone and build wlroots 0.18
git clone https://gitlab.freedesktop.org/wlroots/wlroots.git -b 0.18
cd wlroots
meson setup build --prefix=/usr/local --buildtype=release
ninja -C build
sudo ninja -C build install
sudo ldconfig
cd ..
```

**Option B: Wait for distribution upgrade**

Ubuntu 24.10+ and future Linux Mint releases will include wlroots 0.18.

**Option C: Use a PPA** (if available)

Check [wlroots PPAs](https://launchpad.net/ubuntu/+ppas?name_filter=wlroots) for
community-maintained packages. Use at your own risk.

### XWayland support (optional - for running X11 apps)

```sh
sudo apt install -y xwayland libxcb1-dev libxcb-ewmh-dev libxcb-icccm4-dev
```

### Verify wlroots version

After installation, confirm you have wlroots 0.18:

```sh
pkg-config --modversion wlroots-0.18
```

Expected output: `0.18.x`

### Useful runtime tools (recommended)

```sh
sudo apt install -y foot swaybg wofi grim slurp mako-notifier swayidle swaylock
```

These provide:
- `foot` - lightweight Wayland terminal
- `swaybg` - wallpaper setter
- `wofi` - app launcher
- `grim`/`slurp` - screenshot tools
- `mako-notifier` - notification daemon
- `swayidle`/`swaylock` - idle/lock management

## 2. Build

```sh
git clone https://github.com/ecliptik/fluxland.git
cd fluxland

meson setup build --buildtype=release
ninja -C build
```

Verify everything works:

```sh
ninja -C build test        # run all 7 test suites
```

You should see all tests pass:

```
Ok:                 7
Expected Fail:      0
Fail:               0
```

## 3. Create a .deb package (optional)

Use `checkinstall` for a quick local package:

```sh
sudo apt install -y checkinstall

sudo checkinstall --pkgname=fluxland \
  --pkgversion=0.1.0 \
  --pkgrelease=1 \
  --pkggroup=x11 \
  --maintainer="you@example.com" \
  --requires="libwayland-server0,libxkbcommon0,libinput10,libpixman-1-0,libpango-1.0-0" \
  --backup=no \
  --deldoc=yes \
  --default \
  ninja -C build install
```

This creates `fluxland_0.1.0-1_amd64.deb` in the current directory and
installs it. To share or reinstall:

```sh
sudo dpkg -i fluxland_0.1.0-1_amd64.deb
```

To remove:

```sh
sudo dpkg -r fluxland
```

## 4. Install (without packaging)

If you prefer a direct install:

```sh
sudo ninja -C build install
```

This installs:

| What | Where |
|------|-------|
| `fluxland` | `/usr/local/bin/fluxland` |
| `fluxland-ctl` | `/usr/local/bin/fluxland-ctl` |
| Man pages | `/usr/local/share/man/` |
| Session entry | `/usr/local/share/wayland-sessions/fluxland.desktop` |
| Example configs | `/usr/local/share/fluxland/examples/` |

## 5. Configure

Copy the example configuration to your home directory:

```sh
mkdir -p ~/.config/fluxland
cp /usr/local/share/fluxland/examples/* ~/.config/fluxland/
chmod +x ~/.config/fluxland/startup
```

If you built without installing, copy from the source tree instead:

```sh
mkdir -p ~/.config/fluxland
cp data/* ~/.config/fluxland/
chmod +x ~/.config/fluxland/startup
```

### Key config files

| File | What it does |
|------|-------------|
| `init` | Workspaces, focus policy, toolbar, placement, XKB layout |
| `keys` | Keyboard/mouse bindings, key chains, keymodes |
| `apps` | Per-window rules (size, position, workspace, decorations) |
| `menu` | Right-click root menu (apps, submenus, styles) |
| `style` | Window decoration theme (colors, fonts, textures) |
| `startup` | Shell script run once at launch (wallpaper, bars, daemons) |

### Customize for your setup

Edit `~/.config/fluxland/startup` to enable your preferred tools:

```sh
# Uncomment what you have installed:
swaybg -c '#2e3440' &         # wallpaper (solid color)
# swaybg -i ~/wallpaper.png -m fill &
# waybar &                     # status bar
# mako &                       # notifications
```

Edit `~/.config/fluxland/init` to change focus behavior:

```
# Sloppy focus (focus follows mouse):
session.screen0.focusModel: MouseFocus
session.screen0.autoRaise: true
session.autoRaiseDelay: 250
```

## 6. Test drive

### Option A: From a display manager (GDM, SDDM, LightDM)

After installing, log out. On the login screen, select **fluxland** from
the session menu (gear icon), then log in.

### Option B: From a TTY

Switch to a virtual console (`Ctrl+Alt+F2`), log in, and run:

```sh
fluxland
```

Or launch with a terminal already open:

```sh
fluxland -s foot
```

### Option C: Nested inside another Wayland session (for development)

From an existing Wayland session (GNOME, KDE, Sway, etc.):

```sh
WLR_BACKENDS=wayland ./build/fluxland -s foot
```

This opens fluxland in a window -- ideal for testing without leaving your
current desktop.

### Option D: Headless (for automated testing)

```sh
WLR_BACKENDS=headless WLR_RENDERER=pixman ./build/fluxland
```

### Essential keybindings

Once running, these are the default bindings (`Mod4` = Super/Windows key):

| Key | Action |
|-----|--------|
| `Mod4+Return` | Open terminal (foot) |
| `Mod4+d` | App launcher (wofi) |
| `Mod4+Shift+q` | Close window |
| `Mod4+f` | Fullscreen |
| `Mod4+m` | Maximize |
| `Mod4+n` | Minimize |
| `Alt+Tab` | Cycle windows |
| `Mod4+1`..`9` | Switch workspace |
| `Mod4+Shift+1`..`9` | Send window to workspace |
| `Alt+Left-click` | Move window (drag) |
| `Alt+Right-click` | Resize window (drag) |
| `Right-click desktop` | Root menu |
| `Mod4+r` | Reload all config |
| `Mod4+Shift+e` | Exit compositor |

### Verify IPC works

In a terminal inside fluxland:

```sh
fluxland-ctl ping                # should print "pong"
fluxland-ctl get_workspaces      # list workspaces as JSON
fluxland-ctl get_windows         # list open windows
fluxland-ctl action Exec foot    # open another terminal via IPC
fluxland-ctl list-actions        # see all 50+ available actions
```

### Quick smoke test checklist

- [ ] Compositor starts without crashing
- [ ] Terminal opens with `Mod4+Return`
- [ ] Windows can be moved (`Alt+Left-drag`) and resized (`Alt+Right-drag`)
- [ ] Workspaces switch with `Mod4+1` through `Mod4+4`
- [ ] Right-click on desktop opens the root menu
- [ ] `Mod4+r` reloads config (check by editing `~/.config/fluxland/init`)
- [ ] `fluxland-ctl ping` returns `pong`
- [ ] Window decorations render (title bar, close/minimize/maximize buttons)
- [ ] `Mod4+Shift+e` exits cleanly

## 7. Reporting bugs

Bugs are triaged and resolved by a Claude Code agent team. To file a useful
report:

### Where to report

File an issue at the project's GitHub repository:

```sh
# From the fluxland directory:
gh issue create
```

Or visit the Issues tab on the GitHub repo in your browser.

### What to include

Use this template:

```markdown
## Bug Report

**fluxland version:**
<!-- paste output of: fluxland --version -->

**System info:**
<!-- paste output of: uname -a -->
<!-- paste output of: cat /etc/os-release | head -4 -->

**GPU / driver:**
<!-- paste output of: lspci | grep -i vga -->
<!-- paste output of: cat /proc/driver/nvidia/version (if NVIDIA) -->

**wlroots version:**
<!-- paste output of: pkg-config --modversion wlroots-0.18 -->
<!-- If you built from source: /usr/local/lib/pkgconfig/wlroots-0.18.pc -->

**Steps to reproduce:**
1. Start fluxland with `fluxland -d` (debug mode)
2. Do X
3. Do Y
4. Observe Z

**Expected behavior:**
What should happen.

**Actual behavior:**
What actually happens. Include any error output.

**Debug log:**
<!-- Attach or paste relevant lines from the debug log.
     Start with: fluxland -d 2>&1 | tee /tmp/wm-debug.log
     Then reproduce the bug and attach /tmp/wm-debug.log -->

**Config files (if relevant):**
<!-- Attach init, keys, apps, or style if the bug relates to config -->

**Screenshots:**
<!-- If visual: grim /tmp/screenshot.png -->
```

### Gathering debug info quickly

```sh
# Run with debug logging, capture to file:
fluxland -d 2>&1 | tee /tmp/wm-debug.log

# In another terminal, grab system info:
echo "=== Version ===" > /tmp/wm-bugreport.txt
fluxland --version >> /tmp/wm-bugreport.txt 2>&1
echo "=== OS ===" >> /tmp/wm-bugreport.txt
cat /etc/os-release >> /tmp/wm-bugreport.txt
echo "=== Kernel ===" >> /tmp/wm-bugreport.txt
uname -a >> /tmp/wm-bugreport.txt
echo "=== GPU ===" >> /tmp/wm-bugreport.txt
lspci | grep -i vga >> /tmp/wm-bugreport.txt
echo "=== wlroots ===" >> /tmp/wm-bugreport.txt
pkg-config --modversion wlroots-0.18 >> /tmp/wm-bugreport.txt

cat /tmp/wm-bugreport.txt
```

### Crash reports

If fluxland crashes, a core dump is the most valuable artifact:

```sh
# Enable core dumps for the session:
ulimit -c unlimited

# Run the compositor:
fluxland -d 2>&1 | tee /tmp/wm-debug.log

# After crash, the core file is usually in the current directory or /var/crash/
# Get a backtrace:
sudo apt install -y gdb
gdb ./build/fluxland core -ex "bt full" -ex "quit" > /tmp/wm-backtrace.txt

# Attach /tmp/wm-backtrace.txt and /tmp/wm-debug.log to the issue
```

For better crash diagnostics, build with debug symbols and sanitizers:

```sh
meson setup build-debug --buildtype=debug -Dasan=true -Dubsan=true
ninja -C build-debug
./build-debug/fluxland -d
```

### Bug triage workflow

When the Claude team receives your issue:

1. **Triage** -- categorize as crash, rendering, input, config, or protocol bug
2. **Reproduce** -- build with debug/ASAN and reproduce using your steps
3. **Diagnose** -- identify the root cause in the relevant module
4. **Fix** -- implement the fix with a regression test
5. **Verify** -- confirm the fix resolves the reported behavior
6. **Release** -- include in the next point release

### Severity labels

| Label | Meaning |
|-------|---------|
| `crash` | Compositor crashes or freezes |
| `rendering` | Visual glitches, decoration issues, damage tracking |
| `input` | Keyboard/mouse/touch not working correctly |
| `config` | Parser errors, settings not taking effect |
| `protocol` | Wayland protocol conformance issues |
| `enhancement` | Feature request or improvement |

---

## Further reading

- `man fluxland` -- compositor usage
- `man fluxland-keys` -- keybinding format
- `man fluxland-apps` -- per-window rules
- `man fluxland-style` -- theme/style format
- `man fluxland-menu` -- menu definition format
- [CONTRIBUTING.md](CONTRIBUTING.md) -- development workflow and architecture
- [TODO.md](TODO.md) -- planned features and backlog
