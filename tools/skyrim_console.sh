#!/bin/bash
# Dev: type one console command into the running Skyrim (letters, digits, space and the dot). usage: skyrim_console.sh "player.advskill onehanded 5000"
cd /e/MordhauSkyrim
D=_deps/universal-modder/um/ps1/WinDrive.ps1
drive() { printf "$1" | powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -File $D -Proc SkyrimSE >/dev/null; }
drive 'focus\nscanmode on\nkey 0xC0\n'; sleep 1.5
cmds='focus\nscanmode on\n'
for ch in $(echo -n "$1" | tr 'a-z' 'A-Z' | sed 's/./& /g; s/  / SP /g'); do
  if [ "$ch" = "SP" ]; then code=32; elif [ "$ch" = "." ]; then code=190; else code=$(printf '%d' "'$ch"); fi
  cmds="$cmds$(printf 'hold 0x%X 40' $code)\n"
done
drive "${cmds}key 0x0D\n"; sleep 1.5
drive 'focus\nscanmode on\nkey 0xC0\n'; sleep 1
