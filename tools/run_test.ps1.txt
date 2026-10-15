# Dev test: start Skyrim with the plugin's autotest, enter the test cell through the console, wait for a log line.
# Usage: powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_test.ps1 [-WaitFor 'regex'] [-Cell WhiterunOrigin] [-Seconds 150]
# Leaves the game running and prints its PID; close it with `um win kill <pid>`, then delete DirectionalMelee.dev.txt
# from the game folder so normal play is untouched.
param(
    [string]$WaitFor = 'probe 75%',
    [string]$Cell = 'WhiterunOrigin',
    [int]$Seconds = 150,
    [int]$Mode = 1,            # 1 = automatic swings, 2 = scene only (send input with -Drive)
    [string]$Extra = '',       # extra dev settings, ';'-separated key=value, e.g. 'hold=0.4;twist=0'
    [string]$Drive = ''        # WinDrive commands sent once the weapon is drawn, separated by ';' e.g. 'rel 300 0;mdown 640 360;mup'
)
$driveCmds = @($Drive.Split(';') | ForEach-Object { $_.Trim() } | Where-Object { $_ })
if ($Cell -notmatch '^[A-Za-z0-9]+$') { "bad cell name '$Cell'"; exit 1 }

$game = 'E:\steam object\steamapps\common\Skyrim Special Edition'
$log = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'My Games\Skyrim Special Edition\SKSE\MordhauSkyrim.log'
$winDrive = Join-Path $PSScriptRoot '..\_deps\universal-modder\um\ps1\WinDrive.ps1'

if (Test-Path $log) { Clear-Content $log }

# Test settings live only for the test: without this file the plugin has no clips and leaves Skyrim's combat alone.
$dev = Join-Path $game 'DirectionalMelee.dev.txt'
Set-Content -Path $dev -Encoding ascii -Value (@(('cache=' + (Resolve-Path (Join-Path $PSScriptRoot '..\cache')).Path), "autotest=$Mode") + @($Extra.Split(';') | ForEach-Object { $_.Trim() } | Where-Object { $_ -match '^[a-z]+=-?[0-9.]+$' }))

# Started plainly, SkyrimSE.exe hands itself back to Steam (Bethesda launcher); with its app id set it starts.
# (other mashups' loaders are set aside; tools/other_mashups.ps1 on puts them back - the play launcher does that too)
Get-ChildItem -Path $game -Filter '*.asi' -ErrorAction SilentlyContinue | Where-Object { $_.Name -ne 'DirectionalMelee.asi' } | ForEach-Object { Rename-Item -Path $_.FullName -NewName ($_.Name + '.mordhau-off') -Force }
$env:MORDHAU_SKYRIM = '1'
$env:SteamAppId = '489830'
$env:SteamGameId = '489830'
# through SKSE's loader when it is installed (SkyUI needs SKSE); the loader starts SkyrimSE.exe and ends
$loader = Join-Path $game 'skse64_loader.exe'
if (Test-Path $loader) {
    Start-Process -FilePath $loader -WorkingDirectory $game
    $p = $null
    for ($i = 0; $i -lt 40 -and -not $p; $i++) { Start-Sleep -Milliseconds 500; $p = Get-Process SkyrimSE -ErrorAction SilentlyContinue | Select-Object -First 1 }
    if (-not $p) { 'SKSE loader did not start Skyrim'; exit 1 }
} else {
    $p = Start-Process -FilePath (Join-Path $game 'SkyrimSE.exe') -WorkingDirectory $game -PassThru
}
"pid $($p.Id)"

function Wait-Log([string]$pattern, [int]$timeout) {
    $deadline = (Get-Date).AddSeconds($timeout)
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Seconds 2
        if ($p.HasExited) { "game exited, code $($p.ExitCode)"; return $false }
        if ((Test-Path $log) -and (Select-String -Path $log -Pattern $pattern -Quiet)) { return $true }
    }
    return $false
}

if (-not (Wait-Log 'combat: hooks installed' 90)) { "hooks never installed"; if (Test-Path $log) { Get-Content $log }; exit 1 }
Start-Sleep -Seconds 8  # main menu settles

# console key, `coc <cell>`, Enter: sent as hardware scan codes, only while the game is the foreground window
# the console needs a moment to open before it takes text, so open it in one call and type in the next
$null = @('focus', 'scanmode on', 'key 0xC0') | powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -File $winDrive -Proc SkyrimSE
Start-Sleep -Milliseconds 1500
$cmds = @('focus', 'scanmode on')
foreach ($ch in ("COC $Cell").ToUpper().ToCharArray()) { $cmds += ('hold 0x{0:X} 40' -f [int]$ch) }
$cmds += 'key 0x0D'
$replies = $cmds | powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -File $winDrive -Proc SkyrimSE
"drive: $(@($replies | Where-Object { $_ -eq 'ok' }).Count) ok of $($cmds.Count)"

if ($driveCmds.Count -gt 0) {
    if (Wait-Log 'autotest: weapon drawn' 90) {
        Start-Sleep -Seconds 3
        $r = (@('focus', 'scanmode on') + $driveCmds) | powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -File $winDrive -Proc SkyrimSE
        "input: $(@($r | Where-Object { $_ -eq 'ok' }).Count) ok of $($driveCmds.Count + 2)"
    } else { "weapon never drawn" }
}

$found = Wait-Log $WaitFor $Seconds
"wait for '$WaitFor': $found"
Get-Content $log | Where-Object { $_ -notmatch 'autotest: stage' }
