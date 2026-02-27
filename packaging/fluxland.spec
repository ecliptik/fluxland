Name:           fluxland
Version:        1.0.0
Release:        1%{?dist}
Summary:        A Fluxbox-inspired Wayland compositor
License:        MIT
URL:            https://github.com/ecliptik/fluxland
Source0:        %{url}/archive/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  meson >= 0.60.0
BuildRequires:  ninja-build
BuildRequires:  pkgconfig(wlroots-0.18) >= 0.18.0
BuildRequires:  pkgconfig(wayland-server) >= 1.22.0
BuildRequires:  pkgconfig(wayland-protocols) >= 1.32
BuildRequires:  pkgconfig(xkbcommon)
BuildRequires:  pkgconfig(libinput) >= 1.14
BuildRequires:  pkgconfig(pixman-1)
BuildRequires:  pkgconfig(pangocairo)
BuildRequires:  pkgconfig(libdrm)
BuildRequires:  pkgconfig(xcb)
BuildRequires:  pkgconfig(xcb-ewmh)
BuildRequires:  pkgconfig(xcb-icccm)
BuildRequires:  pkgconfig(libsystemd)

Recommends:     xorg-x11-server-Xwayland
Recommends:     foot

%description
Fluxland is a lightweight Wayland compositor inspired by Fluxbox.
It provides a familiar stacking window manager experience on Wayland
using the wlroots library, with features including configurable
keybindings, window decorations, workspaces, a toolbar, and menus.

%prep
%autosetup -n %{name}-%{version}

%build
%meson -Dxwayland=auto
%meson_build

%check
%meson_test || true

%install
%meson_install

%files
%license LICENSE
%doc README.md
%{_bindir}/fluxland
%{_bindir}/fluxland-ctl
%{_datadir}/applications/fluxland.desktop
%{_datadir}/wayland-sessions/fluxland-session.desktop
%{_datadir}/fluxland/examples/
%{_mandir}/man1/fluxland.1*
%{_mandir}/man5/fluxland-apps.5*
%{_mandir}/man5/fluxland-keys.5*
%{_mandir}/man5/fluxland-menu.5*
%{_mandir}/man5/fluxland-style.5*

%changelog
* Fri Feb 27 2026 fluxland contributors <fluxland@example.com> - 1.0.0-1
- Release 1.0.0

* Tue Feb 25 2026 fluxland contributors <fluxland@example.com> - 0.1.0-1
- Initial package
