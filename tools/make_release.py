"""Puts together the folder other people install from: release/MordhauSkyrim/. Nothing of either game goes in - no
cache, no baked models, clips or textures (those are made on each player's PC from their own MORDHAU). What goes in:
this mashup's own plugin, script, launcher, installer, the weapons plugin (records only), and two open-source loaders
(Ultimate ASI Loader for Skyrim, UE4SS for MORDHAU) with their licences.
usage: make_release.py [<out folder>]"""
import json, os, shutil, sys, hashlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'release', 'MordhauSkyrim')
SKYRIM = r'E:\steam object\steamapps\common\Skyrim Special Edition'
MORDHAU_BIN = os.path.join(ROOT, 'vendor', 'ue4ss')   # (UE4SS as shipped, kept in the project)

def put(src, rel):
    dst = os.path.join(OUT, rel)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    shutil.copy2(src, dst)
    return dst

if os.path.isdir(OUT):
    shutil.rmtree(OUT)
os.makedirs(OUT)
# this mashup's own
put(os.path.join(ROOT, 'build', 'dist', 'DirectionalMelee.asi'), r'payload\skyrim\DirectionalMelee.asi')
put(os.path.join(ROOT, 'mordhau_side', 'PoseStream', 'Scripts', 'main.lua'), r'mordhau_side\PoseStream\Scripts\main.lua')
put(os.path.join(ROOT, 'Play-Mashup.ps1'), 'Play-Mashup.ps1')
put(os.path.join(ROOT, 'installer', 'Install-Mashup.ps1'), 'Install-Mashup.ps1')
put(os.path.join(ROOT, 'installer', 'Uninstall-Mashup.ps1'), 'Uninstall-Mashup.ps1')
put(os.path.join(ROOT, 'installer', 'Make-Mordhau-Online-Safe.ps1'), 'Make-Mordhau-Online-Safe.ps1')
put(os.path.join(ROOT, 'installer', 'README.md'), 'README.md')
put(os.path.join(ROOT, 'HOW-TO-PLAY.md'), 'HOW-TO-PLAY.md')
put(os.path.join(SKYRIM, 'Data', 'MordhauWeapons.esp'), r'payload\skyrim\Data\MordhauWeapons.esp')
# the two loaders (other people's open-source work)
put(os.path.join(SKYRIM, 'dinput8.dll'), r'payload\skyrim\dinput8.dll')
put(os.path.join(MORDHAU_BIN, 'UE4SS.dll'), r'payload\mordhau\UE4SS.dll')
put(os.path.join(MORDHAU_BIN, 'UE4SS-settings.ini'), r'payload\mordhau\UE4SS-settings.ini')
put(os.path.join(ROOT, 'build', 'loader', 'dwmapi.dll'),   # (the mod's own loader: loader/, tools/build_loader.bat)
     r'payload\mordhau\dwmapi.dll.mashup')
for base in ('UE4SS_Signatures', 'Mods'):
    top = os.path.join(MORDHAU_BIN, base)
    for folder, _, files in os.walk(top):
        if os.sep + 'PoseStream' in folder:
            continue  # (this mashup's script comes from mordhau_side/, the cache_dir.txt is this PC's)
        for name in files:
            if name.startswith('__folder_managed_by_'):
                continue  # (a mod manager's marker, left in MORDHAU's folder by a test)
            full = os.path.join(folder, name)
            put(full, os.path.join(r'payload\mordhau', os.path.relpath(full, MORDHAU_BIN)))
for name in os.listdir(os.path.join(ROOT, 'installer', 'LICENSES')):
    put(os.path.join(ROOT, 'installer', 'LICENSES', name), os.path.join('LICENSES', name))

import subprocess
version = subprocess.run(['powershell', '-NoProfile', '-Command', f"(Get-Item '{SKYRIM}\\SkyrimSE.exe').VersionInfo.FileVersion"], capture_output=True, text=True).stdout.strip()
json.dump({'skyrim': version, 'mordhauExeBytes': os.path.getsize(os.path.join(MORDHAU_BIN, 'Mordhau-Win64-Shipping.exe'))},
          open(os.path.join(OUT, 'payload', 'tested-with.json'), 'w'), indent=1)

# nothing of the games may be in it: no baked models, clips or textures, nothing big
bad, total, count = [], 0, 0
for folder, _, files in os.walk(OUT):
    for name in files:
        full = os.path.join(folder, name)
        size = os.path.getsize(full)
        total += size; count += 1
        if name.lower().endswith(('.dmchar', '.dmfull', '.dmclip', '.dds', '.pak', '.bsa', '.nif', '.uasset', '.uexp', '.wav', '.ogg', '.bin')):
            bad.append(os.path.relpath(full, OUT))
print(f'{OUT}: {count} files, {total / 1048576:.1f} MB')
print('game content in it: ' + (', '.join(bad) if bad else 'none'))
sys.exit(1 if bad else 0)
