#!/bin/bash
# Dev: while Skyrim runs, keep MORDHAU running beside it (MORDHAU sometimes closes by itself; the play launcher has
# its own watch for this, the test tools did not). Ends when Skyrim closes.
cd /e/MordhauSkyrim
running() { tasklist //FI "IMAGENAME eq $1" 2>/dev/null | grep -q "${1%%.*}"; }
while running SkyrimSE.exe; do
  if ! running Mordhau-Win64-Shipping.exe; then
    echo "$(date +%H:%M:%S) MORDHAU is gone: starting it again" | tee -a cache/keep_mordhau.log
    bash tools/start_mordhau.sh 1920 1080 >> cache/keep_mordhau.log 2>&1
  fi
  sleep 3
done
echo "$(date +%H:%M:%S) Skyrim closed" | tee -a cache/keep_mordhau.log
