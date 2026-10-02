# Release checklist

Use this before tagging an umm release. The tag-triggered workflow is
`.github/workflows/release.yml`. Conveyance and artifact names are also
summarized in [sysadmin install](sysadmin/install.md).

## Version bump

Keep these identical:

1. `project(umm VERSION x.y.z)` in `CMakeLists.txt` (normative for tags).
2. Runtime `UMM_CLI_VERSION` (set from that CMake project version).

Tag `vX.Y.Z` matching the CMake project version exactly (example: version
`0.1.0` → `v0.1.0`). The release workflow fails if they disagree.

## Tag procedure

1. Complete the bump on `main` with green CI (`.github/workflows/ci.yml`).
2. Confirm `THIRD-PARTY-NOTICES.md` / `tools/build/corresponding-source.json`
   still match the pinned libumm (`tools/build/libumm.env`).
3. Tag `vX.Y.Z` and push the tag. Do not use `*-latest` runners.
4. A `workflow_dispatch` run is a **dry-run**: it builds artifacts and
   `SHA256SUMS` but does **not** create a GitHub release.

## Expected artifact names

The draft release (or the dry-run `release-dist` artifact) must contain:

- Per-OS binary archives `umm-<version>-<os>.tar.gz` for `ubuntu-24.04`,
  `windows-2025`, and `macos-15`. Each archive includes the static `umm`
  CLI, bash/zsh/fish completions, `umm(1)`, `LICENSE`, `NOTICE.md`,
  `THIRD-PARTY-NOTICES.md`, `licenses/`, `install/` ExifTool setup scripts
  (not ExifTool itself), and `README.md`.
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
