#!/bin/bash
# Dev: on the running D: test session (started with the dev key give=1), hand the player an item and put it into the
# hand, then show what the plugin and MORDHAU made of it. A leading + on the name equips it.
# usage: give_test.sh "+Scimitar" [seconds to wait, default 14]
C="/d/MashupTest/cache-fresh"
L="$USERPROFILE/Documents/My Games/Skyrim Special Edition/SKSE/MordhauSkyrim.log"
before=$(grep -c "" "$L")
printf '%s\n' "$1" > "$C/give.tmp" && mv "$C/give.tmp" "$C/give.txt"
n=0; until [ $n -ge ${2:-14} ]; do n=$((n+1)); ping -n 2 127.0.0.1 >/dev/null; done
tail -n +$((before+1)) "$L" | grep -i "give:\|props: w_\|sounds: Skyrim's own sounds\|wish\|asking MORDHAU for weapon\|weapon:\|hit .* for\|error\|not in the cache" | cut -c1-230 | head -14
