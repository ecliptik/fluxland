# Screen Recording with PipeWire

Fluxland supports screen recording via PipeWire when built with `-Dpipewire=enabled`.

## Prerequisites

Install PipeWire development libraries:

```sh
# Debian/Ubuntu
sudo apt install libpipewire-0.3-dev

# Fedora
sudo dnf install pipewire-devel

# Arch
sudo pacman -S pipewire
```

## Building with PipeWire support

```sh
meson setup build -Dpipewire=enabled
ninja -C build
```

## Usage

### Start recording

```sh
# Record the default (first) output
fluxland-ctl screen_cast_start

# Record a specific output
fluxland-ctl screen_cast_start '{"output":"HDMI-A-1"}'
```

### Check status

```sh
fluxland-ctl get_screen_cast
# Returns: {"success":true,"data":{"available":true,"state":"streaming","node_id":42}}
```

### Stop recording

```sh
fluxland-ctl screen_cast_stop
```

### Recording with OBS or FFmpeg

The screen cast creates a PipeWire video source node. Use the node ID with:

```sh
# FFmpeg (replace NODE_ID with actual ID)
ffmpeg -f pipewire -i "NODE_ID" -c:v libx264 output.mp4

# OBS Studio
# Add a "PipeWire Video Capture" source and select "fluxland-screen-cast"
```

## Architecture

- PipeWire event loop is integrated with the Wayland event loop via fd polling
- Video format: BGRx (matching wlroots DRM_FORMAT_XRGB8888)
- Frame delivery: triggered by output frame events
- No threads: all PipeWire processing runs on the main compositor thread
