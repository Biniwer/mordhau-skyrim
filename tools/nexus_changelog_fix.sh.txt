#!/bin/bash
# Dev: on the Nexus Documents page (already open in Firefox), replace one changelog entry's text.
# usage: nexus_changelog_fix.sh <version> <y of that entry's menu button, in window pixels>
cd /e/MordhauSkyrim
powershell -NoProfile -ExecutionPolicy Bypass -File tools/clip.ps1 -File "release/changelogs/$1.txt"
EDIT=$(( $2 + 46 ))
bash tools/ff.sh "click 1186 $2;wait 1800;click 1051 $EDIT;wait 2500;click 940 625;wait 300;key 0x11 down;key 0x41;key 0x11 up;wait 200;key 0x11 down;key 0x56;key 0x11 up;wait 1200;click 1157 787;wait 5000" > /dev/null
bash tools/ff.sh "wait 500" | tail -1
