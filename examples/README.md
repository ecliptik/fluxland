# fluxland Examples

Example configurations and styles for fluxland. Copy any example to your
config directory to get started:

```sh
mkdir -p ~/.config/fluxland
cp -r examples/wave-desktop/* ~/.config/fluxland/
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

## Complete Configurations

Each directory below is a self-contained config you can copy directly to
`~/.config/fluxland/`.

### wave-desktop

A themed desktop inspired by *The Great Wave off Kanagawa*. Includes the
ocean blue style, a startup script that sets the Great Wave as wallpaper,
and nautical workspace names.

**Prerequisites:** Install `swaybg` and download a Great Wave wallpaper to
`~/Pictures/great_wave.jpg`.

### minimal

A bare-minimum configuration for new users. Four workspaces, click-to-focus,
default keybindings, and a solid dark background. Start here and customize.

### sloppy-focus

Focus follows the mouse pointer with auto-raise after 400ms. Good for users
who prefer not to click windows to focus them. Includes opaque resize for a
smoother feel with mouse-driven workflows.

## Customizing

All configuration files are documented with inline comments. See the man
pages for full format documentation:

- `man fluxland` -- compositor usage
- `man fluxland-keys` -- keybinding format
- `man fluxland-apps` -- per-window rules
- `man fluxland-style` -- style format
- `man fluxland-menu` -- menu format
