# Session 07 — Inspect commands (`unmapped`, `conflicts`, `caps`)

Status: **complete** (`unmapped` / `conflicts` / `caps` in `src/commands.cpp`)

## Goal

Read-only inspection: unmapped escape hatch, conflict listing, live
capabilities. Degraded-mode visibility for missing ExifTool starts here via
`caps` (doctor text is session 11).

## Concept

- §2.2 `umm unmapped FILE`, `umm conflicts FILE`, `umm caps FILE|TYPE`
- §2.6 `--json`, `--backend`, batch
- §4.3 degraded modes: `umm caps` / `umm doctor` say what is lost without
  ExifTool (`doctor` in session 11)

## Prerequisites

Session 06 (read path, formatters, fixtures).

## In scope

### `umm unmapped FILE`

- Dump every `Metadata::unmapped()` entry: family, key, value. Read-only.
- Human and `--json`. This is the escape hatch; still no unmapped write.

### `umm conflicts FILE`

- `umm::detectConflict`. List disagreeing properties with each candidate
  source.
- `--fail-on-conflict` for scripting (non-zero when any conflict exists,
  including policy-reconciled disagreement if that is what the libumm API
  reports — read `ConflictReport` / `Resolution` and match libumm, do not
  redefine "conflict").
- `--json` and `--backend`.

### `umm caps FILE|TYPE`

- `umm::capabilities` for a file path or a type token. Show per-backend,
  per-category capability rows — the supported-types answer, live.
- With no ExifTool, rows must still show what Exiv2 can do and what is
  unavailable (video write, PNG EXIF, BMFF breadth, as libumm's capability
  table states). Do not hardcode that list in the CLI; print what libumm
  returns.

## Out of scope

- `umm doctor` narrative and remediation (session 11).
- Merge/sync (session 09).
- Unmapped write (non-goal).

## Expected files

- Command modules for `unmapped`, `conflicts`, `caps`
- Tests (fixture with unmapped data and/or a conflict if the fixture
  generator can produce one; otherwise test JSON shape + a no-conflict file)

## Acceptance

- `unmapped` prints family/key/value and never writes.
- `conflicts` lists candidates; `--fail-on-conflict` exits non-zero when
  libumm reports disagreement.
- `caps` works for a file and for a type; `--json` includes `schema_version`.
- `--backend` pass-through on commands that read.

## Validation

- CLI tests; skip backend-specific cases only when that backend is absent.
- `git diff --check`.
