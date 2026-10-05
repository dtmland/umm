# Release checklist

Use this before tagging an umm release. The tag-triggered workflow is
`.github/workflows/release.yml`. Conveyance and artifact names are also
summarized in [sysadmin install](sysadmin/install.md).

## Version bump

The **only** file to edit for an umm CLI version is `CMakeLists.txt`:

```cmake
project(umm VERSION x.y.z)
```

CMake defines `UMM_CLI_VERSION` from that value. Runtime `umm version`, the
generated `umm(1)` header, release notes, archive names, and the tag check
all read it. Do **not** search-replace the old number across the tree.

`umm` and libumm versions are independent even when they look the same.
Leave these alone unless you are actually changing that other thing:

- `tools/build/libumm.env` — the **libumm** pin, not the CLI version
- `docs/bug-upstream/` and session docs — historical notes (including
  libumm 0.1.0 defects)
- `tests/build/test_release.py` — dummy packaging version (`9.9.9`), not
  the project version
- CLI tests — they assert `UMM_CLI_VERSION`; they must not hardcode
  `umm x.y.z`

Typical sequence:

1. Bump `project(umm VERSION x.y.z)` on a branch (or `main`).
2. Wait for green CI (`.github/workflows/ci.yml`).
3. Merge if needed, then tag (next section).

## Tag procedure

1. Complete the bump on `main` with green CI.
2. Confirm `THIRD-PARTY-NOTICES.md` / `tools/build/corresponding-source.json`
   still match the pinned libumm (`tools/build/libumm.env`).
3. Tag `vX.Y.Z` matching the CMake project version exactly (example: version
   `0.2.0` → `v0.2.0`) and push the tag. The release workflow fails if they
   disagree. Do not use `*-latest` runners.
4. A `workflow_dispatch` run is a **dry-run**: it builds artifacts and
   `SHA256SUMS` but does **not** create a GitHub release.

## Expected artifact names

The draft release (or the dry-run `release-dist` artifact) must contain:

- Per-OS binary archives: `umm-<version>-ubuntu-24.04.tar.gz`,
  `umm-<version>-windows-2025.zip`, and `umm-<version>-macos-15.tar.gz`.
  Each archive includes the static `umm` CLI, bash/zsh/fish completions,
  `umm(1)`, `LICENSE`, `NOTICE.md`, `THIRD-PARTY-NOTICES.md`, `licenses/`,
  `install/` ExifTool setup scripts (not ExifTool itself), and `README.md`.
  Do not ship a Windows `.tar.gz`.
- Corresponding-source tarballs for Exiv2, Expat, and zlib (pinned URLs and
  SHA-256 from `tools/build/corresponding-source.json`, copied from the
  pinned libumm).
- `umm-<version>-src.tar.gz` (Apache-2.0 source).
- `SHA256SUMS` covering every other asset.

Do **not** attach ExifTool. Do **not** publish umm to Homebrew, winget, apt,
or other package managers in v1.

Spot-check:

```sh
sha256sum -c SHA256SUMS
tar -tzf umm-*-ubuntu-24.04.tar.gz | grep share/man/man1/umm.1
```

The workflow never uploads release archives if tests fail (`needs` on the
green matrix; no `if: always()` on asset uploads). Binary conveyance is
GPL-3.0 because of Exiv2; repository `LICENSE` remains Apache-2.0.

## Publish the draft

1. Read the generated notes: umm version, libumm/Exiv2/Expat/zlib pins, and
   the GPL-3.0 / Apache-2.0 statement.
2. Confirm SHA-256 values match `tools/build/corresponding-source.json`.
3. Mark the GitHub release as published.
