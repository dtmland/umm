# Upstream libumm bugs

These notes were written against **libumm 0.1.0**. Both defects are **fixed
in libumm 0.1.1**, which umm now pins (`tools/build/libumm.env`). Keep the
files as the historical record.

umm is a thin CLI: it must not add backend selection, mapping, or ExifTool
process logic. After the pin bump, default `umm read` / `umm unmapped` follow
libumm's type `preferred_backend`, and a Windows `ExifTool.exe` is spawned
directly (no separate Perl).

| Note | Symptom in umm 0.1.0 | libumm locus | Status |
|---|---|---|---|
| [01 — `umm::read` ignores `preferred_backend`](01-read-preferred-backend.md) | `umm read` / `unmapped` on video (and other ExifTool-first types) → `Exiv2 read failed`; `--backend exiftool` works | `src/read.cpp` vs write/`matchTrack` | Fixed in libumm 0.1.1 ([PR 91](https://github.com/dtmland/libumm/pull/91)) |
| [02 — Windows `ExifTool.exe` requires Perl](02-windows-exiftool-exe-perl.md) | `umm doctor` / `--backend exiftool` say Perl is missing though Oliver Betz `ExifTool.exe` exists | `src/backends/exiftool/exiftool_backend.cpp` | Fixed in libumm 0.1.1 ([PR 92](https://github.com/dtmland/libumm/pull/92)) |

CLI-only follow-ups (human-readable `config.toml`, doctor wording if an older
system libumm still reports the Perl absence) stay in this repository.
