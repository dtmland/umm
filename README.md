# umm

`umm` is a command-line tool for reading, writing, reconciling, and
synchronizing media metadata. It is built entirely on
[libumm](https://github.com/dtmland/libumm). The vocabulary is IPTC Photo,
IPTC Video Metadata Hub, and EXIF — not ExifTool or Exiv2 tag names. The CLI
adds argument parsing, output formatting, batch orchestration, and environment
setup only; every metadata decision comes from libumm.

## Install

Download a release archive (`umm-<version>-<os>.tar.gz`) from GitHub Releases
and put `umm` on your `PATH`. Then check the environment:

```sh
umm doctor
umm setup exiftool   # if doctor says ExifTool is missing
umm version
```

`umm setup exiftool` installs ExifTool for the current user and records the
path in the umm config file. umm never bundles ExifTool. Binary archives that
contain Exiv2 are conveyed under GPL-3.0. Building from source is documented
in [docs/sysadmin/install.md](docs/sysadmin/install.md).

## Quick start

```sh
umm doctor
umm version
umm read photo.jpg
umm get photo.jpg creator
umm set photo.jpg creator="Jane Doe"
umm get photo.jpg creator
```

Convenience accessors work on both stills and video (`creator`, `keywords`,
`dateCreated`, `gps`, …). Full canonical ids never retarget the other domain:

```sh
umm set photo.jpg creator="Jane Doe" keywords="nature,landscape" \
  dateCreated="2025-01-15T14:30:00Z"
umm get video.mp4 iptc.video.creator
umm set photo.jpg gps="40.7128,-74.0060"
```

Machine-readable output is `--json` (every document has `"schema_version"`).
`umm COMMAND --help` and `umm(1)` are generated from the same command table.

## License

Source in this repository is Apache-2.0. Binary releases that contain Exiv2
are conveyed under GPL-3.0. ExifTool is located at runtime and is never
redistributed.

## Docs

- [User guide](docs/user/getting-started.md) — commands, examples, config, exit codes, JSON
- [Sysadmin](docs/sysadmin/install.md) — build, ExifTool, licensing, releases
- [Contributing](docs/developer/contributing.md) — architecture, tests, pins
- [Concept](docs/concept.md) — founding design
- [Docs index](docs/README.md)
