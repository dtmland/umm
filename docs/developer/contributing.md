# Contributing

## Architecture

`umm` is a **thin CLI** on libumm. Argument parsing, output formatting, batch
orchestration, and environment setup live here. Metadata semantics,
reconciliation, mapping, and backend behavior do not.

| Piece | Role |
|---|---|
| `src/command_table.cpp` | Single source of truth for commands, synopses, flags |
| `src/args.cpp` | Parse argv; never calls libumm |
| `src/commands.cpp` | Dispatch; pass `ReadOptions` / `WriteOptions` / `SyncOptions` / `MatchOptions` / `CastOptions` through |
| `src/property.cpp` | Accessor vs full-id seam; binds public libumm `Metadata` getters |
| `src/output.cpp` | Human table and JSON (`schema_version`) |
| `src/config.cpp` | TOML `exiftool` key; discovery order |
| `src/errors.cpp` | `umm::ErrorCode` groups → process exit codes |
| `src/batch.cpp` | Sequential file loop; no parallel I/O in v1 |
| `tools/gen/` | `umm-gen-docs` emits `umm(1)` and bash/zsh/fish completions |

Public libumm APIs return `umm::Result<T>`. Do not catch exceptions from
libumm (there are none on the public API). The CLI may use exceptions
internally.

The command-line vocabulary is IPTC Photo, IPTC Video Metadata Hub, EXIF,
and libumm convenience accessors. Base *display* is `umm dumpall` /
`umm dumpunmapped`; writing new base keys is absent. Camera GPS lives on
Location structs (`locationCreated` / `locationShot`), not a `gps` accessor.

## Design records

- [`docs/concept.md`](../concept.md) — authoritative feature set
- [`docs/implementation/`](../implementation/) — session-sized plan. 01–15
  built the 0.1.1 CLI (historical). 16–19 align with libumm 0.1.2. Keep the
  files until a human consolidates them.
- `docs/analysis/` — dated decisions that cannot be resolved from the concept
  (create a record there rather than silently overriding the concept)
- [`docs/bug-upstream/`](../bug-upstream/) — historical libumm 0.1.0 defects
  (fixed in libumm 0.1.1; current pin is 0.1.2); do not implement backend
  selection or ExifTool process logic in this CLI to work around them
- [`.github/copilot-instructions.md`](../../.github/copilot-instructions.md)
  — coding conventions (link, do not fork)

## How to add a command

1. **Table first.** Register the command, synopsis, operands, and flags in
   `commands()` (`src/command_table.cpp`). Completions and `umm(1)` pick this
   up at build time.
2. **Module.** Implement the body in `src/commands.cpp` (or a focused
   neighbor). Call libumm; format results. Map errors with `exit_code_for`.
3. **Tests.** Extend `tests/cli/test_cli.cpp` with generated fixtures (see
   [`tests/README.md`](../../tests/README.md)). Offline pin/workflow checks
   stay in `tests/build/`.
4. **Docs.** Update [user commands](../user/commands.md) and examples in the
   same change if the CLI contract moved (flags, exit codes, JSON, config
   keys). Do not hand-edit generated man/completions.

## Pins and contract tests

`tools/build/libumm.env` is the source of truth for the libumm source
archive. CMake FetchContent uses that URL and SHA-256. Offline:

```sh
python3 -m unittest discover -s tests/build -v
```

Those tests guard pins, CI/release workflows, setup-script policy strings,
and generator/install rules. Do not add a second Exiv2/Expat/zlib download
path.

## Version and release

`project(umm VERSION x.y.z)` in `CMakeLists.txt` is the **only** umm CLI
version. CMake sets `UMM_CLI_VERSION` from it. Do not search-replace the old
number: the libumm pin, historical docs, and packaging-test dummy versions
are independent (they can coincide with the CLI version by accident). See
[`docs/release-checklist.md`](../release-checklist.md). After a green CI
bump, tag `vX.Y.Z` matching CMake exactly.

Default consume-from-source; `UMM_CLI_USE_SYSTEM_LIBUMM=ON` uses
`find_package(umm CONFIG REQUIRED)`.

## Fixtures and goldens

Generate tiny JPEG/XMP at test time. Copy libumm's generated video/conflict
fixtures when FetchContent provides `tests/fixtures`. No multi-megabyte
media corpus.

Goldens in `tests/goldens/` are LF. Comparisons substitute fixture paths
with `FILE` (native, generic, and JSON-escaped forms) and CLI/libumm
versions with `VERSION`. Regenerate after an intentional output change:

```sh
UMM_REGENERATE_GOLDENS=1 ctest --preset default -R umm_cli_unit
```

JSON goldens still assert `"schema_version": 2`.

## Build and CI bar

```sh
cmake --preset default -DUMM_REQUIRE_EXIV2=ON -DUMM_REQUIRE_EXIFTOOL=ON
cmake --build --preset default
ctest --preset default
```

Three-OS CI (Linux, Windows, macOS) with both libumm backends required is
the project bar; do not weaken it. On Windows, `umm-cli-core` defines
`NOMINMAX` and `WIN32_LEAN_AND_MEAN`.

Setup scripts are native `.bat` / PowerShell and POSIX `sh` only — never a
Python helper. ExifTool is located at runtime and never bundled.

When a change affects the CLI contract, update the relevant user docs in the
same patch. Prefer the smallest change that matches `docs/concept.md`.
