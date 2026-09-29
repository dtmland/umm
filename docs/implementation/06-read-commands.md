# Session 06 — Read commands (`version`, `read`, `get`)

Status: **not started**

## Goal

Implement `umm version`, `umm read`, and `umm get` on `umm::read` /
`umm::version()` / `Registry::standards()`. No metadata logic in the CLI.

## Concept

- §2.1 `umm read FILE…`, `umm get FILE PROPERTY…`, `umm version`
- §2.2 `--json`, `--backend`, batch
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

- `umm get photo.jpg iptc.photo.creator` (one or more property ids).
- Print values only (stable, scriptable). `--json` still allowed.
- Exit non-zero if a requested property is absent (session 04 reserved code).
- Unknown property ids: map libumm `unknown_property` through the exit-code
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
- `--backend exiv2` and `--backend exiftool` pass through (skip or xfail a
  backend only when `doctor` would say it is missing — that command is later;
  for now skip if the backend is unavailable and say so in the test).

## Validation

- ctest/CLI tests on generated JPEG/XMP (libumm Tier A strategy: tiny,
  in-repo, generated — do not commit third-party media).
- `git diff --check`.
