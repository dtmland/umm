#!/bin/sh
# Native Linux/macOS ExifTool setup for `umm setup exiftool` (concept §4.2).
# Never redistributes ExifTool. Never mutates PATH or the environment.
# Prefer distro/Homebrew packages; checksum-verified tarball fallback.

set -eu

umm_die() {
  printf 'exiftool.sh: %s\n' "$1" >&2
  exit 1
}

umm_usage() {
  cat <<'EOF'
Usage: exiftool.sh [--config FILE] [--record PATH] [--prefix DIR] [--pin-file FILE] [--force]

Install ExifTool for the current user and record its path in the umm config
(exiftool key). Does not modify PATH.

  Linux:  apt (libimage-exiftool-perl), dnf (perl-Image-ExifTool),
          pacman (perl-image-exiftool), else checksum-verified tarball
  macOS:  brew install exiftool, else checksum-verified tarball

  --config FILE   umm config.toml to write (default: XDG / macOS app support)
  --record PATH   skip install; record this existing binary and exit
  --prefix DIR    per-user prefix for the tarball fallback
  --pin-file FILE tools/build/exiftool.env (fallback pin)
  --force         install even if exiftool is already on PATH
EOF
  exit 0
}

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
config_path=""
record_path=""
prefix=""
pin_file=""
force=0

while [ $# -gt 0 ]; do
  case "$1" in
    -h|--help)
      umm_usage
      ;;
    --config)
      [ $# -ge 2 ] || umm_die "--config requires a path"
      config_path=$2
      shift 2
      ;;
    --record)
      [ $# -ge 2 ] || umm_die "--record requires a path"
      record_path=$2
      shift 2
      ;;
    --prefix)
      [ $# -ge 2 ] || umm_die "--prefix requires a directory"
      prefix=$2
      shift 2
      ;;
    --pin-file)
      [ $# -ge 2 ] || umm_die "--pin-file requires a path"
      pin_file=$2
      shift 2
      ;;
    --force)
      force=1
      shift
      ;;
    *)
      umm_die "unknown argument: $1"
      ;;
  esac
done

umm_escape_toml() {
  printf '%s' "$1" | sed 's/\\/\\\\/g; s/"/\\"/g'
}

umm_default_config() {
  os=$(uname -s)
  if [ -n "${XDG_CONFIG_HOME:-}" ]; then
    printf '%s\n' "$XDG_CONFIG_HOME/umm/config.toml"
    return
  fi
  [ -n "${HOME:-}" ] || umm_die "HOME is not set"
  case "$os" in
    Darwin)
      printf '%s\n' "$HOME/Library/Application Support/umm/config.toml"
      ;;
    *)
      printf '%s\n' "$HOME/.config/umm/config.toml"
      ;;
  esac
}

umm_write_config() {
  cfg=$1
  tool=$2
  dir=$(dirname -- "$cfg")
  mkdir -p "$dir"
  esc=$(umm_escape_toml "$tool")
  {
    printf '# Written by umm setup exiftool. Discovery step 1 (explicit config).\n'
    printf 'exiftool = "%s"\n' "$esc"
  } >"$cfg"
  printf 'exiftool.sh: recorded %s in %s\n' "$tool" "$cfg"
}

if [ -z "$config_path" ]; then
  config_path=$(umm_default_config)
fi

if [ -n "$record_path" ]; then
  [ -e "$record_path" ] || umm_die "record path does not exist: $record_path"
  umm_write_config "$config_path" "$record_path"
  exit 0
fi

found=""
if [ "$force" -eq 0 ]; then
  found=$(command -v exiftool 2>/dev/null || true)
fi
if [ -n "$found" ]; then
  umm_write_config "$config_path" "$found"
  exit 0
fi

installed=""
os=$(uname -s)
if [ "$os" = Darwin ] && command -v brew >/dev/null 2>&1; then
  brew install exiftool
  installed=$(command -v exiftool 2>/dev/null || true)
elif command -v apt-get >/dev/null 2>&1; then
  if command -v sudo >/dev/null 2>&1; then
    sudo apt-get install -y libimage-exiftool-perl || true
  else
    apt-get install -y libimage-exiftool-perl || true
  fi
  installed=$(command -v exiftool 2>/dev/null || true)
elif command -v dnf >/dev/null 2>&1; then
  if command -v sudo >/dev/null 2>&1; then
    sudo dnf install -y perl-Image-ExifTool || true
  else
    dnf install -y perl-Image-ExifTool || true
  fi
  installed=$(command -v exiftool 2>/dev/null || true)
elif command -v pacman >/dev/null 2>&1; then
  if command -v sudo >/dev/null 2>&1; then
    sudo pacman -S --noconfirm perl-image-exiftool || true
  else
    pacman -S --noconfirm perl-image-exiftool || true
  fi
  installed=$(command -v exiftool 2>/dev/null || true)
elif command -v brew >/dev/null 2>&1; then
  brew install exiftool
  installed=$(command -v exiftool 2>/dev/null || true)
fi

if [ -n "$installed" ]; then
  umm_write_config "$config_path" "$installed"
  exit 0
fi

umm_resolve_pin_file() {
  if [ -n "$pin_file" ]; then
    printf '%s\n' "$pin_file"
    return
  fi
  if [ -f "$script_dir/exiftool.env" ]; then
    printf '%s\n' "$script_dir/exiftool.env"
    return
  fi
  if [ -f "$script_dir/../tools/build/exiftool.env" ]; then
    printf '%s\n' "$script_dir/../tools/build/exiftool.env"
    return
  fi
  umm_die "missing pin file (looked next to this script and in ../tools/build/exiftool.env)"
}

pin_file=$(umm_resolve_pin_file)
[ -f "$pin_file" ] || umm_die "missing pin file: $pin_file"

UMM_EXIFTOOL_VERSION=""
UMM_EXIFTOOL_URL=""
UMM_EXIFTOOL_SHA256=""
while IFS= read -r line || [ -n "$line" ]; do
  line=$(printf '%s' "$line" | tr -d '\r')
  case "$line" in
    ''|\#*) continue ;;
  esac
  case "$line" in
    *=*)
      key=${line%%=*}
      value=${line#*=}
      ;;
    *)
      continue
      ;;
  esac
  case "$key" in
    UMM_EXIFTOOL_VERSION) UMM_EXIFTOOL_VERSION=$value ;;
    UMM_EXIFTOOL_URL) UMM_EXIFTOOL_URL=$value ;;
    UMM_EXIFTOOL_SHA256) UMM_EXIFTOOL_SHA256=$value ;;
  esac
done <"$pin_file"

[ -n "$UMM_EXIFTOOL_VERSION" ] || umm_die "missing key: UMM_EXIFTOOL_VERSION"
[ -n "$UMM_EXIFTOOL_URL" ] || umm_die "missing key: UMM_EXIFTOOL_URL"
[ -n "$UMM_EXIFTOOL_SHA256" ] || umm_die "missing key: UMM_EXIFTOOL_SHA256"
[ "${#UMM_EXIFTOOL_SHA256}" -eq 64 ] || umm_die "UMM_EXIFTOOL_SHA256 must be 64 hex characters"

[ -n "${HOME:-}" ] || umm_die "HOME is not set"
if [ -z "$prefix" ]; then
  case "$os" in
    Darwin)
      prefix="$HOME/Library/Application Support/umm/exiftool-$UMM_EXIFTOOL_VERSION"
      ;;
    *)
      data_home=${XDG_DATA_HOME:-$HOME/.local/share}
      prefix="$data_home/umm/exiftool-$UMM_EXIFTOOL_VERSION"
      ;;
  esac
fi
cache_dir=${XDG_CACHE_HOME:-$HOME/.cache}/umm/exiftool
case "$os" in
  Darwin) cache_dir="$HOME/Library/Caches/umm/exiftool" ;;
esac

command -v curl >/dev/null 2>&1 || umm_die "curl is required for the tarball fallback"
command -v tar >/dev/null 2>&1 || umm_die "tar is required for the tarball fallback"

umm_sha256() {
  file=$1
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$file" | awk '{print $1}'
  elif command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "$file" | awk '{print $1}'
  else
    umm_die "sha256sum or shasum is required"
  fi
}

mkdir -p "$cache_dir"
archive_path="$cache_dir/exiftool-$UMM_EXIFTOOL_VERSION.tar.gz"
if [ -f "$archive_path" ]; then
  got=$(umm_sha256 "$archive_path")
  exp=$(printf '%s' "$UMM_EXIFTOOL_SHA256" | tr 'A-F' 'a-f')
  got=$(printf '%s' "$got" | tr 'A-F' 'a-f')
  if [ "$got" != "$exp" ]; then
    rm -f "$archive_path"
    umm_die "checksum mismatch for $archive_path (deleted)"
  fi
else
  part="$archive_path.part"
  rm -f "$part"
  curl -fL --retry 3 -o "$part" "$UMM_EXIFTOOL_URL" || {
    rm -f "$part"
    umm_die "download failed: $UMM_EXIFTOOL_URL"
  }
  mv "$part" "$archive_path"
  got=$(umm_sha256 "$archive_path")
  exp=$(printf '%s' "$UMM_EXIFTOOL_SHA256" | tr 'A-F' 'a-f')
  got=$(printf '%s' "$got" | tr 'A-F' 'a-f')
  if [ "$got" != "$exp" ]; then
    rm -f "$archive_path"
    umm_die "checksum mismatch for $archive_path (deleted)"
  fi
fi

staging="$prefix.staging.$$"
rm -rf "$staging"
mkdir -p "$staging"
tar -xf "$archive_path" -C "$staging" || {
  rm -rf "$staging"
  umm_die "failed to extract $archive_path"
}
script_src=""
if [ -f "$staging/exiftool" ]; then
  script_src="$staging/exiftool"
else
  for candidate in "$staging"/*/exiftool; do
    if [ -f "$candidate" ]; then
      script_src=$candidate
      break
    fi
  done
fi
[ -n "$script_src" ] || {
  rm -rf "$staging"
  umm_die "archive does not contain an exiftool script"
}
src_root=$(dirname "$script_src")
parent=$(dirname "$prefix")
mkdir -p "$parent"
rm -rf "$prefix"
mv "$src_root" "$prefix"
rm -rf "$staging"
installed_script="$prefix/exiftool"
[ -f "$installed_script" ] || umm_die "extract did not produce $installed_script"
chmod +x "$installed_script" 2>/dev/null || true
umm_write_config "$config_path" "$installed_script"
