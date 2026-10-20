#!/usr/bin/env bash
# Builds the SKSE plugin on Linux (clang-cl + lld-link + xwin's copy of Microsoft's SDK: see linux/cross/) and puts it in
# the bridge's mod manager folder, with MORDHAU's side of the link (mordhau_side). One-time setup: apt install clang lld llvm ninja-build zip; xwin --accept-license splat
# --output ~/dev/tools/xwin-sdk; vcpkg cloned and bootstrapped in ~/dev/tools/vcpkg; plugin/extern/CommonLibSSE-NG at
# the commit .github/workflows/build.yml names. CI's MSVC build stays the reference for releases.
# Usage: linux/build-cross.sh [--no-install]     MOD_DIR: where MordhauSkyrim.dll goes (default: Limo's staged bridge)
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
TOOLS=${TOOLS:-$HOME/dev/tools}
BUILD=$ROOT/build-cross
MOD_DIR=${MOD_DIR:-/mnt/fast/Limo/SkyrimSE/46}
CROSS=$ROOT/linux/cross

ML=$(command -v llvm-ml || command -v llvm-ml-19 || true)
[ -n "$ML" ] || { echo "missing: llvm-ml (apt install llvm)" >&2; exit 1; }
for tool in clang-cl lld-link llvm-rc llvm-lib ninja; do
	command -v "$tool" > /dev/null || { echo "missing: $tool (see the setup at the top of this script)" >&2; exit 1; }
done
[ -d "$TOOLS/xwin-sdk/sdk" ] || { echo "missing: $TOOLS/xwin-sdk" >&2; exit 1; }

# DirectXTK's shaders (once; again if vcpkg moves to a DirectXTK whose shader list differs)
if [ ! -f "$TOOLS/directxtk-shaders/shaders.txt" ]; then
	if [ ! -f "$TOOLS/mini-fxc/mini_fxc.exe" ]; then
		X=$TOOLS/xwin-sdk
		mkdir -p "$TOOLS/mini-fxc"
		clang-cl --target=x86_64-pc-windows-msvc /nologo /O2 /MD /EHsc /std:c++17 /imsvc"$X/crt/include" /imsvc"$X/sdk/include/ucrt" \
			/imsvc"$X/sdk/include/um" /imsvc"$X/sdk/include/shared" /c "$CROSS/mini_fxc.cpp" /Fo"$TOOLS/mini-fxc/mini_fxc.obj"
		lld-link /nologo "$TOOLS/mini-fxc/mini_fxc.obj" /out:"$TOOLS/mini-fxc/mini_fxc.exe" /subsystem:console /libpath:"$X/crt/lib/x86_64" \
			/libpath:"$X/sdk/lib/um/x86_64" /libpath:"$X/sdk/lib/ucrt/x86_64" d3dcompiler.lib
	fi
	# Microsoft's compiler (Wine's own fails; e.g. the copy from Firefox 62.0.3's installer, as winetricks takes it)
	[ -f "$TOOLS/mini-fxc/d3dcompiler_47.dll" ] || { echo "put Microsoft's d3dcompiler_47.dll in $TOOLS/mini-fxc" >&2; exit 1; }
	src=$(ls -d "$TOOLS"/vcpkg/buildtrees/directxtk/src/*/Src/Shaders 2> /dev/null | head -1)
	if [ -z "$src" ]; then
		# (vcpkg unpacks DirectXTK's source before it stops at the missing shaders: one failing configure run is enough)
		echo "no DirectXTK source yet: the configure below unpacks it and then stops at directxtk; run this script again" >&2
		rm -rf "$BUILD"
		src=none
	fi
	[ "$src" = none ] || "$CROSS/directxtk-shaders.sh" "$src" "$TOOLS/directxtk-shaders"
fi

# (Sheets.h keeps its old time when the sheets gave the same code: otherwise every file is compiled again)
sheets=$ROOT/plugin/src/gen/Sheets.h
cp -p "$sheets" "$BUILD.sheets" 2> /dev/null || true
python3 "$ROOT/tools/gen_code.py" > /dev/null
cmp -s "$sheets" "$BUILD.sheets" && touch -r "$BUILD.sheets" "$sheets"
rm -f "$BUILD.sheets"
if [ ! -f "$BUILD/build.ninja" ]; then
	cmake -S "$ROOT/plugin" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
		-DCMAKE_TOOLCHAIN_FILE="$TOOLS/vcpkg/scripts/buildsystems/vcpkg.cmake" \
		-DVCPKG_CHAINLOAD_TOOLCHAIN_FILE="$CROSS/clang-cl-msvc.cmake" \
		-DVCPKG_TARGET_TRIPLET=x64-windows-static-md -DVCPKG_OVERLAY_TRIPLETS="$CROSS/triplets" -DVCPKG_HOST_TRIPLET=x64-linux
fi
cmake --build "$BUILD" --target DirectionalMelee MordhauBones
# MORDHAU's loader (dwmapi.dll; as CI builds it - loader/exports.asm is generated on Windows by gen_exports.py)
X=$TOOLS/xwin-sdk
mkdir -p "$BUILD/loader"
"$ML" -m64 /nologo /c /Fo"$BUILD/loader/exports.obj" "$ROOT/loader/exports.asm" > /dev/null
clang-cl --target=x86_64-pc-windows-msvc /nologo /c /O1 /MT /W4 /EHs-c- /GS- /imsvc"$X/crt/include" /imsvc"$X/sdk/include/ucrt" \
	/imsvc"$X/sdk/include/um" /imsvc"$X/sdk/include/shared" /Fo"$BUILD/loader/" "$ROOT/loader/loader.cpp" "$ROOT/loader/exports_table.cpp"
lld-link /nologo /DLL /OUT:"$BUILD/loader/dwmapi.dll" /DEF:"$ROOT/loader/exports.def" /NOIMPLIB "$BUILD"/loader/{loader,exports_table,exports}.obj \
	kernel32.lib user32.lib /libpath:"$X/crt/lib/x86_64" /libpath:"$X/sdk/lib/um/x86_64" /libpath:"$X/sdk/lib/ucrt/x86_64"

[ "${1:-}" = "--no-install" ] && exit 0
if ps -eo comm | grep -qE '^(SkyrimSE|Mordhau-Win64)'; then
	echo "built, not installed: Skyrim or MORDHAU is running ($BUILD/DirectionalMelee.dll)" >&2
	exit 1
fi
dest=$MOD_DIR/SKSE/Plugins
backup=$TOOLS/bridge-dll-backup/$(date +%Y%m%d-%H%M%S)
mkdir -p "$backup"
cp -p "$dest/MordhauSkyrim.dll" "$dest/MordhauSkyrim.pdb" "$dest/MordhauSkyrim/MordhauBones.dll" "$backup/" 2> /dev/null || true
# (copied over the staged files, not replaced: the mod manager's hard links in Skyrim's Data see the new build at once)
cp "$BUILD/DirectionalMelee.dll" "$dest/MordhauSkyrim.dll"
cp "$BUILD/DirectionalMelee.pdb" "$dest/MordhauSkyrim.pdb"
# (the script's bones helper: the plugin copies it into the mod's cache folder at every start)
mkdir -p "$dest/MordhauSkyrim"
cp "$BUILD/MordhauBones.dll" "$dest/MordhauSkyrim/MordhauBones.dll"
# MORDHAU's side of the link (the plugin copies it into the mod's own folder for UE4SS, <cache>/ue4ss, at every start;
# the loader goes into MORDHAU's folder only while MORDHAU starts)
side=$dest/MordhauSkyrim/mordhau
if [ -d "$side" ]; then
	cp -p "$side/Mods/PoseStream/Scripts/main.lua" "$side/dwmapi.dll" "$backup/" 2> /dev/null || true
	cp "$BUILD/loader/dwmapi.dll" "$side/dwmapi.dll"
	cp "$ROOT/mordhau_side/PoseStream/Scripts/main.lua" "$side/Mods/PoseStream/Scripts/main.lua"
	cp "$ROOT"/mordhau_side/UE4SS_Signatures/*.lua "$side/UE4SS_Signatures/"
fi
echo "installed in $MOD_DIR (previous build: $backup)"
