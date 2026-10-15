#!/bin/bash
# Dev: list a Nexus mod's files (id | name | version | size) from the files tab's page source, via the Firefox window.
# usage: fffiles.sh <mod id>
cd /e/MordhauSkyrim
bash tools/ff.sh "key 0x11 down;key 0x4C;key 0x11 up;wait 500;type view-source:https://www.nexusmods.com/skyrimspecialedition/mods/$1?tab=files;key 0x0D;wait 9000;click 400 400;wait 300;key 0x11 down;key 0x41;key 0x11 up;wait 700;key 0x11 down;key 0x43;key 0x11 up;wait 1500;click 400 400" >/dev/null
powershell -NoProfile -Command "Get-Clipboard -Raw | Set-Content -Encoding utf8 'E:\MordhauSkyrim\cache\ff_src.txt'"
python - <<'PY'
import re
s=open(r'E:\MordhauSkyrim\cache\ff_src.txt',encoding='utf-8',errors='replace').read()
seen=set()
for m in re.finditer(r'<dt[^>]*data-id="(\d+)"[^>]*data-name="([^"]*)"[^>]*data-size="(\d+)"[^>]*data-version="([^"]*)"[^>]*data-date="(\d+)"', s):
    if m.group(1) in seen: continue
    seen.add(m.group(1)); print(m.group(1), '|', m.group(2), '|', m.group(4), '|', f"{int(m.group(3))/1024:.1f} MB" if int(m.group(3))>1024 else m.group(3)+' KB')
if not seen:
    ids=sorted(set(re.findall(r'file_id=(\d+)', s))); print('no file rows parsed; file ids seen:', ids[:20]); print(len(s),'chars')
PY
