# Native Windows ExifTool setup for `umm setup exiftool` (concept §4.2 / §7.3).
# Prefer winget OliverBetz.ExifTool (standalone exiftool.exe, no Perl).
# Fallback: checksum-verified upstream Windows .zip into a per-user prefix.
# Never redistributes ExifTool. Never mutates PATH.
# Accepts POSIX-style flags (--config, --record, …) so `umm setup` can
# invoke this script the same way as install/exiftool.sh.

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Write-UmmDie([string]$Message) {
    [Console]::Error.WriteLine("exiftool.ps1: $Message")
    exit 1
}

function Show-Usage {
    @'
Usage: exiftool.ps1 [--config FILE] [--record PATH] [--prefix DIR] [--pin-file FILE] [--force]

Install ExifTool for the current user and record its path in the umm config
(exiftool key). Does not modify PATH. No Perl prerequisite.

  Preferred: winget install -e --id OliverBetz.ExifTool
  Fallback:  checksum-verified upstream Windows .zip (exiftool.exe)

  --config FILE   umm config.toml to write (default: %APPDATA%\umm\config.toml)
  --record PATH   skip install; record this existing binary and exit
  --prefix DIR    per-user prefix for the zip fallback
  --pin-file FILE tools/build/exiftool.env (fallback pin)
  --force         install even if exiftool is already on PATH
'@ | Write-Output
    exit 0
}

$Config = $null
$Record = $null
$Prefix = $null
$PinFile = $null
$Force = $false

$i = 0
while ($i -lt $args.Count) {
    $a = $args[$i]
    switch ($a) {
        { $_ -in @('-h', '--help', '-Help') } { Show-Usage }
        '--config' {
            if ($i + 1 -ge $args.Count) { Write-UmmDie '--config requires a path' }
            $Config = $args[$i + 1]; $i += 2; continue
        }
        '--record' {
            if ($i + 1 -ge $args.Count) { Write-UmmDie '--record requires a path' }
            $Record = $args[$i + 1]; $i += 2; continue
        }
        '--prefix' {
            if ($i + 1 -ge $args.Count) { Write-UmmDie '--prefix requires a directory' }
            $Prefix = $args[$i + 1]; $i += 2; continue
        }
        '--pin-file' {
            if ($i + 1 -ge $args.Count) { Write-UmmDie '--pin-file requires a path' }
            $PinFile = $args[$i + 1]; $i += 2; continue
        }
        '--force' { $Force = $true; $i += 1; continue }
        default { Write-UmmDie "unknown argument: $a" }
    }
}

function Format-TomlString([string]$Value) {
    if ($Value.Contains("'")) {
        $esc = ($Value -replace '\\', '\\' -replace '"', '\"')
        return "`"$esc`""
    }
    return "'$Value'"
}

function Write-UmmConfig([string]$ConfigPath, [string]$ToolPath) {
    $dir = Split-Path -Parent $ConfigPath
    if ($dir) {
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
    }
    $quoted = Format-TomlString $ToolPath
    $text = "# ExifTool path`nexiftool = $quoted`n"
    $utf8 = New-Object System.Text.UTF8Encoding $false
    [System.IO.File]::WriteAllText($ConfigPath, $text, $utf8)
    Write-Output "exiftool.ps1: recorded $ToolPath in $ConfigPath"
}

function Get-DefaultConfigPath {
    if (-not $env:APPDATA) { Write-UmmDie 'APPDATA is not set' }
    return (Join-Path $env:APPDATA 'umm\config.toml')
}

if (-not $Config) { $Config = Get-DefaultConfigPath }

if ($Record) {
    if (-not (Test-Path -LiteralPath $Record)) {
        Write-UmmDie "record path does not exist: $Record"
    }
    Write-UmmConfig $Config $Record
    exit 0
}

function Find-ExifToolOnPath {
    $cmd = Get-Command exiftool -ErrorAction SilentlyContinue
    if ($cmd -and $cmd.Source) { return $cmd.Source }
    $cmd = Get-Command exiftool.exe -ErrorAction SilentlyContinue
    if ($cmd -and $cmd.Source) { return $cmd.Source }
    return $null
}

if (-not $Force) {
    $existing = Find-ExifToolOnPath
    if ($existing) {
        Write-UmmConfig $Config $existing
        exit 0
    }
}

$installed = $null
$winget = Get-Command winget -ErrorAction SilentlyContinue
if ($winget) {
    & winget install -e --id OliverBetz.ExifTool --accept-package-agreements --accept-source-agreements
    $installed = Find-ExifToolOnPath
    if (-not $installed) {
        $pf = ${env:ProgramFiles}
        $pf86 = ${env:ProgramFiles(x86)}
        $candidates = @(
            (Join-Path $env:LOCALAPPDATA 'Programs\ExifTool\exiftool.exe')
        )
        if ($pf) { $candidates += (Join-Path $pf 'ExifTool\exiftool.exe') }
        if ($pf86) { $candidates += (Join-Path $pf86 'ExifTool\exiftool.exe') }
        foreach ($c in $candidates) {
            if ($c -and (Test-Path -LiteralPath $c)) { $installed = $c; break }
        }
    }
}

if ($installed) {
    Write-UmmConfig $Config $installed
    exit 0
}

function Get-PinPath {
    if ($PinFile) { return $PinFile }
    $here = Split-Path -Parent $PSCommandPath
    $next = Join-Path $here 'exiftool.env'
    if (Test-Path -LiteralPath $next) { return $next }
    $repo = Join-Path $here '..\tools\build\exiftool.env'
    if (Test-Path -LiteralPath $repo) { return (Resolve-Path $repo).Path }
    Write-UmmDie 'missing pin file (looked next to this script and in ..\tools\build\exiftool.env)'
}

$pin = Get-PinPath
$ver = $null
$url = $null
$sha = $null
Get-Content -LiteralPath $pin | ForEach-Object {
    $line = $_.Trim()
    if (-not $line -or $line.StartsWith('#')) { return }
    $eq = $line.IndexOf('=')
    if ($eq -lt 1) { return }
    $key = $line.Substring(0, $eq)
    $value = $line.Substring($eq + 1)
    switch ($key) {
        'UMM_EXIFTOOL_VERSION' { $ver = $value }
        'UMM_EXIFTOOL_WINDOWS_URL' { $url = $value }
        'UMM_EXIFTOOL_WINDOWS_SHA256' { $sha = $value }
    }
}

if (-not $ver) { Write-UmmDie 'missing key: UMM_EXIFTOOL_VERSION' }
if (-not $url) { Write-UmmDie 'missing key: UMM_EXIFTOOL_WINDOWS_URL' }
if (-not $sha) {
    Write-UmmDie 'Windows zip fallback has no SHA-256 pin; install with winget: winget install -e --id OliverBetz.ExifTool'
}
if ($sha.Length -ne 64) { Write-UmmDie 'UMM_EXIFTOOL_WINDOWS_SHA256 must be 64 hex characters' }

if (-not $Prefix) {
    if (-not $env:LOCALAPPDATA) { Write-UmmDie 'LOCALAPPDATA is not set' }
    $Prefix = Join-Path $env:LOCALAPPDATA "umm\exiftool-$ver"
}
$cache = Join-Path $env:LOCALAPPDATA 'umm\cache\exiftool'
New-Item -ItemType Directory -Force -Path $cache | Out-Null
$zipPath = Join-Path $cache "exiftool-$ver.zip"

function Get-Sha256([string]$Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

if (Test-Path -LiteralPath $zipPath) {
    $got = Get-Sha256 $zipPath
    if ($got -ne $sha.ToLowerInvariant()) {
        Remove-Item -LiteralPath $zipPath -Force
        Write-UmmDie "checksum mismatch for $zipPath (deleted)"
    }
} else {
    try {
        Invoke-WebRequest -Uri $url -OutFile $zipPath -UseBasicParsing
    } catch {
        if (Test-Path -LiteralPath $zipPath) { Remove-Item -LiteralPath $zipPath -Force }
        Write-UmmDie "download failed: $url"
    }
    $got = Get-Sha256 $zipPath
    if ($got -ne $sha.ToLowerInvariant()) {
        Remove-Item -LiteralPath $zipPath -Force
        Write-UmmDie "checksum mismatch for $zipPath (deleted)"
    }
}

$staging = "$Prefix.staging"
if (Test-Path -LiteralPath $staging) { Remove-Item -LiteralPath $staging -Recurse -Force }
New-Item -ItemType Directory -Force -Path $staging | Out-Null
Expand-Archive -LiteralPath $zipPath -DestinationPath $staging -Force
$exe = Get-ChildItem -LiteralPath $staging -Recurse -File | Where-Object {
    $_.Name -eq 'exiftool.exe' -or $_.Name -like 'exiftool*(-k).exe'
} | Select-Object -First 1
if (-not $exe) {
    Remove-Item -LiteralPath $staging -Recurse -Force
    Write-UmmDie 'archive does not contain exiftool.exe'
}
$parent = Split-Path -Parent $Prefix
if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
if (Test-Path -LiteralPath $Prefix) { Remove-Item -LiteralPath $Prefix -Recurse -Force }
New-Item -ItemType Directory -Force -Path $Prefix | Out-Null
$dest = Join-Path $Prefix 'exiftool.exe'
Copy-Item -LiteralPath $exe.FullName -Destination $dest -Force
Remove-Item -LiteralPath $staging -Recurse -Force
Write-UmmConfig $Config $dest
