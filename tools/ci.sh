#!/bin/sh
# SPDX-License-Identifier: MIT
# ci.sh - Build and test fluxland the way CI does.
#
# Usage: tools/ci.sh deps|build|sanitize|fuzz|ui
#   deps      install build/test dependencies (Debian 13, as root)
#   build     gcc build + C unit tests
#   sanitize  ASan + UBSan build + C unit tests
#   fuzz      clang build of the fuzz targets + a short run of each
#   ui        headless end-to-end UI tests (pytest)
#
# Each step uses its own build directory (ci-<step>) so they can run
# side by side and never clobber a developer's build/.

set -eu

step=${1:-build}
cd "$(dirname "$0")/.."

# fluxland refuses to run as root, and CI containers start as root:
# run everything but "deps" as an unprivileged user.
if [ "$step" != deps ] && [ "$(id -u)" = 0 ]; then
	id ci >/dev/null 2>&1 || useradd --create-home ci
	chown -R ci: .
	exec runuser -u ci -- env FUZZ_SECONDS="${FUZZ_SECONDS:-}" "$0" "$@"
fi

# A private runtime dir for the headless compositors the tests start
if [ -z "${XDG_RUNTIME_DIR:-}" ]; then
	XDG_RUNTIME_DIR=$(mktemp -d)
	export XDG_RUNTIME_DIR
fi

case "$step" in
deps)
	apt-get update
	DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
		build-essential clang libclang-rt-dev meson ninja-build pkg-config \
		git ca-certificates \
		libwlroots-0.18-dev libwayland-dev wayland-protocols \
		libxkbcommon-dev libinput-dev libpixman-1-dev libdrm-dev \
		libpango1.0-dev libcairo2-dev libgdk-pixbuf-2.0-dev \
		libxcb1-dev libxcb-ewmh-dev libxcb-icccm4-dev xwayland \
		libsystemd-dev \
		python3 python3-pytest python3-pytest-timeout python3-pil \
		foot grim wtype wlrctl fonts-dejavu-core
	;;
build)
	meson setup ci-build --wipe -Dwerror=false
	ninja -C ci-build
	meson test -C ci-build --print-errorlogs
	;;
sanitize)
	meson setup ci-sanitize --wipe -Dasan=true -Dubsan=true \
		-Db_lundef=false
	ninja -C ci-sanitize
	export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
	export LSAN_OPTIONS="suppressions=$PWD/tests/sanitizers/lsan.supp"
	# Unit tests stub wlroots (buffers are never really freed), so
	# only memory errors and UB count there.
	ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 \
		meson test -C ci-sanitize --print-errorlogs \
		--no-suite integration
	# Integration tests drive the real compositor: leaks count too.
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 \
		meson test -C ci-sanitize --print-errorlogs \
		--suite integration
	;;
fuzz)
	CC=clang meson setup ci-fuzz --wipe
	ninja -C ci-fuzz
	seconds=${FUZZ_SECONDS:-30}
	[ -n "$seconds" ] || seconds=30
	for target in ci-fuzz/tests/fuzz/fuzz_*; do
		[ -f "$target" ] && [ -x "$target" ] || continue
		name=$(basename "$target" | sed 's/^fuzz_//')
		corpus=tests/fuzz/corpus/$name
		echo "== fuzzing $name for ${seconds}s"
		"$target" -max_total_time="$seconds" -max_len=4096 \
			${corpus:+"$corpus"}
	done
	;;
ui)
	meson setup ci-build -Dwerror=false 2>/dev/null || true
	ninja -C ci-build
	# The harness looks for build/fluxland; point it at this build
	FLUXLAND_BINARY="$PWD/ci-build/fluxland" FLUXLAND_TEST_HEADLESS=1 \
		python3 -m pytest tests/ui/ --timeout=30 -q -p no:cacheprovider
	;;
*)
	echo "usage: $0 deps|build|sanitize|fuzz|ui" >&2
	exit 2
	;;
esac
