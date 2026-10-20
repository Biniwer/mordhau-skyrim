"""Dev: snapshot the project into backups/<name>.zip so work can be rolled back to this point.

In: all source we wrote (plugin/src, plugin build files, tools, sheets, mordhau_side, the launcher, docs, the patch
scripts under _deps), the built plugin, and
the files installed into the two game folders. Out: third-party toolchains and libraries (re-downloadable), build
intermediates, recordings and screenshots except the ones named S..A / demo.
usage: backup.py <name>
"""
import os, sys, zipfile, time
root = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
name = sys.argv[1]
out = os.path.join(root, 'backups', name + '.zip')
SKYRIM = r'E:\steam object\steamapps\common\Skyrim Special Edition'
MORDHAU = r'E:\steam object\steamapps\common\Mordhau\Mordhau\Binaries\Win64'
count = size = 0
with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
    def add(path, arc):
        global count, size
        z.write(path, arc); count += 1; size += os.path.getsize(path)
    def tree(rel, skip=()):
        for d, dirs, files in os.walk(os.path.join(root, rel)):
            dirs[:] = [x for x in dirs if x not in skip]
            for f in files:
                p = os.path.join(d, f)
                add(p, os.path.join('project', os.path.relpath(p, root)))
    for rel, skip in (('plugin', ('extern',)), ('tools', ('__pycache__',)), ('sheets', ()), ('mordhau_side', ()), ('build/dist', ()),
                      # (2026-10-11: these were missing - the loader that guards MORDHAU's launch had its source in no backup)
                      ('loader', ()), ('build/loader', ()), ('installer', ()), ('release', ()), ('vendor', ())):
        tree(rel, skip)
    for f in os.listdir(root):
        if f.lower().endswith(('.md', '.ps1', '.tsv')):
            add(os.path.join(root, f), os.path.join('project', f))
    for f in os.listdir(os.path.join(root, '_deps')):
        p = os.path.join(root, '_deps', f)
        if os.path.isfile(p) and f.endswith('.py'):
            add(p, os.path.join('project', '_deps', f))
    tree('_deps/bak')
    for f in (os.listdir(os.path.join(root, 'shots', 'now')) if os.path.isdir(os.path.join(root, 'shots', 'now')) else []):  # (folder cleared 2026-10-11)
        if f[:2] in [c + '_' for c in 'ABCDEFGHIJKLMNOPQRS'] or f in ('duel.mp4', 'cam_compare.png', 'fp_compare.png', 'looks_sheet.png'):
            add(os.path.join(root, 'shots', 'now', f), os.path.join('project', 'shots', 'now', f))
    for f in ('DirectionalMelee.asi', 'DirectionalMelee.dev.txt', 'dinput8.dll'):
        p = os.path.join(SKYRIM, f)
        if os.path.exists(p): add(p, os.path.join('installed', 'skyrim', f))
    for f in ('UE4SS-settings.ini', 'UE4SS.dll', 'dwmapi.dll.mashup'):
        p = os.path.join(MORDHAU, f)
        if os.path.exists(p): add(p, os.path.join('installed', 'mordhau', f))
    for sub in ('UE4SS_Signatures', 'Mods'):
        for d, dirs, files in os.walk(os.path.join(MORDHAU, sub)):
            for f in files:
                p = os.path.join(d, f)
                add(p, os.path.join('installed', 'mordhau', os.path.relpath(p, MORDHAU)))
    z.writestr('README.txt', f"""MORDHAU x Skyrim mashup - snapshot "{name}", {time.strftime('%Y-%m-%d %H:%M')}

project/    -> E:\MordhauSkyrim (source, sheets, tools, launcher, docs, baked cache, built plugin)
installed/skyrim/  -> {SKYRIM}
installed/mordhau/ -> {MORDHAU}

To go back to this point: close both games, copy project/ over E:\MordhauSkyrim, installed/skyrim over the Skyrim
folder and installed/mordhau over MORDHAU's Binaries\Win64 folder. Not included (unchanged, re-downloadable):
_deps toolchains, plugin/extern (CommonLibSSE-NG at commit d61bca4de), build intermediates. Rebuild is only needed if
plugin/src is changed afterwards (commands in MODLOG.md).
""")
bad = zipfile.ZipFile(out).testzip()
print(f'{out}: {count} files, {size/1e6:.0f} MB in, {os.path.getsize(out)/1e6:.0f} MB zip, integrity {"OK" if bad is None else "BAD: " + bad}')
