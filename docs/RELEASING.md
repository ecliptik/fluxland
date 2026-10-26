# Releasing

1. **Version.** `version:` in `meson.build`; the changelogs in
   `packaging/debian/changelog` (its distribution `unstable` instead of
   `UNRELEASED`, and the date) and `packaging/fluxland.spec`; `pkgver`
   in `packaging/PKGBUILD`.
2. **Checks.** CI green on `main`: build, sanitize, fuzz, ui, wlcs
   (`tools/ci.sh <step>` runs each locally). `docs/COMPATIBILITY.md` is
   current: `tests/compat.py build/fluxland --markdown
   docs/COMPATIBILITY.md` changes nothing. The hardware pass
   ([HARDWARE-TEST.md](HARDWARE-TEST.md)) done on the packages below.
3. **Tag.** `git tag -s vX.Y.Z` on the release commit, pushed to both
   Forgejo and GitHub (`git push origin vX.Y.Z`, `git push github
   vX.Y.Z`); `main` goes to both the same way.
4. **Tarball.** `tools/dist.sh` on the tagged tree: it makes
   `dist-build/meson-dist/fluxland-X.Y.Z.tar.xz` (fluxland with wlroots
   and the libraries it bundles) and its `.sha256sum`.
5. **Packages.** `packaging/build.sh debian|ubuntu|arch|fedora` from that
   tarball, in clean containers; they land in `packaging/out/`.
6. **Release.** A release for the tag with the tarball, its checksum and
   the packages attached (the PKGBUILD, spec and Flatpak manifest
   download the tarball from it).
7. **Checksums.** The tarball's sha256 in `packaging/PKGBUILD`
   (`sha256sums`) and `packaging/com.fluxland.Fluxland.yml` (`sha256`).
