#!/bin/bash
# Dev: send WinDrive commands (separated by ;) to Skyrim; "wait <ms>" pauses.
cd /e/MordhauSkyrim
printf 'focus;scanmode on;%s' "$1" | tr ';' '\n' | sed 's/^ *//; s/^wait /hold 0x87 /' | powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -File _deps/universal-modder/um/ps1/WinDrive.ps1 -Proc SkyrimSE | tr '\r\n' '  '
