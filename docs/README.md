# umm documentation

`umm` is the command-line media metadata tool built on [libumm](https://github.com/dtmland/libumm).
Implementation is in progress. Until session 15 lands audience guides, use
the documents below.

## Founding design

- [Concept](concept.md) — feature set, libumm consumption, backends, skeleton,
  non-goals, and resolved planning questions. Authoritative.

## Implementation sessions

Point each Copilot session at [implementation/00-overview.md](implementation/00-overview.md)
plus **one** session file:

| # | Session |
|---|---|
| 01 | [Project skeleton](implementation/01-project-skeleton.md) |
| 02 | [libumm consumption](implementation/02-libumm-consumption.md) |
| 03 | [CI foundation](implementation/03-ci-foundation.md) |
| 04 | [CLI framework](implementation/04-cli-framework.md) |
| 05 | [Config and output](implementation/05-config-and-output.md) |
| 06 | [Read commands](implementation/06-read-commands.md) |
| 07 | [Inspect commands](implementation/07-inspect-commands.md) |
| 08 | [Write commands](implementation/08-write-commands.md) |
| 09 | [Reconcile commands](implementation/09-reconcile-commands.md) |
| 10 | [Geotag](implementation/10-geotag.md) |
| 11 | [Doctor and setup](implementation/11-doctor-and-setup.md) |
| 12 | [Completions and man pages](implementation/12-completions-and-man.md) |
| 13 | [Integration tests](implementation/13-integration-tests.md) |
| 14 | [Release engineering](implementation/14-release-engineering.md) |
| 15 | [User, sysadmin, and developer docs](implementation/15-documentation.md) |

Agent rules: [`.github/copilot-instructions.md`](../.github/copilot-instructions.md).

## Audience guides (after session 15)

Planned trees: `docs/user/`, `docs/sysadmin/`, `docs/developer/`. Session 15
replaces this planning index with the user-facing one and keeps a pointer to
the concept and implementation history.
