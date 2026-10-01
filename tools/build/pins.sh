#!/bin/sh
# Export the libumm pins from libumm.env (thin wrapper; the env file is the
# source of truth). Usage: . tools/build/pins.sh
_umm_pins_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
set -a
# shellcheck disable=SC1091
. "$_umm_pins_dir/libumm.env"
set +a
unset _umm_pins_dir
