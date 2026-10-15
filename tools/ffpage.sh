#!/bin/bash
# Dev: open a URL in the Firefox window and print the page's text (select all + copy). usage: ffpage.sh <url> [wait ms]
cd /e/MordhauSkyrim
bash tools/ff.sh "key 0x11 down;key 0x4C;key 0x11 up;wait 500;type $1;key 0x0D;wait ${2:-9000};click 60 420;wait 300;key 0x11 down;key 0x41;key 0x11 up;wait 600;key 0x11 down;key 0x43;key 0x11 up;wait 900;click 60 420" >/dev/null
powershell -NoProfile -Command "Get-Clipboard" | grep -v "^\s*$"
