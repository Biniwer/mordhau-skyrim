#!/bin/bash
# Dev: the user's own Skyrim (E:) runs the mod the way players get it since 2026-10-11 - the SKSE plugin in
# Data/SKSE/Plugins with MORDHAU's side beside it, started by skse64_loader.exe, no launcher window. This puts the
# current build there (the mod-manager package, freshly made). Both games must be closed.
# usage: install_own.sh
cd /e/MordhauSkyrim
if tasklist | grep -q -i "SkyrimSE\|Mordhau-Win64"; then echo "close both games first"; exit 1; fi
python tools/make_modmanager_release.py | head -1
D="/e/steam object/steamapps/common/Skyrim Special Edition/Data"
cp -r release/MordhauSkyrim-ModManager/SKSE "$D/"
cp release/MordhauSkyrim-ModManager/MordhauWeapons.esp "$D/"
python tools/install_own_ini.py
cmp build/dist/DirectionalMelee.asi "$D/SKSE/Plugins/MordhauSkyrim.dll" && echo "plugin in place"
cmp mordhau_side/PoseStream/Scripts/main.lua "$D/SKSE/Plugins/MordhauSkyrim/mordhau/Mods/PoseStream/Scripts/main.lua" && echo "script in place"
