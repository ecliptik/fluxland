# Migrating from Fluxbox

fluxland reads Fluxbox's own files the way Fluxbox does. Your `init`,
`keys`, `apps`, `menu`, `windowmenu`, `overlay`, `slitlist` and styles
work unchanged; what changes is what X11 did for you, which a Wayland
compositor does itself.

## Try it with your setup as it is

Install fluxland, log out, and pick **Fluxland** in your display
manager (or run `fluxland` from a text console).

With no `~/.config/fluxland` of its own, fluxland reads `~/.fluxbox`
directly, so you can try it without changing anything. Fluxbox keeps
working as before.

`fluxland -c` checks every file and reports anything it doesn't
understand, with the line.

## Make it fluxland's own

```sh
fluxland --import-fluxbox      # from ~/.fluxbox, or --import-fluxbox=DIR
```

This makes `~/.config/fluxland` from your Fluxbox directory and prints
every change it makes:

- `init`, `keys`, `apps`, `menu`, `windowmenu`, `overlay`, `slitlist`,
  and the `styles`, `backgrounds` and `pixmaps` directories are copied,
  with paths into `~/.fluxbox` pointed at the new place.
- A Fluxbox style that isn't installed any more points at fluxland's
  copy of it (fluxland ships the Fluxbox styles).
- The `startup` script loses its `exec fluxbox` (fluxland runs it once
  it has started), and the programs in it that only work on an X
  display are commented out, each with the reason (see below).
- fbsetbg's last wallpaper becomes fluxland's.

Files already in `~/.config/fluxland` are kept unless you add `--force`.
After the import, the two setups are independent.

## What works the same

- **keys**: every command Fluxbox has, with the same syntax: key chains,
  `KeyMode`s, `MacroCmd`/`ToggleCmd`/`If`/`ForEach`, client patterns,
  mouse bindings with every context, `OnTitlebar Double Mouse1`, wheel
  buttons. `fluxland -l` lists the commands.
- **apps**: `[app]`, `[group]`, `[transient]`, `[startup]` with every
  attribute; the window menu's Remember... writes it as Fluxbox does.
- **init**: Fluxbox's keys, with Fluxbox's defaults (toolbar, slit,
  iconbar, tabs, focus models, workspace warping, snapping, tooltips...).
  Menu toggles and style choices are saved back to it.
- **menu**: every tag, includes, `[submenu]`s, `[stylesdir]`,
  `[wallpapers]`, `[workspaces]`, `[config]`, encodings.
- **styles**: Fluxbox styles, drawn as Fluxbox draws them (textures,
  gradients, bevels, pixmaps, fonts with effects, round corners).

## What's different on Wayland

### The startup script

fluxland runs `~/.config/fluxland/startup` after it has started, with
`WAYLAND_DISPLAY` and `DISPLAY` (Xwayland) set. Programs that need an X
display of their own have Wayland counterparts or are fluxland's job:

| In ~/.fluxbox/startup | In fluxland |
|---|---|
| `fbsetbg`, `feh --bg-*`, `nitrogen`, `hsetroot`, `xsetroot` | the style's `background`, or the `SetWallpaper` command; fluxland draws the desktop |
| `xcompmgr`, `compton`, `picom` | not needed: a Wayland compositor composites |
| `setxkbmap`, `xmodmap` | `session.xkb.layout`, `.variant`, `.options`, `.model`, `.rules` in init |
| `xrandr` | `session.output.<connector>.mode`, `.scale`, `.position`, `.transform`, `.enabled` in init |
| `xinput`, `synclient` | `session.input[.touchpad\|.pointer\|.device.<name>].<setting>` in init (tap to click, natural scrolling, acceleration...) |
| `xscreensaver`, `xautolock` | `swayidle` and `swaylock` |
| `xbindkeys` | bind the keys in the keys file |
| `xset`, `numlockx`, `unclutter` | see `fluxland-init(5)`; no X server to set |
| `idesk`, `fbpager` | they draw on, or page, X's own windows |

The manual pages have every key: `fluxland-init(5)`.

### Your other tools

| Fluxbox | fluxland |
|---|---|
| `fluxbox-remote 'command'` | `fluxland-ctl action 'command'` (any command line, `MacroCmd {...} {...}` too) |
| `fbrun` | the `CommandDialog` command |
| `wmctrl`, `xdotool` | `fluxland-ctl get_windows`, `fluxland-ctl -w ID action ...`; `wtype` to type |
| `import`, `scrot` | `grim` (and `slurp` for a region) |
| screen sharing | `xdg-desktop-portal-wlr`, which fluxland sets up |
| `Restart` | the same as `Reconfigure`: a compositor can't restart under its windows |

### X11 programs

X11 programs run through Xwayland and are ordinary windows: decorations,
tabs, `apps` rules, every command. Window Maker dockapps go into the slit
as they do in Fluxbox.

The system tray shows StatusNotifierItem icons (what current programs and
waybar use). Old programs with only an X11 (XEmbed) tray icon don't
show one.

### init keys with no meaning on Wayland

These are accepted and ignored: `session.configVersion`,
`colorsPerChannel`, `cacheLife`, `cacheMax`, `forcePseudoTransparency`,
`imageDither` (both), `groupFile`, `screen0.allowRemoteActions`,
`screen0.slit.acceptKdeDockapps`, `screen0.antialias` and
`screen0.opaqueResizeDelay`.

## More

- `fluxland(1)`, and `fluxland-init(5)`, `fluxland-keys(5)`,
  `fluxland-apps(5)`, `fluxland-menu(5)`, `fluxland-style(5)`,
  `fluxland-startup(5)` for each file.
- [COMPATIBILITY.md](COMPATIBILITY.md): what fluxland supports of each
  Fluxbox file, item by item.
- [QUICKSTART.md](QUICKSTART.md) to start from fluxland's defaults
  instead.
