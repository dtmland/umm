# Session 19 — Contract docs and completions

Status: **not started**

## Goal

Publish the 0.1.2 CLI contract: user/sysadmin/developer docs, README,
completions/`umm(1)` verification, examples that match sessions 16–18, and
the Windows binary release archive as **`.zip`** (not `.tar.gz`).
No new commands.

## Concept

- §1–§2 every command, GPS via Location, dump views, cast, map, workflows
- §2.6 / §7.2 JSON `schema_version` policy
- §6 non-goals (`dumpall` / `dumpunmapped`; no base write)
- §7.4 completions and man pages from the command table

## Prerequisites

Sessions 16–18 complete (dump/GPS pin, `cast`, `map` all landed).

## In scope

### User docs (`docs/user/`)

Rewrite pages that still describe the 0.1.1 surface:

- [getting-started.md](../user/getting-started.md) — first `read` / `get` /
  `set`; Location GPS not `gps=`
- [commands.md](../user/commands.md) — every §2.2 command and flags from the
  **command table** (including `dumpall`, `dumpunmapped`, `cast`, `map`,
  `read --report-casts`). Remove `unmapped`. `merge --use` is BASEKEY.
- [examples.md](../user/examples.md) — concept §2.7 workflows; geotag +
  `locationCreated`; cast preview/apply; map
- [json-schema.md](../user/json-schema.md) — live `schema_version`, `base_key`,
  dump/cast/map documents, geotag dry-run vs Location write. Breaking-change
  policy unchanged (major / pre-1.0 0.y bump already done in session 16)
- [exit-codes.md](../user/exit-codes.md) — only if 16–18 reserved a new code
  (do not invent one here)
- [config.md](../user/config.md) — unchanged unless a flag name moved

Do not copy libumm's property encyclopedia. Link to libumm v0.1.2
[user guide](https://github.com/dtmland/libumm/blob/v0.1.2/docs/user/guide.md)
and [property reference](https://github.com/dtmland/libumm/blob/v0.1.2/docs/user/properties/README.md).

### Indexes and developer docs

- Root [README.md](../../README.md) — drop `gps` examples; show Location
  GPS and mention `dumpall` / `cast` / `map` only as far as a quick start
  needs
- [docs/README.md](../README.md) — index; implementation sessions 16–19 are
  the active plan; 01–15 remain historical
- [docs/developer/contributing.md](../developer/contributing.md) — dump
  commands, no `gps`, pass-through includes `CastOptions`; how to add a
  command still starts at the command table
- [docs/sysadmin/install.md](../sysadmin/install.md) — pin 0.1.2 / CLI 0.2.0
  only where it states versions; licensing unchanged; Windows archive is
  `.zip`
- [docs/release-checklist.md](../release-checklist.md) — CMake VERSION is
  still the only CLI version (now 0.2.0); expected artifacts list Windows
  `.zip` and Unix `.tar.gz`

### Windows zip release archive

Session 14 ships every OS as `umm-<version>-<os>.tar.gz`. Change **only**
the Windows binary archive to zip.

- Name: `umm-<version>-windows-2025.zip` (same version/OS tokens as today).
- Linux and macOS stay `umm-<version>-ubuntu-24.04.tar.gz` and
  `umm-<version>-macos-15.tar.gz`.
- Corresponding-source Exiv2/Expat/zlib tarballs and
  `umm-<version>-src.tar.gz` stay `.tar.gz`.
- Archive **contents** stay the same (static `umm.exe`, completions, `umm(1)`,
  notices, `install/` scripts, `README.md`). Do not add ExifTool.
- Implement in `tools/build/package_release.py`: choose `.zip` when `--os`
  is the Windows runner id (`windows-2025`). Use the stdlib `zipfile`
  module; do not add a packaging dependency.
- `.github/workflows/release.yml`: upload the Windows zip (the current
  `path: release-dist/umm-*.tar.gz` misses it). `SHA256SUMS` greps must
  expect `umm-.*windows-2025\.zip$`, not `.tar.gz`.
- Offline `tests/build/test_release.py` must assert Windows → zip, other
  binary OS ids → tar.gz, and that the workflow does not require a Windows
  `.tar.gz`.
- Update [tools/build/README.md](../../tools/build/README.md) and the root
  README install line so they do not claim every OS is `.tar.gz`.

### Completions and man

- Confirm `umm-gen-docs` emits `dumpall`, `dumpunmapped`, `cast`, `map`,
  `--report-casts`, `--apply`, `--group`, `--force`, `--include-approximate`,
  `--layers` from the table.
- Completions still offer accessor names from `accessor_names()` (no `gps`).
- Existing contract tests in `tests/cli/` and `tests/build/test_docs.py` must
  pass. Do not hand-edit generated artifacts.

### Goldens / tests

- Only fill gaps 16–18 deferred (example: help-text goldens that still say
  `unmapped`). Do not add a new test framework.

## Out of scope

- New CLI commands or flags.
- Re-pinning libumm.
- Deleting sessions 01–15.
- Package-manager publication of umm.

## Expected files

- `docs/user/*`, `docs/README.md`, `docs/developer/contributing.md`,
  `docs/sysadmin/install.md`, `docs/release-checklist.md` as needed
- Root `README.md`
- `.github/copilot-instructions.md` only if it still contradicts the
  0.1.2 vocabulary after this planning change
- Test/help goldens if they still mention `unmapped` / `gps` / `raw_key`
- `tools/build/package_release.py`, `.github/workflows/release.yml`,
  `tests/build/test_release.py`, `tools/build/README.md` (Windows zip)

## Acceptance

- User command reference matches `umm COMMAND --help` / the command table.
- No remaining user-facing `umm unmapped`, `gps=`, or `raw_key` as *current*
  contract (historical notes in sessions 01–15 are fine).
- README quick start round-trips `creator` and Location GPS.
- Docs-gen contract tests pass.
- `docs/README.md` points at sessions 16–19 as the active implementation
  plan.
- Windows binary artifact is `umm-<version>-windows-2025.zip`; Linux/macOS
  binaries remain `.tar.gz`; no Windows `.tar.gz` in `SHA256SUMS`.

## Validation

- `python3 -m unittest discover -s tests/build -v` (includes
  `test_release.py` zip/tar contract)
- CLI help / golden tests that session 19 touched
- `git diff --check`
