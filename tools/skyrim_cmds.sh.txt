#!/bin/bash
# Dev: type several console commands into the running Skyrim in ONE console session (opened once, closed once).
# Letters, digits, space, dot and minus only. usage: skyrim_cmds.sh "coc Riverwood" "player.additem 12EB7 1" ...
# A command "WAIT <seconds>" pauses (with the console closed and reopened around it: a cell load closes it itself).
cd /e/MordhauSkyrim
D=_deps/universal-modder/um/ps1/WinDrive.ps1
drive() { printf "$1" | powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -File $D -Proc SkyrimSE >/dev/null; }
keys() {
  local out=""
  for ch in $(echo -n "$1" | tr 'a-z' 'A-Z' | sed 's/./& /g; s/  / SP /g'); do
    if [ "$ch" = "SP" ]; then code=32; elif [ "$ch" = "." ]; then code=190; elif [ "$ch" = "-" ]; then code=189; else code=$(printf '%d' "'$ch"); fi
    out="$out$(printf 'hold 0x%X 40' $code)\n"
  done
  printf '%s' "$out"
}
open=0
for cmd in "$@"; do
  if [[ "$cmd" == WAIT* ]]; then
    sleep "${cmd#WAIT }"; open=0; continue      # (after a cell load the console is closed)
  fi
  if [ $open = 0 ]; then drive 'focus\nscanmode on\nkey 0xC0\n'; sleep 1.5; open=1; fi
  drive "focus\nscanmode on\n$(keys "$cmd")key 0x0D\n"; sleep 1.2
done
if [ $open = 1 ]; then drive 'focus\nscanmode on\nkey 0xC0\n'; sleep 1; fi
