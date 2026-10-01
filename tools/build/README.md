# tools/build

- `libumm.env` — pins the libumm source archive (`UMM_LIBUMM_VERSION`,
  `UMM_LIBUMM_URL`, `UMM_LIBUMM_SHA256`). CMake reads it at configure time.
- `pins.sh` — validates `libumm.env` and, under GitHub Actions, appends the pins to `GITHUB_ENV`.
- `linux-packages.txt` — apt prerequisites for the libumm build in CI.

libumm has no release yet, so the pin is a commit archive.
