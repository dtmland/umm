# Session 05 — Config and output

Status: **complete** (config in `src/config.*`, formatters in `src/output.*`; schema draft in [json-schema.md](json-schema.md))

## Goal

User config (TOML, one `exiftool` path key) and stable output: human table by
default, `--json` with `schema_version` on every read-type command.

## Concept

- §2.6 `--json` on every read-type command; stable schema documented
  alongside the tool
- §4.2 discovery order: explicit config → `UMM_EXIFTOOL` → PATH; config is
  how `umm setup` records the binary (setup itself is session 11)
- §7.1 TOML locations and trivial parse into libumm `ExifToolConfig`
- §7.2 `schema_version` integer; breaking changes = CLI semver **major**;
  additive fields allowed within a major

## Prerequisites

Session 04 (flags and dispatch exist).

## In scope

### Config

- Search order for the config file (first existing file wins; document it):
  - `$XDG_CONFIG_HOME/umm/config.toml` (default XDG: `~/.config/umm/config.toml`)
  - `%APPDATA%\umm\config.toml` on Windows
  - `~/Library/Application Support/umm/config.toml` on macOS
- Format: TOML, human-editable, comments allowed.
- v1 keys: the ExifTool path (name it `exiftool` unless libumm's
  `ExifToolConfig` suggests a clearer 1:1 field — read the header).
- Parse **without a dependency-heavy TOML library** if a few-line reader
  suffices for comments + one string key. If a parser is added, justify it
  against existing deps first.
- Apply the path as libumm explicit config (discovery step 1). Do not mutate
  `PATH`. `UMM_EXIFTOOL` remains valid as step 2 (libumm's rule).
- Missing config file is not an error.

### Output

- Human table for read-type commands (property id, value; extra columns when
  flags request provenance / resolution).
- `--json`: one document per invocation (batch: an array or a top-level
  object with a file list — pick one, document it, keep it stable). Every
  document includes integer `"schema_version"` starting at `1`.
- Document the schema in a stub that session 15 will publish (for example
  `docs/user/json-schema.md` draft or `docs/implementation/json-schema.md`).
- Read-type commands that must honor `--json` once implemented: `read`,
  `get`, `unmapped`, `conflicts`, `caps`, `version`, `doctor`, and dry-run
  reports from write/sync/geotag (those commands land later; the formatter
  API must be ready).
- Do not print backend-specific tag names as the primary vocabulary.
- Struct-valued properties: compact summary in the human table, full JSON
  object/array with `--json` (concept §2.5). Formatter API must support both.

## Out of scope

- Writing the config from `umm setup exiftool` (session 11).
- Filling command bodies (sessions 06–11).
- Completions/man (session 12).

## Expected files

- Config load + apply-to-libumm
- Human and JSON formatters
- Schema draft document

## Acceptance

- Config path resolution matches §7.1 per OS.
- A config file with `exiftool = "…"` is honored as explicit libumm config.
- JSON output always contains `"schema_version"`.
- Schema draft states: breaking change → CLI major; additive OK within major.

## Validation

- Tests with a temp config file (and absence of config).
- Golden or substring tests for JSON `schema_version` using a stub command or
  `version` if session 06 has not landed — a tiny formatter unit test is
  enough.
