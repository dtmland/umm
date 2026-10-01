# Session 10 — Geotag

Status: **not started**

## Goal

`umm geotag --track T.gpx FILE…`: import a track, match capture times, write
positions through `umm::write`. No track-specific write path in the CLI.

## Concept

- §2.2 `umm geotag --track T.gpx FILE…` via `umm::importTrack` /
  `matchTrack` / `write`; `--offset` for naive timestamps
- §2.4 GPS, timestamps, and geotag (workflow vs. direct `gps` set)
- §2.6 `--backend`, batch, no parallel writes, `--json` for reports

## Prerequisites

Session 08 (write path). Read `umm/track.hpp` in pinned libumm.

## In scope

- `--track` required; formats are whatever `umm::importTrack` accepts
  (GPX / NMEA / KML). The CLI does not parse tracks itself.
- Per file: `umm::read` to get capture time, `matchTrack`, then `umm::write`
  of `exif.gps.position` (and related fields libumm's match result uses — read
  the API; do not invent GPS property names).
- `--offset` maps to naive-timestamp handling. libumm uses
  `MatchOptions::naive_utc_offset_minutes` and optionally
  `camera_clock_offset_seconds`. The concept's `--offset` is for **naive
  timestamps**. Implement that mapping by reading `MatchOptions`; if both
  clock-skew and naive-offset are useful, add a second flag only when the
  concept's `--offset` is insufficient — prefer one concept-named `--offset`
  for naive UTC offset minutes, and document units.
- `--backend`, `--policy`, `--dry-run` consistent with `set`.
- Fail a file with libumm's `invalid_value` when a naive timestamp has no
  offset (do not assume UTC).
- Sequential files only.

- Photos and video both; read back with `umm get FILE gps` (or
  `exif.gps.position`).

## Out of scope

- Direct coordinate writes (`umm set … gps=…`) — session 08.

- Bundling tracks or map UIs.
- Implementing GPX/NMEA/KML parsers in this repo.

## Expected files

- `geotag` command module
- A tiny generated GPX (or reuse libumm's if present in the fetch) plus a
  fixture whose capture time falls on the track

## Acceptance

- Matching file writes a position `umm get` can read.
- Naive capture time without `--offset` fails with the semantics exit group.
- `--dry-run` writes nothing.
- Bad/missing track file maps to I/O or format exit groups from libumm.

## Validation

- Generated track + still fixture; no third-party media.
- `git diff --check`.
