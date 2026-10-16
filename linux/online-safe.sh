#!/bin/bash
# Before playing MORDHAU online (and after any crash): no file of the mod's loader may be in MORDHAU's folder.
# The launch wrapper does this at every start and end of Skyrim; this is the check by hand.
set -euo pipefail
BIN=${MORDHAU:-$HOME/Games/Heroic/Mordhau}/Mordhau/Binaries/Win64
pgrep -f Mordhau-Win64-Shipping.exe >/dev/null && { echo "MORDHAU is running: close it first"; exit 1; }
removed=$(find "$BIN" -maxdepth 1 -iname 'dwmapi.dll*' -print -delete)
[ -n "$removed" ] && echo "removed: $removed"
if find "$BIN" -maxdepth 1 -iname 'dwmapi.dll*' | grep -q .; then
    echo "NOT SAFE: a dwmapi.dll is still in $BIN - delete it by hand"; exit 1
fi
echo "MORDHAU's folder is clean: safe for normal play"
