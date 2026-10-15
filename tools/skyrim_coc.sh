#!/bin/bash
# Dev: in a running Skyrim (main menu or in game), type `coc <cell>` into the console, wait, draw the weapon.
cd /e/MordhauSkyrim
D=_deps/universal-modder/um/ps1/WinDrive.ps1
drive() { printf "$1" | powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -File $D -Proc SkyrimSE >/dev/null; }
L="$USERPROFILE/Documents/My Games/Skyrim Special Edition/SKSE/MordhauSkyrim.log"
until grep -q "combat: hooks installed" "$L" 2>/dev/null; do sleep 2; done; sleep ${2:-10}
drive 'focus\nscanmode on\nkey 0xC0\n'; sleep 1.5
cmds='focus\nscanmode on\n'
for ch in $(echo -n "COC ${1:-WhiterunOrigin}" | tr 'a-z' 'A-Z' | sed 's/./& /g; s/  / SP /g'); do
  if [ "$ch" = "SP" ]; then code=32; else code=$(printf '%d' "'$ch"); fi
  cmds="$cmds$(printf 'hold 0x%X 40' $code)\n"
done
drive "${cmds}key 0x0D\n"; sleep 22
# (the console closes by itself when the cell loads: pressing its key again here OPENED it, and the old draw key R
# was then typed into it. The weapon is drawn with 1 now; R is the alternate grip.)
drive 'focus\nscanmode on\nkey 0x31\n'; sleep 3         # 1: draw the weapon
tail -3 "$L" | cut -c1-170
