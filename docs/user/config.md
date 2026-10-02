# Config

umm reads a TOML file with one v1 key: the ExifTool path. First existing
candidate wins. A missing file is not an error; a malformed file exits 1
(usage).

## Paths

- Linux / other: `$XDG_CONFIG_HOME/umm/config.toml`, else
  `~/.config/umm/config.toml`
- Windows: `%APPDATA%\umm\config.toml`
- macOS: `$XDG_CONFIG_HOME/umm/config.toml` if that variable is set, then
  `~/Library/Application Support/umm/config.toml`

`umm setup exiftool` writes the first candidate (creating parent directories)
even if the file did not exist.

## Keys

```toml
# Written by umm setup exiftool. Discovery step 1 (explicit config).
exiftool = "/home/you/bin/exiftool"
```

Comments (`#`) are allowed. Unknown keys are ignored. The path is applied as
libumm's explicit ExifTool location. umm never mutates `PATH`.

## Discovery order

1. Config `exiftool` key (this file)
2. Environment variable `UMM_EXIFTOOL`
3. `exiftool` / `exiftool.exe` on `PATH`

`umm doctor` reports which step hit. Package-manager installs track the
platform version rather than an exact pin; doctor compares the discovered
version to the libumm-tested version as advisory information.
