#!/bin/bash
# Dev: download a Nexus mod's main file through the signed-in Firefox window (Manual -> requirements popup -> slow download).
# usage: ffget.sh <mod id> [x of the Manual button, default 716]   -> prints the newest file in Downloads
cd /e/MordhauSkyrim
before=$(ls -t "$USERPROFILE/Downloads" | head -1)
bash tools/ff.sh "key 0x11 down;key 0x4C;key 0x11 up;wait 500;type https://www.nexusmods.com/skyrimspecialedition/mods/$1?tab=files;key 0x0D;wait 9000;click ${2:-716} 366;wait 4500;click 842 210;wait 5500;click 881 76;wait 2500;click 449 538;wait 3000" >/dev/null
for i in $(seq 1 40); do sleep 2; now=$(ls -t "$USERPROFILE/Downloads" | grep -v "\.part$" | head -1); [ "$now" != "$before" ] && ! ls "$USERPROFILE/Downloads" | grep -q "\.part$" && break; done
ls -lt "$USERPROFILE/Downloads" | sed -n 2p | awk '{print $5, $9, $10, $11, $12, $13, $14}'
[ "$now" = "$before" ] && echo "NOTHING NEW"
