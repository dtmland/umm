# Session 18 — Map (`umm::describe`)

Status: **complete**

Landed: `umm map PROPERTY [FILE]` calls `umm::describe` (registry-only or file
mode). `--layers` filters display only. JSON uses libumm field names plus
`schema_version` and `command`. Operand order is property first.

## Goal

Add `umm map PROPERTY [FILE]`: print libumm's property map (definition,
representations, casts, cross-media partner). Optional file fills values and
cast-group statuses. No registry or mapping tables in this repo.

## Concept

- §2.2 `umm map PROPERTY [FILE] [--layers representations,casts,cross-media]
  [--json]` → `umm::describe`
- §2.3 cross-media names (`locationCreated`) expand to both domain ids
- §2.6 `--json` on inspect commands; `--backend` when a file is read

## Prerequisites

Session 16 complete. Session 17 is **not** required (17 and 18 may run in
parallel). File-mode statuses should match `umm::cast(..., dry_run)` because
that is what libumm fills — do not reimplement cast in the CLI.

## In scope

### Operand order

- Concept order is **property first**, optional file second:
  `umm map PROPERTY [FILE]`.
- This is not `Operands::file_then_args`. Extend the command table / args
  parser with the smallest seam that preserves `PROPERTY [FILE]` (for
  example a new `Operands` kind, or `Operands::args` with command-local
  parsing). Do not silently accept `umm map FILE PROPERTY`.
- `PROPERTY` is a registry id (`iptc.photo.creator`) or a cross-media
  accessor name (`locationCreated`, `creator`, …). Unknown → libumm
  `unknown_property` → semantics exit.
- With `FILE`, pass `umm::describe(property_id, path)`. Without `FILE`,
  `umm::describe(property_id)` (registry only). Read
  `include/umm/describe.hpp`.

### `--layers`

- Concept: `--layers representations,casts,cross-media`.
- Default: all layers the `PropertyMap` contains.
- Restrict **display** to the named layers. Do not drop data from the
  libumm call; filtering is output-only.
- Invalid layer name → usage exit.

### Output

- Human: readable sections for definition, representations (L1), cast rules
  (L2), cross-media partner (L3). A cross-media query prints both domain
  ids (`PropertyMap::properties`).
- `--json`: the map plus `schema_version` and `command`. Present libumm
  field names (`query`, `properties`, `layers`, `cross_media`, …). Do not
  invent a second schema.
- File mode: include values, consumed base entries, and cast-group statuses
  as libumm fills them.
- `--backend` applies only when `FILE` is present (read options for
  describe's file overload — if `describe` has no backend parameter, pass
  nothing extra and do not fake a pin). Read the header.

### Tests

- `umm map locationCreated --json` expands to photo `locationCreated` and
  video `locationShot` (or whatever the pin returns).
- `umm map iptc.photo.creator` has a cross-media partner when libumm says so.
- Unknown property → semantics exit.
- File mode on a generated fixture fills `value` / statuses without writing.
- `--layers representations` omits cast/cross-media sections in human output
  (JSON may still be easier to assert by presence/absence of keys you chose
  to filter).
- Completions/docs-gen still build from the updated table.

## Out of scope

- `umm cast` (session 17).
- Audience docs (session 19).
- Generating or vendoring `docs/user/properties/` from libumm. Link to
  libumm's property reference in session 19; do not copy the encyclopedia.
- Writing mappings, overlay, or cast JSON in this repo.

## Expected files

- `src/command_table.cpp`, `src/command_table.hpp` / `src/args.cpp` if a new
  operand kind is required
- `src/commands.cpp`
- Tests + goldens for `map` (registry-only and file mode)
- Do not hand-edit generated man/completions

## Acceptance

- `umm map PROPERTY` prints the map with no file I/O.
- `umm map PROPERTY FILE` fills values/statuses and does not write.
- Cross-media names expand to both domains.
- `--layers` filters display.
- `--json` includes `schema_version`.
- Unknown names use the semantics exit group.

## Validation

- CLI tests; skip file-mode backend cases only when that backend is absent.
- `git diff --check`
