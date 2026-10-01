# Session 04 — CLI framework

Status: **complete** (see [exit-codes.md](exit-codes.md); code in `src/`, tests in `tests/cli/`)

## Goal

Land argument parsing, the command table (single source of truth), global
flags, batch orchestration, and the exit-code contract. Commands may still be
stubs that return a clear "not implemented" until their session.

## Concept

- §1 (CLI contributes parsing, formatting, batch, environment — no metadata
  logic)
- §2.2 command list (register every command now; implement later)
- §2.6 `--backend`, batch / globs / `--recursive`, no parallel writes,
  exit-code contract from `umm::ErrorCode` groups
- §5 `src/` command modules
- §5 no exceptions across the libumm boundary
- §7.4 command table is the source of truth for later completions and man pages

## Prerequisites

Sessions 01–02 (executable linked to libumm). Session 03 may land in parallel
after 02.

## In scope

- A **property-addressing seam**: one function used by `get`/`set`/`rm` that
  takes a CLI name (convenience accessor such as `creator`, or full id such as
  `iptc.photo.creator`) plus the file's `MediaDomain` and returns what libumm
  says it is. It delegates to libumm's accessor/registry API in the pinned
  headers (concept §2.3); it must not hardcode the accessor table. Unknown
  names map to `unknown_property`. Stub it here, fill in with sessions 06/08.

- A **command table** in source (data, not scattered `if`/`else` strings)
  listing every concept §2.2 command: `read`, `get`, `set`, `rm`, `unmapped`,
  `conflicts`, `merge`, `sync`, `caps`, `geotag`, `doctor`, `setup`,
  `version`. Include usage synopsis and which flags apply. Completions and
  `umm(1)` (session 12) must be generated from this table later — design it
  that way now.
- Argument parsing for:
  - Subcommand dispatch
  - Global `--backend exiv2|exiftool` (pass through to
    `ReadOptions`/`WriteOptions` when a command runs; stubs may ignore)
  - Batch: file operands, shell-expanded globs as operands, `--recursive`
  - `--json` recognized globally or on read-type commands (formatting is
    session 05; parsing the flag belongs here)
- Batch driver: iterate files sequentially. **No parallel writes** (and no
  parallel reads in v1 either — keep the loop simple). On per-file failure,
  record it, continue, and exit non-zero summarizing failures.
- Exit-code contract mapped from `umm::ErrorCode` **groups**. Read
  `include/umm/result.hpp` in the pinned libumm. Document the mapping in
  `docs/implementation/` or a stub `docs/` note that session 15 will promote
  to user docs. Typical grouping (confirm against the header, do not invent
  extra codes):

  | Exit | Meaning |
  |---|---|
  | 0 | success |
  | 1 | usage / unknown command / bad arguments |
  | 2 | I/O (`io_*`) |
  | 3 | format (`format_*`) |
  | 4 | backend (`backend_*`) |
  | 5 | capability (`unsupported_*`) |
  | 6 | semantics (`conflict_unresolved`, `invalid_value`, `unknown_property`) |
  | 64 | mixed per-file failures in a batch (or the first failure's group — pick one, document it, stick to it) |
  | 70 | `internal` |

  `umm get` absent-property is a non-zero exit (session 06); reserve a
  documented code (semantics group or a dedicated "not found" code) here so
  later sessions do not collide.
- Convert libumm `Error` to that contract at the libumm boundary; do not
  catch exceptions from libumm (there are none on the public API).
- `umm --help` / `umm <cmd> --help` from the command table.
- Unknown commands and missing operands → usage exit.

## Out of scope

- Human/JSON formatters (session 05).
- Real command bodies (sessions 06–11).
- Config file (session 05).
- Generating completion scripts and man pages (session 12).

## Expected files

- `src/` modules: `main.cpp` dispatch, command table, args, batch, errors
- A short exit-code note (path recorded so session 15 can move it)

## Acceptance

- `umm`, `umm --help`, and `umm not-a-command` behave as a real CLI.
- Every §2.2 command name is dispatched (stub is acceptable).
- `--backend`, `--recursive`, `--json` parse without crashing.
- Batch over several missing files exits non-zero and reports each failure.
- Exit-code mapping is written down and implemented for usage + a forced
  libumm-style error if easy; remaining codes wired as commands land.

## Validation

- Unit or CLI tests for help, unknown command, and exit codes — smallest
  possible, no new test harness unless CMake/ctest is already there.
- `git diff --check`.
