# Session 17 — Cast and `read --report-casts`

Status: **complete**

## Goal

Expose libumm's opt-in cast engine: `umm cast` and `umm read --report-casts`.
Preview by default; persist only on `--apply`. No cast heuristics in this
repo.

## Concept

- §2.2 `umm read FILE…` — `--report-casts` lists `castCandidates()` without
  applying them
- §2.2 `umm cast FILE up|down|side` — `umm::cast`; `--group`, `--force`,
  `--include-approximate`; `--apply` persists through `umm::write`
- §2.6 `--json`, `--backend`, batch, no parallel writes

## Prerequisites

Session 16 complete (pin 0.1.2, dump/GPS migration compiling).

## In scope

### `umm read --report-casts`

- Set `ReadOptions::report_casts` (default off in libumm).
- Human and `--json` present `Metadata::castCandidates()` **separately** from
  canonical properties. A preview must not look like stored data.
- `--report-casts` does not call `umm::cast` with `dry_run = false` and does
  not write.
- Keep `--sources` behavior from session 16.

### `umm cast FILE up|down|side`

- Register in the command table. Direction is a **positional** operand
  (`up` / `down` / `side`), matching the concept synopsis — not a `--direction`
  flag (that name is already `umm sync`).
- Call `umm::cast(path, CastDirection, CastOptions)`. Read
  `include/umm/umm.hpp` in the pin for `CastOptions` /
  `CastReport` / `CastCandidate` / `CastStatus`. Do not invent statuses.
- Default is preview: `CastOptions::dry_run` stays **true**.
- `--apply` sets `dry_run = false`. Applied casts persist through the
  library's write path (`umm::cast` then `umm::write` as the pin does — read
  the implementation comments / headers; the CLI must not add a second write
  engine).
- `--group NAME` fills `CastOptions::groups` (repeatable if the flag table
  allows; otherwise comma-separated — pick one, document it in the table
  summary, match libumm). Empty groups = all groups for that direction.
- `--force` → `CastOptions::force` (`needs_force`).
- `--include-approximate` → `CastOptions::include_approximate` (required to
  apply approximate groups such as `videoCreated`).
- `--backend` → `ReadOptions` / `WriteOptions` as other mutating commands
  do. Do not add a `--downcast` flag on `set` (library default
  `WriteOptions::downcast` stays libumm's).
- `--json` on preview and apply reports. Include `schema_version`.
- Batch: concept shows `FILE` singular. Support the existing sequential file
  loop if operands are `FILE…` plus one direction token; do not parallelize.
  Direction must be valid or usage-exit.

### Output

- Print what libumm returns: per-group status (`can_cast`, `equal`,
  `needs_force`, `target_not_storable`, `ambiguous`, … — only names the
  header actually has), source/target, previews. Empty sources are omitted
  if libumm omits them.
- Unknown direction → usage (exit 1).
- Map libumm errors through the existing exit-code contract.

### Tests

- Generated fixtures only (Tier A). Prefer tiny JPEG/MP4 the existing helper
  can produce. Do not commit third-party media.
- Preview does not change file bytes.
- `--apply` without `--include-approximate` does not apply approximate
  groups (assert via libumm statuses / read-back).
- `--report-casts` on `read` includes candidates and still lists canonical
  properties separately.
- `--json` is valid and versioned.
- Skip ExifTool-only cases when that backend is absent.

## Out of scope

- `umm map` (session 18).
- Audience docs (session 19).
- Inventing cast groups, heuristics, or a CLI-side status enum.
- `WriteOptions::downcast` as a `set` flag.
- Changing dump/GPS work from session 16.

## Expected files

- `src/command_table.cpp` (`cast`; `read --report-casts`)
- `src/commands.cpp` (and args if operand order needs a small extension)
- Tests + goldens for `cast` preview/apply and `read --report-casts`
- Do not hand-edit generated man/completions

## Acceptance

- `umm cast photo.jpg up` prints statuses and does not write.
- `umm cast photo.jpg up --apply` persists through libumm; a second preview
  can show `equal` where applicable.
- `--force` / `--include-approximate` / `--group` pass through to
  `CastOptions`.
- `umm read FILE --report-casts` lists `castCandidates()` without applying.
- `--json` includes `schema_version`.
- Sequential files only.

## Validation

- CLI tests with generated fixtures.
- `git diff --check`
