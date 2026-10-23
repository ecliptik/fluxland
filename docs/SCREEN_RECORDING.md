# Screenshots, Screen Recording and Screen Sharing

Fluxland doesn't record the screen itself. It implements the standard
wlroots capture protocols, and the usual tools build on them:

| Protocol | Used by |
|----------|---------|
| `wlr-screencopy-unstable-v1` | grim, wf-recorder, wl-screenrec, xdg-desktop-portal-wlr |
| `wlr-export-dmabuf-unstable-v1` | obs-wlrobs, older recorders |

## Screenshots

```sh
grim ~/screenshot.png                      # all outputs
grim -g "$(slurp)" ~/region.png            # a region
grim -o HDMI-A-1 ~/output.png              # one output
```

## Recording

```sh
wf-recorder -f ~/recording.mp4             # all outputs
wf-recorder -g "$(slurp)" -f ~/region.mp4  # a region
wf-recorder -o HDMI-A-1 -f ~/output.mp4    # one output
```

Stop with Ctrl+C.

## Screen sharing (browsers, OBS, video calls)

Firefox, Chromium, OBS, Zoom and other apps share the screen through
[xdg-desktop-portal](https://flatpak.github.io/xdg-desktop-portal/). On
wlroots compositors the backend is
[xdg-desktop-portal-wlr](https://github.com/emersion/xdg-desktop-portal-wlr),
which captures with screencopy and streams over PipeWire.

Install `xdg-desktop-portal`, `xdg-desktop-portal-wlr` and
`xdg-desktop-portal-gtk` (for file choosers), and make sure PipeWire is
running. When fluxland starts it sets `XDG_CURRENT_DESKTOP=fluxland` and
imports its environment into the D-Bus and systemd user sessions, so the
portal picks up `fluxland-portals.conf` and the right `WAYLAND_DISPLAY`.

Choosing what to share: xdg-desktop-portal-wlr asks with a chooser
command (slurp by default). See its `config` for `chooser_type`.
