@echo off
rem Builds the mod's own loader (loader\ -> build\loader\dwmapi.dll). See sheets/live_link.json: own_loader.
setlocal
call E:\VSBuildTools\VC\Auxiliary\Build\vcvars64.bat >nul
cd /d E:\MordhauSkyrim\loader
python gen_exports.py || exit /b 1
if not exist ..\build\loader mkdir ..\build\loader
ml64 /nologo /c /Fo..\build\loader\exports.obj exports.asm || exit /b 1
cl /nologo /c /O1 /MT /W4 /EHs-c- /GS- /Fo..\build\loader\ loader.cpp exports_table.cpp || exit /b 1
link /nologo /DLL /OUT:..\build\loader\dwmapi.dll /DEF:exports.def /NOIMPLIB /NOEXP ..\build\loader\loader.obj ..\build\loader\exports_table.obj ..\build\loader\exports.obj kernel32.lib user32.lib || exit /b 1
echo built ..\build\loader\dwmapi.dll
