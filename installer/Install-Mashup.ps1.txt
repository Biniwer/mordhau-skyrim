# Installs the MORDHAU x Skyrim mashup into your own copies of both games.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File Install-Mashup.ps1
#
# Nothing of either game is in this package. MORDHAU's models and animations are read from YOUR MORDHAU the first time
# you play (about 2 GB are prepared on your disk; that takes a little while once).
#
# MORDHAU IS ONLY EVER RUN OFFLINE BY THIS: a local match on your own PC. Its anti-cheat is not touched or worked
# around, and the file that lets the mashup's script load is only in MORDHAU's folder while a mashup session starts -
# a normal MORDHAU launch (Steam, online play) has none of it. Do not use this to play online.
param(
    [string]$SkyrimPath = '',    # the folder with SkyrimSE.exe (default: found through Steam)
    [string]$MordhauPath = '',   # MORDHAU's folder (the one holding "Mordhau\Binaries") (default: found through Steam)
    [string]$CacheFolder = '',   # where MORDHAU's prepared models go (default: the folder "cache" beside this script)
    [switch]$Force               # install although a check failed (another Skyrim version, ...)
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
function Say($t) { Write-Host "[install] $t" }
function Stop-Install($t) { Write-Host "[install] STOPPED: $t" -ForegroundColor Red; exit 1 }

function SteamGame([string]$Relative) {
    $steam = (Get-ItemProperty -Path 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue).SteamPath
    if (-not $steam) { return $null }
    $libraries = @($steam)
    $list = Join-Path $steam 'steamapps\libraryfolders.vdf'
    if (Test-Path $list) {
        foreach ($line in Get-Content $list) {
            if ($line -match '"path"\s+"([^"]+)"') { $libraries += ($Matches[1] -replace '\\\\', '\') }
        }
    }
    foreach ($library in $libraries) {
        $path = Join-Path $library ('steamapps\common\' + $Relative)
        if (Test-Path $path -PathType Leaf) { return (Split-Path (Resolve-Path $path).Path -Parent) }
    }
    return $null
}

# --- where the two games are
$skyrim = if ($SkyrimPath) { $SkyrimPath.TrimEnd('\') } else { SteamGame 'Skyrim Special Edition\SkyrimSE.exe' }
if ($MordhauPath) {
    $mordhauRoot = $MordhauPath.TrimEnd('\')
    $mordhauBin = Join-Path $mordhauRoot 'Mordhau\Binaries\Win64'
} else {
    $mordhauBin = SteamGame 'Mordhau\Mordhau\Binaries\Win64\Mordhau-Win64-Shipping.exe'
    $mordhauRoot = if ($mordhauBin) { (Resolve-Path (Join-Path $mordhauBin '..\..\..')).Path } else { $null }
}
if (-not $skyrim -or -not (Test-Path "$skyrim\SkyrimSE.exe")) { Stop-Install 'Skyrim Special Edition was not found. Install it through Steam, or give its folder: -SkyrimPath "<folder with SkyrimSE.exe>"' }
if (-not $mordhauBin -or -not (Test-Path "$mordhauBin\Mordhau-Win64-Shipping.exe")) { Stop-Install 'MORDHAU was not found. Install it through Steam, or give its folder: -MordhauPath "<...\Mordhau>"' }
if (-not (Test-Path (Join-Path $mordhauRoot 'Mordhau\Content\Paks'))) { Stop-Install "MORDHAU's game files (Mordhau\Content\Paks) are not under $mordhauRoot" }
Say "Skyrim:  $skyrim"
Say "MORDHAU: $mordhauRoot"

# --- checks
if (Get-Process SkyrimSE, Mordhau-Win64-Shipping -ErrorAction SilentlyContinue) { Stop-Install 'Close Skyrim and MORDHAU first.' }
$tested = Get-Content (Join-Path $root 'payload\tested-with.json') -Raw | ConvertFrom-Json
$skyrimVersion = (Get-Item "$skyrim\SkyrimSE.exe").VersionInfo.FileVersion
if ($skyrimVersion -ne $tested.skyrim) {
    $text = "This build was made for Skyrim Special Edition $($tested.skyrim); yours is $skyrimVersion. The plugin will most likely not work with it."
    if ($Force) { Say "WARNING: $text" } else { Stop-Install "$text (-Force installs anyway)" }
}
# the plugin finds Skyrim's own functions through the "Address Library" (another modder's work, not ours to hand out)
$addressLibrary = Join-Path $skyrim ('Data\SKSE\Plugins\versionlib-' + ($skyrimVersion -replace '\.', '-') + '.bin')
if (-not (Test-Path $addressLibrary)) {
    Stop-Install ("The 'Address Library for SKSE Plugins' is missing (" + (Split-Path $addressLibrary -Leaf) + "). Get it from Nexus Mods - https://www.nexusmods.com/skyrimspecialedition/mods/32444 , the 'All in one' file for your game version - and put its SKSE folder into Skyrim's Data folder (or install it with your mod manager). SKSE itself is NOT needed. Then run this installer again.")
}
$mordhauSize = (Get-Item "$mordhauBin\Mordhau-Win64-Shipping.exe").Length
if ($mordhauSize -ne $tested.mordhauExeBytes) {
    Say "NOTE: your MORDHAU is another build than the one this was tested with. It may work; if MORDHAU's character never appears, that is why."
}
$drive = (Split-Path $(if ($CacheFolder) { $CacheFolder } else { $root }) -Qualifier).TrimEnd(':')
$free = (Get-PSDrive -Name $drive -ErrorAction SilentlyContinue).Free
if ($free -and $free -lt 4GB) { Say 'WARNING: less than 4 GB free where the prepared models go (about 2 GB are needed).' }

$placed = New-Object System.Collections.ArrayList   # every file this install puts down (for Uninstall-Mashup.ps1)
$kept = New-Object System.Collections.ArrayList     # files of yours that were set aside (put back on uninstall)
function Place([string]$From, [string]$To) {
    New-Item -ItemType Directory -Force (Split-Path $To -Parent) | Out-Null
    Copy-Item $From $To -Force
    [void]$placed.Add($To)
}

# --- Skyrim's side: the plugin, the loader that loads it, the MORDHAU weapons as Skyrim items
Place (Join-Path $root 'payload\skyrim\DirectionalMelee.asi') "$skyrim\DirectionalMelee.asi"
$asiLoader = "$skyrim\dinput8.dll"
if (Test-Path $asiLoader) {
    if ((Get-Item $asiLoader).VersionInfo.ProductName -like '*ASI-Loader*') {
        Say 'An ASI loader (dinput8.dll) is in Skyrim''s folder already: left as it is.'
    } else {
        Copy-Item $asiLoader "$asiLoader.before-mashup" -Force
        [void]$kept.Add($asiLoader)
        Say 'WARNING: Skyrim''s folder has another dinput8.dll (some other mod''s). It is kept as dinput8.dll.before-mashup and replaced by the ASI loader; that other mod will not load until you uninstall this.'
        Place (Join-Path $root 'payload\skyrim\dinput8.dll') $asiLoader
    }
} else {
    Place (Join-Path $root 'payload\skyrim\dinput8.dll') $asiLoader
}
$esp = "$skyrim\Data\MordhauWeapons.esp"
Place (Join-Path $root 'payload\skyrim\Data\MordhauWeapons.esp') $esp
$plugins = Join-Path $env:LOCALAPPDATA 'Skyrim Special Edition\Plugins.txt'
if (Test-Path $plugins) {
    $lines = @(Get-Content $plugins)
    if (-not ($lines | Where-Object { $_ -match '^\*?MordhauWeapons\.esp\s*$' })) {
        Add-Content -Path $plugins -Value '*MordhauWeapons.esp'
        Say 'MordhauWeapons.esp switched on in Skyrim''s plugin list.'
    }
} else {
    New-Item -ItemType Directory -Force (Split-Path $plugins -Parent) | Out-Null
    Set-Content -Path $plugins -Encoding ascii -Value '*MordhauWeapons.esp'
}

# --- MORDHAU's side: UE4SS (the script host), this mashup's script, and the loader under a name MORDHAU does not load
$hadUe4ss = Test-Path "$mordhauBin\UE4SS.dll"
if ($hadUe4ss) { Say 'UE4SS is in MORDHAU''s folder already: its program and settings are left as they are; only this mashup''s script is added.' }
else {
    Place (Join-Path $root 'payload\mordhau\UE4SS.dll') "$mordhauBin\UE4SS.dll"
    Place (Join-Path $root 'payload\mordhau\UE4SS-settings.ini') "$mordhauBin\UE4SS-settings.ini"
    Get-ChildItem (Join-Path $root 'payload\mordhau\UE4SS_Signatures') -File | ForEach-Object { Place $_.FullName "$mordhauBin\UE4SS_Signatures\$($_.Name)" }
    Get-ChildItem (Join-Path $root 'payload\mordhau\Mods') -Recurse -File | ForEach-Object {
        Place $_.FullName (Join-Path "$mordhauBin\Mods" $_.FullName.Substring((Join-Path $root 'payload\mordhau\Mods').Length + 1))
    }
}
Place (Join-Path $root 'payload\mordhau\dwmapi.dll.mashup') "$mordhauBin\dwmapi.dll.mashup"
Place (Join-Path $root 'mordhau_side\PoseStream\Scripts\main.lua') "$mordhauBin\Mods\PoseStream\Scripts\main.lua"
$modsList = "$mordhauBin\Mods\mods.txt"
if (-not (Test-Path $modsList)) { Set-Content -Path $modsList -Encoding ascii -Value 'PoseStream : 1' }
elseif (-not (Select-String -Path $modsList -Pattern '^\s*PoseStream\s*:' -Quiet)) { Set-Content -Path $modsList -Encoding ascii -Value (@('PoseStream : 1') + @(Get-Content $modsList)) }
if (Test-Path "$mordhauBin\dwmapi.dll") {
    Say 'NOTE: MORDHAU''s folder has a dwmapi.dll (a script loader that is always on). The mashup''s launcher will set it aside after each session, so MORDHAU starts without it otherwise.'
}

# --- where things are, for the launcher and the uninstaller
$cache = if ($CacheFolder) { $CacheFolder } else { Join-Path $root 'cache' }
New-Item -ItemType Directory -Force $cache | Out-Null
$record = [ordered]@{
    installed  = (Get-Date).ToString('s')
    skyrim     = $skyrim
    mordhau    = $mordhauRoot
    cache      = $cache
    bySteam    = (-not $SkyrimPath -and -not $MordhauPath)
    hadUe4ss   = $hadUe4ss
    placed     = @($placed)
    setAside   = @($kept)
}
$record | ConvertTo-Json -Depth 4 | Set-Content -Path (Join-Path $root 'mashup-install.json') -Encoding utf8
Say "$($placed.Count) files placed. Nothing of the games themselves was changed or replaced."
Say ''
Say ('TO PLAY:   powershell -NoProfile -ExecutionPolicy Bypass -File "' + (Join-Path $root 'Play-Mashup.ps1') + '"')
Say 'The first start prepares MORDHAU''s models from your own MORDHAU (about 2 GB, a few minutes, once).'
Say 'TO REMOVE: Uninstall-Mashup.ps1 in this folder.'
