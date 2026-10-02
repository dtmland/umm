# libumm: Windows standalone ExifTool.exe requires Perl

File against **dtmland/libumm**. Type: Bug.

## Summary

The ExifTool adapter always locates a separate Perl interpreter and always
spawns `perl exiftool_script …`. That is correct for the Unix `exiftool`
script. It is wrong for the official Windows packaging (`exiftool.exe` /
Oliver Betz / winget `OliverBetz.ExifTool`), which is a standalone launcher
with Perl built in.

On a Windows machine with only that exe:

- `BackendAvailability.available == false`
- `reason == "Perl interpreter not found"`
- `umm doctor` reports ExifTool unavailable even though the exe exists and
  `--version` works
- `umm get --backend exiftool FILE PROP` is rejected because the backend is
  unavailable

libumm’s own end-user story for umm (concept §4.2 / §7.3) is: Windows uses
the upstream exe, **no Perl prerequisite**, no tarball. The adapter does not
implement that.

## Current behavior

`src/backends/exiftool/exiftool_backend.cpp` `resolve()`:

- `script_` from `ExifToolConfig.exiftool_script` / `UMM_EXIFTOOL` / PATH
  `exiftool`
- `perl_` from `ExifToolConfig.perl_interpreter` / PATH `perl`
- If `perl_` is missing → `"Perl interpreter not found"` (even when `script_`
  is a regular `.exe`)

`ensure_process()` / version probe always spawn:

```text
perl  script  -charset utf8  …  -stay_open True  -@ -
perl  script  -ver
```

`include/umm/backend.hpp` documents Perl as required. `tools/get-exiftool/`
on Windows is still tarball+Perl; that is a developer/CI path, not the umm
end-user path.

## Expected behavior

If the discovered ExifTool path is a Windows executable (`.exe`, including
`exiftool.exe` / `ExifTool.exe`):

- Do **not** require `perl` / `perl.exe` on PATH
- Spawn the exe directly (`ExifTool.exe -stay_open True -@ - …`, and
  `ExifTool.exe -ver` for the version probe)
- `availability().available == true` when the exe exists and the version
  probe works
- Absence reason should be about ExifTool, not Perl

If the path is the Perl script (`exiftool` without `.exe`, or a `.pl` file),
keep today’s `perl script …` behavior.

## Suggested implementation

- In `resolve()`, treat `.exe` (case-insensitive) as a native binary: skip
  the Perl requirement; set the spawn program to `script_` and omit the extra
  script argument (or pass argv so the first argument is not duplicated).
- Version probe: `{exe, "-ver"}` instead of `{perl, script, "-ver"}`.
- `-stay_open` child: `{exe, "-charset", "utf8", …}` instead of
  `{perl, script, …}`.
- Tests: configure `exiftool_script` to a fake/real `.exe` without setting
  `perl_interpreter` and with Perl missing from PATH; availability must not
  be `"Perl interpreter not found"`. Keep existing Perl-script tests on Unix.
- Docs: `backend.hpp` ExifToolConfig comment, NOTICE/THIRD-PARTY-NOTICES
  “locates Perl” wording should say Perl is required only for the script
  packaging.

## Seen from umm 0.1.0 on Windows

Config recorded by `umm setup exiftool`:

```text
exiftool = "C:\Users\…\AppData\Local\Programs\ExifTool\ExifTool.exe"
```

`umm doctor`:

```text
BACKEND     AVAILABLE  VERSION
exiv2        yes        0.28.9
exiftool     no         Perl interpreter not found

EXIFTOOL
  discovery  config
  path       C:\Users\…\ExifTool.exe
```

`exiftool` on PATH prints the man page. No Strawberry Perl is installed, and
none should be required.

The umm CLI cannot work around this: `ExifToolConfig` has no “run exe
directly” flag, and spawning is inside the adapter. After this fix, bump the
umm libumm pin.
