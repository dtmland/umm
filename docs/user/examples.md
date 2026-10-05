# Examples

Prose uses generated-fixture style names (`photo.jpg`, `video.mp4`,
`hike.gpx`). Names are convenience accessors or canonical ids
(`iptc.photo.*`, `iptc.video.*`, `exif.*`), never ExifTool tag names.

## read with sources and JSON

```sh
umm read photo.jpg
umm read video.mp4
umm read photo.jpg --sources
umm read photo.jpg --json
umm read photo.jpg --sources --json
```

## get for scripting

```sh
umm get photo.jpg creator
umm get photo.jpg creator keywords locationCreated
umm get photo.jpg iptc.photo.creator
umm get video.mp4 iptc.video.creator --json
```

Human `get` prints values only. A missing name exits 7.

## set / rm with policy and dry-run

```sh
umm set photo.jpg creator="Jane Doe" keywords="hiking,summit"
umm set photo.jpg --dry-run creator="Ada"
umm set photo.jpg --policy sidecar headline="Summit"
umm set photo.jpg locationCreated --json '{"name":"NYC","countryCode":"US"}'
umm set photo.jpg iptc.photo.creatorsContactInfo --json '{"email":"jane@example.com"}'
umm set video.mp4 iptc.video.contributor --json '[{"name":"Alice","role":"director"}]'
umm rm photo.jpg headline
umm rm photo.jpg --dry-run creator keywords
```

Photo and video with the same accessor:

```sh
umm set photo.jpg creator="Jane Doe" dateCreated="2025-01-15T14:30:00Z"
umm set video.mp4 creator="Jane Doe" dateCreated="2025-01-15T14:30:00Z"
umm set video.mp4 iptc.video.dateReleased="2025-01-20T00:00:00Z"
```

GPS without geotag (Location struct fields):

```sh
umm set photo.jpg locationCreated --json \
  '[{"gpsLatitude":40.7128,"gpsLongitude":-74.0060}]'
umm set video.mp4 iptc.video.locationShot --json \
  '[{"gpsLatitude":40.7128,"gpsLongitude":-74.0060}]'
umm get photo.jpg locationCreated
umm get video.mp4 locationCreated
```

## conflicts → merge → sync

```sh
umm conflicts photo.jpg
umm conflicts photo.jpg --fail-on-conflict
umm merge photo.jpg iptc.photo.creator --use exif.image.artist
umm merge photo.jpg iptc.photo.creator --value "Jane Doe"
umm merge photo.jpg iptc.photo.creator --use Xmp.dc.creator --container sidecar
umm sync photo.jpg
umm sync photo.jpg --direction embedded-to-sidecar --dry-run
```

`merge` requires a full property id. Unresolved conflicts make `sync` fail.

## geotag

```sh
umm geotag --track hike.gpx photo1.jpg photo2.jpg
umm geotag --track hike.gpx --offset=120 photo.jpg
umm geotag --track hike.gpx --dry-run photo.jpg
umm geotag --track video_track.gpx video.mp4
umm get video.mp4 locationCreated
```

`--offset` is minutes for naive capture times, not clock skew. Geotag writes
Location GPS, not a `gps` accessor.

## cast preview / apply

```sh
umm cast photo.jpg side
umm cast photo.jpg side --json
umm cast photo.jpg side --apply
umm read photo.jpg --report-casts
umm cast photo.jpg up --group videoCreated --include-approximate --apply
```

Default is preview (no write). `--apply` persists through libumm.

## map

```sh
umm map locationCreated
umm map locationCreated --json
umm map iptc.photo.creator
umm map locationCreated photo.jpg
umm map locationCreated --layers representations
```

Operand order is `PROPERTY [FILE]`. `--layers` filters display only.

## caps

```sh
umm caps photo.jpg
umm caps JPEG
umm caps video.mp4 MP4
umm caps JPEG --json
```

## batch and recursive

```sh
umm read photo1.jpg photo2.jpg
umm set photo1.jpg photo2.jpg creator="Photographer" keywords="event"
umm read --recursive album/
umm geotag --track hike.gpx --recursive stills/
```

Writes stay sequential. A missing file is reported; other files still run.

## write with one backend, read with the other

```sh
umm set --backend exiv2 photo.jpg creator="Jane"
umm get --backend exiftool photo.jpg creator
umm set --backend exiftool photo.jpg headline="Summit"
umm read --backend exiv2 photo.jpg
```

Skip or fail cleanly if the requested backend is unavailable (`umm doctor`).

## dump views, doctor, setup, version

```sh
umm dumpall photo.jpg
umm dumpunmapped photo.jpg
umm dumpunmapped video.mp4
umm doctor
umm doctor --json
umm setup exiftool
umm version
umm version --json
```

## Photo workflow

```sh
umm set photo.jpg creator="Jane" keywords="hiking" dateCreated="2025-01-15T14:30:00Z"
umm geotag --track hike.gpx photo.jpg
umm get photo.jpg creator keywords locationCreated
```

## Video workflow

```sh
umm set video.mp4 iptc.video.creator --json '{"name":"Director","role":"director"}'
umm set video.mp4 iptc.video.dateCreated="2025-01-15T14:30:00Z"
umm geotag --track video_track.gpx video.mp4
umm get video.mp4 iptc.video.creator iptc.video.dateCreated iptc.video.locationShot
```
