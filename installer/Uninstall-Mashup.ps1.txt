# Takes the MORDHAU x Skyrim mashup out of both games again: every file Install-Mashup.ps1 put down (it wrote the list
# into mashup-install.json), and puts back what it had set aside. Your saves, your other mods and the games are not
# touched. -KeepCache leaves the prepared MORDHAU models (about 2 GB) where they are.
param([switch]$KeepCache)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
function Say($t) { Write-Host "[uninstall] $t" }
$file = Join-Path $root 'mashup-install.json'
if (-not (Test-Path $file)) { Say 'Nothing to do: no mashup-install.json here (not installed from this folder).'; exit 0 }
if (Get-Process SkyrimSE, Mordhau-Win64-Shipping -ErrorAction SilentlyContinue) { Say 'Close Skyrim and MORDHAU first.'; exit 1 }
$record = Get-Content $file -Raw | ConvertFrom-Json
$removed = 0
foreach ($path in @($record.placed)) {
    if (Test-Path $path) { Remove-Item $path -Force; $removed++ }
}
# what the launcher writes while playing
$skyrim = $record.skyrim; $mordhauBin = Join-Path $record.mordhau 'Mordhau\Binaries\Win64'
foreach ($path in @("$skyrim\DirectionalMelee.dev.txt", "$mordhauBin\dwmapi.dll", "$mordhauBin\dwmapi.dll.old", "$mordhauBin\Mods\PoseStream\cache_dir.txt")) {
    if (Test-Path $path) { Remove-Item $path -Force -ErrorAction SilentlyContinue }
}
foreach ($folder in @("$mordhauBin\Mods\PoseStream\Scripts", "$mordhauBin\Mods\PoseStream")) {
    if ((Test-Path $folder) -and -not (Get-ChildItem $folder -Force)) { Remove-Item $folder -Force }
}
if (-not $record.hadUe4ss) {
    foreach ($folder in @("$mordhauBin\UE4SS_Signatures", "$mordhauBin\Mods")) {
        if (Test-Path $folder) { Remove-Item $folder -Recurse -Force -ErrorAction SilentlyContinue }
    }
    Remove-Item "$mordhauBin\UE4SS.log" -Force -ErrorAction SilentlyContinue
}
foreach ($path in @($record.setAside)) {
    if (Test-Path "$path.before-mashup") { Move-Item "$path.before-mashup" $path -Force; Say "put back: $path" }
}
# other mashups' loaders the launcher may have left set aside
Get-ChildItem -Path $skyrim -Filter '*.asi.mordhau-off' -ErrorAction SilentlyContinue | ForEach-Object { Rename-Item $_.FullName ($_.Name -replace '\.mordhau-off$', '') }
$plugins = Join-Path $env:LOCALAPPDATA 'Skyrim Special Edition\Plugins.txt'
if (Test-Path $plugins) {
    $lines = @(Get-Content $plugins | Where-Object { $_ -notmatch '^\*?MordhauWeapons\.esp\s*$' })
    Set-Content -Path $plugins -Encoding ascii -Value $lines
}
if (-not $KeepCache -and $record.cache -and (Test-Path $record.cache)) {
    Remove-Item $record.cache -Recurse -Force
    Say "the prepared models are removed: $($record.cache)"
}
Remove-Item $file -Force
Say "$removed files removed. Skyrim and MORDHAU are as they were. (Weapons from MORDHAU that are in a save simply vanish from it.)"
