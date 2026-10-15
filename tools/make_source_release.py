"""Makes release/MordhauSkyrim-Source.zip: the complete corresponding source of the mod, for the "Source code" file on
its Nexus page (GPL-3.0-or-later: the plugin is built on CommonLibSSE-NG).
In: the plugin's source and build files, the loader, the MORDHAU script and UE4SS files as shipped, the design sheets
(the build generates code from them), the tools, the package texts, every licence text, and how to build.
Out: the development journal and one-off patch scripts (working notes with private details), third-party libraries
that are fetched by the build (named in BUILDING.md with their versions), build output, the cache made from the games.
usage: make_source_release.py"""
import os, re, zipfile

root = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
out = os.path.join(root, 'release', 'MordhauSkyrim-Source.zip')
NL = chr(10)

BUILDING = NL.join([
    '# MORDHAU x Skyrim - source',
    '',
    'Licence: GPL-3.0-or-later (COPYING). Copyright (C) 2026 spock290. Third-party licences: installer/LICENSES.',
    '',
    '## What is where',
    '',
    '- `plugin/` - the SKSE plugin (C++23, CMake, vcpkg). `plugin/src/gen/Sheets.h` is generated from `sheets/` by `tools/gen_code.py`.',
    '- `loader/` - the loader `dwmapi.dll` that checks how MORDHAU was started before anything else is loaded. `loader/gen_exports.py` writes the export tables; `tools/build_loader.bat` builds it.',
    '- `mordhau_side/` - the Lua script that runs in MORDHAU under UE4SS, and this mod\'s UE4SS signature file.',
    '- `sheets/` - the design as data (JSON): every number and mapping the code uses. Change a sheet, then run `tools/gen_code.py`.',
    '- `tools/` - what builds the package and what was used to test it. Many are written for the author\'s PC (paths to the games at the top of each file).',
    '- `installer/` - the texts and licence files that go into the download.',
    '',
    '## Building the plugin',
    '',
    '1. Visual Studio 2022 with the C++ workload, CMake 3.21 or newer, vcpkg, Python 3.',
    '2. Put CommonLibSSE-NG into `plugin/extern/CommonLibSSE-NG`: https://github.com/alandtse/CommonLibSSE-NG , version 10.0.0 (commit d61bca4de789428aa7d98a770b1323ddf1bb855c).',
    '3. `python tools/gen_code.py`',
    '4. Configure `plugin/` with CMake and the vcpkg toolchain (triplet `x64-windows-static-md`; the libraries are listed in `plugin/vcpkg.json`), then build the target `DirectionalMelee` in `RelWithDebInfo`. The result, `DirectionalMelee.dll`, is the file shipped as `MordhauSkyrim.dll`.',
    '5. `tools/build_loader.bat` builds `dwmapi.dll`.',
    '6. `python tools/make_modmanager_release.py` puts the package together. It takes UE4SS v3.0.1 (https://github.com/UE4SS-RE/RE-UE4SS) from `vendor/ue4ss`, and `MordhauWeapons.esp` as `tools/make_items_esp.py` makes it from your own `Skyrim.esm`.',
    '',
    'The same source is at https://github.com/thnokkvi-lang/mordhau-skyrim .',
    '',
    'Nothing of MORDHAU or Skyrim is in this archive, and nothing of either is needed to compile; both games are needed to run and to test.',
    ''])

take = [('plugin', ('extern', 'build')), ('loader', ()), ('mordhau_side', ()), ('sheets', ()), ('tools', ('__pycache__',)), ('installer', ())]
loose = ['COPYING', 'HOW-TO-PLAY.md', 'release/description.md', 'release/credits.txt', 'release/README.txt', 'release/safety-measures.txt']
skip_ext = ('.pyc', '.obj', '.pdb', '.dll', '.exe', '.zip', '.png', '.jpg', '.mp4', '.dds', '.bin')
# (the words to look for are personal themselves: they are kept in _deps/private_words.txt, one per line, which never
# goes into this archive - written into this file, they were published with it)
words_file = os.path.join(root, '_deps', 'private_words.txt')
words = [w.strip() for w in open(words_file, encoding='utf-8')] if os.path.exists(words_file) else []
if not words:
    print('NOTE: no _deps/private_words.txt - only the general words are looked for')
private = re.compile('|'.join(re.escape(w) for w in [w for w in words if w] + ['paypal', 'gmail']).encode(), re.I)
found = []
with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    z.writestr('BUILDING.md', BUILDING)
    count = 1
    for top, skip in take:
        for d, dirs, files in os.walk(os.path.join(root, top)):
            dirs[:] = [x for x in dirs if x not in skip]
            for f in sorted(files):
                if f.startswith(('unquote_', 'quoted_comments')):   # (they hold the very quotes they take out)
                    continue
                if f.lower().endswith(skip_ext) and not (top == 'installer'):
                    continue
                p = os.path.join(d, f)
                rel = os.path.relpath(p, root).replace(os.sep, '/')
                data = open(p, 'rb').read()
                if private.search(data) and rel != 'tools/make_source_release.py':   # (this file names the general words it looks for)
                    found.append(rel)
                # (every file keeps its own name: adding .txt to scripts to get past Nexus's quarantine got the mod hidden
                # for "disguising file types", 2026-10-15)
                z.write(p, rel)
                count += 1
    for rel in loose:
        p = os.path.join(root, rel)
        if os.path.exists(p):
            z.write(p, rel)
            count += 1
print(f'{out}: {count} files, {os.path.getsize(out) / 1e6:.1f} MB')
print('files with private words in them (to look at before this goes anywhere):', found or 'none')
