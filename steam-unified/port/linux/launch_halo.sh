#!/usr/bin/env bash
set -euo pipefail
DIR="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
cd "$DIR"
export LD_LIBRARY_PATH="$DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export HALO_DATA_ROOT="${HALO_DATA_ROOT:-$DIR/assets}"
export HALO_SAVE_ROOT="${HALO_SAVE_ROOT:-$HOME/.local/share/halo-linux}"
export HALO_FULLSCREEN="${HALO_FULLSCREEN:-true}"
export HALO_MENUS=pc
unset HALO_MENU_OPEN
unset HALO_CAMPAIGN_FD HALO_LEGACY_FD HALO_ONLINE_CAMPAIGN
# Retain each prior session so match/crash evidence survives later launches.
if [[ -f "$DIR/halo-runtime.log" ]]; then
  mkdir -p "$DIR/log-archive"
  stamp=$(date -u +%Y%m%dT%H%M%SZ)
  cp -p "$DIR/halo-runtime.log" "$DIR/log-archive/halo-runtime-$stamp-$$.log"
  mv -f "$DIR/halo-runtime.log" "$DIR/halo-runtime.previous.log"
fi
exec ./halo "$@" > "$DIR/halo-runtime.log" 2>&1
