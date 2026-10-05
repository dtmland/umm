# umm implementation plan — overview

This directory is the session-sized execution plan for the `umm` CLI. The
founding design is [`docs/concept.md`](../concept.md). Do not invent a parallel
feature set.

Point each Copilot (or human) implementation session at:

1. [`.github/copilot-instructions.md`](../../.github/copilot-instructions.md)
2. This overview
3. **One** session document below
4. The concept sections that session cites

Implement the assigned session only. If a listed prerequisite is not done,
stop.

There are two series:

- **Sessions 01–15** built the CLI against **libumm 0.1.1**. They are
  **complete** and historical. Do not reopen them to land 0.1.2 work.
- **Sessions 16–19** align the existing tree with **libumm 0.1.2** and the
  lifted concept. That is the active plan.

## Standing constraints

These apply to every session:

- **Thin CLI.** No metadata semantics, reconciliation, mapping, casting, or
  backend logic in this repo. Call libumm; format and orchestrate.
- **Canonical vocabulary.** IPTC Photo, IPTC Video Metadata Hub, and EXIF
  property ids from libumm, plus libumm's convenience accessors (`creator`,
  `locationCreated`, …; concept §2.3). The CLI features **every** accessor
  libumm ships and resolves them through libumm (`MediaDomain`, typed
  accessors/registry in the pinned headers) — never a CLI-side copy of the
  accessor table. No ad-hoc tag names on the command line. Base *display* is
  `umm dumpall` / `umm dumpunmapped`; writing new base keys is absent. There
  is no `umm unmapped` command and no `gps` accessor / `exif.gps.position`.
- **Error model.** No exceptions across the libumm boundary. Map `umm::Result`
  / `umm::ErrorCode` to the CLI exit-code contract (session 04, published in
  `docs/user/exit-codes.md`). The CLI may use exceptions internally.
- **Pins.** `tools/build/libumm.env` is the source of truth for the libumm
  source archive (`UMM_LIBUMM_VERSION` + `UMM_LIBUMM_SHA256`). Offline contract
  tests guard pins and workflows. Session 16 moves the pin from 0.1.1 to
  0.1.2; do not invent a SHA-256.
- **Linkage.** Static libumm by default. Exiv2/Expat/zlib come from *inside*
  the libumm build; umm adds no second acquisition path.
- **Backends.** Both required in CI. Exiv2 is compiled in (users never install
  it). ExifTool is out-of-process, never bundled, discovered at runtime
  (explicit config → `UMM_EXIFTOOL` → PATH).
- **License.** Apache-2.0 for umm source. Binary releases that contain Exiv2
  are conveyed under GPL-3.0 (concept §4.1).
- **Writes.** Per-file atomic via libumm. **No parallel writes in v1.**
- **Setup scripts.** Native `.bat`/PowerShell and POSIX `sh` only — never a
  Python helper.
- **Command table.** Single source of truth for the CLI surface, completions,
  and `umm(1)`.
- **v1 non-goals** (concept §6 and §2.6): no GUI, watch mode, database, asset
  management, thumbnailing, transcoding, image processing, long-running
  daemon, or package-manager publication of umm itself.

New design decisions that cannot be resolved from the concept go under
`docs/analysis/` as dated records. Do not silently override the concept.

When the concept's abbreviated accessor table disagrees with pinned
`include/umm/metadata.hpp`, the headers win (concept §2.3).

## libumm 0.1.2 delta (why sessions 16–19 exist)

libumm 0.1.2 is a pre-1.0 source break. The CLI cannot stay on 0.1.1 APIs.
Read libumm's `docs/developer/release-notes.md` and the pinned headers; do
not invent field names. The user-visible delta versus the 0.1.1 CLI:

| Area | 0.1.1 CLI (sessions 01–15) | 0.1.2 concept / library |
|---|---|---|
| Pin | `tools/build/libumm.env` → 0.1.1 | 0.1.2 release source archive |
| Base views | `umm unmapped` → `Metadata::unmapped()` | `umm dumpall` / `umm dumpunmapped` → `dumpAll()` / `dumpUnmapped()` |
| Provenance | `SourceRef::raw_key` | `SourceRef::base_key` |
| Types | `UnmappedKey` / `UnmappedEntry` | `BaseKey` / `BaseEntry` |
| GPS | accessor `gps`, id `exif.gps.position` | Location struct GPS: `locationCreated[0]` (photo) / `locationShot[0]` (video) |
| Geotag | write `exif.gps.position` | write Location GPS through `umm::write` |
| Merge | `--use RAWKEY` | `--use BASEKEY` |
| Casts | absent | `umm cast`; `umm read --report-casts` |
| Property map | absent | `umm map` → `umm::describe` |
| JSON | `schema_version` 1; `unmapped`; `raw_key` | bump `schema_version`; dump commands; `base_key` |
| Rating / accessors | `rating` treated photo-only; `gps` bound | every accessor the pin ships; no `gps` |

Pass-through options already used: `ReadOptions` / `WriteOptions` /
`SyncOptions` / `MatchOptions`. Session 17 adds `CastOptions`. Session 18
adds `umm::describe`.

## Sessions

### Series A — v0.1.1 CLI (complete, historical)

| # | Document | Stage | Depends | Progress |
|---|---|---|---|---|
| 01 | [Project skeleton](01-project-skeleton.md) | 0 Foundation | — | complete |
| 02 | [libumm consumption](02-libumm-consumption.md) | 0 Foundation | 01 | complete |
| 03 | [CI foundation](03-ci-foundation.md) | 0 Foundation | 02 | complete |
| 04 | [CLI framework](04-cli-framework.md) | 1 Plumbing | 01, 02 | complete |
| 05 | [Config and output](05-config-and-output.md) | 1 Plumbing | 04 | complete |
| 06 | [Read commands](06-read-commands.md) | 2 Read | 04, 05 | complete |
| 07 | [Inspect commands](07-inspect-commands.md) | 2 Read | 06 | complete |
| 08 | [Write commands](08-write-commands.md) | 3 Write | 04, 05, 06 | complete |
| 09 | [Reconcile commands](09-reconcile-commands.md) | 3 Write | 07, 08 | complete |
| 10 | [Geotag](10-geotag.md) | 3 Write | 08 | complete |
| 11 | [Doctor and setup](11-doctor-and-setup.md) | 4 Tooling | 05, 07 | complete |
| 12 | [Completions and man pages](12-completions-and-man.md) | 4 Tooling | 04 (after 06–11) | complete |
| 13 | [Integration tests](13-integration-tests.md) | 5 Harden | 06–11 | complete |
| 14 | [Release engineering](14-release-engineering.md) | 5 Harden | 03, 12 | complete |
| 15 | [User, sysadmin, and developer docs](15-documentation.md) | 5 Harden | 04–14 | complete |

Do not delete these files until a human consolidates them.

### Series B — libumm 0.1.2 alignment (active)

| # | Document | Stage | Depends | Progress |
|---|---|---|---|---|
| 16 | [Pin and source-break migration](16-pin-and-source-break.md) | 6 Align | 01–15 | not started |
| 17 | [Cast and `--report-casts`](17-cast.md) | 7 New APIs | 16 | not started |
| 18 | [Map (`umm::describe`)](18-map.md) | 7 New APIs | 16 | not started |
| 19 | [Contract docs and completions](19-docs-and-contract.md) | 8 Contract | 16–18 | not started |

When a session is finished, set its Progress to **complete** in this table
(values: `not started`, `in progress`, `complete`).

**Depends** lists prerequisite sessions. Sessions 17 and 18 may run in
parallel after 16. Session 19 starts only when 16–18 are complete.

## Concept coverage

Every section of [`docs/concept.md`](../concept.md) (libumm 0.1.2 lift) is
assigned to series B. Series A covered the previous concept; do not drop
rows from that historical map, and do not use it as the 0.1.2 assignment.

| Concept | Sessions |
|---|---|
| §1 Purpose (thin CLI, canonical model) | 00, all |
| §2.1 `read`/`get`/`set`/`rm` vocabulary (no `umm write`) | 04 (historical), 16 (keep) |
| §2.2 `read` (`--sources`, `--report-casts`), `get`, `version` | 16 (existing), 17 (`--report-casts`) |
| §2.2 `cast` | 17 |
| §2.2 `set`, `rm` | 16 (GPS/accessor seam) |
| §2.2 `dumpall`, `dumpunmapped` | 16 |
| §2.2 `conflicts`, `caps` | 16 (JSON/provenance field names) |
| §2.2 `merge` (`--use BASEKEY`), `sync` | 16 |
| §2.2 `map` | 18 |
| §2.2 `geotag`, §2.4 GPS/timestamps | 16 |
| §2.2 `doctor`, `setup exiftool` | 11 (historical; 16 only if headers force a compile fix) |
| §2.3 convenience accessors + full ids (every libumm accessor; no `gps`) | 16 |
| §2.5 struct / bag value syntax (`--json`) | 16 (Location GPS JSON) |
| §2.6 `--json` | 16 (schema bump), 17–18 (new commands) |
| §2.6 `--backend` | 16–18 |
| §2.6 batch, globs, `--recursive`, no parallel writes | 16–18 |
| §2.6 exit-code contract | 04, 16 (no new codes unless libumm adds a group) |
| §2.6 / §2.7 workflows | 19 |
| §3.1 FetchContent pin, `libumm.env`, static link | 16 |
| §3.2 `find_package` / `UMM_CLI_USE_SYSTEM_LIBUMM` | 02, 03 (historical), 16 (pin still works) |
| §3.3 version and standards reporting | 16 (CLI 0.2.0) |
| §4.1 Exiv2-in, GPL-3.0 binary conveyance | 16 (corresponding-source pin copy) |
| §4.2 ExifTool never bundled; native setup scripts | 11 (historical) |
| §4.3 degraded modes | 07 / 11 (historical) |
| §5 skeleton and conventions | 00, 16 |
| §6 non-goals (`dumpall` / `dumpunmapped`; no base write) | 00, 16, 19 |
| §7.1 TOML config paths | 05 (historical) |
| §7.2 JSON `schema_version` | 16 (bump), 19 (publish) |
| §7.3 Windows ExifTool packaging | 11 (historical) |
| §7.4 completions and man pages from command table | 16–18 (table), 19 (verify + user docs) |

## How a session document is structured

Each session states: goal, concept citations, prerequisites, in scope, out of
scope, expected files, acceptance criteria, and validation. Out of scope is
binding — later sessions own that work.
