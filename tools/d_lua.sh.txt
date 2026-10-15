#!/bin/bash
# Dev: run a Lua snippet inside the MORDHAU of the D: test copy and print what it answered. usage: d_lua.sh '<lua>'
U="/d/MashupTest/Mordhau/Mordhau/Binaries/Win64/UE4SS.log"; C=/d/MashupTest/cache-fresh
before=$(grep -a -c "" "$U")
printf '%s\n' "$1" > $C/live_cmd.tmp && mv $C/live_cmd.tmp $C/live_cmd.lua
until tail -n +$((before+1)) "$U" | grep -a -q "snippet"; do sleep 0.3; done
tail -n +$((before+1)) "$U" | grep -a "snippet" | cut -c50-260
