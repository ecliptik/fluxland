# Testing on real hardware

What a VM and the headless tests can't show: a GPU and KMS, real input
devices, laptop switches, suspend, input methods and the portals. Run
through it on a real machine before a release; the notes in brackets are
what the first pass is for (a Lenovo IdeaPad 5 2-in-1 14AHP9: Radeon
780M, a touchpad and touchscreen, a USB-C monitor, Linux Mint 22).

Note anything that doesn't work as it says, with the log:

```sh
fluxland -d 2> ~/fluxland.log      # from a text console, or:
journalctl --user -b | grep fluxland
```

## Install

- [ ] `sudo apt install ./fluxland_2.0.0-1_amd64.deb` (the Ubuntu one for
  Mint 22) installs with its dependencies; `fluxland --version` says
  2.0.0 and wlroots 0.20.2.
- [ ] `fluxland -c` on your configuration (or `~/.fluxbox`) reports
  nothing unexpected.
- [ ] The display manager (LightDM on Mint) offers **Fluxland**, and it
  starts. So does `fluxland` from a text console (Ctrl+Alt+F3).

## Display

- [ ] The built-in panel at its native mode; no tearing or flicker
  moving and resizing windows. (Radeon 780M, DRM backend)
- [ ] `session.output.eDP-1.scale: 1.5` (HiDPI) scales everything:
  decorations, toolbar, menus, and programs (foot, Firefox) stay sharp.
- [ ] Plug in the external monitor: it lights up, windows can move to
  it; unplug it: its windows, the toolbar and the slit come back to the
  panel. (USB-C monitor that also charges)
- [ ] `session.output.<name>.position` and `.mode` put the two where
  they belong; `fluxland-ctl get_outputs` agrees.
- [ ] Suspend and resume (lid or menu): the outputs come back, input
  works, windows are where they were.

## Input

- [ ] The touchpad: `session.input.touchpad.tap: true`,
  `naturalScroll: true` and the acceleration settings take effect after
  a reconfigure (Mod4+r). (FTCS0038:00, Focal Tech I2C HID)
- [ ] Touchpad gestures, if bound in the keys file.
- [ ] **Mod1 Shift Tab** cycles windows backwards (its keysym arrives as
  ISO_Left_Tab; only a real keyboard shows this).
- [ ] Media, volume and brightness keys reach what they're bound to.
- [ ] The touchscreen: tapping focuses and clicks; dragging scrolls in
  Firefox.
- [ ] The keyboard layout from `session.xkb.layout` (and a second one
  with a switching option) works in Wayland and X11 programs.

## Laptop switches

- [ ] Closing the lid runs `session.switch.lidClose` (with nothing set,
  what logind does). With the USB-C monitor charging, wait until the
  battery shows it isn't before closing the lid (csd-power blocks the
  lid switch while it charges).
- [ ] Folding into tablet mode runs `session.switch.tabletModeOn`;
  unfolding runs `tabletModeOff`.

## Programs

- [ ] **fcitx5**: its candidate window appears next to the text being
  typed in a Wayland program (foot, GTK) and in an X11 one. Checked on
  the VM: fcitx5 gets the keys and its preedit reaches foot; the
  candidate window wasn't (fcitx5 there wouldn't load pinyin).
- [ ] **Screen sharing**: in Firefox, share a window and the whole
  screen (meet.jit.si or a WebRTC test page) through
  xdg-desktop-portal-wlr: the picture moves, the right thing is shared.
  It needs the GPU: portal-wlr 0.7 asks screencopy for dmabufs, which
  software rendering (a VM without 3D, `WLR_RENDERER=pixman`) has none
  of, and it gives up ("unable to receive a valid format"). Checked on
  the VM: fluxland-portals.conf routes ScreenCast to wlr, and the
  session gets as far as that.
- [ ] Screenshots: `grim`, and `grim -g "$(slurp)"`.
- [ ] An X11 program (xterm, or a game through Steam/Wine) gets
  decorations, can be moved, tabbed and fullscreened.
- [ ] A game that locks the pointer (any first-person game) can turn
  around freely and releases it on leaving.
- [ ] `swaylock` locks; nothing shows through it, and unlocking works.
- [ ] A notification daemon (mako) and a panel (waybar) start from the
  startup script; waybar sits beside the toolbar, not under it.

## Your Fluxbox setup

- [ ] With only `~/.fluxbox` (move `~/.config/fluxland` aside): keys,
  menu, apps rules and style work as in Fluxbox; the startup script's X
  programs are left out (see the log), and Fluxbox doesn't start.
- [ ] `fluxland --import-fluxbox` then fluxland with the result: the
  same.
