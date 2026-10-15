#!/bin/bash
# Dev: with the bow test running, record 4 s at 60 fps from the next draw and lay out the 6 frames from the release on
# (shots/now/fl.png), found by the "loosed" time in the log.
cd /e/MordhauSkyrim
L="$USERPROFILE/Documents/My Games/Skyrim Special Edition/SKSE/MordhauSkyrim.log"
n=$(grep -c "bowtest: draw" "$L"); for i in $(seq 1 80); do [ $(grep -c "bowtest: draw" "$L") -gt $n ] && break; sleep 0.25; done
FF="$LOCALAPPDATA/universal-modder/ffmpeg/bin/ffmpeg.exe"
m=$(grep -c "archery: MORDHAU loosed" "$L")
t0=$(date +%s.%N)
"$FF" -y -loglevel error -f lavfi -i "gfxcapture=window_exe=SkyrimSE.exe:capture_cursor=0:max_framerate=60,hwdownload,format=bgra" -t 4 -c:v libx264 -preset ultrafast -crf 16 shots/live/flight.mkv &
for i in $(seq 1 200); do [ $(grep -c "archery: MORDHAU loosed" "$L") -gt $m ] && break; sleep 0.02; done
t1=$(date +%s.%N); wait
F="$TEMP/claude/E--MordhauSkyrim/4d5fd6ae-a06d-4535-916a-544df30abf7b/scratchpad/fl"; mkdir -p "$F"; rm -f "$F"/g*.png
"$FF" -y -loglevel error -i shots/live/flight.mkv -vf "fps=60,scale=1280:-2" "$(cygpath -w "$F")\g%04d.png"
python - "$F" "$t0" "$t1" "${1:-0.5}" <<'PY'
import sys,glob,os
from PIL import Image
files=sorted(glob.glob(os.path.join(sys.argv[1],'g*.png')))
dt=float(sys.argv[3])-float(sys.argv[2]); crop=float(sys.argv[4])
# (the recorder needs about 0.9 s to start; the log line is seen up to 0.1 s late)
start=max(0,int((dt-1.0)*60)); print(len(files),'frames; loosed %.2f s after the recorder was started; showing from frame'%dt,start)
o=Image.new('RGB',(960*4,540*3))
for k in range(12):
    i=min(len(files)-1,start+k*2)
    im=Image.open(files[i]); w,h=im.size; a=(1-crop)/2
    o.paste(im.crop((int(w*a),int(h*a),int(w*(1-a)),int(h*(1-a)))).resize((960,540)),(960*(k%4),540*(k//4)))
o.save(r'E:\MordhauSkyrim\shots\now\fl.png')
PY
