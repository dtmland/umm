# Third-party notices

This file is the release-grade inventory for binary distributions of umm that
include the Exiv2 backend (compiled in via libumm). Pins match the **pinned
libumm** source archive (`tools/build/libumm.env`); they are not a second
independent set.

umm's own source remains licensed under the Apache License, Version 2.0 (see
`LICENSE`). Because Exiv2 is GPL-2.0-or-later, a binary that combines
Apache-2.0 umm and libumm code with the Exiv2 backend is conveyed under
**GPL-3.0**.

Full license texts live in `licenses/`. The exact pinned source archives to
attach as corresponding source (GPLv3 §6) are listed in
`tools/build/corresponding-source.json`, copied from the pinned libumm
release.

## libumm

- Version: 0.1.2
- License: Apache-2.0
- Pin: `tools/build/libumm.env`
- Role: statically linked library; umm adds no metadata semantics of its own

libumm is not vendored as a git submodule. Binary releases statically absorb
the pinned libumm sources via FetchContent.

## Exiv2

- Version: 0.28.9
- Copyright: Copyright (C) 2004-2026 Exiv2 authors.
- License: GPL-2.0-or-later
- Full text: `licenses/GPL-2.0.txt` (Exiv2's base license, extracted from the
  pinned Exiv2 `COPYING`); combined binaries are conveyed under
  `licenses/GPL-3.0.txt`
- Corresponding source: `tools/build/corresponding-source.json` (id `exiv2`)

Exiv2 is not bundled in this source repository. It arrives statically inside
libumm. Binary releases that contain the Exiv2 backend absorb a copy of the
pinned Exiv2 sources.

## Expat

- Version: 2.6.4
- Copyright: Copyright (c) 1998-2000 Thai Open Source Software Center Ltd and
  Clark Cooper; Copyright (c) 2001-2022 Expat maintainers
- License: MIT (Expat)
- Full text: `licenses/Expat.txt` (extracted from the pinned Expat `COPYING`)
- Corresponding source: `tools/build/corresponding-source.json` (id `expat`)

Expat is used by Exiv2's XMP support. Acquisition happens inside the libumm
build; umm adds no second path.

## zlib

- Version: 1.3.1
- Copyright: (C) 1995-2022 Jean-loup Gailly and Mark Adler
- License: Zlib
- Full text: `licenses/Zlib.txt` (extracted from the pinned zlib `LICENSE`)
- Corresponding source: `tools/build/corresponding-source.json` (id `zlib`)

zlib is used by Exiv2's PNG metadata support. Acquisition happens inside the
libumm build; umm adds no second path.

## ExifTool

ExifTool is invoked **out-of-process only** and is **never bundled** or
redistributed with umm. umm locates an installed ExifTool at runtime (explicit
config → `UMM_EXIFTOOL` → PATH). `umm setup exiftool` runs native install
scripts under `install/` that obtain ExifTool from upstream or the platform
package manager. ExifTool is therefore not part of the combined binary and is
not listed in the corresponding-source manifest.

## Other components not distributed in umm binaries

- GitHub Actions runners, Ninja, and CMake are build tools only.
- Completions and `umm(1)` in release archives are generated from umm's
  command table at build time (Apache-2.0, this repository).
