#!/bin/bash
# Records Skyrim's window (60 fps) with the sound of BOTH games (Skyrim's own, and MORDHAU's - the fighting sounds come
# from MORDHAU) and makes one mp4 of it. usage: record_demo.sh <name> <seconds>   -> recordings/<name>.mp4
cd /e/MordhauSkyrim
NAME=${1:-demo}; SECS=${2:-180}
FF="$LOCALAPPDATA/universal-modder/ffmpeg/bin/ffmpeg.exe"
LOOP=_deps/universal-modder/um/ps1/ProcLoopback.ps1
mkdir -p recordings
V=recordings/$NAME.video.mkv; A1=recordings/$NAME.skyrim.raw; A2=recordings/$NAME.mordhau.raw
rm -f "$V" "$A1" "$A2" recordings/$NAME.a1.txt recordings/$NAME.a2.txt
SP=$(powershell -NoProfile -Command "(Get-Process SkyrimSE -ErrorAction SilentlyContinue | Select-Object -First 1).Id" | tr -d '\r')
MP=$(powershell -NoProfile -Command "(Get-Process Mordhau-Win64-Shipping -ErrorAction SilentlyContinue | Select-Object -First 1).Id" | tr -d '\r')
[ -z "$SP" ] && { echo "Skyrim is not running"; exit 1; }
# the two sound captures first (each needs a second or two to start; the time each really began is noted)
(sleep $((SECS + 6)); echo stop) | powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -File $LOOP -TargetPid $SP -Out "$(cygpath -w "$A1")" 2>/dev/null | while read -r line; do date +%s.%N > recordings/$NAME.a1.txt; break; done &
if [ -n "$MP" ]; then
  (sleep $((SECS + 6)); echo stop) | powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -File $LOOP -TargetPid $MP -Out "$(cygpath -w "$A2")" 2>/dev/null | while read -r line; do date +%s.%N > recordings/$NAME.a2.txt; break; done &
fi
for i in $(seq 1 40); do [ -s recordings/$NAME.a1.txt ] && break; sleep 0.25; done
T0=$(date +%s.%N)
"$FF" -y -loglevel error -f lavfi -i "gfxcapture=window_exe=SkyrimSE.exe:capture_cursor=0:max_framerate=60,hwdownload,format=bgra" -t $SECS -c:v libx264 -preset veryfast -crf 21 -pix_fmt yuv420p "$V"
sleep 7
# offsets: how long before the picture each sound capture began (the recorder needs about 0.45 s before its first frame: measured 2026-10-08 by matching movement in the picture to sound onsets; 0.9 put the sound half a second early)
off() { [ -s "$1" ] && python -c "print('%.3f' % ($T0 + 0.45 - float(open(r'$1').read())))" || echo 0; }
O1=$(off recordings/$NAME.a1.txt); O2=$(off recordings/$NAME.a2.txt)
if [ -s "$A2" ]; then
  "$FF" -y -loglevel error -i "$V" -ss $O1 -f f32le -ar 48000 -ac 2 -i "$A1" -ss $O2 -f f32le -ar 48000 -ac 2 -i "$A2" \
    -filter_complex "[1:a]volume=1.0[a];[2:a]volume=${MORDHAU_GAIN:-6.0}[b];[a][b]amix=inputs=2:normalize=0[m]" -map 0:v -map "[m]" -c:v copy -c:a aac -b:a 192k -shortest recordings/$NAME.mp4
else
  "$FF" -y -loglevel error -i "$V" -ss $O1 -f f32le -ar 48000 -ac 2 -i "$A1" -map 0:v -map 1:a -c:v copy -c:a aac -b:a 192k -shortest recordings/$NAME.mp4
fi
ls -la recordings/$NAME.mp4 | awk '{printf "recordings/'$NAME'.mp4  %.0f MB\n", $5/1048576}'
