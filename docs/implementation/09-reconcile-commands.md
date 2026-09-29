# Session 09 — Reconcile commands (`merge`, `sync`)

Status: **not started**

## Goal

Conflict resolution and carrier synchronization: `umm merge` and `umm sync`,
thin wrappers over `umm::merge` + `umm::write` and `umm::synchronize`.

## Concept

- §2.1 `umm merge FILE PROP --use RAWKEY|--value V`
- §2.1 `umm sync FILE` with `--direction both|embedded-to-sidecar|sidecar-to-embedded`,
  `--dry-run`
- §2.2 `--json`, `--backend`, batch, no parallel writes

## Prerequisites

Sessions 07–08 (`conflicts` for setup/asserts; `set`/`write` path).

## In scope

### `umm merge FILE PROP --use RAWKEY|--value V`

- Exactly one of `--use` or `--value` (concept table).
- `--use RAWKEY`: `umm::detectConflict` / read, then `umm::merge(metadata, entry, source, container)`
  as required by the libumm overload. When the same raw key exists in both
  embedded and sidecar, accept an explicit container flag if the API needs it
  (read the header; add `--container embedded|sidecar` only if necessary).
- `--value V`: `umm::merge(metadata, property_id, value)` override; provenance
  retained (libumm rule — do not call `set` for this path).
- Persist with `umm::write` unless `--dry-run`.
- `--backend`, `--json` for the report / resulting metadata.

### `umm sync FILE`

- `umm::synchronize` with `SyncOptions`.
- `--direction both|embedded-to-sidecar|sidecar-to-embedded` → `SyncDirection`.
- `--dry-run` → `SyncOptions::dry_run`; print `SyncReport`.
- Unresolved `Resolution::conflict` on direction `both` fails with
  `conflict_unresolved` (libumm). Surface that through the exit-code contract;
  tell the user to `umm merge` first (message only — no extra policy).

## Out of scope

- Geotag (session 10).
- Changing reconciliation precedence (libumm's policy; not this repo).

## Expected files

- `merge` and `sync` command modules
- Tests with a fixture that has embedded vs sidecar disagreement if the
  generator can produce one

## Acceptance

- `--use` persists the chosen candidate; `--value` persists an override.
- `umm sync --dry-run` does not mutate files and prints a report.
- `--direction` values match the concept strings.
- Sequential batch only.

## Validation

- Round-trip with `umm conflicts` / `umm read --sources`.
- `git diff --check`.
