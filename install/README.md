Native ExifTool setup scripts wrapped by `umm setup exiftool`.

- Windows: `exiftool.bat` → `exiftool.ps1` (winget `OliverBetz.ExifTool`, then
  a checksum-verified upstream `.zip`; standalone `exiftool.exe`, no Perl)
- Linux/macOS: `exiftool.sh` (`apt` / `dnf` / `pacman` / `brew`, then a
  checksum-verified tarball)

Scripts record the installed path in the umm user config (`exiftool` key) and
do not modify `PATH`. umm never redistributes ExifTool.
