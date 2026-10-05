CLI unit and integration tests: `tests/cli/test_cli.cpp` (run via
`ctest --preset default`). Offline build-contract tests: `tests/build/`.

## Fixture strategy

This repo follows libumm's Tier A approach: **generate tiny JPEG/XMP (and copy
libumm's generated video/conflict fixtures when FetchContent provides them) at
test time**. No multi-megabyte binaries and no third-party media corpus (Tier B
is a v1 non-goal).

- Minimal JPEG bytes live in `test_cli.cpp` (`kMinimalJpeg`, plus a naive-EXIF
  JPEG for geotag `--offset`). Tests write them into a temp directory and stamp
  metadata through libumm (`umm::write` / `umm set`).
- Video, dump, conflicting, and paired-sidecar cases copy
  `tests/fixtures` from the pinned libumm source tree when
  `UMM_LIBUMM_FIXTURES` is defined. Those copies are skipped cleanly when the
  tree is absent (for example `UMM_CLI_USE_SYSTEM_LIBUMM`).
- `--backend` cases skip when that backend is unavailable.
- Sidecar pairing: `umm set --policy sidecar` writes a same-stem `.xmp`; on a
  case-sensitive filesystem the tests also rename to `.XMP` (libumm looks for
  `.xmp` then `.XMP`).
- Default tests are offline (no network).

## Golden outputs

Stable human and JSON stdout for `version`, `caps JPEG`, and `read`/`get` on a
generated still (plus `dumpunmapped`/`dumpall`/`conflicts` when libumm fixtures
exist) live in `tests/goldens/`. Comparisons:

- Normalize CRLF to LF.
- Substitute the fixture path with `FILE` (native, generic, and JSON-escaped
  forms, so Windows `\\` in JSON still collapses).
- Substitute CLI and libumm version strings with `VERSION`.
  `test_version_command` asserts `UMM_CLI_VERSION` from CMake
  `project(umm VERSION)`; do not hardcode `umm x.y.z` in CLI tests.
- Force `--backend exiv2` for read/get/dump/conflicts goldens so extra
  write-sync tags stay pinned. Caps goldens require both backends.

JSON goldens assert `"schema_version": 2`.

Regenerate after an intentional output change:

    UMM_REGENERATE_GOLDENS=1 ctest --preset default -R umm_cli_unit

Windows (cmd): `set UMM_REGENERATE_GOLDENS=1` then the same `ctest`. Keep the
files LF (`* text=auto eol=lf` in `.gitattributes`).

## What the suite covers

Read/get tests generate a tiny in-memory JPEG and write metadata through
libumm. Video cases copy libumm's generated `tests/fixtures` when FetchContent
provides them. Skip a `--backend` case when that backend is unavailable.
Inspect tests use `caps JPEG` plus libumm's `unknown-tags.jpg` /
`full-conflicting.jpg` when present (`dumpall` / `dumpunmapped` /
`conflicts`). Write tests cover set/get round-trip,
`--dry-run` (bytes, size, mtime), sidecar policy, batch I/O (one good file +
one missing file still processes the good file), `--recursive` on a small
tree, and cross-backend smoke (`set` with one backend, `get` with the other).
Table-driven get/set walks every libumm accessor on photo and video fixtures,
plus Location GPS `--json`. Reconcile tests cover
`merge --value` and `sync --dry-run`; paired sidecar `--use` uses libumm's
`paired.jpg` when present. Geotag tests generate a tiny GPX plus a still whose
`dateCreated` falls on the track; naive timestamps without `--offset` use the
semantics exit group. Doctor tests cover `--json` `schema_version` and
discovery. Setup tests drive the native script `--record` branch into a temp
config. Completions/`umm(1)` are generated from the command table; unit tests
check that every command name is a subset of the man page and each completion
script. Offline `tests/build/` guards pins, workflows, setup-script policy
strings, and generator/install rules.
