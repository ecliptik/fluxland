# Built from the release tarball (tools/dist.sh): it carries wlroots 0.20
# and the libraries fluxland needs newer than a distribution may have,
# under subprojects/, and fluxland links them in statically.
Name:           fluxland
Version:        2.0.0
Release:        1%{?dist}
Summary:        A Wayland compositor that works the way Fluxbox does
License:        MIT
URL:            https://github.com/ecliptik/fluxland
Source0:        %{url}/releases/download/v%{version}/%{name}-%{version}.tar.xz

BuildRequires:  gcc
BuildRequires:  meson >= 1.3.0
BuildRequires:  ninja-build
BuildRequires:  bison
BuildRequires:  pkgconfig(hwdata)
BuildRequires:  pkgconfig(cairo)
BuildRequires:  pkgconfig(egl)
BuildRequires:  pkgconfig(expat)
BuildRequires:  pkgconfig(gbm)
BuildRequires:  pkgconfig(gdk-pixbuf-2.0)
BuildRequires:  pkgconfig(glesv2)
BuildRequires:  pkgconfig(lcms2)
BuildRequires:  pkgconfig(libdisplay-info)
BuildRequires:  pkgconfig(libdrm)
BuildRequires:  pkgconfig(libffi)
BuildRequires:  pkgconfig(libinput) >= 1.14
BuildRequires:  pkgconfig(libliftoff)
BuildRequires:  pkgconfig(libseat)
BuildRequires:  pkgconfig(libsystemd)
BuildRequires:  pkgconfig(libudev)
BuildRequires:  pkgconfig(libxml-2.0)
BuildRequires:  pkgconfig(pangocairo)
BuildRequires:  pkgconfig(pixman-1)
BuildRequires:  pkgconfig(wayland-server)
BuildRequires:  pkgconfig(xcb)
BuildRequires:  pkgconfig(xcb-composite)
BuildRequires:  pkgconfig(xcb-errors)
BuildRequires:  pkgconfig(xcb-ewmh)
BuildRequires:  pkgconfig(xcb-icccm)
BuildRequires:  pkgconfig(xcb-render)
BuildRequires:  pkgconfig(xcb-res)
BuildRequires:  pkgconfig(xcb-xfixes)
BuildRequires:  pkgconfig(xkbcommon)
BuildRequires:  pkgconfig(xwayland)
# wlroots and the newer libraries, linked in from the tarball
Provides:       bundled(wlroots) = 0.20.2

Recommends:     xorg-x11-server-Xwayland
Recommends:     foot
Recommends:     xdg-desktop-portal-wlr
Recommends:     xdg-desktop-portal-gtk

%description
Fluxland is a Wayland compositor that works the way Fluxbox does: an
existing ~/.fluxbox (init, keys, apps, menu, styles) works unchanged, or
is imported with fluxland --import-fluxbox. It has Fluxbox's toolbar,
slit, tabs, menus, key chains and modes, window rules and styles, and
runs X11 programs through Xwayland.

%prep
%autosetup -n %{name}-%{version}

%build
# %%meson turns every optional feature on, the bundled wlroots' too: let
# them find what's there instead
%meson --auto-features=auto -Dxwayland=enabled -Dwerror=false
%meson_build

%check
# The unit tests (the integration ones start a compositor)
%meson_test --suite fluxland --no-suite integration

%install
# fluxland only: nothing of the bundled libraries is installed
DESTDIR=%{buildroot} meson install -C %{_vpath_builddir} --no-rebuild \
	--skip-subprojects

%files
%license LICENSE
%doc README.md
%{_bindir}/fluxland
%{_bindir}/fluxland-ctl
%{_datadir}/wayland-sessions/fluxland.desktop
%{_datadir}/xdg-desktop-portal/fluxland-portals.conf
%{_datadir}/fluxland/examples/
%{_datadir}/fluxland/styles/
%{_mandir}/man1/fluxland.1*
%{_mandir}/man1/fluxland-ctl.1*
%{_mandir}/man5/fluxland-apps.5*
%{_mandir}/man5/fluxland-init.5*
%{_mandir}/man5/fluxland-keys.5*
%{_mandir}/man5/fluxland-menu.5*
%{_mandir}/man5/fluxland-startup.5*
%{_mandir}/man5/fluxland-style.5*

%changelog
* Sun Oct 25 2026 fluxland contributors <fluxland@example.com> - 2.0.0-1
- fluxland 2.0, for Fluxbox users: an existing ~/.fluxbox works
  unchanged (init, keys, apps, menu, styles), fluxland --import-fluxbox
- wlroots 0.20, bundled and linked statically
- X11 programs are ordinary windows; libinput and output settings in
  init; portals; WLCS in CI

* Mon Mar 03 2026 Fluxland Contributors <noreply@fluxland.dev> - 1.1.0-1
- Fluxbox style compatibility with 37 bundled styles
- GdkPixbuf pixmap support for XPM/JPEG/BMP
- PipeWire screen cast, touchpad gestures, AT-SPI bridge
- Performance: iconbar dirty tracking, decoration position guards
- Multiple critical bug fixes (UAF, type confusion, config hardening)

* Fri Feb 27 2026 fluxland contributors <fluxland@example.com> - 1.0.0-1
- Release 1.0.0

* Tue Feb 25 2026 fluxland contributors <fluxland@example.com> - 0.1.0-1
- Initial package
