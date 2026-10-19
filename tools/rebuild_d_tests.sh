#!/bin/bash
# Dev: rebuilds the D: test copies that tools/d_test.sh and tools/safety_tests.sh use (D:\MashupTest), from the games
# on E:: Skyrim 1.7.104 (the Steam install), Skyrim 1.6.1170 (the playthrough's folder) and MORDHAU (the Steam install).
# Only the games' own files are copied - by name, since the E: folders also hold mods - plus SKSE, Address Library and
# SkyUI from downloads/. MORDHAU without anything of this mod (UE4SS, its script, the loader), crash dumps or mod.io
# downloads. Both games must be closed (MORDHAU's archives are read while it is copied). Takes 15-20 minutes.
# usage: rebuild_d_tests.sh
cd /e/MordhauSkyrim
if tasklist | grep -q -i "SkyrimSE\|Mordhau-Win64"; then echo "close both games first"; exit 1; fi
T=/d/MashupTest
SE="/e/steam object/steamapps/common/Skyrim Special Edition"
AE="/e/Skyrim 1.6.1170"
MH='E:\steam object\steamapps\common\Mordhau'
[ -e "$T" ] && { echo "$T exists already - not touched"; exit 1; }

# Skyrim's own files only: the game, its inis, Bethesda's plugins and archives, the intro video
vanilla() {
  local src=$1 dst=$2
  mkdir -p "$dst/Data" "$dst/Skyrim"
  for f in SkyrimSE.exe SkyrimSELauncher.exe bink2w64.dll steam_api64.dll High.ini Low.ini Medium.ini Ultra.ini Skyrim_Default.ini Skyrim.ccc installscript.vdf; do
    cp -p "$src/$f" "$dst/" || { echo "missing: $src/$f"; return 1; }
  done
  [ -f "$src/Skyrim/SkyrimPrefs.ini" ] && cp -p "$src/Skyrim/SkyrimPrefs.ini" "$dst/Skyrim/"
  (cd "$src/Data" && cp -p Skyrim.esm Update.esm Dawnguard.esm HearthFires.esm Dragonborn.esm MarketplaceTextures.bsa "Skyrim - "*.bsa _ResourcePack.bsa _ResourcePack.esl "$dst/Data/")
  (cd "$src/Data" && for f in cc*.esm cc*.esl cc*.bsa; do [ -f "$f" ] && cp -p "$f" "$dst/Data/"; done)
  cp -rp "$src/Data/Video" "$dst/Data/"
}
# SKSE (loader, its dll, its scripts), the one Address Library file for the version, SkyUI
extras() {
  local dst=$1 skse=$2 versionlib=$3
  cp -p "$skse"/skse64_loader.exe "$skse"/skse64_1_*.dll "$dst/"
  cp -rp "$skse/Data/." "$dst/Data/"
  mkdir -p "$dst/Data/SKSE/Plugins"
  cp -p "$versionlib" "$dst/Data/SKSE/Plugins/"
  cp -p downloads/skyui/SkyUI_SE.esp downloads/skyui/SkyUI_SE.bsa "$dst/Data/"
}

echo "== Skyrim 1.7.104 -> $T/Skyrim Special Edition"
vanilla "$SE" "$T/Skyrim Special Edition" && extras "$T/Skyrim Special Edition" downloads/skse/skse64_2_03_01 "$SE/Data/SKSE/Plugins/versionlib-1-7-104-0.bin" || exit 1
echo "== Skyrim 1.6.1170 -> $T/Skyrim1170"
vanilla "$AE" "$T/Skyrim1170" && extras "$T/Skyrim1170" downloads/skse1170/skse64_2_02_06 "$AE/Data/SKSE/Plugins/versionlib-1-6-1170-0.bin" || exit 1
for v in "$T/Skyrim Special Edition" "$T/Skyrim1170"; do
  echo "$v: $(powershell -NoProfile -Command "(Get-Item '$(cygpath -w "$v/SkyrimSE.exe")').VersionInfo.FileVersion")"
done

echo "== MORDHAU -> $T/Mordhau (about 39 GB)"
# (through PowerShell: through cmd's start the quotes round the path with a space were lost and robocopy refused)
powershell -NoProfile -Command "\$p = Start-Process robocopy -ArgumentList '\"$MH\" D:\\MashupTest\\Mordhau /E /XD Mods UE4SS_Signatures .modio /XF UE4SS* dwmapi* crash_*.dmp MordhauSkyrim-closed*.txt /NFL /NDL /NP /MT:8 /R:1 /W:1' -NoNewWindow -PassThru; \$p.PriorityClass = 'Idle'; \$p.WaitForExit(); 'robocopy exit ' + \$p.ExitCode" | tail -6
ls "$T/Mordhau/Mordhau/Binaries/Win64"
du -sh "$T"/* | tail -4
