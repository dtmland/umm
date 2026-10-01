# JSON output schema (draft; session 15 publishes under `docs/user/`)

Every `--json` invocation prints **one** document with an integer
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
  arrays (concept §2.5). The human table prints a compact summary instead.
- `umm read --sources` adds `resolution`, `sources` (`raw_key`, `backend`,
  `container`), and `preferred_source` when set, on each property object.
- `umm get` uses the requested name as `id` (accessor or full property id).
  When a requested property is absent, `ok` is false, `error` lists the
  missing names, and found properties are still included.
- Commands without per-file results (`version`, `doctor`) add their own
  top-level fields next to `schema_version` and `command`. `umm version`
  adds `version` (CLI), `libumm`, and `standards` (`standard`, `version`,
  `source_document` from `Registry::StandardInfo`).
- Property ids on `read` are libumm canonical ids, never backend tag names.
- `umm unmapped` adds `unmapped`: `[{family, key, value}]` per file (not
  canonical property ids).
- `umm conflicts` adds `conflicts`: `[{property_id, resolution,
  preferred_source, candidates: [{value, family, primary_key, sources}]}]`.
- `umm caps` adds `capabilities`: `{file_type, preferred_backend,
  sidecar_recommended, backends: [{backend, available, identify_only,
  categories, location, notes}]}`. Access values are `none` / `read` /
  `read_write` / `create`.

## Versioning (concept §7.2)

A breaking change (removing/renaming a field or changing its type) bumps
`schema_version` and requires a CLI semver **major**. Additive fields are
allowed within a major.

## Config file (session 05)

TOML; first existing file wins. One v1 key: `exiftool = "/path/to/exiftool"`,
applied as libumm's explicit ExifTool path (discovery step 1).

- Linux/other: `$XDG_CONFIG_HOME/umm/config.toml`, else `~/.config/umm/config.toml`
- Windows: `%APPDATA%\umm\config.toml`
- macOS: `$XDG_CONFIG_HOME/umm/config.toml` (if set), then
  `~/Library/Application Support/umm/config.toml`

A missing file is not an error; a malformed one exits 1.
