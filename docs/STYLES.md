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

## Upstream Styles

27 styles bundled with the original Fluxbox window manager, imported from the
[Fluxbox repository](https://github.com/fluxbox/fluxbox). These styles are
included in [`examples/styles/upstream/`](../examples/styles/upstream/).

### Directory-based styles (with pixmaps)

#### arch

Light grey pixmap style with blue accents by tenner.

![arch](screenshots/styles/arch.png)

#### bloe

Soft lavender-grey pixmap style with purple title text by tenner.

![bloe](screenshots/styles/bloe.png)

#### BlueFlux

Blue gradient style with XPM pixmap titlebars and toolbar by BioNiK.

![BlueFlux](screenshots/styles/BlueFlux.png)

#### bora_black

Dark grey pixmap style with monochrome gradients by tenner.

![bora_black](screenshots/styles/bora_black.png)

#### bora_blue

Deep blue pixmap style with steel blue borders by tenner.

![bora_blue](screenshots/styles/bora_blue.png)

#### bora_green

Forest green pixmap style with bright green borders by tenner.

![bora_green](screenshots/styles/bora_green.png)

#### carp

Compact dark pixmap style with small fonts and black borders by tenner.

![carp](screenshots/styles/carp.png)

#### Emerge

Clean white/light grey style with raised bevels and crossdiagonal gradients by ikaro.

![Emerge](screenshots/styles/Emerge.png)

#### green_tea

Dark style with muted green borders and pixmap textures by tenner.

![green_tea](screenshots/styles/green_tea.png)

#### ostrich

Dark grey style with warm brown/sienna accents by tenner.

![ostrich](screenshots/styles/ostrich.png)

#### zimek_bisque

Warm bisque/tan style with olive borders and red title text by tenner.

![zimek_bisque](screenshots/styles/zimek_bisque.png)

#### zimek_darkblue

Dark navy blue style with slate grey menus and muted text by tenner.

![zimek_darkblue](screenshots/styles/zimek_darkblue.png)

#### zimek_green

Olive green style with bright green menu titles by tenner.

![zimek_green](screenshots/styles/zimek_green.png)

### Single-file styles (gradient/solid only)

#### Artwiz

Dark steel grey style with diagonal gradients and sunken toolbar elements.

![Artwiz](screenshots/styles/Artwiz.png)

#### BlueNight

Dark blue-tinted style with solid black titlebars and grey-blue menus by mack@incise.org.

![BlueNight](screenshots/styles/BlueNight.png)

#### Flux

Deep indigo/navy style with crossdiagonal cream gradients on menus and labels.

![Flux](screenshots/styles/Flux.png)

#### LemonSpace

Warm lemon-gold style with raised vertical gradients by xlife@zuavra.net.

![LemonSpace](screenshots/styles/LemonSpace.png)

#### Makro

Muted gold/khaki style with sunken vertical gradients by skypher.

![Makro](screenshots/styles/Makro.png)

#### MerleyKay

Slate blue interlaced style with raised gradients by skypher.

![MerleyKay](screenshots/styles/MerleyKay.png)

#### Meta

Soft blue gradient style with parentrelative toolbar elements by joel carlbark.

![Meta](screenshots/styles/Meta.png)

#### Nyz

Dark teal-blue style with diagonal gradients and grey labels.

![Nyz](screenshots/styles/Nyz.png)

#### Operation

Dark blue style with parentrelative toolbar and crossdiagonal gradients.

![Operation](screenshots/styles/Operation.png)

#### Outcomes

Dark charcoal style with raised bevel gradients and interlaced labels.

![Outcomes](screenshots/styles/Outcomes.png)

#### qnx-photon

Light grey QNX Photon-inspired style with blue gradient menu titles by skypher.

![qnx-photon](screenshots/styles/qnx-photon.png)

#### Results

Dark charcoal/indigo style with raised bevel gradients and interlaced labels.

![Results](screenshots/styles/Results.png)

#### Shade

Minimal dark style with flat solid titlebars and cream crossdiagonal menus.

![Shade](screenshots/styles/Shade.png)

#### Twice

Dark red/maroon style with diagonal gradients and grey labels.

![Twice](screenshots/styles/Twice.png)

## Using Styles

### Install a bundled style

Copy a community or upstream style directory to your styles path:

```sh
cp -r examples/styles/community/Graphite ~/.config/fluxland/styles/
cp -r examples/styles/upstream/BlueFlux ~/.config/fluxland/styles/
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
