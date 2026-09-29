# Session 01 — Project skeleton

Status: **not started**

## Goal

Seed the repository layout from concept §5 so later sessions have a C++20
CMake project, license files, and empty trees to fill. No libumm fetch and no
CLI commands yet.

## Concept

- §4.1 (Apache-2.0 source; GPL-3.0 note for future binary releases)
- §5 project skeleton (directories, `LICENSE`, `NOTICE.md`)
- §6 non-goals (do not add extra product surface)

## Prerequisites

None.

## In scope

- `CMakeLists.txt`: C++20, `CXX_STANDARD_REQUIRED`, extensions off, project
  version `0.1.0`. A `umm` (or `umm-cli`) executable target from `src/` that
  links nothing extra yet. CMake presets consistent with a three-OS build.
- Directory layout: `src/`, `tests/`, `tools/build/`, `install/`, `docs/`,
  `.github/workflows/` (workflows themselves are session 03).
- `LICENSE` — Apache-2.0 for this repository's source.
- `NOTICE.md` — project name, copyright, Apache-2.0 pointer, and a short note
  that **binary releases containing Exiv2 will be conveyed under GPL-3.0**
  (full notices and corresponding source are session 14).
- `.gitignore` for CMake build trees, IDE junk, and FetchContent/cache dirs.
- `.gitattributes` with `* text=auto eol=lf` (or equivalent) so generated and
  contract-tested files stay byte-stable on Windows checkouts.
- Placeholder `src/main.cpp` that can be compiled (for example a stub
  `main` returning 0). Do not implement commands.

## Out of scope

- FetchContent / `find_package` for libumm (session 02).
- GitHub Actions (session 03).
- Argument parsing, command table (session 04).
- `THIRD-PARTY-NOTICES.md` contents and vendored license texts (session 14).
- Setup scripts under `install/` (session 11).

## Expected files

- `CMakeLists.txt`, `CMakePresets.json` (if used)
- `src/main.cpp`
- `LICENSE`, `NOTICE.md`
- `.gitignore`, `.gitattributes`
- Empty or README-only `tests/`, `tools/build/`, `install/` so the tree matches
  §5

## Acceptance

- `cmake --preset default` (or the documented equivalent) configures and
  builds a `umm` binary on at least one OS.
- Source license is Apache-2.0; NOTICE mentions future GPL-3.0 binary
  conveyance.
- Layout matches concept §5 at the top level.

## Validation

- Configure and build the stub.
- `git diff --check`.
