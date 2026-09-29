# Session 13 — Integration tests

Status: **not started**

## Goal

Harden the CLI test corpus: generated tiny fixtures, golden-output tests, and
the same fixture strategy libumm uses (Tier A in-repo). Earlier sessions'
tests stay; this session fills gaps and makes the strategy explicit.

## Concept

- §5 `tests/` — CLI integration tests against libumm's fixture strategy:
  generate tiny fixtures; golden-output tests
- §2.2 batch failures, `--json`, `--backend` verification workflows
- §6 no third-party product scope; still no committed proprietary media
  unless checksummed Tier B is later justified (not required for v1 CLI)

## Prerequisites

Commands from sessions 06–11 exist. This session may add tests those sessions
deferred.

## In scope

- Document the fixture strategy for *this* repo (short developer note is
  enough; session 15 can promote it): generate tiny JPEG/XMP (and whatever
  else libumm's generator supports) at test time or commit generator +
  tiny outputs. No multi-megabyte binaries.
- Golden tests for stable human and JSON output of `read`, `get`, `version`,
  `caps`, `unmapped`, `conflicts` on a frozen fixture. JSON comparisons
  should ignore volatile fields if any (none expected besides path
  normalization — define that).
- Batch tests: one good file + one missing file → non-zero, good file still
  processed.
- Cross-backend smoke: write with `--backend exiv2`, read with
  `--backend exiftool` (and the reverse) on a type both can write. Skip
  cleanly if a backend is missing.
- `--recursive` on a small directory tree.
- Write dry-run vs real write file-size/mtime or content hash.
- Windows path and `.xmp` pairing cases if not already covered (libumm
  pairing rules: same stem, `.xmp` then `.XMP` on case-sensitive FS).
- Keep using existing ctest; do not add a new test framework.

## Out of scope

- Tier B checksummed third-party corpus (libumm has one; CLI v1 does not
  need it unless a gap cannot be reproduced with generated files).
- Release packaging tests (session 14).

## Expected files

- `tests/` generator / fixtures / goldens
- Developer note on how to regenerate goldens

## Acceptance

- `ctest` on a default CI image covers read, write, batch, json schema
  version, and dry-run.
- Goldens are LF-normalized.
- No network required for the default test set.

## Validation

- Full `ctest` on at least one OS locally in the session; CI remains the
  three-OS bar.
- `git diff --check`.
