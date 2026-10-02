# Upstream libumm bugs

These are **libumm** defects. umm is a thin CLI: it must not add backend
selection, mapping, or ExifTool process logic. File each note as a libumm
issue (or point a libumm implementation session at it).

Creating GitHub issues in `dtmland/libumm` from this umm session was not
possible (API 403). Copy the bodies below when filing.

| Note | Symptom in umm | libumm locus |
|---|---|---|
| [01 — `umm::read` ignores `preferred_backend`](01-read-preferred-backend.md) | `umm read` / `unmapped` on video (and other ExifTool-first types) → `Exiv2 read failed`; `--backend exiftool` works | `src/read.cpp` vs write/`matchTrack` |
| [02 — Windows `ExifTool.exe` requires Perl](02-windows-exiftool-exe-perl.md) | `umm doctor` / `--backend exiftool` say Perl is missing though Oliver Betz `ExifTool.exe` exists | `src/backends/exiftool/exiftool_backend.cpp` |

CLI-only follow-ups (human-readable `config.toml`, doctor wording) stay in
this repository and do not unblock either bug.
