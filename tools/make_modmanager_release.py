"""Puts together the mod-manager package: release/MordhauSkyrim-ModManager/ and its zip. Its top level is what goes
into Skyrim's Data folder (what Vortex and Mod Organizer 2 expect): the weapons plugin, the SKSE plugin, and - beside
the SKSE plugin - MORDHAU's side, which the plugin itself puts into the player's MORDHAU when a session starts
(sheets/live_link.json: managed_session). No installer, no launcher, no scripts to run. Nothing of either game is in it.
usage: make_modmanager_release.py [<out folder>]"""
import os, shutil, sys, zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'release', 'MordhauSkyrim-ModManager')
SKYRIM = r'E:\steam object\steamapps\common\Skyrim Special Edition'
MORDHAU_BIN = os.path.join(ROOT, 'vendor', 'ue4ss')   # (UE4SS as shipped, kept in the project: MORDHAU's own folder changes with every install test)
PLUGINS = os.path.join('SKSE', 'Plugins')
SIDE = os.path.join(PLUGINS, 'MordhauSkyrim')

def put(src, rel):
    dst = os.path.join(OUT, rel)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    shutil.copy2(src, dst)

if os.path.isdir(OUT):
    shutil.rmtree(OUT)
os.makedirs(OUT)
# this mashup's own
put(os.path.join(ROOT, 'build', 'dist', 'DirectionalMelee.asi'), os.path.join(PLUGINS, 'MordhauSkyrim.dll'))   # (the same file: an SKSE plugin under this name)
put(os.path.join(ROOT, 'installer', 'modmanager', 'MordhauSkyrim.ini'), os.path.join(PLUGINS, 'MordhauSkyrim.ini'))
put(os.path.join(SKYRIM, 'Data', 'MordhauWeapons.esp'), 'MordhauWeapons.esp')
put(os.path.join(ROOT, 'mordhau_side', 'PoseStream', 'Scripts', 'main.lua'), os.path.join(SIDE, 'mordhau', 'Mods', 'PoseStream', 'Scripts', 'main.lua'))
put(os.path.join(ROOT, 'installer', 'modmanager', 'README.md'), os.path.join(SIDE, 'README.md'))
# the play guide: HOW-TO-PLAY.md with the parts about the launcher script replaced (so the two never drift apart)
guide = open(os.path.join(ROOT, 'HOW-TO-PLAY.md'), encoding='utf-8').read()
def swap(text, start, end, new):
    a = text.index(start)
    b = text.index(end, a) if end else len(text)
    return text[:a] + new + text[b:]
guide = swap(guide, '> **MORDHAU is run OFFLINE only**', '## Start', r"""> **MORDHAU is run OFFLINE only** - a local match on your PC, in a window behind Skyrim, started and closed by the
> mod. This does not touch MORDHAU's anti-cheat and must never be used for online play.

> **Before you play MORDHAU online again after a crash:** look into MORDHAU's `Mordhau\Binaries\Win64` folder and
> delete `dwmapi.dll` if it is there. See the README.

""")
guide = swap(guide, '## Start', '## Controls', r"""## Start

Start Skyrim the way you always do with mods (through SKSE, from your mod manager).

1. MORDHAU opens by itself in a window; Skyrim keeps the keyboard. It needs one to four minutes to open its local
   match and spawn your mercenary. Until then Skyrim plays as usual.
2. **The very first start** prepares MORDHAU's models from your own MORDHAU first, once Skyrim's main menu is up (about
   2 GB, under a minute on a fast disk). MORDHAU starts after that.
3. In Skyrim: load a save (use a new one or one you do not mind losing). When *MORDHAU is live* appears at the top
   left, **draw a melee weapon**.
4. Keep MORDHAU's window open behind Skyrim. Do not minimise it and do not play in it. It closes with Skyrim.

Your MORDHAU mercenary is the one whose arms you see in first person. In third person you see your Skyrim character,
moving as MORDHAU moves.

""")
guide = swap(guide, "- **MORDHAU's character never appears / attacks do nothing:**", '- **Logs to send', r"""- **MORDHAU's character never appears / attacks do nothing:** look at MORDHAU's own window - if it shows a menu, join
  the match and spawn by hand once. If there is no MORDHAU window at all, look into `MordhauSkyrim.log` (Documents\My Games\Skyrim Special Edition\SKSE).
- **Everything is back to normal Skyrim combat in the middle of a session:** MORDHAU closed. The mod starts it again
  by itself within a minute.
""")
guide = swap(guide, '- **To get rid of it:**', '## Settings', "- **To get rid of it:** see *Remove* in the README.\n\n")
guide = swap(guide, '## Settings', None, r"""## Settings

`MordhauSkyrim.ini` beside the plugin (`Data\SKSE\Plugins`): MORDHAU somewhere Steam does not know, easier enemies
(`SimpleEnemies=1`), the size of MORDHAU's window, or the mod switched off (`StartMordhau=0`). Which mercenary: put its
name alone in a file `player.txt` in the cache folder (`%LOCALAPPDATA%\MordhauSkyrim\cache`).
""")
assert 'Play-Mashup' not in guide and '.ps1' not in guide, 'the guide still names a script'
os.makedirs(os.path.join(OUT, SIDE), exist_ok=True)
open(os.path.join(OUT, SIDE, 'HOW-TO-PLAY.md'), 'w', encoding='utf-8', newline='\n').write(guide)
# UE4SS, the script host for MORDHAU (other people's open-source work), with its loader under its real name
put(os.path.join(MORDHAU_BIN, 'UE4SS.dll'), os.path.join(SIDE, 'mordhau', 'UE4SS.dll'))
put(os.path.join(MORDHAU_BIN, 'UE4SS-settings.ini'), os.path.join(SIDE, 'mordhau', 'UE4SS-settings.ini'))
put(os.path.join(ROOT, 'build', 'loader', 'dwmapi.dll'),   # (the mod's own loader: loader/, tools/build_loader.bat)
     os.path.join(SIDE, 'mordhau', 'dwmapi.dll'))
for base in ('UE4SS_Signatures', 'Mods'):
    for folder, _, files in os.walk(os.path.join(MORDHAU_BIN, base)):
        if os.sep + 'PoseStream' in folder:
            continue  # (this mashup's script comes from mordhau_side/, the cache_dir.txt is this PC's)
        for name in files:
            if name.startswith('__folder_managed_by_'):
                continue  # (a mod manager's marker, left in MORDHAU's folder by a test)
            full = os.path.join(folder, name)
            put(full, os.path.join(SIDE, 'mordhau', os.path.relpath(full, MORDHAU_BIN)))
for name in os.listdir(os.path.join(ROOT, 'installer', 'LICENSES')):
    if 'ASI' not in name.upper():   # (the ASI loader is not in this package)
        put(os.path.join(ROOT, 'installer', 'LICENSES', name), os.path.join(SIDE, 'LICENSES', name))

# nothing of the games may be in it: no baked models, clips or textures, nothing big
bad, total, count = [], 0, 0
archive = OUT + '.zip'
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    for folder, _, files in os.walk(OUT):
        for name in files:
            full = os.path.join(folder, name)
            total += os.path.getsize(full); count += 1
            if name.lower().endswith(('.dmchar', '.dmfull', '.dmclip', '.dds', '.pak', '.bsa', '.nif', '.uasset', '.uexp', '.wav', '.ogg', '.bin')):
                bad.append(os.path.relpath(full, OUT))
            z.write(full, os.path.relpath(full, OUT))
print(f'{OUT}: {count} files, {total / 1048576:.1f} MB; zip {os.path.getsize(archive) / 1048576:.1f} MB')
print('game content in it: ' + (', '.join(bad) if bad else 'none'))
sys.exit(1 if bad else 0)
