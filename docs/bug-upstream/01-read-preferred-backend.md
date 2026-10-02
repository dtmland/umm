# libumm: `umm::read` ignores `preferred_backend`

File against **dtmland/libumm**. Type: Bug.

## Summary

Default `umm::read` (and therefore CLI `umm read` / `umm get` / `umm unmapped`
with no `--backend`) uses `BackendManager::firstAvailable()`, which is always
Exiv2 when it is compiled in. Exiv2 cannot usefully read video (and other
ExifTool-preferred types). The call fails with `Exiv2 read failed` even when
ExifTool is available. Forcing `--backend exiftool` works.

This is not MP4-specific. `registry/capabilities/policy.json` already marks
ExifTool as `preferred_backend` for video and other Exiv2-weak types,
including at least:

- Video/audio containers: MP4, MOV, MKV, AVI, WAV, ASF
- BMFF stills: HEIC, HEIF, AVIF, JXL, CR3
- Some RAW: RAF, RW2, MRW, SR2
- GIF, BMP

## Current behavior

`src/read.cpp` `select_backend()`:

```cpp
if (!options.backend.empty()) {
  return manager.get(options.backend);
}
return manager.firstAvailable();
```

`src/write.cpp` / `src/core/sidecar.cpp` already honor `caps.preferred_backend`
when that backend is available, then fall back to `firstAvailable()`.
`matchTrack` documents the same gap:

> High-level path matching uses the type's preferred backend when it is
> available (ExifTool for MP4/MOV). `umm::read` itself still defaults to
> first-available, which is Exiv2 and cannot read video.

So geotag can read an MP4 while `umm::read` on the same file fails.

## Expected behavior

When `ReadOptions::backend` is empty, `umm::read` / `load_read` should pick
the type's `preferred_backend` if that backend is available, matching write
and `matchTrack`. If the preferred backend is missing, fall back to
`firstAvailable()`. `--backend` remains an explicit override (no fallback).

Do **not** implement this in the umm CLI. umm is a thin CLI: pass
`ReadOptions` through and present libumm results.

## Design notes (do not invent dual-backend reads in v1)

1. **Should a failed Exiv2 read fall through to ExifTool?** Yes for
   *type-level* selection: if preferred is ExifTool, never start with Exiv2.
   If Exiv2 is preferred and the whole read fails, a fallback to ExifTool is
   reasonable when ExifTool is available (same spirit as write). Do not merge
   a partial Exiv2 document with ExifTool tag-by-tag in one `read` — Exiv2
   typically fails the whole read on unsupported containers rather than
   failing per property.

2. **Should `unmapped` always call both backends?** No.
   `Metadata::unmapped()` is “what the selected backend could not map.”
   Calling both would mix vocabularies, duplicate keys, and cost a second
   process. Default: preferred backend. To see the other backend, pass
   `ReadOptions.backend`. A union-unmapped is a later feature if ever wanted.

3. **Does `read` already know which tool to use?** Yes, via the capability
   table’s `preferred_backend`. Canonical property ids do not pick a backend;
   the type does.

4. **Are there legitimate one-command dual-backend cases?** Default
   read/get/set/unmapped/sync should use one backend. `--backend` is the
   verification workflow (write with one, read with the other). Conflicts are
   carrier/format disagreements (EXIF vs IPTC vs XMP, embedded vs sidecar),
   not Exiv2 vs ExifTool.

## Suggested implementation

- Teach `src/read.cpp` `select_backend` / `load_read` to consult
  `capabilities(media).preferred_backend` the same way `selected_backend` in
  `src/core/sidecar.cpp` does.
- Keep explicit `options.backend` as a hard pin.
- If preferred is unavailable, fall back to `firstAvailable()`.
- Tests: MP4/MOV (and at least one other ExifTool-preferred type) default
  `umm::read` succeeds when ExifTool is available; JPEG still defaults to
  Exiv2; `--backend exiv2` on MP4 still fails honestly.
- Update the comment in `src/track.cpp` once read no longer defaults to
  Exiv2-first.

## Seen from umm 0.1.0

`umm read video.mp4` / `umm unmapped video.mp4` → `Exiv2 read failed`.
`umm read --backend exiftool video.mp4` works.
