# umm documentation

`umm` is the command-line media metadata tool built on
[libumm](https://github.com/dtmland/libumm). Guides are split by audience.
`umm COMMAND --help` and `umm(1)` are generated from the command table; these
pages explain workflows, config, and licensing without repeating libumm's
property encyclopedia.

## User

People running `umm`:

- [Getting started](user/getting-started.md) — install, `doctor`, first `read` / `get` / `set`
- [Command reference](user/commands.md) — every command and its flags
- [Examples](user/examples.md) — real command lines
- [Config](user/config.md) — TOML paths, `exiftool`, `UMM_EXIFTOOL`
- [Exit codes](user/exit-codes.md) — scripting contract
- [JSON schema](user/json-schema.md) — `--json` and `schema_version`

Canonical property semantics live in
[libumm user docs](https://github.com/dtmland/libumm/blob/main/docs/user/guide.md)
(see **Cross-media accessors** for the full accessor table).

## Sysadmin

Building, installing, deploying, and redistributing binaries:

- [Install](sysadmin/install.md) — CMake, ExifTool, licensing, release artifacts, completions
- [Release checklist](release-checklist.md) — tagging and draft GitHub releases

## Developer

Working on umm itself:

- [Contributing](developer/contributing.md) — architecture, adding a command, pins, tests
- [Concept](concept.md) — founding design (authoritative feature set; libumm v0.1.2 lift)
- [Implementation sessions](implementation/00-overview.md) — 01–15 historical (libumm 0.1.1); **16–19** align the CLI with libumm 0.1.2
- [Upstream libumm bugs](bug-upstream/README.md) — historical 0.1.0 notes; both fixed in libumm 0.1.1
- [Copilot instructions](../.github/copilot-instructions.md) — coding conventions

Do not delete session documents under `docs/implementation/` until a human
consolidates them. Active implementation follows `docs/concept.md` and
sessions 16–19. Later work that cannot be resolved from the concept goes
under `docs/analysis/` (when any exist).
