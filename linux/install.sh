#!/bin/bash
# Installs the mashup for Skyrim SE (Steam, Proton) + MORDHAU (Epic, installed by Heroic) on Linux.
#
#   linux/install.sh <folder of the unzipped MordhauSkyrim-Data build artifact>
#
# What it does:
#   - copies the package (SKSE/Plugins/MordhauSkyrim.dll, MORDHAU's side beside it) into Skyrim's Data folder
#   - sets MordhauPath in MordhauSkyrim.ini to the Heroic install, as Proton sees it (drive Z:)
#   - makes MordhauWeapons.esp from your own Skyrim.esm and switches it on in the prefix's Plugins.txt
#   - tells Wine, in SKYRIM'S prefix only, to load the mod's own dwmapi.dll for Mordhau-Win64-Shipping.exe (Heroic's
#     MORDHAU prefix, the one used for online play, is not touched)
# Override the locations with SKYRIM=..., MORDHAU=..., PROTON=... in the environment.
set -euo pipefail

PKG=${1:?usage: install.sh <unzipped MordhauSkyrim-Data folder>}
HERE=$(cd "$(dirname "$0")/.." && pwd)
STEAM=$HOME/.local/share/Steam
# the Steam library (Steam's own folder or one added under Settings > Storage) that holds app $1
library() {
    local lib
    while IFS= read -r lib; do
        [ -f "$lib/steamapps/appmanifest_$1.acf" ] && { echo "$lib"; return; }
    done < <(grep -o '"path"[[:space:]]*"[^"]*"' "$STEAM/steamapps/libraryfolders.vdf" | cut -d'"' -f4)
    echo "$STEAM"
}
LIB=$(library 489830)
SKYRIM=${SKYRIM:-$LIB/steamapps/common/Skyrim Special Edition}
COMPAT=$LIB/steamapps/compatdata/489830
PFX=$COMPAT/pfx
MORDHAU=${MORDHAU:-$HOME/Games/Heroic/Mordhau}
PROTON=${PROTON:-$(library 1493710)/steamapps/common/Proton - Experimental}
LOCAL=$PFX/drive_c/users/steamuser/AppData/Local

say() { echo "[install] $*"; }
stop() { echo "[install] STOPPED: $*" >&2; exit 1; }

[ -f "$PKG/SKSE/Plugins/MordhauSkyrim.dll" ] || stop "$PKG has no SKSE/Plugins/MordhauSkyrim.dll"
[ -f "$SKYRIM/SkyrimSE.exe" ] || stop "Skyrim SE not found at $SKYRIM"
[ -f "$MORDHAU/Mordhau/Binaries/Win64/Mordhau-Win64-Shipping.exe" ] || stop "MORDHAU not found at $MORDHAU"
[ -d "$PFX" ] || stop "Skyrim's Proton prefix is missing: start Skyrim once from Steam first"
[ -x "$PROTON/proton" ] || stop "Proton not found at $PROTON"
pgrep -f 'SkyrimSE.exe|Mordhau-Win64-Shipping.exe' >/dev/null && stop "close Skyrim and MORDHAU first"

# the plugin finds Skyrim's functions through the Address Library (Nexus Mods, mod 32444); SKSE starts it
ls "$SKYRIM"/Data/SKSE/Plugins/versionlib-*.bin >/dev/null 2>&1 ||
    say "WARNING: no Address Library (Data/SKSE/Plugins/versionlib-*.bin) yet - get it from Nexus Mods, mod 32444"
[ -f "$SKYRIM/skse64_loader.exe" ] ||
    say "WARNING: no skse64_loader.exe in Skyrim's folder yet - get SKSE64 for your Skyrim version"

say "copying the plugin and MORDHAU's side into $SKYRIM/Data"
cp -r "$PKG/SKSE" "$SKYRIM/Data/"

# Linux path -> the same path through Proton's drive Z:
winpath=Z:$(printf '%s' "$MORDHAU" | tr '/' '\\')
ini=$SKYRIM/Data/SKSE/Plugins/MordhauSkyrim.ini
sed -i "s|^MordhauPath=.*|MordhauPath=${winpath//\\/\\\\}|" "$ini"
say "MordhauPath=$winpath"

say "making MordhauWeapons.esp from your Skyrim.esm"
LOCALAPPDATA=$LOCAL python3 -I "$HERE/tools/make_items_esp.py" "$SKYRIM"

# The plugin is built with Visual Studio 2022, whose std::mutex needs a current msvcp140.dll; Skyrim's prefix comes
# with the 2016 one (14.00.24215), on which the plugin's first mutex lock reads address 0 and SKSE reports "fatal
# error occurred while loading plugin". Steam keeps the 2022 redistributable in Steamworks Shared.
redist="$(library 228980)/steamapps/common/Steamworks Shared/_CommonRedist/vcredist/2022/VC_redist.x64.exe"
[ -f "$redist" ] || stop "VC++ 2022 redistributable not found at $redist"
say "installing the VC++ 2022 runtime into Skyrim's prefix"
STEAM_COMPAT_CLIENT_INSTALL_PATH=$STEAM STEAM_COMPAT_DATA_PATH=$COMPAT \
    "$PROTON/proton" run "$redist" /install /quiet /norestart

say "Wine: native dwmapi.dll for Mordhau-Win64-Shipping.exe, in Skyrim's prefix only"
STEAM_COMPAT_CLIENT_INSTALL_PATH=$STEAM STEAM_COMPAT_DATA_PATH=$COMPAT \
    "$PROTON/proton" run reg add \
    'HKCU\Software\Wine\AppDefaults\Mordhau-Win64-Shipping.exe\DllOverrides' \
    /v dwmapi /d native,builtin /f

say "done. In Steam, set Skyrim's launch options to:"
say "  $HERE/linux/skyrim-launch.sh %command%"
