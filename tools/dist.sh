#!/bin/sh
# SPDX-License-Identifier: MIT
# dist.sh - Make the release tarball: fluxland's source and that of every
# library it can bundle (subprojects/: wlroots and the libraries newer
# than distributions ship), so packages build from it offline.
#
# Usage: tools/dist.sh   -> dist-build/meson-dist/fluxland-VERSION.tar.xz
#
# It is made from the committed tree (meson dist), with each subproject
# at the revision its wrap pins.

set -eu
cd "$(dirname "$0")/.."

# Every library through its subproject, so all of them are in the tarball
# (meson dist includes the subprojects a build used)
meson setup dist-build --wipe -Dwerror=false \
	--force-fallback-for=wayland-server,wayland-client,wayland-protocols,pixman-1,wlroots-0.20,xkbcommon,libdrm,libdisplay-info,libliftoff
meson dist -C dist-build --include-subprojects --no-tests --formats xztar
ls -l dist-build/meson-dist/
