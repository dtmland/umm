# Session 06 — Read commands (`version`, `read`, `get`)

Status: **complete** (`umm version` / `read` / `get` in `src/commands.cpp`; value JSON/summary in `src/value_format.*`)

## Goal

Implement `umm version`, `umm read`, and `umm get` on `umm::read` /
`umm::version()` / `Registry::standards()`. No metadata logic in the CLI.

## Concept

- §2.1 `read` vs `get` vocabulary; §2.2 `umm read FILE…`, `umm get FILE PROPERTY…`,
  `umm version`
- §2.3 convenience accessors and full property ids
- §2.5 struct display
- §2.6 `--json`, `--backend`, batch
- §3.3 version and standards reporting

## Prerequisites

Sessions 04–05.

## In scope

### `umm version`

- Print CLI version, `umm::version()`, and `Registry::standards()` (read the
  actual registry API in pinned libumm headers; do not invent field names).
- Human and `--json` forms. JSON includes `schema_version`.
- This is how a user answers "which IPTC TR does this binary implement?"
  without opening docs.

### `umm read FILE…`

- Call `umm::read` per file. Pass `--backend` into `ReadOptions`.
- Default: human table of canonical properties.
- `--sources`: include provenance (libumm source refs) and resolution states.
  Read `Metadata` in libumm headers for the real field names.
- `--json` for machine output.
- Batch + `--recursive` via session 04 driver.

### `umm get FILE PROPERTY…`

- `umm get photo.jpg creator` (convenience accessor) and
  `umm get photo.jpg iptc.photo.creator` (full id); one or more per call.
  Mixed lists are allowed (`umm get photo.jpg creator keywords gps`).
- Support **every** accessor libumm ships (concept §2.3 table plus the
  Tier 1–3 remainder: `altTextAccessibility`, `personShown`, `supplier`, …),
  resolved through the session 04 addressing seam and libumm's headers — do
  not hand-copy the list. Video files resolve accessors to `iptc.video.*`
  (e.g. `locationCreated` → `iptc.video.locationShot`); full ids never
  retarget the other domain; `rating` is photo-only; `gps` is
  `exif.gps.position`. `shownEvent` on photos spans `eventName` +
  `eventIdentifier`.
- Struct values: compact summary by default, JSON object/array with `--json`.
- Print values only (stable, scriptable). `--json` still allowed.
- Exit non-zero if a requested property is absent (session 04 reserved code).
- Unknown property names (accessor or id): map libumm `unknown_property` through the exit-code
  contract; do not invent CLI-side property aliases.

## Out of scope

- `unmapped`, `conflicts`, `caps` (session 07).
- Writes (session 08+).

## Expected files

- Command modules for `version`, `read`, `get`
- Tests using tiny generated fixtures (may start the fixture helper that
  session 13 standardizes)

## Acceptance

- `umm version` shows CLI version, libumm version, and standards table.
- `umm read` on a fixture prints canonical properties; `--sources` adds
  provenance/resolution; `--json` is valid and versioned.
- `umm get` prints requested values and fails when absent.
- Every libumm accessor is reachable on a photo fixture and (where defined)
  a video fixture; a table-driven test enumerates accessors from libumm, so a
  pin bump that adds one is covered or fails loudly.
- `get video.mp4 creator` reads `iptc.video.creator`; `get photo.jpg
  iptc.video.creator` does not retarget.
- `--backend exiv2` and `--backend exiftool` pass through (skip or xfail a
  backend only when `doctor` would say it is missing — that command is later;
  for now skip if the backend is unavailable and say so in the test).

## Validation

- ctest/CLI tests on generated JPEG/XMP (libumm Tier A strategy: tiny,
  in-repo, generated — do not commit third-party media).
- `git diff --check`.
