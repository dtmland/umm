# Install, build, and redistribute

## Release binary

GitHub Releases attach per-OS archives for `ubuntu-24.04` (`.tar.gz`),
`windows-2025` (`.zip`), and `macos-15` (`.tar.gz`). Each archive includes
the static `umm` CLI, bash/zsh/fish completions, `umm(1)`, `LICENSE`,
`NOTICE.md`, `THIRD-PARTY-NOTICES.md`, `licenses/`, `install/` ExifTool
setup scripts (not ExifTool itself), and `README.md`. Corresponding-source
tarballs and `umm-<version>-src.tar.gz` stay `.tar.gz`.

Verify checksums with `SHA256SUMS` on the release. Tag procedure and asset
names: [release checklist](../release-checklist.md).

## Build from source

Requirements: C++20, CMake 3.21 or newer (the `default` preset asks for
3.24), Ninja. Default configure FetchContent-pins libumm from
`tools/build/libumm.env` (`UMM_LIBUMM_VERSION` + `UMM_LIBUMM_SHA256`) and
links **statically**. Exiv2, Expat, and zlib come from *inside* the libumm
build; umm adds no second acquisition path.

```sh
cmake --preset default
cmake --build --preset default
ctest --preset default
cmake --install build/default --prefix /usr/local
```

CI configures with `-DUMM_REQUIRE_EXIV2=ON -DUMM_REQUIRE_EXIFTOOL=ON` so both
backends are present. Linux package names for a from-source build live in
`tools/build/linux-packages.txt`.

### Installed libumm

```sh
cmake --preset default -DUMM_CLI_USE_SYSTEM_LIBUMM=ON -DCMAKE_PREFIX_PATH=/path/to/libumm
```

`find_package(umm CONFIG REQUIRED)` must see a libumm install. CI builds this
mode on Ubuntu against a prefix produced in the same run.

## ExifTool

ExifTool is out-of-process, never bundled, discovered at runtime (config →
`UMM_EXIFTOOL` → `PATH`). `umm setup exiftool` runs native scripts in
`install/`:

- **Windows:** `install/exiftool.bat` → `install/exiftool.ps1`. Prefer
  `winget install -e --id OliverBetz.ExifTool` (standalone `exiftool.exe`,
  no Perl). Fallback: checksum-verified upstream `.zip` into a per-user
  prefix.
- **Linux:** `install/exiftool.sh` — `apt install libimage-exiftool-perl`,
  `dnf install perl-Image-ExifTool`, `pacman -S perl-image-exiftool`; else a
  checksum-verified upstream tarball into a per-user prefix.
- **macOS:** same script — prefer `brew install exiftool`, else checksum-verified
  tarball.

The script records the path in the user config (`exiftool` key) and does not
modify `PATH`. Package-manager versions are not exact pins.

## doctor as the support first step

Ask the user for `umm doctor` (and `umm doctor --json`) before deeper
debugging. It reports backend availability, discovery step, ExifTool/Perl
paths, tested version, and what is lost without ExifTool (live capability
rows). `--backend exiftool` with no ExifTool fails with the same
remediation.

## Completions and man page

Generated at build time from the command table (`umm-gen-docs`) into the
CMake build directory, then installed via GNUInstallDirs:

| Artifact | Destination |
|---|---|
| `umm` binary | `${CMAKE_INSTALL_BINDIR}` |
| `umm.1` | `${CMAKE_INSTALL_MANDIR}/man1` |
| bash completion | `${CMAKE_INSTALL_DATADIR}/bash-completion/completions/umm` |
| zsh completion | `${CMAKE_INSTALL_DATADIR}/zsh/site-functions/_umm` |
| fish completion | `${CMAKE_INSTALL_DATADIR}/fish/vendor_completions.d/umm.fish` |
| notices | `${CMAKE_INSTALL_DOCDIR}` |

On a typical Unix prefix that is `bin/umm`, `share/man/man1/umm.1`,
`share/bash-completion/completions/umm`, `share/zsh/site-functions/_umm`,
`share/fish/vendor_completions.d/umm.fish`, and `share/doc/umm/`.

## Redistribution

- **Source** in this repository: Apache-2.0 (`LICENSE`).
- **Binaries that contain Exiv2** (the v1 default): conveyed under
  **GPL-3.0**. Ship `THIRD-PARTY-NOTICES.md`, verbatim texts in `licenses/`,
  and the corresponding-source tarballs for Exiv2, Expat, and zlib pinned in
  `tools/build/corresponding-source.json` (copied from the pinned libumm).
- **Do not** redistribute ExifTool. Artistic/GPL dual license never enters
  umm's release analysis.
- **Do not** publish umm to Homebrew, winget, apt, or other package managers
  in v1.

An Exiv2-free Apache-only build is not planned for v1.

## CI and artifacts

Three-OS CI (Linux, Windows, macOS) with both backends required:
`.github/workflows/ci.yml`. Tag `vX.Y.Z` matching `project(umm VERSION …)`
in `CMakeLists.txt` triggers a **draft** release
(`.github/workflows/release.yml`). `workflow_dispatch` is a dry-run that
builds artifacts and `SHA256SUMS` but does not create a GitHub release.
Expected names: [release checklist](../release-checklist.md).
