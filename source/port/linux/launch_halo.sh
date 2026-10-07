#!/usr/bin/env bash
set -euo pipefail
DIR="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
cd "$DIR"
export LD_LIBRARY_PATH="$DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
# The data root must contain maps/ (your own prepared maps, shaders.bin and loading.tga).
export HALO_DATA_ROOT="${HALO_DATA_ROOT:-$DIR/assets}"
export HALO_FULLSCREEN="${HALO_FULLSCREEN:-true}"
# The native menu retains characters and Online Games; network lobbies use the responsive roster.
export HALO_MENUS="${HALO_MENUS:-xbox}"
campaign="${HALO_CAMPAIGN_CLIENT:-$DIR/../OpenCE-Campaign/launch_halo.sh}"
handoff="$(mktemp "${XDG_RUNTIME_DIR:-/tmp}/nxhalo-campaign.XXXXXX")"
trap 'rm -f "$handoff"' EXIT
if [[ -x "$campaign" ]]; then
  exec 9>"$handoff"
  export HALO_CAMPAIGN_FD=9
else
  unset HALO_CAMPAIGN_FD
fi
set +e
./halo "$@"
result=$?
set -e
exec 9>&-
if [[ -x "$campaign" ]] && grep -qx campaign "$handoff"; then
  rm -f "$handoff"
  trap - EXIT
  unset HALO_CAMPAIGN_FD HALO_DATA_ROOT HALO_SAVE_ROOT HALO_MENUS HALO_MENU_OPEN HALO_NETWORK_TEST
  exec "$campaign"
fi
exit "$result"
