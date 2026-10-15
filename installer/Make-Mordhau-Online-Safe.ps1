# Makes sure MORDHAU's folder holds nothing that loads the mashup's script, so that MORDHAU can be started normally
# (Steam, online) exactly as it shipped. The mashup's launcher does this by itself at every start and end; run this
# when in doubt - after a crash, a power cut, or before playing MORDHAU online.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File Make-Mordhau-Online-Safe.ps1
param([string]$MordhauPath = '')
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
function Say($t) { Write-Host "[safe] $t" }
$bin = $null
if ($MordhauPath) { $bin = Join-Path $MordhauPath.TrimEnd('\') 'Mordhau\Binaries\Win64' }
elseif (Test-Path (Join-Path $root 'mashup-install.json')) {
    $record = Get-Content (Join-Path $root 'mashup-install.json') -Raw | ConvertFrom-Json
    $bin = Join-Path $record.mordhau 'Mordhau\Binaries\Win64'
}
if (-not $bin -or -not (Test-Path $bin)) {
    $steam = (Get-ItemProperty -Path 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue).SteamPath
    $libraries = @($steam)
    $list = if ($steam) { Join-Path $steam 'steamapps\libraryfolders.vdf' } else { $null }
    if ($list -and (Test-Path $list)) { foreach ($line in Get-Content $list) { if ($line -match '"path"\s+"([^"]+)"') { $libraries += ($Matches[1] -replace '\\', '\') } } }
    foreach ($library in $libraries) {
        if ($library -and (Test-Path (Join-Path $library 'steamapps\common\Mordhau\Mordhau\Binaries\Win64\Mordhau-Win64-Shipping.exe'))) { $bin = Join-Path $library 'steamapps\common\Mordhau\Mordhau\Binaries\Win64' }
    }
}
if (-not $bin -or -not (Test-Path $bin)) { Say 'MORDHAU was not found. Give its folder: -MordhauPath "<...\Mordhau>"'; exit 1 }
if (Get-Process Mordhau-Win64-Shipping -ErrorAction SilentlyContinue) { Say 'MORDHAU is running. Close it first, then run this again.'; exit 1 }
$removed = 0
foreach ($name in @('dwmapi.dll', 'dwmapi.dll.old')) {
    $file = Join-Path $bin $name
    if (Test-Path $file) { Remove-Item $file -Force; $removed++; Say "removed: $file" }
}
if (Test-Path (Join-Path $bin 'dwmapi.dll')) { Say 'NOT SAFE: dwmapi.dll is still in MORDHAU''s folder. Do not play online; delete it by hand.'; exit 1 }
Say ("MORDHAU's folder is clean" + $(if ($removed) { " ($removed file(s) taken out)" } else { ' (nothing had been left there)' }) + ': it starts without the mashup. Safe for normal play.')
