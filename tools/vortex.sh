#!/bin/bash
# Dev: send WinDrive commands (';'-separated) to Vortex, then screenshot it to shots/mm/<name>.png
cd /e/MordhauSkyrim
printf '%s\n' "focus" $(echo "$1" | tr ' ' '_' | tr ';' ' ') | tr '_' ' ' | powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -File _deps/universal-modder/um/ps1/WinDrive.ps1 -Proc Vortex | tr '\n' ' '
sleep ${3:-2}
(cd _deps/universal-modder && python -m um win shot --exe Vortex.exe E:/MordhauSkyrim/shots/mm/$2.png >/dev/null)
