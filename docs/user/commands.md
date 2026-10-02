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
file-batch commands: `read`, `get`, `set`, `rm`, `unmapped`, `conflicts`,
`merge`, `sync`, `geotag`.

`--json` is accepted everywhere. Inspect commands always emit a document.
Mutating commands emit JSON for `--dry-run` reports (`set`, `rm`, `geotag`);
`merge` and `sync` also emit JSON when `--json` is set without `--dry-run`.
See [JSON schema](json-schema.md).

`--backend` is passed through to libumm `ReadOptions` / `WriteOptions` /
`SyncOptions`. With no `--backend`, libumm uses the type's preferred backend
when that backend is available (ExifTool for video and other Exiv2-weak
types, Exiv2 for JPEG); otherwise the first available backend. An explicit
`--backend` is a hard pin with no fallback.

Batch commands process files **sequentially**. There are no parallel writes
in v1. Per-file failures are recorded, processing continues, and the process
exit code summarizes them ([exit codes](exit-codes.md)).

Property names on `get` / `set` / `rm` are convenience accessors or full
ids (`iptc.photo.*`, `iptc.video.*`, `exif.*`). `merge` takes a **full
property id** only.

## read

```text
umm read [options] FILE...
```

Print all canonical metadata (human table by default).

| Flag | Summary |
|---|---|
| `--sources` | show provenance for each property |

`--sources` adds resolution state and source refs (`raw_key`, `backend`,
`container`) from libumm. Property ids are canonical, never backend tag
names.

## get

```text
umm get [options] FILE PROPERTY...
```

Print named properties. The first operand is the file (or a directory with
`-r`); remaining operands are accessors or full ids. Mixed lists are allowed
(`creator keywords gps`). Human output is values only. Exit 7 if a requested
property is absent; unknown names map to the semantics group (6).

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

## unmapped

```text
umm unmapped [options] FILE...
```

Dump unmapped entries (family, key, value). Read-only.

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
umm merge [options] FILE PROPERTY (--use RAWKEY | --value V)
```

Resolve a conflict and persist. `PROPERTY` is a full id.

| Flag | Summary |
|---|---|
| `--use RAWKEY` | choose a candidate source |
| `--value V` | supply an override value |
| `--container WHERE` | `embedded` \| `sidecar` when a raw key is ambiguous |
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

## caps

```text
umm caps [options] FILE|TYPE...
```

Show per-backend capability rows for a file path or a type token (`JPEG`,
`MP4`, …). This is the live supported-types answer. Not a file-batch command
(`-r` is not accepted).

## geotag

```text
umm geotag --track TRACK [options] FILE...
```

Correlate capture times with a GPX/NMEA/KML track and write
`exif.gps.position`. Workflow command, not a property accessor; known
coordinates use `umm set … gps=`.

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
