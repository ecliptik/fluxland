#!/bin/sh
# SPDX-License-Identifier: MIT
# build.sh - Build fluxland's packages in a clean container of each
# distribution, from the release tarball (tools/dist.sh), offline: the
# tarball carries wlroots and the newer libraries.
#
# Usage: packaging/build.sh TARGET [TARBALL]
#   debian   Debian 13 (trixie) .deb
#   ubuntu   Ubuntu 24.04 (noble) .deb, for Linux Mint 22 too
#   arch     Arch Linux package (PKGBUILD)
#   fedora   Fedora .rpm (fluxland.spec)
# TARBALL defaults to the one tools/dist.sh made. Packages land in
# packaging/out/TARGET/.
#
# Uses podman, or docker.

set -eu
cd "$(dirname "$0")/.."

target=${1:?usage: packaging/build.sh debian|ubuntu|arch|fedora [TARBALL]}
tarball=${2:-$(ls dist-build/meson-dist/fluxland-*.tar.xz 2>/dev/null | tail -1)}
[ -f "$tarball" ] || { echo "no tarball: run tools/dist.sh first" >&2; exit 1; }
tarball=$(cd "$(dirname "$tarball")" && pwd)/$(basename "$tarball")
version=$(basename "$tarball" .tar.xz | sed 's/^fluxland-//')

engine=$(command -v podman || command -v docker) ||
	{ echo "podman or docker is needed" >&2; exit 1; }
out=$PWD/packaging/out/$target
mkdir -p "$out"

run() {
	image=$1
	shift
	"$engine" run --rm \
		-v "$tarball:/in/fluxland-$version.tar.xz:ro" \
		-v "$PWD/packaging:/in/packaging:ro" \
		-v "$out:/out" \
		-e VERSION="$version" \
		"$image" sh -euc "$@"
}

deb() {
	run "$1" '
		apt-get update -q
		DEBIAN_FRONTEND=noninteractive apt-get install -yq \
			--no-install-recommends devscripts equivs build-essential
		mkdir /build && cd /build
		cp /in/fluxland-$VERSION.tar.xz fluxland_$VERSION.orig.tar.xz
		tar xf fluxland_$VERSION.orig.tar.xz
		cd fluxland-$VERSION
		cp -r /in/packaging/debian .
		DEBIAN_FRONTEND=noninteractive mk-build-deps -ir \
			-t "apt-get -yq --no-install-recommends" debian/control
		dpkg-buildpackage -us -uc -b
		cp ../*.deb /out/'
}

case "$target" in
debian)
	deb docker.io/library/debian:trixie
	;;
ubuntu)
	deb docker.io/library/ubuntu:noble
	;;
arch)
	# makepkg won't run as root: a build user, its makedepends first
	run docker.io/library/archlinux:latest '
		pacman -Syu --noconfirm --needed base-devel
		useradd -m build
		mkdir /build && cp /in/fluxland-$VERSION.tar.xz \
			/in/packaging/PKGBUILD /build/
		sed -i "s/^pkgver=.*/pkgver=$VERSION/" /build/PKGBUILD
		chown -R build /build && cd /build
		# (sh is bash here, PKGBUILDs use its arrays)
		. ./PKGBUILD
		pacman -S --noconfirm --needed "${depends[@]}" "${makedepends[@]}"
		runuser -u build -- makepkg --noconfirm
		cp /build/*.pkg.tar.zst /out/'
	;;
fedora)
	run registry.fedoraproject.org/fedora:latest '
		dnf -yq install rpm-build rpmdevtools dnf-plugins-core
		rpmdev-setuptree
		cp /in/fluxland-$VERSION.tar.xz ~/rpmbuild/SOURCES/
		sed "s/^Version:.*/Version:        $VERSION/" \
			/in/packaging/fluxland.spec > ~/rpmbuild/SPECS/fluxland.spec
		dnf -yq builddep ~/rpmbuild/SPECS/fluxland.spec
		rpmbuild -bb ~/rpmbuild/SPECS/fluxland.spec
		cp ~/rpmbuild/RPMS/*/*.rpm /out/'
	;;
*)
	echo "unknown target: $target" >&2
	exit 2
	;;
esac
ls -l "$out"
