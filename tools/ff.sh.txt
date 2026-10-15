#!/bin/bash
# Dev: drive the Firefox window (WinDrive commands separated by ';') and save a screenshot to shots/now/ff_s.png (1600 wide).
# usage: ff.sh "click 904 548;wait 2000"
cd /e/MordhauSkyrim
cmds="focus\n"; IFS=';' read -ra parts <<< "$1"
run() { printf "$1" | powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -File _deps/universal-modder/um/ps1/WinDrive.ps1 -Proc firefox 2>&1 | tr '\n' ' '; }
for p in "${parts[@]}"; do
  p=$(echo "$p" | sed 's/^ *//')
  if [[ "$p" == wait* ]]; then run "$cmds"; cmds="focus\n"; sleep $(echo "${p#wait }" | awk '{print $1/1000}'); else cmds="$cmds$p\n"; fi
done
[ "$cmds" != "focus\n" ] && run "$cmds"
echo
(cd _deps/universal-modder && python -m um win shot --exe firefox.exe E:/MordhauSkyrim/shots/now/ff.png >/dev/null 2>&1)
python -c "
from PIL import Image
im=Image.open('shots/now/ff.png'); print(im.size); im.resize((1600,int(1600*im.size[1]/im.size[0]))).save('shots/now/ff_s.png')"
