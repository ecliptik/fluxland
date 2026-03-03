# Fluxbox Style Compatibility

fluxland natively supports Fluxbox style files. Styles from
[box-look.org](https://www.box-look.org/browse?cat=139&ord=latest) and other
Fluxbox style archives work without modification.

## Community Styles

10 community styles from box-look.org tested and verified on fluxland. These
styles are included in [`examples/styles/community/`](../examples/styles/community/).

### [Carbonit](https://www.box-look.org/p/1016944/)

Dark style with subtle pixmap textures on titlebars and buttons.

![Carbonit](screenshots/styles/Carbonit.png)

### [Neon](https://www.box-look.org/p/1016708/)

Dark style with neon-accented window decorations and black menus.

![Neon](screenshots/styles/Neon.png)

### [Soft White](https://www.box-look.org/p/1017014/)

Light style with white/grey pixmap titlebar and warm grey menus.

![Soft White](screenshots/styles/Soft_White.png)

### [Raven](https://www.box-look.org/p/1017008/)

Dark style with bold white text, black menus, and beveled decorations.

![Raven](screenshots/styles/Raven.png)

### [Graphite](https://www.box-look.org/p/1016816/)

Clean gradient-based style with diamond bullet markers and styled window buttons.

![Graphite](screenshots/styles/Graphite.png)

### [Clear](https://www.box-look.org/p/1016899/)

Light blue/cream style with raised bevels, triangle bullet markers, and styled buttons.

![Clear](screenshots/styles/Clear.png)

### [Plastick](https://www.box-look.org/p/1016818/)

Green/olive style with distinctive pixmap titlebar textures and button icons.

![Plastick](screenshots/styles/Plastick.png)

### [Black Glass](https://www.box-look.org/p/1017007/)

Dark glass style with gradient titlebars, styled handles, and wide borders.

![Black Glass](screenshots/styles/Black_Glass.png)

### [Coffee Dream](https://www.box-look.org/p/1016989/)

Warm brown/coffee palette with left-aligned title text and muted tones.

![Coffee Dream](screenshots/styles/CoffeeDream.png)

### [blackdelux](https://www.box-look.org/p/1016962/)

Minimal dark style with thin borders and teal menu title accent.

![blackdelux](screenshots/styles/blackdelux.png)

## Using Styles

### Install a community style

Copy a style directory to your styles path:

```sh
cp -r examples/styles/community/Graphite ~/.config/fluxland/styles/
```

### Apply at runtime

Switch styles without restarting via IPC:

```sh
fluxland-ctl action SetStyle ~/.config/fluxland/styles/Graphite
```

Or use the Styles submenu in the root menu — it lists all styles found in
your configured styles directory.

### Install styles from box-look.org

1. Download a Fluxbox style archive from [box-look.org](https://www.box-look.org/browse?cat=139&ord=latest)
2. Extract to `~/.config/fluxland/styles/`
3. Apply with `SetStyle` or from the Styles menu

Most pure Fluxbox styles work directly. Styles that require matching GTK
themes will only affect window decorations, menus, and the toolbar — the
GTK portion is not used.

### Create your own

See [`fluxland-style(5)`](../man/fluxland-style.5) for the complete style
file format reference. The default style at
[`data/style`](../data/style) is a good starting point.

## Style Rendering Support

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
[`docs/STYLE_COMPAT_REPORT.md`](STYLE_COMPAT_REPORT.md).
