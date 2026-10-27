# Releasing

1. **Version.** `version:` in `meson.build`; the changelogs in
   `CHANGELOG.md` (the date), `packaging/debian/changelog` (its
   distribution `unstable` instead of `UNRELEASED`, and the date) and
   `packaging/fluxland.spec`; `pkgver` in `packaging/PKGBUILD`.
2. **Checks.** CI green on `main` (GitHub Actions: build, sanitize, fuzz,
   ui, wlcs; `tools/ci.sh <step>` runs each locally). `docs/COMPATIBILITY.md`
   is current: `tests/compat.py build/fluxland --markdown
   docs/COMPATIBILITY.md` changes nothing. The hardware pass
   ([HARDWARE-TEST.md](HARDWARE-TEST.md)) done on the packages below.
3. **Tag.** `git tag -a vX.Y.Z` (`-s` with a signing key) on the release
   commit, pushed to Forgejo and GitHub (`git push origin vX.Y.Z`,
   `git push github vX.Y.Z`); `main` goes to both the same way. A release
   candidate is `vX.Y.Z-rcN`, published as a prerelease.
4. **Tarball.** `tools/dist.sh` on the tagged tree: it makes
   `dist-build/meson-dist/fluxland-X.Y.Z.tar.xz` (fluxland with wlroots
   and the libraries it bundles).
5. **Packages.** `packaging/build.sh debian|ubuntu|arch|fedora` from that
   tarball, in clean containers; they land in `packaging/out/`. Install
   each in a clean container of its distribution and run
   `fluxland --version`.
6. **Release.** `gh release create vX.Y.Z` with the tarball, the packages
   and a `SHA256SUMS` of all of them attached, and the notes (what's new,
   install lines, the changelog's fixes). GitHub changes `~` to `.` in
   file names, so name release candidates' files `X.Y.Z.rcN`.
7. **Checksums.** The tarball's sha256 in `packaging/PKGBUILD`
   (`sha256sums`) and `packaging/com.fluxland.Fluxland.yml` (`sha256`),
   in a commit after the tag (the tarball contains these files).
