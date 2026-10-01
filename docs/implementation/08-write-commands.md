# Session 08 — Write commands (`set`, `rm`)

Status: **complete** (`set` / `rm` in `src/commands.cpp`; value parse in `src/value_format.*`; setters in `src/property.*`)

## Goal

Write canonical properties through libumm's policy engine: `umm set` and
`umm rm`. Sequential files only.

## Concept

- §2.1 no `umm write` command; §2.2 `umm set FILE ASSIGN…`, `umm rm FILE PROP…`
- §2.3 convenience accessors + full ids; §2.4 GPS/dates via `set`;
  §2.5 struct values via `--json`
- §2.6 `--backend`, batch, **no parallel writes in v1**
- §6 no unmapped write; no ad-hoc tag names

## Prerequisites

Sessions 04–06 (batch driver, formatters, read for round-trip tests).

## In scope

### `umm set FILE ASSIGN…`

- `ASSIGN` is `NAME=VALUE` where `NAME` is a convenience accessor
  (`creator="Jane"`, `keywords="a,b"`, `dateCreated=…`, `gps="40.7,-74.0"`,
  `rating=…`) or a full id (`iptc.photo.creator=…`), resolved via the session
  04 seam. All libumm accessors are supported for photo and video; accessors
  follow `MediaDomain` (sniffed video → `iptc.video.*`), `rating` is
  photo-only (error on video).
- Struct / multi-valued properties: `NAME --json '<object|array>'`
  (`locationCreated`, `iptc.photo.creatorsContactInfo`,
  `iptc.video.contributor`, …). Mixed accessor / full-id / `--json` operands in
  one invocation are allowed (concept §2.6 example).
- Parse values as libumm `Value` types. Value
  parsing must match libumm `Value` types — read the headers (strings,
  numbers, dates, GPS, etc.). Do not invent a second type system.
- `umm::read` (or equivalent construct-metadata path if libumm documents one),
  apply sets on the canonical `Metadata`, then `umm::write`.
- `--policy embedded|sidecar|sidecar-required|preferred` maps to
  `StoragePolicy` in `umm/umm.hpp` (`embedded_only`, `sidecar_only`,
  `sidecar_required`, `preferred`). Use those enum names; CLI flag labels
  follow the concept table.
- `--dry-run` sets `WriteOptions::dry_run` and prints the `WriteReport`
  (human and `--json`).
- `--backend` → `WriteOptions::backend`.

### `umm rm FILE PROP…`

- Operands are accessors or full ids. Clear properties across all synchronized representations via `umm::write`
  (libumm write-sync). Not a raw-tag delete.
- Same `--policy`, `--dry-run`, `--backend`, batch rules as `set`.

### Batch / safety

- Use the session 04 sequential loop. Do not thread or process-pool writes.
- Per-file atomicity is libumm's; the CLI must not write in place on its own.

## Out of scope

- `merge` / `sync` (session 09).
- `geotag` (session 10).
- Unmapped write (forbidden).

## Expected files

- `set` and `rm` command modules
- Tests: set then read back; dry-run leaves bytes unchanged; rm clears the
  property; bad property id → documented exit code

## Acceptance

- `umm set photo.jpg iptc.photo.creator=Ada` and `umm set photo.jpg creator=Ada`
  persist through `umm get`; the same accessor on `video.mp4` stores
  `iptc.video.creator`.
- `umm set photo.jpg gps="40.7128,-74.0060"` equals the `exif.gps.position`
  form; struct `--json` round-trips; `rating` on video fails with the
  semantics exit group.
- `--dry-run` prints a `WriteReport` and does not modify the file.
- `--policy sidecar` does not rewrite embedded bytes when libumm reports
  sidecar-only (assert via `WriteReport` / sidecar presence).
- Batch write failures are sequential, summarized, non-zero exit.
- Unknown / invalid values map to the semantics exit group.

## Validation

- Generated fixtures only (Tier A).
- `git diff --check`.
