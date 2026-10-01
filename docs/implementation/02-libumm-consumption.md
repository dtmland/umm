# Session 02 — libumm consumption

Status: **complete**

## Goal

Consume libumm the way concept §3 specifies: checksum-pinned FetchContent by
default, optional `find_package(umm)` behind one CMake switch, static link,
no second Exiv2/Expat/zlib acquisition path.

## Concept

- §3.1 source consumption (default v1)
- §3.2 installed-package consumption
- §5 `tools/build/libumm.env`, CMake FetchContent / `find_package`
- §5 conventions: pins in env files; offline contract tests for pins

## Prerequisites

Session 01 complete.

## In scope

- `tools/build/libumm.env` pinning `UMM_LIBUMM_VERSION` and
  `UMM_LIBUMM_SHA256` for a **libumm release source archive**. Read the actual
  libumm release assets; do not invent a hash. A small `pins.sh` (or
  equivalent) that exports those variables for CI is fine if it stays a thin
  wrapper over the env file.
- CMake FetchContent: `FetchContent_Declare(libumm URL … URL_HASH SHA256=…)`
  + `FetchContent_MakeAvailable`, then
  `target_link_libraries(<umm-exe> PRIVATE umm::umm)`.
- `UMM_CLI_USE_SYSTEM_LIBUMM=ON` selects `find_package(umm CONFIG REQUIRED)`
  instead, guarded by libumm's version macros from `umm/version.hpp`. Read
  those headers; do not guess macro names.
- Static linkage by default. Exiv2/Expat/zlib must come from libumm's build
  unchanged — umm must not add a parallel FetchContent for them.
- Offline Python `unittest` contract tests under `tests/build/` that assert:
  the env file defines the two pin variables, CMake references the env/pin
  (not a free-floating URL), and `URL_HASH` is present. No network in those
  tests.
- The stub executable may call `umm::version()` to prove the link. Full
  `umm version` output (CLI version + standards table) is session 06.

## Out of scope

- CI matrix and the **CI job** that builds both consumption modes (session 03
  adds the jobs; this session must make both modes *possible*).
- Command-line interface (session 04+).
- Release corresponding-source tarballs (session 14).

## Expected files

- `tools/build/libumm.env` (and optional `tools/build/pins.sh`)
- CMake modules or `CMakeLists.txt` changes for FetchContent / find_package
- `tests/build/` pin contract tests

## Acceptance

- Default configure fetches pinned libumm and links `umm::umm` statically.
- `-DUMM_CLI_USE_SYSTEM_LIBUMM=ON` configures against an installed libumm
  (document the exact flag; keep this name unless a later dated decision
  changes it).
- Pin contract tests pass without network.
- No umm-side Exiv2/Expat/zlib download logic.

## Validation

- Default configure + build (network allowed here).
- `python3 -m unittest discover -s tests/build -v` (offline).
- Read the generated/build files enough to confirm libumm arrived via the pin,
  not via an unpinned git clone.
