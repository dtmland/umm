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

## Standing constraints

These apply to every session:

- **Thin CLI.** No metadata semantics, reconciliation, mapping, or backend
  logic in this repo. Call libumm; format and orchestrate.
- **Canonical vocabulary.** IPTC Photo, IPTC Video Metadata Hub, EXIF property
  ids from libumm, plus libumm's convenience accessors (`creator`, `gps`, …;
  concept §2.3). The CLI features **every** accessor libumm ships and
  resolves them through libumm (`MediaDomain`, typed accessors/registry in the
  pinned headers) — never a CLI-side copy of the accessor table. No ad-hoc tag
  names on the command line. Unmapped display
  only (`umm unmapped`); no unmapped write.
- **Error model.** No exceptions across the libumm boundary. Map `umm::Result`
  / `umm::ErrorCode` to the CLI exit-code contract (session 04). The CLI may
  use exceptions internally.
- **Pins.** `tools/build/libumm.env` is the source of truth for the libumm
  source archive (`UMM_LIBUMM_VERSION` + `UMM_LIBUMM_SHA256`). Offline contract
  tests guard pins and workflows.
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

## Sessions

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
| 15 | [User, sysadmin, and developer docs](15-documentation.md) | 5 Harden | 04–14 | not started |

When a session is finished, set its Progress to **complete** in this table
(values: `not started`, `in progress`, `complete`).

**Depends** lists prerequisite sessions (from each session's Prerequisites
section). A session may start once all its dependencies are complete; sessions
whose dependencies are met can run in parallel. After 02, sessions 03 and 04
can run in parallel; after 06, sessions 07 and 08 can; sessions 10 and 11 can
run alongside 09 once their dependencies are complete.

## Concept coverage

Every section of [`docs/concept.md`](../concept.md) is assigned. Do not drop
rows when splitting or merging work.

| Concept | Sessions |
|---|---|
| §1 Purpose (thin CLI, canonical model) | 00, all |
| §2.2 `read`, `get`, `version` | 06 |
| §2.2 `unmapped`, `conflicts`, `caps` | 07 |
| §2.2 `set`, `rm` | 08 |
| §2.2 `merge`, `sync` | 09 |
| §2.2 `geotag`, §2.4 GPS/timestamps | 10 (workflow), 08 (direct `gps`/`dateCreated` set) |
| §2.1 `read`/`get`/`set`/`rm` vocabulary (no `umm write`) | 04, 06, 08 |
| §2.3 convenience accessors + full ids (every libumm accessor) | 04 (resolver seam), 06, 08 |
| §2.5 struct / bag value syntax (`--json`) | 06, 08 |
| §2.2 `doctor`, `setup exiftool` | 11 |
| §2.6 `--json` | 05 (schema), 06–07, 11 (read-type) |
| §2.6 `--backend` | 04, 06–10 |
| §2.6 batch, globs, `--recursive`, no parallel writes | 04, 08–10 |
| §2.6 exit-code contract | 04, 15 |
| §2.6 no GUI / watch / database / asset management | 00 (non-goals) |
| §3.1 FetchContent pin, `libumm.env`, static link | 02 |
| §3.2 `find_package` / `UMM_CLI_USE_SYSTEM_LIBUMM` | 02, 03 |
| §3.3 version and standards reporting | 06 |
| §4.1 Exiv2-in, GPL-3.0 binary conveyance | 01 (LICENSE/NOTICE), 14 |
| §4.2 ExifTool never bundled; native setup scripts | 11 |
| §4.3 degraded modes | 07 (`caps`), 11 (`doctor`) |
| §5 skeleton (CMake, `src/`, `tests/`, `install/`, workflows) | 01, 02, 03, 11, 13, 14 |
| §5 conventions (pins, offline tests, 3-OS CI, no libumm exceptions, native scripts) | 00, 02, 03, 04, 11 |
| §6 non-goals | 00 |
| §7.1 TOML config paths | 05 |
| §7.2 JSON `schema_version` | 05 |
| §7.3 Windows ExifTool packaging | 11 |
| §7.4 completions and man pages from command table | 04 (table), 12 |

## How a session document is structured

Each session states: goal, concept citations, prerequisites, in scope, out of
scope, expected files, acceptance criteria, and validation. Out of scope is
binding — later sessions own that work.
