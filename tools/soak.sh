#!/bin/bash
# Dev: watch both games for N minutes; report if either closes, and how the stream is doing.
end=$(( $(date +%s) + ${1:-6}*60 ))
while [ $(date +%s) -lt $end ]; do
  sleep 10
  m=$(tasklist //FI "IMAGENAME eq Mordhau-Win64-Shipping.exe" 2>/dev/null | grep -c Mordhau)
  s=$(tasklist //FI "IMAGENAME eq SkyrimSE.exe" 2>/dev/null | grep -c Skyrim)
  age=$(( $(date +%s) - $(stat -c %Y /e/MordhauSkyrim/cache/live_pose.bin 2>/dev/null || echo 0) ))
  if [ "$m" = "0" ] || [ "$s" = "0" ] || [ $age -gt 5 ]; then echo "$(date +%H:%M:%S) PROBLEM mordhau=$m skyrim=$s stream age ${age}s"; exit 1; fi
done
echo "$(date +%H:%M:%S) both games ran the whole ${1:-6} minutes, stream fresh"
