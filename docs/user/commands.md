# Command reference

Flags and synopses come from the command table (`src/command_table.cpp`),
which also generates `umm(1)` and shell completions. Do not treat this page
as a second flag list: if help and this page ever disagree, trust
`umm COMMAND --help`.

Usage:

```text
umm [global options] COMMAND [options] [operands]
```

## Global options

| Flag | Summary |
|---|---|
| `--backend BACKEND` | force a backend: `exiv2` or `exiftool` |
| `--json` | machine-readable JSON output |
| `-h`, `--help` | show help |

`--recursive` / `-r` (descend into directory operands) is accepted by
file-batch commands: `read`, `get`, `set`, `rm`, `dumpall`, `dumpunmapped`,
`conflicts`, `merge`, `sync`, `cast`, `geotag`.

`--json` is accepted everywhere. Inspect commands always emit a document
(`read`, `get`, `dumpall`, `dumpunmapped`, `conflicts`, `caps`, `map`,
`cast`, `version`, `doctor`). Mutating commands emit JSON for `--dry-run`
reports (`set`, `rm`, `geotag`); `merge` and `sync` also emit JSON when
`--json` is set without `--dry-run`. See [JSON schema](json-schema.md).

`--backend` is passed through to libumm `ReadOptions` / `WriteOptions` /
`SyncOptions`. `CastOptions` has no backend field; global `--backend` is
accepted on `cast` but not forwarded. `umm map` does not pass a backend into
`umm::describe`. With no `--backend`, libumm uses the type's preferred
backend when that backend is available (ExifTool for video and other
Exiv2-weak types, Exiv2 for JPEG); otherwise the first available backend.
An explicit `--backend` is a hard pin with no fallback.

Batch commands process files **sequentially**. There are no parallel writes
in v1. Per-file failures are recorded, processing continues, and the process
exit code summarizes them ([exit codes](exit-codes.md)).

Property names on `get` / `set` / `rm` are convenience accessors or full
ids (`iptc.photo.*`, `iptc.video.*`, `exif.*`). `merge` takes a **full
property id** only. Camera GPS is Location struct fields, not a `gps`
accessor.

## read

```text
umm read [options] FILE...
```

Print all canonical metadata (human table by default).

| Flag | Summary |
|---|---|
| `--sources` | show provenance for each property |
| `--report-casts` | list `castCandidates()` without applying them |

`--sources` adds resolution state and source refs (`base_key`, `backend`,
`container`) from libumm. Property ids are canonical, never backend tag
names. `--report-casts` prints cast candidates separately from stored
properties and does not write.

## get

```text
umm get [options] FILE PROPERTY...
```

Print named properties. The first operand is the file (or a directory with
`-r`); remaining operands are accessors or full ids. Mixed lists are allowed
(`creator keywords locationCreated`). Human output is values only. Exit 7 if
a requested property is absent; unknown names map to the semantics group
(6).

## set

```text
umm set [options] FILE ASSIGN...
```

Assign properties and persist. `FILE…` may be several files (or `-r`);
`ASSIGN…` starts at the first `NAME=VALUE` or property name.

| Flag | Summary |
|---|---|
| `--policy POLICY` | `embedded` \| `sidecar` \| `sidecar-required` \| `preferred` |
| `--dry-run` | report what would change without writing |

Default policy is `preferred`. Scalar and bag values use `NAME=VALUE`
(keywords are comma-separated). Structure properties take
`NAME --json '<object|array>'`. `--dry-run` prints the libumm `WriteReport`.

## rm

```text
umm rm [options] FILE PROPERTY...
```

Clear named properties across synchronized representations and persist. Same
`--policy` and `--dry-run` as `set`. Not a raw-tag delete.

## dumpall

```text
umm dumpall [options] FILE...
```

Dump every base entry (family, key, value) in source order. Read-only.
There is no command to write a new base key.

## dumpunmapped

```text
umm dumpunmapped [options] FILE...
```

Dump base entries that no canonical property consumed. Read-only. There is
no `umm unmapped` command.

## conflicts

```text
umm conflicts [options] FILE...
```

List disagreeing properties with each candidate source.

| Flag | Summary |
|---|---|
| `--fail-on-conflict` | exit non-zero when conflicts exist |

## merge

```text
umm merge [options] FILE PROPERTY (--use BASEKEY | --value V)
```

Resolve a conflict and persist. `PROPERTY` is a full id.

| Flag | Summary |
|---|---|
| `--use BASEKEY` | choose a candidate source |
| `--value V` | supply an override value |
| `--container WHERE` | `embedded` \| `sidecar` when a base key is ambiguous |
| `--policy POLICY` | `embedded` \| `sidecar` \| `sidecar-required` \| `preferred` |
| `--dry-run` | report what would change without writing |

Exactly one of `--use` or `--value` is required.

## sync

```text
umm sync [options] FILE...
```

Make embedded and sidecar carriers agree.

| Flag | Summary |
|---|---|
| `--direction DIR` | `both` \| `embedded-to-sidecar` \| `sidecar-to-embedded` |
| `--dry-run` | report what would change without writing |

Default direction is `both`. Unresolved conflicts fail; run `umm merge`
first.

## cast

```text
umm cast [options] FILE... up|down|side
```

Preview (default) or apply a cast direction. Direction is a positional
operand (`up`, `down`, or `side`), not `--direction` (that flag belongs to
`sync`). Default is preview (`CastOptions::dry_run`). `--apply` persists
through `umm::cast` / `umm::write`. Empty `--group` means all groups for
the direction.

| Flag | Summary |
|---|---|
| `--group NAME` | cast group (repeatable; empty = all for the direction) |
| `--force` | apply groups whose status is `needs_force` |
| `--include-approximate` | apply approximate groups such as `videoCreated` |
| `--apply` | persist through `umm::cast` (default is dry-run preview) |

## caps

```text
umm caps [options] FILE|TYPE...
```

Show per-backend capability rows for a file path or a type token (`JPEG`,
`MP4`, …). This is the live supported-types answer. Not a file-batch command
(`-r` is not accepted).

## map

```text
umm map [options] PROPERTY [FILE]
```

Print the property map (definition, representations, casts, cross-media
partner) from `umm::describe`. `PROPERTY` is a registry id
(`iptc.photo.creator`) or a cross-media accessor name (`locationCreated`,
`creator`, …). Operand order is **property first**, optional file second;
`umm map FILE PROPERTY` is not accepted. Optional `FILE` fills values and
cast-group statuses and does not write. A cross-media name expands to both
domain ids. Unknown properties map to the semantics group (6).

| Flag | Summary |
|---|---|
| `--layers LAYERS` | `representations,casts,cross-media` (display filter) |

Default is all layers the map contains. `--layers` filters **display** only;
it does not change the libumm call. Invalid layer names exit 1 (usage).
Not a file-batch command (`-r` is not accepted).

## geotag

```text
umm geotag --track TRACK [options] FILE...
```

Correlate capture times with a GPX/NMEA/KML track and write
`locationCreated[0]` GPS (photo) or `locationShot[0]` GPS (video). Workflow
command, not a property accessor; known coordinates use
`umm set … locationCreated --json`.

| Flag | Summary |
|---|---|
| `--track TRACK` | GPX/NMEA/KML track file (required) |
| `--offset MINUTES` | naive UTC offset in minutes (not clock skew) |
| `--policy POLICY` | `embedded` \| `sidecar` \| `sidecar-required` \| `preferred` |
| `--dry-run` | report what would change without writing |

`--offset` sets `MatchOptions::naive_utc_offset_minutes` when the capture
time has no zone (0 treats naive times as UTC). The CLI does not parse
tracks; `umm::importTrack` does.

## doctor

```text
umm doctor [options]
```

Report backend availability and ExifTool discovery (config / `UMM_EXIFTOOL` /
`PATH`), plus `umm setup exiftool` remediation when ExifTool is missing.

## setup

```text
umm setup exiftool
```

Install ExifTool for the current user and record the path in the umm config
(`exiftool` key). Does not modify `PATH`. umm never bundles ExifTool.
Per-OS behavior is in `umm setup --help` and
[sysadmin install](../sysadmin/install.md).

## version

```text
umm version [options]
```

Print the CLI version, libumm version, and the registry standards table
(which IPTC TR this binary implements).
