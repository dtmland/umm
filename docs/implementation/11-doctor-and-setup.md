# Session 11 — Doctor and setup (`umm doctor`, `umm setup exiftool`)

Status: **complete** (`install/exiftool.{bat,ps1,sh}`; `doctor` / `setup` in `src/commands.cpp`)

## Goal

Make ExifTool acquisition painless and diagnosable: native per-platform
install scripts wrapped by `umm setup exiftool`, and `umm doctor` that
reports discovery and remediation. Never bundle or redistribute ExifTool.

## Concept

- §2.2 `umm doctor`, `umm setup exiftool`
- §4.2 full ExifTool policy (discovery, scripts, winget/brew/apt, checksum
  fallbacks, config recording, no PATH mutation, license)
- §4.3 degraded modes; `--backend exiftool` with no ExifTool fails with
  remediation
- §5 `install/` scripts; native `.bat`/PowerShell and POSIX `sh`; never a
  Python helper
- §7.1 config path written by setup
- §7.3 Windows packaging decision (Oliver Betz / winget, not tarball+Perl)

## Prerequisites

Session 05 (config read/write location). Session 07 (`caps`) for overlap in
degraded-mode facts.

## In scope

### `install/` scripts (CLI-owned, not libumm's tarball+Perl helper)

- **Windows:** `install/exiftool.bat` wrapping `install/exiftool.ps1`.
  Prefer `winget install -e --id OliverBetz.ExifTool`. Fallback:
  checksum-verified upstream Windows `.zip` into a **per-user** prefix.
  Standalone `exiftool.exe`, **no Perl prerequisite**, no tarball.
- **Linux:** POSIX `install/exiftool.sh` — distro package first
  (`apt install libimage-exiftool-perl`, `dnf install perl-Image-ExifTool`,
  `pacman -S perl-image-exiftool`, …), else checksum-verified upstream
  tarball into a per-user prefix.
- **macOS:** same `exiftool.sh` (or a clearly shared POSIX script) — prefer
  `brew install exiftool`, else checksum-verified upstream download into a
  per-user prefix.
- Scripts finish by recording the installed location in the umm user config
  (`exiftool` key) so discovery step 1 hits. No `PATH` or environment
  mutation required.
- Direct-download fallbacks are checksum-verified. Package-manager installs
  track the platform version (not an exact pin).
- umm distributes **nothing** of ExifTool (Artistic/GPL dual license never
  enters release analysis).

### `umm setup exiftool`

- Invokes the native script for the current OS. `--help` explains what will
  run. Non-zero if the script fails.

### `umm doctor`

- Report which backends are usable.
- Which ExifTool (and Perl, **where relevant** — not on the Windows exe
  packaging) was found and **via which discovery step** (config /
  `UMM_EXIFTOOL` / PATH / not found).
- Advisory: discovered ExifTool version vs the libumm-tested version.
- Print `umm setup exiftool` remediation when the backend is unavailable.
- With no ExifTool, state precisely what is lost (align with live
  `umm::capabilities` / `umm caps` — do not maintain a second handwritten
  loss list if libumm already answers).
- `--json` supported (read-type).
- With `--backend exiftool` and no ExifTool, other commands fail with this
  same remediation message (hook in the backend-select path if not already
  done).

## Out of scope

- Shipping ExifTool inside umm archives (forbidden).
- Python helpers (forbidden).
- Reusing libumm `tools/get-exiftool/` tarball+Perl as the Windows path
  (forbidden by §4.2 / §7.3).
- Release notices (session 14).

## Expected files

- `install/exiftool.bat`, `install/exiftool.ps1`, `install/exiftool.sh`
- `setup` and `doctor` command modules
- Contract tests: scripts exist, are not Python, Windows path mentions
  winget / Oliver Betz, Linux/macOS mention apt/dnf/pacman and brew

## Acceptance

- `umm doctor` shows discovery step and remediation.
- `umm setup exiftool --help` documents per-OS behavior.
- Config is written by a successful setup (testable via a mocked/scripted
  dry path if full winget/apt cannot run in CI; still run the script's
  "already installed / record path" branch).
- No ExifTool bits in the umm source tree except scripts that download or
  invoke a package manager.

## Validation

- Offline contract tests on script presence and policy strings.
- `umm doctor --json` schema_version.
- `git diff --check`.
