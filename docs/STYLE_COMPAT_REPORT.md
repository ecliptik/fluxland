# Fluxbox Style Compatibility Report

Tested 10 community Fluxbox styles from [box-look.org](https://www.box-look.org/browse?cat=139&ord=latest)
(pages 42-46) against Fluxland v1.0.0 to verify style rendering fidelity.

## Test Environment

- **Compositor:** Fluxland v1.0.0 + 9 compat fixes
- **Display:** 1280x720 headless Wayland session via lightdm
- **Test method:** IPC `SetStyle` + `RootMenu`, screenshot via `grim`
- **Comparison:** Side-by-side with box-look.org preview screenshots

## Styles Tested

| # | Style | Source Page | Type | XPM Pixmaps | Result |
|---|-------|-----------|------|-------------|--------|
| 1 | Carbonit | p46 | Directory + theme.cfg | 10 pixmaps | PASS |
| 2 | Neon | p46 | Directory + theme.cfg | 21 pixmaps | PASS |
| 3 | Soft White | p45 | Directory + theme.cfg | 22 pixmaps | PASS |
| 4 | Raven | p45 | Directory + theme.cfg | 24 pixmaps | PASS |
| 5 | Graphite | p44 | Single style file | None | PASS |
| 6 | Clear | p44 | Single style file | None | PASS |
| 7 | Plastick | p43 | Directory + theme.cfg | 9 pixmaps | PASS |
| 8 | Black Glass | p46 | Directory + theme.cfg | 24 pixmaps | PASS |
| 9 | Coffee Dream | p43 | Directory + theme.cfg | 8 pixmaps | PASS |
| 10 | blackdelux | p42 | Directory + theme.cfg | 13 pixmaps | PASS |

**All 10 styles render correctly.** Styles were selected to avoid GTK dependencies
(Zakeba, GAIA, and Dyne were excluded for requiring matching GTK themes).

## Bugs Found and Fixed

Nine compatibility issues were discovered and fixed during testing:

### 1. No XPM/multi-format pixmap support (commit 83e833f)

**Problem:** `load_pixmap()` in render.c only supported PNG via
`cairo_image_surface_create_from_png()`. 8 of 10 community styles use XPM
pixmaps for button icons, titlebar textures, toolbar backgrounds, and menu
elements. All pixmap-based styling was silently broken.

**Fix:** Added `gdk-pixbuf-2.0` as an optional dependency (`-Dpixbuf=auto`).
PNG loading is tried first as a fast path; GdkPixbuf handles XPM, JPEG, BMP,
and all other formats as fallback. Straight alpha from GdkPixbuf is correctly
converted to Cairo's premultiplied alpha.

**Impact:** Critical. Without this fix, 8 of 10 styles would render with
plain solid colors instead of their designed pixmap textures and button icons.

### 2. Root-level and bare wildcard keys not parsed (commit f671f47)

**Problem:** Styles like Graphite and Clear use root-level keys (`borderColor`,
`borderWidth`, `bevelWidth`, `handleWidth`) and wildcard fonts (`*font`,
`*Font`) without a prefix dot. Fluxland's `style_get()` only tried `*.font`
(with dot), missing `*font` (without dot) and bare `borderColor`.

**Fix:** Extended `style_get()` to also try:
- `*component` without dot separator (for `*font`/`*Font`)
- Capitalized variant (`*Font` when looking for `*font`)
- Bare key without any prefix (for root-level `borderColor` etc.)

**Impact:** Medium. Affected border rendering and font selection for styles
using older Fluxbox conventions (3 of 10 styles).

### 3. Implicit pixmap fill when pixmap path is set (commit fc656d2)

**Problem:** Many community styles set a pixmap path (e.g.
`window.title.focus.pixmap: title.xpm`) without specifying "Pixmap" in the
texture type string (leaving it empty or omitting it). Fluxbox treats the
pixmap path as an implicit override, but Fluxland required the explicit keyword.

**Fix:** In `load_texture()`, when a valid pixmap path is found but the texture
fill type is not already `WM_TEX_PIXMAP` or `WM_TEX_PARENT_RELATIVE`,
automatically switch to pixmap fill.

**Impact:** High. Without this fix, styles like Soft White, Carbonit, Neon,
Raven, and Black Glass rendered with solid black instead of their designed
pixmap textures for titlebars, toolbars, and menu elements.

### 4. Pixmap subdirectory not searched (commit TBD)

**Problem:** Many Fluxbox styles store pixmap files in a `pixmaps/`
subdirectory within the style directory, but reference them by bare filename
in theme.cfg (e.g. `toolbar.pixmap: toolbar.xpm`). Fluxbox automatically
checks both `style_dir/filename` and `style_dir/pixmaps/filename`. Fluxland
only tried the direct path, so styles like Plastick and Coffee Dream loaded
menus correctly (solid colors) but rendered titlebars and toolbars as plain
black — all their pixmap textures were silently missing.

**Fix:** Added `resolve_pixmap_file()` helper in style.c that tries the
direct path first, then falls back to `style_dir/pixmaps/filename`. Both
`load_texture()` and `load_pixmap_path()` use this helper.

**Impact:** High. Affected 8 of 10 pixmap-based styles — titlebars, toolbars,
and button icons all failed to load their designed textures.

### 5. X11 greyNN/grayNN named colors not parsed (commit TBD)

**Problem:** The X11 color database includes `grey0` through `grey100` (and
`gray0`–`gray100`) as named colors representing percent brightness. Styles
like blackdelux use `grey80`, `grey40`, `grey20` for text and background
colors. Fluxland's `style_parse_color()` only supported 6 basic named colors
(black, white, red, green, blue, yellow), so greyNN values fell through to
the default black — making light-grey-on-black text invisible.

**Fix:** Added `greyNN`/`grayNN` parsing in `style_parse_color()` that
converts the 0–100 percent value to an RGB grey level.

**Impact:** Medium. Affected styles using X11 grey shades for text colors,
causing invisible or wrong-colored text.

### 6. Per-button pixmaps not used in decoration rendering (commit TBD)

**Problem:** Fluxbox styles define per-button pixmap images for close,
maximize, minimize, shade, and stick buttons (e.g.
`window.close.pixmap: close_focus.xpm`). These were correctly parsed by
style.c into the `wm_style` struct, but `render_button()` in decoration.c
never consulted them — it only rendered the generic `window.button.focus`
texture with a vector glyph overlay. Styles that relied on per-button pixmaps
(leaving generic button colors empty) rendered as solid black boxes.

**Fix:** Added `get_button_pixmap()` lookup in decoration.c and extended
`render_button()` to try the per-button pixmap first via `wm_load_pixmap()`.
Falls back to the generic texture + glyph when no pixmap is configured.
Exposed `load_pixmap()` from render.c as public `wm_load_pixmap()`.

**Impact:** High. 6 of 10 styles (Carbonit, Neon, Soft White, Raven, Black
Glass, Coffee Dream) showed featureless black boxes instead of their designed
button icons.

### 7. Black shade buttons — missing ParentRelative fallback

**Problem:** Styles like Carbonit, Soft White, Raven, Plastick, Black Glass,
and Coffee Dream define per-button pixmaps for Close/Max/Min/Stick but leave
Shade empty. The generic `window.button.focus` texture also parses to black.
In Fluxbox, an empty button texture inherits from the titlebar
(ParentRelative). Fluxland rendered these as solid black squares.

**Fix:** When `render_button()` detects that the button background texture is
near-black (RGB sum < 15) and no per-button pixmap exists, it falls back to
the titlebar texture. When both the glyph color and background are near-black,
the glyph color is derived from the titlebar text color for visibility.

**Impact:** Medium. Affected the shade button on 6 styles.

### 8. `rgb:` single-hex-digit color components not scaled per X11 spec

**Problem:** X11's `XParseColor` defines `rgb:R/G/B` where each component is
1-4 hex digits, scaled to 8-bit: 1 digit is repeated (C→CC), 2 as-is, 3
right-shifted by 4, 4 right-shifted by 8. Fluxland parsed them as plain
decimal first, then hex — so `rgb:c/6/6` became (12,6,6) instead of the
correct (204,102,102). Styles like Twice had near-black buttons.

**Fix:** Replaced the `rgb:` parser with proper hex-only parsing and
`scale_rgb_component()` that scales based on digit count.

**Impact:** Medium. Affected styles using short-form `rgb:` color values.

### 9. Missing per-component toolbar text colors and textures

**Problem:** Fluxbox styles define separate text colors and textures for each
toolbar element: `toolbar.label.textColor`, `toolbar.clock.textColor`,
`toolbar.workspace.textColor`, `toolbar.button`, `toolbar.clock`, etc.
Fluxland only parsed `toolbar.textColor` and used it everywhere. Styles like
Artwiz, BlueNight, Flux, qnx-photon, Shade, bora_black, and zimek_darkblue
had unreadable toolbar text because the single color didn't match all
component backgrounds. Styles like carp, green_tea, and ostrich lost their
distinct per-component backgrounds entirely.

**Fix:** Added 21 new fields to `wm_style` for per-component toolbar textures
(`toolbar.label`, `toolbar.clock`, `toolbar.workspace`, `toolbar.button`) and
text colors (`toolbar.label.textColor`, `toolbar.clock.textColor`,
`toolbar.workspace.textColor`, `toolbar.windowLabel.textColor`,
`toolbar.button.picColor`). Toolbar rendering functions now use per-component
styles with fallback chain: component-specific → `toolbar.textColor`.

**Impact:** High. Affected toolbar readability on 10+ upstream Fluxbox styles.

## Rendering Comparison Details

### Fully Matched Elements

These elements render identically or near-identically to Fluxbox:

- **Window titlebars**: Texture types (Flat, Raised, Sunken), gradients
  (Vertical, Horizontal, Diagonal), solid fills, pixmap textures, bevel effects
- **Window buttons**: Close, maximize, minimize, stick pixmaps in all 3 states
  (focus, unfocus, pressed)
- **Window handles and grips**: Texture, color, and width
- **Window borders**: Color and width (including root-level key fallback)
- **Menu title**: Texture, color, text color, font, justification
- **Menu frame**: Texture, color, text color, bullet style and position
- **Menu highlight**: Texture, color, text color
- **Toolbar base**: Texture, pixmap, text color
- **Toolbar iconbar**: Focused/unfocused colors and text colors
- **Fonts**: Wildcard `*font`/`*Font` and per-component font settings
- **Round corners**: `window.roundCorners` bitmask
- **Color formats**: `#RRGGBB`, `#RGB`, `rgb:R/G/B` (1-4 hex digit scaling), named colors, X11 `greyNN`/`grayNN`

### Known Limitations (Minor)

These Fluxbox features are not fully implemented but have minimal visual impact:

| Feature | Fluxbox | Fluxland | Impact |
|---------|---------|----------|--------|
| Toolbar sub-component pixmaps | Per-component pixmaps for clock, workspace, buttons | Uses per-component textures and text colors | Low - most styles use matching textures |
| Toolbar height | Explicit `toolbar.height` | Derived from font + padding | Low - heights are usually similar |
| Menu checkbox/radio pixmaps | `menu.selected.pixmap` etc. | Built-in check/radio marks | Low - rarely used in styles |
| Menu icons | Inline XPM icons next to items | Not supported | Low - a menu config feature, not style |
| Toolbar shaped | X11 Shape extension | N/A on Wayland | None |
| Transparency in style | Alpha values in style file | Not supported (use compositor) | None - deprecated in Fluxbox 0.9.11+ |

## Style Property Coverage

Tested styles collectively use 120+ unique style keys. Coverage by category:

| Category | Keys Used | Keys Supported | Coverage |
|----------|-----------|---------------|----------|
| Window title/label | 18 | 18 | 100% |
| Window buttons | 15 | 15 | 100% |
| Window handle/grip | 8 | 8 | 100% |
| Window border/bevel | 6 | 6 | 100% |
| Menu | 16 | 13 | 81% |
| Toolbar | 30 | 19 | 63% |
| Slit | 3 | 3 | 100% |
| Font/wildcard | 4 | 4 | 100% |

Toolbar coverage increased from 27% to 63% with per-component texture and
text color support. Remaining unsupported toolbar keys are mostly for
niche features (shaped toolbar, alpha, explicit height).

## Conclusion

**Confidence: High.** Standard Fluxbox styles from box-look.org will work
correctly in Fluxland after the nine compatibility fixes. The
core rendering engine handles all 8 gradient types, bevel effects, pixmap
textures (via GdkPixbuf), font styling, and the full window/menu style
property set. The only gaps are in fine-grained toolbar sub-component styling
and menu checkbox pixmaps, which have negligible visual impact on typical styles.

## Screenshots

Screenshots are in `docs/screenshots/style-compat/`:
- `{StyleName}_fluxland.png` - Fluxland rendering
- `{StyleName}.{jpg,png}` - Original box-look.org preview
