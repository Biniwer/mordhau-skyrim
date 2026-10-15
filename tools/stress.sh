#!/bin/bash
# Dev: one stress run - start Skyrim in a cell with extra dev keys, wait, spam input, report.
# usage: stress.sh <cell> "<extra dev keys>" <spam seconds> <seed> [menus 0/1]
cd /e/MordhauSkyrim
taskkill //IM SkyrimSE.exe //F >/dev/null 2>&1; sleep 2
tasklist | grep -qi mordhau-win64 || bash tools/start_mordhau.sh 1920 1080 >/dev/null 2>&1
cp build/dist/DirectionalMelee.asi "/e/steam object/steamapps/common/Skyrim Special Edition/DirectionalMelee.asi" || exit 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_test.ps1 -Mode 2 -Cell "$1" -Extra "char=1;charoffset=0;move=1;$2" -WaitFor 'autotest: weapon drawn' -Seconds 140 2>&1 | grep -E "exited|FAILED" | cut -c1-160
L="$USERPROFILE/Documents/My Games/Skyrim Special Edition/SKSE/MordhauSkyrim.log"
sleep 20
echo "== $1 [$2]: $(grep -E 'crowd:|duel: opponent 1' "$L" | tail -1 | cut -c16-110)"
bash tools/spam_test.sh "$3" "$4" "${5:-0}"
echo "  figures drawn / stand-ins: $(grep -a -c 'holds BP_' "/e/steam object/steamapps/common/Mordhau/Mordhau/Binaries/Win64/UE4SS.log") weapon hand-overs in MORDHAU's log so far"
