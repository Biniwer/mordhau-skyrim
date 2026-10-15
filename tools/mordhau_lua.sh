#!/bin/bash
# Dev: run a Lua snippet inside the running MORDHAU (through the pose-stream script) and print what it logged.
# usage: mordhau_lua.sh '<lua>'   (PAWN = local character, PC = its controller)   |   mordhau_lua.sh --reload
W="/e/steam object/steamapps/common/Mordhau/Mordhau/Binaries/Win64"
before=$(grep -a -c "" "$W/UE4SS.log")
if [ "$1" = "--reload" ]; then
  cp /e/MordhauSkyrim/mordhau_side/PoseStream/Scripts/main.lua "$W/Mods/PoseStream/Scripts/main.lua"
  printf '%s
' 'dofile([[E:\MordhauSkyrim\mordhau_side\PoseStream\Scripts\main.lua]]) return "new script loaded"' > /e/MordhauSkyrim/cache/live_cmd.tmp && mv /e/MordhauSkyrim/cache/live_cmd.tmp /e/MordhauSkyrim/cache/live_cmd.lua
  sleep 1
else
  printf '%s\n' "$1" > /e/MordhauSkyrim/cache/live_cmd.tmp && mv /e/MordhauSkyrim/cache/live_cmd.tmp /e/MordhauSkyrim/cache/live_cmd.lua
  sleep ${2:-1}
fi
tail -n +$((before+1)) "$W/UE4SS.log" | grep -a -v "tick error" | cut -c22-420 | head -${3:-40}
