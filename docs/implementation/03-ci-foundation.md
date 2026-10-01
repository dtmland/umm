# Session 03 — CI foundation

Status: **complete** (workflow contract-tested; run 36881295234 on `main` green on Linux, Windows, macOS, and the system-libumm job)

## Goal

Three-OS CI with both libumm backends required, plus a second consumption-mode
build so `find_package` package files stay honest. Copy structure from
libumm's workflows; adapt names and pins to this repo.

## Concept

- §3.2 CI builds both FetchContent and system-libumm modes
- §5 `.github/workflows/` — 3-OS CI copied from libumm
- §5 conventions: offline contract tests for workflows; both backends required

## Prerequisites

Session 02 complete (pins and both CMake modes exist).

## In scope

- `.github/workflows/ci.yml` (name may vary) with `fail-fast: false` matrix:
  Linux, Windows, macOS (current GitHub-hosted images; match libumm's current
  trio unless those images are gone — read libumm's workflow, do not guess
  stale runner tags).
- Permissions `contents: read`. Concurrency group per ref; cancel in-progress
  PR runs.
- Steps, in order of intent:
  1. Checkout
  2. Offline build-contract tests (`python3 -m unittest discover -s tests/build`)
  3. Load pins from `tools/build/libumm.env`
  4. Acquire whatever the **libumm** build needs on that OS (packages, Ninja,
     MSVC env, ExifTool/Perl as libumm's CI does). umm does not invent a
     second backend story; follow libumm's current CI so both backends are
     actually present.
  5. Configure with backends required
  6. Build and test
- A **second** job or matrix axis that builds with
  `UMM_CLI_USE_SYSTEM_LIBUMM=ON` against a libumm prefix produced in that run
  (or a documented equivalent) so the package path does not rot.
- Offline workflow contract tests: the CI file includes the three OSes, runs
  contract tests before configure, and does not drop `contents: read`.
- Cache FetchContent/backend downloads where libumm does, keyed on pin values.

## Out of scope

- Tag-triggered release workflow, notices, corresponding source (session 14).
- Installing ExifTool via `umm setup` (session 11). CI may install ExifTool
  the same way libumm CI does so tests can run.
- Expanding test fixtures (session 13).

## Expected files

- `.github/workflows/ci.yml`
- `tests/build/` workflow contract tests (extend session 02 tests)

## Acceptance

- PR/push CI runs on Linux, Windows, and macOS.
- Default jobs require both backends.
- At least one job exercises `UMM_CLI_USE_SYSTEM_LIBUMM=ON`.
- Workflow contract tests fail if an OS is removed from the matrix or if
  contract tests are dropped from the workflow.

## Validation

- Offline `python3 -m unittest discover -s tests/build -v`.
- After merge/push, confirm the workflow file is valid YAML and matches the
  contract tests. Do not claim CI is green without reading a run.
