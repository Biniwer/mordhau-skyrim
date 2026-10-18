#!/usr/bin/env bash
# DirectXTK's HLSL shaders, compiled on Linux: its CompileShaders.cmd needs Windows and fxc.exe. mini_fxc.exe (built from
# mini_fxc.cpp by build-cross.sh) runs under Proton's Wine in its own prefix with Microsoft's d3dcompiler_47.dll and
# compiles the same list with the same options. vcpkg then builds DirectXTK with USE_PREBUILT_SHADERS (see the triplet).
# Usage: directxtk-shaders.sh <DirectXTK Src/Shaders folder> <output folder>
set -euo pipefail
SRC=$(realpath "$1")
OUT=$(realpath -m "$2")
TOOLS=${TOOLS:-$HOME/dev/tools}
FXC=$TOOLS/mini-fxc
PROTON=${PROTON:-$HOME/.local/share/Steam/steamapps/common/Proton - Experimental}
export WINEPREFIX=$TOOLS/wine-fxc WINEDLLOVERRIDES="d3dcompiler_47=n" WINEDEBUG=-all
mkdir -p "$OUT"
[ -f "$FXC/d3dcompiler_47.dll" ] || { echo "Microsoft's d3dcompiler_47.dll is missing in $FXC" >&2; exit 1; }

win() { echo "Z:${1//\//\\}"; }
list=$OUT/shaders.txt
: > "$list"
# the PC lines (call :CompileShader%1 ...; the xbox-only ones name their rule with "xbox")
grep -E '^call :CompileShader(SM4|HLSL)?%1 ' "$SRC/CompileShaders.cmd" | tr -d '\r' | while read -r _ rule file stage entry; do
	case $rule in
	:CompileShaderSM4%1) ext=fx target=${stage}_4_0 ;;
	:CompileShaderHLSL%1) ext=hlsl target=${stage}_4_0_level_9_1 ;;
	*) ext=fx target=${stage}_4_0_level_9_1 ;;
	esac
	echo "$(win "$SRC/$file.$ext")|$target|$entry|$(win "$OUT/${file}_$entry.inc")|${file}_$entry" >> "$list"
done
echo "$(wc -l < "$list") shaders"
"$PROTON/files/bin/wine" "$FXC/mini_fxc.exe" "$(win "$list")"
