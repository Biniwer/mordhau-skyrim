"""Dev: the licence texts that go into the download (installer/LICENSES) and the project's own COPYING, put together
from the copies the libraries themselves ship (so no text is typed by hand).
2026-10-12: Nexus moderation (case 239313): the plugin is built on alandtse's CommonLibSSE-NG, which is
GPL-3.0-or-later with a modding / linking exception - not MIT, as the credits said until then. A plugin that links it
is a combined work: GPL-3.0-or-later, source offered, open permissions.
usage: make_licences.py"""
import os, shutil

root = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
out = os.path.join(root, 'installer', 'LICENSES')
lib = os.path.join(root, 'plugin', 'extern', 'CommonLibSSE-NG')
share = os.path.join(root, '_deps', 'vcpkg_installed', 'x64-windows-static-md', 'share')
NL = chr(10)


def read(path):
    return open(path, encoding='utf-8', errors='replace').read().replace('\r\n', NL).strip() + NL


def write(name, text):
    open(os.path.join(out, name), 'w', encoding='utf-8', newline=NL).write(text)
    print(name, len(text))


gpl = read(os.path.join(lib, 'COPYING.txt'))
open(os.path.join(root, 'COPYING'), 'w', encoding='utf-8', newline=NL).write(gpl)
write('GPL-3.0.txt', gpl)
write('MordhauSkyrim.txt', NL.join([
    'MORDHAU x Skyrim ("Mordhau combat passthrough", Nexus Mods, Skyrim Special Edition mod 194747)',
    'Copyright (C) 2026 spock290',
    '',
    'This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public',
    'License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later',
    'version.',
    '',
    'This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied',
    'warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.',
    '',
    'The full text of the licence is in GPL-3.0.txt beside this file.',
    '',
    'This covers the mod\'s own work: MordhauSkyrim.dll (the SKSE plugin), dwmapi.dll (the loader in the mordhau folder),',
    'Mods/PoseStream/Scripts/main.lua (the MORDHAU script), MordhauWeapons.esp, the design sheets and the tools they are',
    'built with. The plugin is built on CommonLibSSE-NG (GPL-3.0-or-later with a modding and linking exception, see',
    'CommonLibSSE-NG.txt) and is conveyed under the same licence and exception.',
    '',
    'Source code: the complete corresponding source of this version is offered on the mod\'s page on Nexus Mods, under',
    'Files, as "Source code" (https://www.nexusmods.com/skyrimspecialedition/mods/194747?tab=files).',
    '',
    'The other parts of the download keep their own licences, each in its own file in this folder.',
    '',
]))
write('CommonLibSSE-NG.txt', NL.join([
    'CommonLibSSE-NG - https://github.com/alandtse/CommonLibSSE-NG (alandtse and contributors; a fork of CommonLibSSE-NG',
    'by CharmedBaryon, itself based on CommonLibSSE by Ryan McKenzie and powerof3).',
    'The SKSE plugin MordhauSkyrim.dll is compiled against and statically linked with it (version 10.0.0, commit d61bca4de).',
    '',
    'Licence: GPL-3.0-or-later (full text: GPL-3.0.txt beside this file) WITH the Modding Exception AND the GPL-3.0 Linking',
    'Exception (with Corresponding Source) below.',
    '',
    '=== EXCEPTIONS.md, as CommonLibSSE-NG ships it ===',
    '',
    read(os.path.join(lib, 'EXCEPTIONS.md')),
    '=== Originally based on code under the MIT licence (licenses/LICENSE-MIT.txt in CommonLibSSE-NG) ===',
    '',
    read(os.path.join(lib, 'licenses', 'LICENSE-MIT.txt')),
    '=== licenses/LICENSE-hde64.txt in CommonLibSSE-NG ===',
    '',
    read(os.path.join(lib, 'licenses', 'LICENSE-hde64.txt')),
]))
for folder, name, what in (('spdlog', 'spdlog', 'https://github.com/gabime/spdlog'), ('fmt', 'fmt', 'https://github.com/fmtlib/fmt'),
                           ('directxtk', 'DirectXTK', 'https://github.com/microsoft/DirectXTK'), ('directxmath', 'DirectXMath', 'https://github.com/microsoft/DirectXMath'),
                           ('rapidcsv', 'rapidcsv', 'https://github.com/d99kris/rapidcsv'), ('xbyak', 'xbyak', 'https://github.com/herumi/xbyak'),
                           ('nlohmann-json', 'nlohmann-json', 'https://github.com/nlohmann/json'), ('SimpleIni', 'SimpleIni', 'https://github.com/brofield/simpleini'),
                           ('toml11', 'toml11', 'https://github.com/ToruNiina/toml11')):
    write(name + '.txt', f'{name} - {what}{NL}A library the SKSE plugin MordhauSkyrim.dll is built with (through vcpkg), linked into it.{NL}{NL}' + read(os.path.join(share, folder, 'copyright')))
write('SkyCraft.txt', NL.join([
    'SkyCraft by chasmlol - MIT licence.',
    'Two techniques in MordhauSkyrim.dll follow SkyCraft\'s way of doing them: drawing into Skyrim\'s frame right after its',
    '3D scene and before its post-processing, and dealing a hit so that Skyrim shows the enemy\'s health bar and gives',
    'kill credit. No file of SkyCraft is included.',
    '',
    read(os.path.join(root, '_deps', 'SkyCraft', 'LICENSE')),
]))
write('TrueDirectionalMovement.txt', NL.join([
    'True Directional Movement by ersh1 - https://github.com/ersh1/TrueDirectionalMovement',
    'Licence: GPL-3.0 (as its repository states; full text: GPL-3.0.txt beside this file).',
    'Its modder interface header, TrueDirectionalMovementAPI.h, is part of the source of MordhauSkyrim.dll (plugin/src/api),',
    'copied as the file itself asks, so that the plugin can ask a running True Directional Movement to leave the player\'s',
    'movement, turning and target lock alone while MORDHAU is linked. No other file of True Directional Movement is included.',
    '',
]))
ue = read(os.path.join(out, 'UE4SS.txt'))
if 'jsbProfiler' not in ue:
    head, rest = ue.split(NL, 1)
    write('UE4SS.txt', 'UE4SS (RE-UE4SS) by the UE4SS-RE team - included unchanged: UE4SS.dll, UE4SS-settings.ini and the Lua mods that ship with it' + NL +
          '(Mods/ConsoleEnablerMod, Mods/Keybinds, Mods/shared/UEHelpers, Mods/shared/jsbProfiler, Mods/mods.txt).' + NL +
          'UE4SS_Signatures/GUObjectArray.lua and Mods/PoseStream are this mod\'s own (see MordhauSkyrim.txt); the loader dwmapi.dll is this mod\'s own too, not UE4SS\'s.' + NL + rest)
old = os.path.join(out, 'Ultimate-ASI-Loader.txt')
print('kept for the old launcher package only:', os.path.basename(old), os.path.exists(old))
