# fluxland Examples

Example configurations and styles for fluxland. Each example only contains
files that differ from the defaults in `data/`. To use an example, first copy
the defaults, then overlay the example-specific files:

```sh
mkdir -p ~/.config/fluxland
cp data/* ~/.config/fluxland/
cp examples/wave-desktop/* ~/.config/fluxland/
chmod +x ~/.config/fluxland/startup
```

## Styles

Ready-to-use style files in `styles/`. Copy to `~/.config/fluxland/style`
or point `session.styleFile` in your `init` to the path.

| Style | Description |
|---|---|
| `default.style` | Clean dark style with Nord-inspired colors |
| `wave.style` | Deep indigo and ocean blue, inspired by Hokusai's *The Great Wave off Kanagawa* |
| `hc-dark.style` | High-contrast dark (WCAG AAA) -- black background, white/yellow text |
| `hc-light.style` | High-contrast light (WCAG AAA) -- white background, black/blue text |

### Applying a style

Copy the style file:

```sh
cp examples/styles/wave.style ~/.config/fluxland/style
```

Or set the path in `~/.config/fluxland/init`:

```
session.styleFile: /path/to/wave.style
```

Then press `Mod4+r` to reload.

## Example Configurations

Each directory below contains only the files that differ from defaults.
Start by copying `data/*`, then overlay the example on top.

### wave-desktop

A themed desktop inspired by *The Great Wave off Kanagawa*. Overrides `init`
(nautical workspace names, wave style path) and `startup` (Great Wave
wallpaper via swaybg).

```sh
cp data/* ~/.config/fluxland/
cp examples/wave-desktop/* ~/.config/fluxland/
cp examples/styles/wave.style ~/.config/fluxland/styles/
chmod +x ~/.config/fluxland/startup
```

**Prerequisites:** Install `swaybg` and download a Great Wave wallpaper to
`~/Pictures/great_wave.jpg`.

### minimal

A bare-minimum configuration for new users. Overrides `init` (trimmed
settings) and `startup` (minimal startup). Four workspaces, click-to-focus,
and a solid dark background. Start here and customize.

```sh
cp data/* ~/.config/fluxland/
cp examples/minimal/* ~/.config/fluxland/
chmod +x ~/.config/fluxland/startup
```

### sloppy-focus

Focus follows the mouse pointer with auto-raise after 400ms. Overrides `init`
(MouseFocus settings, opaque resize) and `startup`. Good for users who prefer
not to click windows to focus them.

```sh
cp data/* ~/.config/fluxland/
cp examples/sloppy-focus/* ~/.config/fluxland/
chmod +x ~/.config/fluxland/startup
```

## Customizing

All configuration files are documented with inline comments. See the man
pages for full format documentation:

- `man fluxland` -- compositor usage
- `man fluxland-keys` -- keybinding format
- `man fluxland-apps` -- per-window rules
- `man fluxland-style` -- style format
- `man fluxland-menu` -- menu format
