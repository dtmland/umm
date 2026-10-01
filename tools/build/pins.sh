#!/bin/sh
# Load and validate tools/build/libumm.env (fail closed).
# Usage: sh tools/build/pins.sh [libumm.env]
# When GITHUB_ENV is set, appends the pins there for GitHub Actions.

set -eu

die() {
  printf 'pins.sh: %s\n' "$1" >&2
  exit 1
}

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
env_file=${1:-"$script_dir/libumm.env"}
[ -f "$env_file" ] || die "missing pin file: $env_file"

required_keys="UMM_LIBUMM_VERSION UMM_LIBUMM_URL UMM_LIBUMM_SHA256"
for key in $required_keys; do unset "$key" || true; done

while IFS= read -r line || [ -n "$line" ]; do
  line=$(printf '%s' "$line" | tr -d '\r')
  case "$line" in '' | \#*) continue ;; esac
  case "$line" in *=*) key=${line%%=*}; value=${line#*=} ;; *) die "malformed line: $line" ;; esac
  case "$key" in '' | *[!A-Z0-9_]*) die "invalid key: $key" ;; esac
  case "$value" in '' | *[!A-Za-z0-9._+:/-]*) die "invalid or empty value for $key" ;; esac
  eval "$key=\$value"
done < "$env_file"

for key in $required_keys; do
  eval "value=\${$key-}"
  [ -n "$value" ] || die "missing key: $key"
done
case "$UMM_LIBUMM_SHA256" in *[!0-9a-f]*) die "UMM_LIBUMM_SHA256 is not lowercase hex" ;; esac
[ "${#UMM_LIBUMM_SHA256}" -eq 64 ] || die "UMM_LIBUMM_SHA256 must be 64 hex characters"

if [ -n "${GITHUB_ENV:-}" ]; then
  for key in $required_keys; do
    eval "value=\${$key}"
    printf '%s=%s\n' "$key" "$value" >> "$GITHUB_ENV"
  done
fi
