#!/bin/bash
# Dev: on the running D: test session, open the inventory and step down the item list; whenever the plugin's log says
# a MORDHAU weapon's own model has just been shown there, the picture of that moment is kept as
# shots/inv/<prop>.png. usage: inventory_walk.sh [steps, default 60]
cd /e/MordhauSkyrim
L="$USERPROFILE/Documents/My Games/Skyrim Special Edition/SKSE/MordhauSkyrim.log"
mkdir -p shots/inv
bash tools/skyrim_keys.sh "key 0x49;wait 1800;key 0x44;wait 2000" >/dev/null 2>&1
seen=$(grep -c "is shown in the inventory" "$L")
i=0
while [ $i -lt ${1:-60} ]; do
  i=$((i+1))
  now=$(grep -c "is shown in the inventory" "$L")
  if [ "$now" -gt "$seen" ]; then
    seen=$now
    name=$(grep "is shown in the inventory" "$L" | tail -1 | sed 's/.*model of \(w_[A-Za-z0-9]*\) is shown.*/\1/')
    bash tools/skyrim_keys.sh "wait 900" >/dev/null 2>&1
    powershell -NoProfile -ExecutionPolicy Bypass -File tools/grab_skyrim.ps1 -Out "E:\\MordhauSkyrim\\shots\\inv\\$name.png" >/dev/null
    echo "$i $name"
  fi
  bash tools/skyrim_keys.sh "key 0x53;wait 700" >/dev/null 2>&1
done
now=$(grep -c "is shown in the inventory" "$L")
if [ "$now" -gt "$seen" ]; then
  name=$(grep "is shown in the inventory" "$L" | tail -1 | sed 's/.*model of \(w_[A-Za-z0-9]*\) is shown.*/\1/')
  powershell -NoProfile -ExecutionPolicy Bypass -File tools/grab_skyrim.ps1 -Out "E:\\MordhauSkyrim\\shots\\inv\\$name.png" >/dev/null
  echo "end $name"
fi
ls shots/inv | wc -l
