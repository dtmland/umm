# Getting started

`umm` reads and writes media metadata using libumm's canonical model (IPTC
Photo, IPTC Video Metadata Hub, EXIF). It does not invent tag names. Point
property semantics at
[libumm's user guide](https://github.com/dtmland/libumm/blob/v0.1.2/docs/user/guide.md)
and the
[property reference](https://github.com/dtmland/libumm/blob/v0.1.2/docs/user/properties/README.md);
this page is the CLI path from install to a first write.

## Install

Unpack a GitHub Release archive (`umm-<version>-ubuntu-24.04.tar.gz`,
`umm-<version>-windows-2025.zip`, or `umm-<version>-macos-15.tar.gz`) and put
the `umm` binary on your `PATH`. Archives also contain `umm(1)`, shell
completions, licenses, and ExifTool *setup scripts* — not ExifTool itself.
Source is Apache-2.0; binaries that contain Exiv2 are conveyed under
GPL-3.0.

To build from source, see [sysadmin install](../sysadmin/install.md).

## Check the environment

```sh
umm doctor
```

Doctor reports which backends are usable, which ExifTool was found (config,
`UMM_EXIFTOOL`, or `PATH`), and the libumm-tested ExifTool version as
advisory information. If ExifTool is missing:

```sh
umm setup exiftool
umm doctor
```

Without ExifTool, umm still works wherever Exiv2 capability rows allow.
`umm caps FILE` and doctor say what is lost (typically video write, PNG EXIF,
BMFF breadth). `--backend exiftool` with no ExifTool fails and prints the
same `umm setup exiftool` remediation.

Confirm which IPTC technical revision this binary implements:

```sh
umm version
```

## First read, get, and set

```sh
umm read photo.jpg
umm read video.mp4
umm get photo.jpg creator
umm set photo.jpg creator="Jane Doe"
umm get photo.jpg creator
```

- `read` dumps **all** canonical properties (human table by default).
- `get` prints **named** properties only and exits 7 if a requested name is
  absent.
- `set` assigns names and persists through libumm. There is no `umm write`
  command.

Machine-readable output:

```sh
umm read photo.jpg --json
umm get photo.jpg creator --json
```

Every `--json` document includes integer `"schema_version"` (currently `2`).
See [JSON schema](json-schema.md).

## Accessors vs full ids

On `get`, `set`, and `rm`, a name is either a **convenience accessor**
(`creator`, `locationCreated`, `dateCreated`, …) or a **full canonical id**
(`iptc.photo.creator`, `iptc.video.creator`). Accessors follow the file's
media domain: a video sniffed by `umm::read` stores `iptc.video.*` under the
same short name. Full ids never retarget the other domain. There is no `gps`
accessor and no `exif.gps.position`; camera GPS is Location struct fields
on `locationCreated` (photo) / `locationShot` (video).

```sh
umm set photo.jpg creator="Jane Doe" keywords="nature,landscape" \
  dateCreated="2025-01-15"
umm set video.mp4 creator="Jane Doe"
umm get photo.jpg iptc.photo.creator
umm get video.mp4 iptc.video.creator
umm set photo.jpg locationCreated --json \
  '[{"gpsLatitude":40.7128,"gpsLongitude":-74.0060}]'
umm set photo.jpg locationCreated --json '{"name":"NYC","countryCode":"US"}'
```

The CLI features every accessor libumm ships. The mapping is not copied here;
see libumm **Cross-media accessors**. Discover what is on a file with
`umm read FILE --json`. `umm get` is for known names.

## Base views (read-only)

`umm dumpall FILE` dumps every base entry libumm saw (family, key, value).
`umm dumpunmapped FILE` dumps base entries that no canonical property
consumed. Both are read-only: there is no base-key write path and no ad-hoc
tag names on `set` / `rm`. There is no `umm unmapped` command.

## Non-goals (v1)

- No GUI, watch mode, database, or asset management.
- No thumbnailing, transcoding, or image processing.
- No long-running daemon.
- No package-manager publication of umm itself.
- No metadata semantics outside libumm's registry.
- No writing new base keys.

## Next

- [Command reference](commands.md) — flags aligned with `umm(1)`
- [Examples](examples.md)
- [Config](config.md), [exit codes](exit-codes.md)
