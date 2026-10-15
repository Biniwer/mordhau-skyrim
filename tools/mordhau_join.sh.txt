#!/bin/bash
# Dev: in a running MORDHAU local match, join and spawn as the first mercenary in the list. Args: window width height.
D=/e/MordhauSkyrim/_deps/universal-modder/um/ps1/WinDrive.ps1
W="/e/steam object/steamapps/common/Mordhau/Mordhau/Binaries/Win64"
drive() { printf "$1" | powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -File "$D" -Proc Mordhau-Win64-Shipping | tr '\r\n' '  '; }
X=${1:-1280}; Y=${2:-720}
drive "focus\nclick $((X/2)) $((Y/2))\n"; sleep 3            # Join the Match
drive "focus\nclick $((X*320/1280)) $((Y*218/720))\n"; sleep 2   # first mercenary in the list
drive "focus\nclick $((X*622/1280)) $((Y*655/720))\n"; sleep 6   # Play
drive "focus\nscanmode on\nclick $((X/2)) $((Y/2))\n"            # spawn
for i in $(seq 1 15); do sleep 1; [ -f /e/MordhauSkyrim/cache/live_pose.bin ] && break; done
echo; ls -la /e/MordhauSkyrim/cache/live_pose.bin 2>&1; grep -a "PoseStream" "$W/UE4SS.log" | tail -4 | cut -c1-250
