#!/bin/bash
# Dev: send WinDrive commands (';'-separated) to MORDHAU's window, wait, and screenshot it to shots/now/<name>.png
# usage: mordhau_ui.sh "<commands>" <name> [seconds to wait]
cd /e/MordhauSkyrim
printf 'focus;%s' "$1" | tr ';' '\n' | powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -File _deps/universal-modder/um/ps1/WinDrive.ps1 -Proc Mordhau-Win64-Shipping | tr '\r\n' '  '
sleep ${3:-3}
(cd _deps/universal-modder && python -m um win shot --exe Mordhau-Win64-Shipping.exe E:/MordhauSkyrim/shots/now/$2.png --scale 0.5 >/dev/null)
