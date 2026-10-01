CLI unit tests: `tests/cli/test_cli.cpp` (run via `ctest --preset default`). Offline build-contract tests: `tests/build/`.

Read/get tests generate a tiny in-memory JPEG and write metadata through libumm. Video cases copy libumm's generated `tests/fixtures` when FetchContent provides them. Skip a `--backend` case when that backend is unavailable. Inspect tests use `caps JPEG` plus libumm's `unknown-tags.jpg` / `full-conflicting.jpg` when present.
