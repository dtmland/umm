# Session 12 — Completions and man pages

Status: **not started**

## Goal

Generate bash/zsh/fish completions and `umm(1)` from the command table at
build time. Ship them in later release archives (session 14).

## Concept

- §7.4 generate both from the command table at build time (single source of
  truth); ship bash/zsh/fish completions and `umm(1)` in release archives
- §2.2 / session 04 command table

## Prerequisites

Session 04 (command table). Prefer after sessions 06–11 so the table's flags
are complete; if the table is already exhaustive from 04+later edits, this
can follow 11.

## In scope

- Build-time generator that reads the **same** command table the CLI uses
  (not a second handwritten list). If generation is a small C++ tool or a
  script that emits from a checked-in table dump, keep it boring.
- Outputs:
  - bash completion
  - zsh completion
  - fish completion
  - `umm(1)` man page (section 1)
- Completions offer the convenience accessor names (and full-id prefixes
  `iptc.photo.`, `iptc.video.`, `exif.`) for `get`/`set`/`rm`, generated from
  libumm's accessor list at build time, not a hand-maintained copy.
- CMake install rules for completions and the man page (standard locations).
- Completions cover subcommands and the flags in the table (`--json`,
  `--backend`, `--recursive`, `--policy`, `--dry-run`, `--sources`,
  `--fail-on-conflict`, `--direction`, `--track`, `--offset`, `--use`,
  `--value`, …).
- Man page: name, synopsis, description (thin CLI on libumm), command
  summaries, global flags, exit codes (pointer is enough if the full table
  lives in user docs), see also (`exiftool`, `exiv2` as external tools — not
  as if umm bundled them).

## Out of scope

- Release archive layout (session 14 must include these artifacts).
- Full user manual (session 15). The man page is a summary, not the guide.

## Expected files

- Generator + CMake wiring
- Generated artifacts: either committed (libumm style for byte-stable
  contract tests) or produced in the build dir with a contract that the
  generator runs in CI. Pick one; if generated files are committed, pin LF
  in `.gitattributes`.

## Acceptance

- `umm --help` command list matches the man page NAME/SYNOPSIS commands.
- Completions mention every subcommand in the table.
- CI builds the generator (or verifies committed outputs).

## Validation

- Contract test: command names in the table ⊆ man page and ⊆ each completion
  script.
- `git diff --check`.
