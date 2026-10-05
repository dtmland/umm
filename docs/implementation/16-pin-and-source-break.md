# Session 16 — Pin libumm 0.1.2 and source-break migration

Status: **in progress**

## Goal

Move the existing CLI from libumm 0.1.1 to **libumm 0.1.2** and migrate every
*already shipped* command onto the 0.1.2 public API and concept vocabulary.
The tree must configure, compile, and pass tests for the migrated surface.
Do not add `umm cast` or `umm map`.

## Concept

- §2.2 `dumpall`, `dumpunmapped`; `merge … --use BASEKEY`; `geotag` Location
  GPS; existing `read` / `get` / `set` / `rm` / `conflicts` / `caps` / `sync`
  / `doctor` / `setup` / `version`
- §2.3 accessors from pinned headers; no `gps` accessor; no
  `exif.gps.position`
- §2.4 GPS on Location structs; geotag write-back
- §2.5 Location `--json`
- §2.6 `--json`, `--backend`, batch, no parallel writes
- §3.1 pin in `tools/build/libumm.env`
- §4.1 corresponding source copied from the pinned libumm
- §6 base display via dump commands; no base write
- §7.2 breaking JSON → bump `schema_version` and CLI semver (pre-1.0: 0.2.0)

## Prerequisites

Sessions 01–15 complete (current tree).

## In scope

### Pin

- `tools/build/libumm.env`: `UMM_LIBUMM_VERSION=0.1.2`,
  `UMM_LIBUMM_URL` for the v0.1.2 source archive,
  `UMM_LIBUMM_SHA256` of that archive. **Compute** the hash; do not invent it.
- Copy Exiv2/Expat/zlib URL+SHA-256 pins from the pinned libumm into
  `tools/build/corresponding-source.json` (existing session 14 rule). Update
  `libumm_version` and `pin_source` strings. Do not invent a second pin set.
- Refresh `THIRD-PARTY-NOTICES.md` / `licenses/` only if the pinned libumm
  notices changed.
- Offline pin/workflow contract tests must still pass.

### CLI version and JSON schema

- `project(umm VERSION …)` in `CMakeLists.txt` is the only CLI version. Bump
  it to **0.2.0** (pre-1.0 breaking change). Do not search-replace version
  strings elsewhere; tests assert `UMM_CLI_VERSION`.
- Bump JSON `"schema_version"` (currently `1`) because this session removes
  or renames fields (`raw_key` → `base_key`, `unmapped` command/key). Additive
  fields alone would not require a bump.

### Command table and dump views

- Remove `umm unmapped`. Do **not** keep it as an alias.
- Add `umm dumpall FILE…` → `Metadata::dumpAll()` (every base entry, source
  order, read-only).
- Add `umm dumpunmapped FILE…` → `Metadata::dumpUnmapped()` (entries no
  canonical property consumed, read-only).
- Human and `--json`. JSON per file should present libumm's base entries
  (`family`, `key`, `value` — read `BaseEntry` in the pin; include
  `cast_source` if the struct has it). No write path.
- `--backend`, `--recursive`, batch via the existing driver.

### Provenance, merge, types

- Replace every `Unmapped*` type and `raw_key` with `Base*` / `base_key`
  from the pinned headers (`SourceRef`, `WriteReport::written`, merge
  `--use`).
- `umm merge FILE PROP --use BASEKEY|--value V`: help text, flag value name,
  and JSON use **base key**. Keep `--container embedded|sidecar` if the
  libumm overload still needs it when the same base key is ambiguous.
- `umm read --sources` JSON/human: `base_key`, not `raw_key`.

### Property addressing (no `gps`)

- `src/property.cpp`: drop `gps`, `exif.gps.position`, and
  `Datatype::gps_coordinate`. Camera GPS is Location struct fields
  (`gpsLatitude` / `gpsLongitude` / …) on `locationCreated` /
  `locationShot`.
- Rebind accessors from **public** `Metadata` getters in the pinned
  `metadata.hpp`. Do not hand-copy the concept table. If `rating` is no
  longer photo-only in the headers, drop `photo_only`. If a getter
  disappeared, remove the binding.
- `umm set` / `get` / `rm` of GPS uses Location `--json` (concept §2.4 / §2.5),
  not `gps="lat,lon"`.
- Unknown names still map to `unknown_property`.

### Geotag

- Still `umm::importTrack` / `matchTrack` / `umm::write`. No track parser
  here.
- Persist by merging the match into `locationCreated[0]` GPS (photo) or
  `locationShot[0]` GPS (video). Read `track.hpp` comments and libumm
  write-back tests; do not invent property ids.
- `--dry-run` still prints the intended GPS write without `umm::write`.
  `TrackMatch::position` remains `GpsCoordinate` for the match; that is not
  a canonical property.
- `--offset` remains `MatchOptions::naive_utc_offset_minutes`.
- Sequential files only.

### Tests

- Fix anything that does not compile (`unmapped`, `gps`, `raw_key`,
  `gps_coordinate`).
- Update goldens under `tests/goldens/` (LF). Regenerate with
  `UMM_REGENERATE_GOLDENS=1` where that is the repo method.
- Replace table-driven `gps` round-trips with Location GPS `--json`.
- Keep accessor coverage table-driven from libumm getters so a pin bump
  that adds an accessor fails loudly.
- Do not add a new test framework.

## Out of scope

- `umm cast` and `umm read --report-casts` (session 17).
- `umm map` (session 18).
- Audience docs, README examples, contributing prose (session 19). Completions
  and `umm(1)` regenerate from the command table at build time; do not
  hand-edit them. If a docs-gen contract test fails because the table
  changed, fix the test or generator — not a committed man page.
- Changing doctor/setup scripts unless the pin forces a compile fix.
- Windows binary archive `.zip` vs `.tar.gz` (session 19). Keep existing
  packaging tests green; do not switch the Windows artifact here.
- Parallel writes, unmapped/base write, a `gps` compatibility alias.

## Expected files

- `tools/build/libumm.env`
- `tools/build/corresponding-source.json` (and notices if the pin moved them)
- `CMakeLists.txt` (`project(umm VERSION 0.2.0)`)
- `src/command_table.cpp`, `src/commands.cpp`, `src/property.cpp`,
  `src/output.cpp`, `src/value_format.cpp` as needed
- `tests/cli/test_cli.cpp`, `tests/goldens/*`, `tests/build/` pin contracts
- `docs/user/json-schema.md` only if tests or comments embed `schema_version`
  1 as the live contract — prefer session 19 for the full user rewrite; this
  session must not leave the published schema describing `unmapped` /
  `raw_key` as current. **Minimum:** update `docs/user/json-schema.md` field
  names and `schema_version` so it is not false. Broader command-reference
  rewrite is session 19.

## Acceptance

- Default CMake configure fetches libumm **0.1.2** via the env pin and links
  `umm::umm`.
- The CLI binary builds on the session's OS.
- `umm dumpall` / `umm dumpunmapped` print base entries and never write.
- `umm unmapped` is unknown command (usage exit).
- `umm get FILE gps` / `umm set FILE gps=…` are unknown property (semantics).
- `umm set photo.jpg locationCreated --json '[{"gpsLatitude":…,"gpsLongitude":…}]'`
  round-trips on `umm get` / `umm read`.
- `umm geotag --track …` writes Location GPS that `umm get FILE locationCreated`
  (photo) or the video Location accessor/id can read.
- `umm read --sources --json` uses `base_key`.
- `umm merge --help` says `BASEKEY`.
- JSON documents use the new `schema_version`.
- `umm version` reports CLI 0.2.0 and libumm 0.1.2.
- Offline `python3 -m unittest discover -s tests/build -v` passes.
- CLI tests for the migrated surface pass; skip a backend only when it is
  absent.

## Validation

- Configure + build against the pin (network allowed for FetchContent).
- `ctest` (or the documented equivalent) for CLI tests.
- `python3 -m unittest discover -s tests/build -v`
- `git diff --check`
