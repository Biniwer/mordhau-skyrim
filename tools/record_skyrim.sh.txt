#!/bin/bash
# Dev: record Skyrim's window while sending input. usage: record_skyrim.sh <name> <seconds> '<WinDrive commands separated by ;>'
# ("wait <ms>" is turned into a hold of an unused key.) Writes shots/live/<name>.mkv and a contact sheet <name>.png.
cd /e/MordhauSkyrim
FF="$LOCALAPPDATA/universal-modder/ffmpeg/bin/ffmpeg.exe"; D=_deps/universal-modder/um/ps1/WinDrive.ps1
printf 'focus\n' | powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -File $D -Proc SkyrimSE >/dev/null; sleep 1
"$FF" -y -loglevel error -f lavfi -i "gfxcapture=window_exe=SkyrimSE.exe:capture_cursor=0:max_framerate=60,hwdownload,format=bgra" -t $2 -vf scale=1280:-2 -c:v libx264 -preset ultrafast shots/live/$1.mkv &
sleep 1.2
printf 'focus;scanmode on;%s' "$3" | tr ';' '\n' | sed 's/^ *//; s/^wait /hold 0x87 /' | powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -File $D -Proc SkyrimSE | tr '\r\n' '  '
wait
"$FF" -y -loglevel error -i shots/live/$1.mkv -vf "fps=${4:-4},crop=iw*0.5:ih*0.9:iw*0.25:ih*0.1,scale=420:-2,tile=6x${5:-3}" -frames:v 1 shots/live/$1.png
echo; ls -la shots/live/$1.png | awk '{print $5}'
