# Fluxbox Theme Compatibility

fluxland natively supports Fluxbox style files. Themes from
[box-look.org](https://www.box-look.org/browse?cat=139&ord=latest) and other
Fluxbox theme archives work without modification.

## Community Themes

10 community themes from box-look.org tested and verified on fluxland. These
themes are included in [`examples/themes/community/`](../examples/themes/community/).

### [Carbonit](https://www.box-look.org/p/1016944/)

Dark theme with subtle pixmap textures on titlebars and buttons.

![Carbonit](screenshots/themes/Carbonit.png)

### [Neon](https://www.box-look.org/p/1016708/)

Dark theme with neon-accented window decorations and black menus.

![Neon](screenshots/themes/Neon.png)

### [Soft White](https://www.box-look.org/p/1017014/)

Light theme with white/grey pixmap titlebar and warm grey menus.

![Soft White](screenshots/themes/Soft_White.png)

### [Raven](https://www.box-look.org/p/1017008/)

Dark theme with bold white text, black menus, and beveled decorations.

![Raven](screenshots/themes/Raven.png)

### [Graphite](https://www.box-look.org/p/1016816/)

Clean gradient-based theme with diamond bullet markers and styled window buttons.

![Graphite](screenshots/themes/Graphite.png)

### [Clear](https://www.box-look.org/p/1016899/)

Light blue/cream theme with raised bevels, triangle bullet markers, and styled buttons.

![Clear](screenshots/themes/Clear.png)

### [Plastick](https://www.box-look.org/p/1016818/)

Green/olive theme with distinctive pixmap titlebar textures and button icons.

![Plastick](screenshots/themes/Plastick.png)

### [Black Glass](https://www.box-look.org/p/1017007/)

Dark glass theme with gradient titlebars, styled handles, and wide borders.

![Black Glass](screenshots/themes/Black_Glass.png)

### [Coffee Dream](https://www.box-look.org/p/1016989/)

Warm brown/coffee palette with left-aligned title text and muted tones.

![Coffee Dream](screenshots/themes/CoffeeDream.png)

### [blackdelux](https://www.box-look.org/p/1016962/)

Minimal dark theme with thin borders and teal menu title accent.

![blackdelux](screenshots/themes/blackdelux.png)

## Using Themes

### Install a community theme

Copy a theme directory to your styles path:

```sh
cp -r examples/themes/community/Graphite ~/.config/fluxland/styles/
```

### Apply at runtime

Switch themes without restarting via IPC:

```sh
fluxland-ctl action SetStyle ~/.config/fluxland/styles/Graphite
```

Or use the Styles submenu in the root menu — it lists all themes found in
your configured styles directory.

### Install themes from box-look.org

1. Download a Fluxbox theme archive from [box-look.org](https://www.box-look.org/browse?cat=139&ord=latest)
2. Extract to `~/.config/fluxland/styles/`
3. Apply with `SetStyle` or from the Styles menu

Most pure Fluxbox themes work directly. Themes that require matching GTK
themes will only affect window decorations, menus, and the toolbar — the
GTK portion is not used.

### Create your own

See [`fluxland-style(5)`](../man/fluxland-style.5) for the complete style
file format reference. The default style at
[`data/style`](../data/style) is a good starting point.

## Theme Rendering Support

fluxland supports the full Fluxbox style property set for window decorations,
menus, and the toolbar:

| Feature | Status |
|---------|--------|
| Texture types (Flat, Raised, Sunken) | Supported |
| Gradients (Vertical, Horizontal, Diagonal, Crossdiagonal, Pipecross, Rectangle, Pyramid, Elliptic) | Supported |
| Solid fills and bevel effects | Supported |
| XPM, PNG, JPEG, BMP pixmap textures | Supported (via GdkPixbuf) |
| Window button pixmaps (close, maximize, minimize, stick) | Supported |
| Per-component fonts and colors | Supported |
| Round corners (`window.roundCorners`) | Supported |
| Color formats (`#RRGGBB`, `#RGB`, `rgb:R/G/B`, named colors) | Supported |
| Menu title, frame, and highlight styling | Supported |
| Toolbar base texture and iconbar colors | Supported |
| Wildcard fonts (`*font`, `*Font`) | Supported |

For the full compatibility report including detailed test methodology, see
[`docs/THEME_COMPAT_REPORT.md`](THEME_COMPAT_REPORT.md).
