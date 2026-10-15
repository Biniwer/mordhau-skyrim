#!/bin/bash
# Dev: (re)start Skyrim with the freshly built plugin in the scene-only test, next to an already running MORDHAU.
# usage: live_run.sh [autotest mode, default 2] [extra dev settings]
cd /e/MordhauSkyrim
taskkill //IM SkyrimSE.exe //F >/dev/null 2>&1; sleep 2
cp build/dist/DirectionalMelee.asi "/e/steam object/steamapps/common/Skyrim Special Edition/DirectionalMelee.asi" || exit 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_test.ps1 -Mode ${1:-2} -Extra "char=1;charoffset=0;move=1;${2}" -WaitFor 'autotest: weapon drawn' -Seconds 100 2>&1 | grep -E "link:|weapon drawn|wait for|exited|FAILED|warn|error" | cut -c1-200
