#!/bin/bash
# Steam launch wrapper for Skyrim SE with the mashup. Launch options:
#
#   /path/to/mordhau-skyrim/linux/skyrim-launch.sh %command%
#
# - starts SKSE (skse64_loader.exe in place of SkyrimSELauncher.exe) when it is installed
# - while Skyrim runs, keeps cache/launch_args.txt filled with a fresh Epic login from Heroic's bundled legendary:
#   the plugin adds those arguments when it starts MORDHAU (plugin/src/Session.cpp ExtraArguments). Without them the
#   Epic build quits with "cannot reach backend services". A login lasts minutes only, hence the refresh.
# - before and after, takes the mod's loader (dwmapi.dll, dwmapi.dll.old*) out of MORDHAU's folder
set -uo pipefail

APP=2e26af55c1a146e7a939dcc1014fc9fd    # MORDHAU on the Epic Games Store
HGL=com.heroicgameslauncher.hgl
LEG=/app/bin/heroic/resources/app.asar.unpacked/build/bin/x64/linux/legendary
CFG=$HOME/.var/app/$HGL/config/heroic/legendaryConfig/legendary
MORDHAU=${MORDHAU:-$HOME/Games/Heroic/Mordhau}
BIN=$MORDHAU/Mordhau/Binaries/Win64
COMPAT=${STEAM_COMPAT_DATA_PATH:-$HOME/.local/share/Steam/steamapps/compatdata/489830}
CACHE=$COMPAT/pfx/drive_c/users/steamuser/AppData/Local/MordhauSkyrim/cache
ARGS=$CACHE/launch_args.txt
EVERY=90    # seconds between logins

log() { echo "[mordhau-skyrim] $*" >&2; }

clear_loader() {
    rm -f "$BIN/dwmapi.dll" "$BIN"/dwmapi.dll.old*
}

refresh() {
    local tmp=$ARGS.tmp pid seen="" since
    while true; do
        mkdir -p "$CACHE"
        if flatpak run --command=$LEG --env=LEGENDARY_CONFIG_PATH=$CFG $HGL \
            launch $APP --json --skip-version-check 2>/dev/null |
            python3 -I -c 'import json,sys; print("\n".join(json.load(sys.stdin)["egl_parameters"]))' \
            > "$tmp" 2>/dev/null && [ -s "$tmp" ]; then
            chmod 600 "$tmp"
            mv -f "$tmp" "$ARGS"
        else
            rm -f "$tmp"
            log "could not get an Epic login from legendary (logged in to Epic in Heroic?)"
        fi
        # an Epic login is good for one start only: once a new MORDHAU has started (and read the file), a fresh one
        # is put in at once, so the plugin's next start - after a crash, seconds later - never finds a used one
        since=0
        while [ $since -lt $EVERY ]; do
            pid=$(pgrep -f Mordhau-Win64-Shipping.exe | head -1)
            if [ -n "$pid" ] && [ "$pid" != "$seen" ]; then seen=$pid; break; fi
            sleep 2; since=$((since + 2))
        done
    done
}

clear_loader
refresh &
refresher=$!
trap 'kill $refresher 2>/dev/null; rm -f "$ARGS" "$ARGS.tmp"; clear_loader' EXIT

# SKSE in place of Bethesda's launcher, when it is there
args=("$@")
for i in "${!args[@]}"; do
    case ${args[$i]} in
        *SkyrimSELauncher.exe)
            skse=${args[$i]%SkyrimSELauncher.exe}skse64_loader.exe
            [ -f "$skse" ] && args[$i]=$skse && log "starting through SKSE" ;;
    esac
done

"${args[@]}"
