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
running. When fluxland starts it sets `XDG_CURRENT_DESKTOP=fluxland:wlroots`
(unless it is already set) and imports `WAYLAND_DISPLAY`, `DISPLAY`,
`XDG_CURRENT_DESKTOP`, `XDG_SESSION_TYPE` and `FLUXLAND_SOCK` into the
D-Bus and systemd user sessions, so the portal picks up
`fluxland-portals.conf` and reaches the right display. At exit they are
emptied again.

A fluxland running nested (inside another session) or headless doesn't
touch the user session, so it can't take over the outer session's
portals. `session.updateActivationEnvironment: true` (or `false`) in
`init` overrides that; see fluxland-init(5).

If screen sharing still shows nothing, check that the portal sees the
session: `systemctl --user show-environment | grep -E 'WAYLAND|DESKTOP'`,
and restart it with `systemctl --user restart xdg-desktop-portal`.

Choosing what to share: xdg-desktop-portal-wlr asks with a chooser
command (slurp by default). See its `config` for `chooser_type`.
