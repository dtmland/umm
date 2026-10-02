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
# ExifTool path
exiftool = '/home/you/bin/exiftool'
```

On Windows, `umm setup exiftool` writes a single-quoted path so backslashes
stay readable:

```toml
# ExifTool path
exiftool = 'C:\Users\you\AppData\Local\Programs\ExifTool\ExifTool.exe'
```

Comments (`#`) are allowed. Unknown keys are ignored. The path is applied as
libumm's explicit ExifTool location. umm never mutates `PATH`. Double-quoted
values still work; `\\` and `\"` are the only escapes — other backslashes are
literal, so a hand-edited `exiftool = "C:\Users\..."` is accepted.

## Discovery order

1. Config `exiftool` key (this file)
2. Environment variable `UMM_EXIFTOOL`
3. `exiftool` / `exiftool.exe` on `PATH`

`umm doctor` reports which step hit. Package-manager installs track the
platform version rather than an exact pin; doctor compares the discovered
version to the libumm-tested version as advisory information.
