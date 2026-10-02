# tools/build

- `libumm.env` — pins the libumm source archive (`UMM_LIBUMM_VERSION`,
  `UMM_LIBUMM_URL`, `UMM_LIBUMM_SHA256`). CMake reads it at configure time.
- `pins.sh` — validates `libumm.env` and, under GitHub Actions, appends the pins to `GITHUB_ENV`.
- `linux-packages.txt` — apt prerequisites for the libumm build in CI.
- `corresponding-source.json` — Exiv2/Expat/zlib pin URLs and SHA-256 copied
  from the pinned libumm; attached to each umm release (GPLv3 §6).
- `fetch_corresponding_source.py` — downloads and checksum-verifies those
  archives (never ExifTool).
- `package_release.py` — builds per-OS `umm-<version>-<os>.tar.gz` archives
  and `SHA256SUMS`.
- `generate_release_notes.py` — draft GitHub release notes from CMake version
  and pins.
