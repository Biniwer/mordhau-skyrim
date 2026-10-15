#!/bin/bash
# Dev: download one Nexus file (slow download) through the signed-in Firefox window. usage: ffdl.sh <mod id> <file id>
cd /e/MordhauSkyrim
D="$USERPROFILE/Downloads"; before=$(ls "$D" | wc -l)
bash tools/ff.sh "key 0x11 down;key 0x4C;key 0x11 up;wait 500;type https://www.nexusmods.com/skyrimspecialedition/mods/$1?tab=files&file_id=$2;key 0x0D;wait 9000;click 449 538;wait 8000" >/dev/null
for i in $(seq 1 150); do [ $(ls "$D" | grep -v "\.part$" | wc -l) -gt $before ] && ! ls "$D" | grep -q "\.part$" && break; sleep 2; done
ls -lt "$D" | sed -n 2p | cut -c30-220
