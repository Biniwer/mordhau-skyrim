#!/bin/bash
# Dev: the whole safety test set before a release, unattended, in one go (about 15 minutes). The player's games must be
# closed; the screen is used. MORDHAU is started fewer times than when the tests were run one by one: after a test ends
# it on purpose, the plugin's own session starts it again and the next test waits for that, Skyrim staying open.
# usage: safety_tests.sh <last released backup, e.g. nexus-0.1.17>      -> prints a summary; the full output in shots/now/safety.txt
cd /e/MordhauSkyrim
LOG="$USERPROFILE/Documents/My Games/Skyrim Special Edition/SKSE/MordhauSkyrim.log"
OUT=shots/now/safety.txt
: > "$OUT"
say() { echo "$*" | tee -a "$OUT"; }
lives() { grep -c "session: MORDHAU is live" "$LOG" 2>/dev/null; }
wait_live() { local n=$1 t=0; until [ "$(lives)" -gt "$n" ] || [ $t -ge 300 ]; do sleep 3; t=$((t + 3)); done; [ "$(lives)" -gt "$n" ]; }

say "== package = build: $(cmp -s build/RelWithDebInfo/DirectionalMelee.dll release/MordhauSkyrim-ModManager/SKSE/Plugins/MordhauSkyrim.dll && echo yes || echo NO)"
say "== isolation check against $1"
python tools/isolation_check.py "$1" 2>&1 | tee -a "$OUT" | grep -E "DIFFERENT|NOT all|all identical"
say "== loader"
python tools/loader_test.py 2>&1 | tee -a "$OUT" | grep -E "process lived|folder clean"

say "== session on 1.7.104 (duel)"
timeout 600 bash tools/d_test.sh start "duel=1;autotest=3" >> "$OUT" 2>&1 || { say "the test session did not start - see $OUT"; exit 1; }
sleep 25
say "errors so far: $(grep -c '\[error\]' "$LOG"), hits: $(grep -c 'hit:' "$LOG")"
grep "audio:" "$LOG" | cut -c1-220 | tee -a "$OUT"
python tools/stall_test.py spike 3 2>&1 | tee -a "$OUT"
python tools/stall_test.py spike 12 2>&1 | tee -a "$OUT"
n=$(lives); python tools/stop_test.py 2>&1 | tee -a "$OUT" | grep -E "gone after|left"
wait_live "$n" || say "MORDHAU did NOT come back"
sleep 10
n=$(lives); python tools/stop_test.py PS_testUnknown 2>&1 | tee -a "$OUT" | grep -E "gone after|left"
wait_live "$n" || say "MORDHAU did NOT come back"
sleep 10
python tools/stall_test.py silent 2>&1 | tee -a "$OUT"
say "errors in the whole session: $(grep -c '\[error\]' "$LOG")"
bash tools/d_test.sh stop >> "$OUT" 2>&1

say "== leftover (the known last-session-key case for 30 s)"
LEFTOVER_KNOWN_SECONDS=30 python tools/leftover_test.py 2>&1 | tee -a "$OUT" | grep -E "^---|removed again" | cut -c1-200
# (the loader's warning popups for the hand-started MORDHAUs stay open on the screen and take the focus: closed here)
powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='powershell.exe'\" | Where-Object { \$_.CommandLine -match 'offline only' } | ForEach-Object { Stop-Process -Id \$_.ProcessId -Force }" 2>/dev/null

say "== session on 1.6.1170 (duel)"
D_TEST_SKYRIM=/d/MashupTest/Skyrim1170 timeout 600 bash tools/d_test.sh start "duel=1;autotest=3" >> "$OUT" 2>&1
sleep 30
say "hooks: $(grep -c 'combat: hooks installed' "$LOG"), render hook: $(grep -c 'render hook installed' "$LOG"), errors: $(grep -c '\[error\]' "$LOG"), hits: $(grep -c 'hit:' "$LOG")"
D_TEST_SKYRIM=/d/MashupTest/Skyrim1170 bash tools/d_test.sh stop >> "$OUT" 2>&1
say "plugin list entries afterwards: $(grep -c '^\*' "$LOCALAPPDATA/Skyrim Special Edition/Plugins.txt")"
