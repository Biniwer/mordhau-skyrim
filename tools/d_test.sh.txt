#!/bin/bash
# Dev: the mod-manager package on the D: copies (D:\MashupTest), started through SKSE with a test scene.
# usage: d_test.sh start "<dev keys, ;-separated>" [cell]   -> unpacks release/MordhauSkyrim-ModManager.zip into the D: Skyrim's Data,
#                                                              starts it, waits for MORDHAU and for the duel's opponent
#        d_test.sh stop                                      -> closes both games, puts the plugin list back
cd /e/MordhauSkyrim
S="${D_TEST_SKYRIM:-/d/MashupTest/Skyrim Special Edition}"   # (D_TEST_SKYRIM: another test copy, e.g. /d/MashupTest/Skyrim1170)
LOG="$USERPROFILE/Documents/My Games/Skyrim Special Edition/SKSE/MordhauSkyrim.log"
LIST="$LOCALAPPDATA/Skyrim Special Edition/Plugins.txt"
if [ "$1" = "start" ]; then
  (cd "$S/Data" && "/c/Program Files/7-Zip/7z.exe" x -y /e/MordhauSkyrim/release/MordhauSkyrim-ModManager.zip > /dev/null)
  python tools/d_test_ini.py
  printf '%s\n' "$2" | tr ';' '\n' > "$S/DirectionalMelee.dev.txt"
  [ "$(grep -c "^*" "$LIST")" -ge 3 ] && cp "$LIST" downloads/Plugins.before-d-test.txt   # (never a list a test has already cut down)
  printf '*MordhauWeapons.esp\r\n' > "$LIST"
  : > "$LOG"
  (cd "$S" && SteamAppId=489830 SteamGameId=489830 MORDHAU_SKYRIM= cmd //c start "" skse64_loader.exe)
  until grep -q "session: MORDHAU is live" "$LOG" || ! tasklist | grep -qi SkyrimSE; do sleep 3; done
  tasklist | grep -qi SkyrimSE || { echo "SKYRIM IS GONE (crashed?)"; tail -3 "$LOG"; exit 1; }
  bash tools/skyrim_coc.sh "${3:-WhiterunOrigin}" 3 >/dev/null 2>&1
  [ -n "$D_TEST_NODUEL" ] && { echo "in the cell, no duel"; exit 0; }   # (D_TEST_NODUEL=1: a scene without the duel opponent)
  until grep -q "duel: opponent 1 placed" "$LOG" || ! tasklist | grep -qi SkyrimSE; do sleep 2; done
  tasklist | grep -qi SkyrimSE || { echo "SKYRIM IS GONE (crashed?)"; tail -3 "$LOG"; exit 1; }
  grep -o "duel: opponent 1 placed ([^)]*" "$LOG"
elif [ "$1" = "stop" ]; then
  taskkill //F //IM SkyrimSE.exe >/dev/null 2>&1
  until ! tasklist | grep -q -i "Mordhau-Win64\|SkyrimSE"; do sleep 2; done
  cp downloads/Plugins.before-d-test.txt "$LIST"
  : > "$S/DirectionalMelee.dev.txt"
  echo "both closed; plugin list:"; tail -5 "$LIST"
fi
