#!/bin/bash
# Dev: on the running D: test session, open the inventory and take a picture of every row while stepping down the
# list: shots/inv_all/NN.png. usage: inventory_walk_all.sh [steps, default 50] [key: 0x53 down (default), 0x57 up]
cd /e/MordhauSkyrim
mkdir -p shots/inv_all
bash tools/skyrim_keys.sh "key 0x49;wait 1800;key 0x44;wait 2000" >/dev/null 2>&1
i=0
while [ $i -lt ${1:-50} ]; do
  n=$(printf '%02d' $i)
  powershell -NoProfile -ExecutionPolicy Bypass -File tools/grab_skyrim.ps1 -Out "E:/MordhauSkyrim/shots/inv_all/$n.png" >/dev/null
  i=$((i+1))
  bash tools/skyrim_keys.sh "key ${2:-0x53};wait 1300" >/dev/null 2>&1
done
ls shots/inv_all | wc -l
