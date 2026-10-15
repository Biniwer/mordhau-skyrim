#!/bin/bash
# Dev: (re)start Skyrim's scene test, sheathe, orbit the camera to the character's front, zoom in, screenshot.
# usage: shot_front.sh <name>   -> shots/now/<name>.png (full) and <name>_crop.png
cd /e/MordhauSkyrim
bash tools/live_run.sh 2 | tail -1 | cut -c1-70
bash tools/skyrim_keys.sh 'key 0x52;wait 2500;rel 1500 0;wait 300;rel 1500 0;wait 300;rel 1200 0;wait 500;wheel 120;wait 200;wheel 120;wait 200;wheel 120;wait 200;wheel 120;wait 200;wheel 120;wait 200;wheel 120;wait 200;wheel 120;wait 1500' >/dev/null
(cd _deps/universal-modder && python -m um win shot --exe SkyrimSE.exe E:/MordhauSkyrim/shots/now/$1.png >/dev/null)
python -c "
from PIL import Image
im=Image.open('shots/now/$1.png'); w,h=im.size
im.crop((int(w*0.3),int(h*0.12),int(w*0.7),h)).resize((960,int(960*h*0.88/(w*0.4)))).save('shots/now/$1_crop.png')"
grep -E "lit by Skyrim|FAILED|shader .* failed" "$USERPROFILE/Documents/My Games/Skyrim Special Edition/SKSE/MordhauSkyrim.log" | head -3 | cut -c1-230
