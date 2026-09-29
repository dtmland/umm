# Session 15 — User, sysadmin, and developer docs

Status: **not started**

## Goal

Audience-split documentation, a real README, and a getting-started path with
examples. Everything a CLI project needs without extra manuals that duplicate
libumm's metadata semantics.

## Concept

- §1–§2 (what the tool is, every command, cross-cutting flags)
- §3.3 version/standards reporting (point users at `umm version`)
- §4 doctor/setup, degraded modes, GPL binary conveyance
- §5 `docs/` — exit codes, JSON schema, doctor/setup guides
- §6 non-goals (state them so issues can be closed)
- §7 config paths, JSON schema_version policy, completions/man

## Prerequisites

Sessions 04–14 should have landed the behavior this session describes. If a
prior session's stub doc exists (exit codes, JSON schema), **move or rewrite**
it into the audience trees below rather than leaving two sources of truth.

## In scope

### Layout

- `docs/README.md` — index by audience (replace the planning-time index)
- `docs/user/` — people running `umm`
- `docs/sysadmin/` — building, installing, deploying, licensing of binaries
- `docs/developer/` — working on umm itself
- Root `README.md` — short, example-heavy, links into `docs/`

Do not copy libumm's property encyclopedia. Link to libumm user docs for
canonical property semantics.

### User docs

- Getting started: install a release binary (or build), `umm doctor`,
  `umm setup exiftool` if needed, `umm version`, first `umm read` / `umm get`
  / `umm set`.
- Command reference: every §2.1 command with flags from the command table
  (keep aligned with `umm(1)`; do not contradict).
- Examples (real command lines, generated-fixture style files in prose):
  - read + `--sources` + `--json`
  - get for scripting
  - set / rm with `--policy` and `--dry-run`
  - conflicts → merge → sync
  - geotag with `--track` and `--offset`
  - caps
  - batch `--recursive`
  - write with one `--backend`, read with the other
- Config file: TOML paths (§7.1), `exiftool` key, `UMM_EXIFTOOL`.
- Exit-code contract (session 04 mapping).
- JSON schema and `schema_version` policy (§7.2).
- Unmapped display vs no unmapped write.
- Non-goals in one short subsection.

### Sysadmin docs

- Build from source (CMake presets, C++20, FetchContent pin).
- `UMM_CLI_USE_SYSTEM_LIBUMM`.
- Installing ExifTool via `umm setup exiftool` and platform notes (winget /
  apt / dnf / pacman / brew, checksum fallbacks).
- `umm doctor` as the support first step.
- Redistribution: Apache-2.0 source; **GPL-3.0 binary** when Exiv2 is
  linked; corresponding source attachments; ExifTool not redistributed.
- CI/release artifact names, `SHA256SUMS`.
- Completions and man page install locations.

### Developer docs

- Architecture: thin CLI, command table, no metadata logic, libumm boundary
  (`Result`, no exceptions).
- Pointer to `docs/concept.md` and `docs/implementation/` (history of how
  it was built; when sessions complete, a short implementation-history
  summary may replace the status table later — do not delete session docs
  until a human consolidates them).
- How to add a command (table first, then module, then tests, then docs/man).
- Pins, offline contract tests, fixture/golden regeneration.
- Coding conventions already in `.github/copilot-instructions.md` (link, do
  not fork).

### Root README

- One-paragraph purpose (canonical-model CLI on libumm).
- Install (release archive + `umm setup exiftool`).
- Quick start examples (copy a subset of user getting-started).
- License one-liner (source Apache-2.0; binaries with Exiv2 GPL-3.0).
- Links: user guide, sysadmin, contributing/developer, concept.

Keep the README under ~100–120 lines. Details live in `docs/`.

## Out of scope

- Re-implementing libumm's reconciliation policy or property registry in
  umm docs.
- Package-manager formula files (v1 non-goal).
- Marketing site.

## Expected files

- `README.md` (replace the stub)
- `docs/README.md`
- `docs/user/getting-started.md`, `docs/user/commands.md`,
  `docs/user/examples.md`, `docs/user/exit-codes.md`,
  `docs/user/json-schema.md`, `docs/user/config.md` (split/combine if a
  single `docs/user/guide.md` stays readable — **do not omit topics**)
- `docs/sysadmin/install.md` (build, ExifTool, licensing, releases)
- `docs/developer/contributing.md` (and architecture or a section therein)
- Optional `docs/release-checklist.md` if session 14 left a stub

## Acceptance

- Every §2.1 command appears in user docs with an example.
- Exit codes, JSON `schema_version`, config paths, doctor/setup, GPL binary
  note, and non-goals are written down.
- README runs a new user through doctor → read → get → set without
  requiring other files, then links deeper.
- Docs index lists user / sysadmin / developer.
- No second conflicting flag list vs the command table / man page.

## Validation

- Read each new doc end-to-end; grep docs for each command name.
- Confirm examples use canonical property ids (`iptc.photo.*`, `exif.gps.*`,
  `iptc.video.*`), not ExifTool tag names.
- `git diff --check`.
