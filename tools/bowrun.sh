#!/bin/bash
# Dev: restart both games in the bow test (Riverwood), switch to first person and record one draw-and-loose cycle.
# usage: bowrun.sh <name> [extra dev keys] [first person 1/0] [seconds]   -> shots/live/<name>.mkv / .png (12 frames)
cd /e/MordhauSkyrim
bash tools/start_mordhau.sh 1920 1080 >/dev/null 2>&1
taskkill //IM SkyrimSE.exe //F >/dev/null 2>&1; sleep 2
cp build/dist/DirectionalMelee.asi "/e/steam object/steamapps/common/Skyrim Special Edition/DirectionalMelee.asi"
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_test.ps1 -Mode 2 -Cell Riverwood -Extra "char=1;charoffset=0;move=1;god=1;bowtest=1;$2" -WaitFor 'bowtest: draw' -Seconds 220 2>&1 | grep -E "exited|FAILED" | cut -c1-100
L="$USERPROFILE/Documents/My Games/Skyrim Special Edition/SKSE/MordhauSkyrim.log"
[ "${3:-1}" = "1" ] && bash tools/skyrim_keys.sh 'key 0x50;wait 2500' >/dev/null
n=$(grep -c "bowtest: draw" "$L"); for i in $(seq 1 40); do [ $(grep -c "bowtest: draw" "$L") -gt $n ] && break; sleep 0.25; done
S=${4:-5}
bash tools/record_skyrim.sh $1 $S "wait $((S*1000-500))" 3 2 >/dev/null 2>&1
FF="$LOCALAPPDATA/universal-modder/ffmpeg/bin/ffmpeg.exe"
"$FF" -y -loglevel error -i shots/live/$1.mkv -vf "fps=12/$S,scale=640:-2,tile=4x3" -frames:v 1 shots/live/$1.png
echo "loosed $(grep -c 'archery: MORDHAU loosed' "$L"), exceptions $(grep -c exception "$L")"; grep -E "archery: the arrow on|FAILED|not available" "$L" | cut -c16-150 | head -4
grep -a -E "archery: the bow's|frame error|stand-in error|ranged" "/e/steam object/steamapps/common/Mordhau/Mordhau/Binaries/Win64/UE4SS.log" | tail -3 | cut -c22-200
