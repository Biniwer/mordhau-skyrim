#!/bin/bash
# Dev: report when MORDHAU exits (or after N minutes), with the reason its log gives.
end=$(( $(date +%s) + ${1:-5}*60 ))
while [ $(date +%s) -lt $end ]; do
  sleep 5
  if [ "$(tasklist //FI "IMAGENAME eq Mordhau-Win64-Shipping.exe" 2>/dev/null | grep -c Mordhau)" = "0" ]; then
    echo "$(date +%H:%M:%S) MORDHAU exited: $(grep -a 'Engine exit requested' "$LOCALAPPDATA/Mordhau/Saved/Logs/Mordhau.log" | head -1 | cut -c1-110)"; exit 1
  fi
done
echo "$(date +%H:%M:%S) MORDHAU still running after ${1:-5} minutes"
