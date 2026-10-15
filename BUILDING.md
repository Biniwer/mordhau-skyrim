# MORDHAU x Skyrim - source

Licence: GPL-3.0-or-later (COPYING). Copyright (C) 2026 spock290. Third-party licences: installer/LICENSES.

## What is where

- `plugin/` - the SKSE plugin (C++23, CMake, vcpkg). `plugin/src/gen/Sheets.h` is generated from `sheets/` by `tools/gen_code.py`.
- `loader/` - the loader `dwmapi.dll` that checks how MORDHAU was started before anything else is loaded. `loader/gen_exports.py` writes the export tables; `tools/build_loader.bat` builds it.
- `mordhau_side/` - the Lua script that runs in MORDHAU under UE4SS, and this mod's UE4SS signature file.
- `sheets/` - the design as data (JSON): every number and mapping the code uses. Change a sheet, then run `tools/gen_code.py`.
- `tools/` - what builds the package and what was used to test it. Many are written for the author's PC (paths to the games at the top of each file).
- `installer/` - the texts and licence files that go into the download.

## Building the plugin

1. Visual Studio 2022 with the C++ workload, CMake 3.21 or newer, vcpkg, Python 3.
2. Put CommonLibSSE-NG into `plugin/extern/CommonLibSSE-NG`: https://github.com/alandtse/CommonLibSSE-NG , version 10.0.0 (commit d61bca4de789428aa7d98a770b1323ddf1bb855c).
3. `python tools/gen_code.py`
4. Configure `plugin/` with CMake and the vcpkg toolchain (triplet `x64-windows-static-md`; the libraries are listed in `plugin/vcpkg.json`), then build the target `DirectionalMelee` in `RelWithDebInfo`. The result, `DirectionalMelee.dll`, is the file shipped as `MordhauSkyrim.dll`.
5. `tools/build_loader.bat` builds `dwmapi.dll`.
6. `python tools/make_modmanager_release.py` puts the package together. It takes UE4SS v3.0.1 (https://github.com/UE4SS-RE/RE-UE4SS) from `vendor/ue4ss`, and `MordhauWeapons.esp` as `tools/make_items_esp.py` makes it from your own `Skyrim.esm`.

The same source is at https://github.com/thnokkvi-lang/mordhau-skyrim .

Nothing of MORDHAU or Skyrim is in this archive, and nothing of either is needed to compile; both games are needed to run and to test.
