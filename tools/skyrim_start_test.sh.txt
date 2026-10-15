#!/bin/bash
# Dev: does starting Skyrim (plainly, to its main menu) make a running MORDHAU close? Repeats N times.
G="/e/steam object/steamapps/common/Skyrim Special Edition"; W="/e/steam object/steamapps/common/Mordhau/Mordhau/Binaries/Win64"
for i in $(seq 1 ${1:-3}); do
  (cd "$G" && SteamAppId=489830 SteamGameId=489830 cmd //c start "" SkyrimSE.exe)
  sleep 55
  alive=$(tasklist //FI "IMAGENAME eq Mordhau-Win64-Shipping.exe" 2>/dev/null | grep -c Mordhau)
  echo "start $i: MORDHAU alive=$alive  exit-watch: $(grep -a 'EXIT WATCH' "$W/UE4SS.log" | grep -v ExecuteConsoleCommand | tail -1 | cut -c22-120)"
  taskkill //IM SkyrimSE.exe //F >/dev/null 2>&1; sleep 6
  [ "$alive" = "0" ] && { grep -a 'Engine exit requested' "$LOCALAPPDATA/Mordhau/Saved/Logs/Mordhau.log" | head -1 | cut -c1-110; break; }
done
