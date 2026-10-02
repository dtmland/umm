# JSON output

Every `--json` invocation prints **one** document with integer
`"schema_version"` (currently `1`). Batch invocations use a single top-level
object with a `files` array (one entry per file, in operand order).

```json
{
  "schema_version": 1,
  "command": "read",
  "files": [
    { "path": "a.jpg", "ok": true,
      "properties": [ { "id": "iptc.photo.creator", "value": ["Jane"] } ] },
    { "path": "b.jpg", "ok": false, "error": "no such file: b.jpg" }
  ]
}
```

- `value` is the full JSON form; struct-valued properties are objects or
  arrays. The human table prints a compact summary instead.
- `umm read --sources` adds `resolution`, `sources` (`raw_key`, `backend`,
  `container`), and `preferred_source` when set, on each property object.
- `umm get` uses the requested name as `id` (accessor or full property id).
  When a requested property is absent, `ok` is false, `error` lists the
  missing names, and found properties are still included.
- Commands without per-file results (`version`, `doctor`) add their own
  top-level fields next to `schema_version` and `command`. `umm version`
  adds `version` (CLI), `libumm`, and `standards` (`standard`, `version`,
  `source_document` from `Registry::StandardInfo`).
- `umm doctor` adds `backends` (`id`, `available`, `version`, `reason`),
  `exiftool` (`discovery` = `config` / `env` / `path` / `not_found`, `path`,
  `version`, `tested_version`, optional `perl`), `remediation` when ExifTool
  is missing, and `lost` (live `umm::capabilitiesForType` objects) for types
  that prefer ExifTool or where Exiv2 is identify-only.
- `--dry-run --json` on `geotag` adds per-file `gps` (the `GpsCoordinate`
  that would be written) and `match` (`exact` / `interpolated` / `nearest`).
- Property ids on `read` are libumm canonical ids, never backend tag names.
- `umm unmapped` adds `unmapped`: `[{family, key, value}]` per file (not
  canonical property ids).
- `umm conflicts` adds `conflicts`: `[{property_id, resolution,
  preferred_source, candidates: [{value, family, primary_key, sources}]}]`.
- `umm caps` adds `capabilities`: `{file_type, preferred_backend,
  sidecar_recommended, backends: [{backend, available, identify_only,
  categories, location, notes}]}`. Access values are `none` / `read` /
  `read_write` / `create`.
- `--dry-run` on `set` / `rm` / `sync` / `geotag` (and `--json` on those
  reports) adds `report` per file when a write was considered. Write reports:
  `{method, backend, formats, written: [{family, key}]}` (`method` is
  `embedded` / `sidecar` / `mixed`). Sync reports add
  `carriers: [{container, written}]`. `geotag --dry-run` does not call
  `umm::write`; JSON carries `gps` / `match` instead of `report`.
- `umm merge --json` may include `report` after a write, or `merged: true` on
  `--dry-run`.

GPS objects use `latitude`, `longitude`, and optional `altitude_meters` /
`gps_time`. Date-times are ISO-8601 strings.

## Versioning

A breaking change (removing or renaming a field, or changing its type) bumps
`schema_version` and requires a CLI semver **major**. Additive fields are
allowed within a major.
