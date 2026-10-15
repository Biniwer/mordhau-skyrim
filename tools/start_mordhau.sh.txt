#!/bin/bash
# Dev: start MORDHAU windowed with the pose-stream script, wait for its local map, join and spawn. Args: width height.
W="/e/steam object/steamapps/common/Mordhau/Mordhau/Binaries/Win64"
taskkill //IM Mordhau-Win64-Shipping.exe //F >/dev/null 2>&1; sleep 2
rm -f /e/MordhauSkyrim/cache/live_pose.bin
# keep the last session's script log (UE4SS starts it afresh; needed when MORDHAU closed by itself): the newest 12
mkdir -p /e/MordhauSkyrim/cache/logs
[ -s "$W/UE4SS.log" ] && cp "$W/UE4SS.log" "/e/MordhauSkyrim/cache/logs/UE4SS-$(date -r "$W/UE4SS.log" +%m%d-%H%M%S).log"
ls -t /e/MordhauSkyrim/cache/logs/UE4SS-*.log 2>/dev/null | tail -n +13 | xargs -r rm -f
cp /e/MordhauSkyrim/mordhau_side/PoseStream/Scripts/main.lua "$W/Mods/PoseStream/Scripts/main.lua"   # always the current script
printf '%s' "${MASHUP_CACHE:-E:\\MordhauSkyrim\\cache}" > "$W/Mods/PoseStream/cache_dir.txt"   # where the shared cache is (the script reads this)
[ -f "$W/dwmapi.dll.mashup" ] && cp "$W/dwmapi.dll.mashup" "$W/dwmapi.dll"   # the loader is only present for mashup sessions
date +%s > /e/MordhauSkyrim/cache/live_session.txt
(cd "$W" && SteamAppId=629760 SteamGameId=629760 cmd //c start "" Mordhau-Win64-Shipping.exe -windowed -ResX=${1:-1280} -ResY=${2:-720})
sleep 10
for i in $(seq 1 120); do sleep 2; tasklist //FI "IMAGENAME eq Mordhau-Win64-Shipping.exe" 2>/dev/null | grep -q Mordhau || { echo "MORDHAU is not running"; break; }; grep -a -q "opening local map" "$W/UE4SS.log" && grep -a -q "WaitingToStart to InProgress" "$LOCALAPPDATA/Mordhau/Saved/Logs/Mordhau.log" 2>/dev/null && break; done
echo "map up after $((i*2+10)) s"; sleep 6
# the script asks MORDHAU to spawn by itself; the menu clicks are only the fallback
fresh() { [ -f /e/MordhauSkyrim/cache/live_pose.bin ] && [ $(( $(date +%s) - $(stat -c %Y /e/MordhauSkyrim/cache/live_pose.bin) )) -le 2 ]; }   # (written in the last 2 s)
for i in $(seq 1 25); do sleep 1; fresh && break; done
if fresh; then echo "spawned without the menu"; grep -a "PoseStream" "$W/UE4SS.log" | tail -4 | cut -c1-250
else bash /e/MordhauSkyrim/tools/mordhau_join.sh "$@"; fi
# the loader is loaded now: set it aside again so a normal MORDHAU launch has none
if [ -f "$W/dwmapi.dll.mashup" ] && [ -f "$W/dwmapi.dll" ]; then rm -f "$W/dwmapi.dll.old"; mv "$W/dwmapi.dll" "$W/dwmapi.dll.old"; fi
