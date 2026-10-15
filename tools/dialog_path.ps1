# Dev: types a file path into the "File name" box of an open Windows file dialog (by its window title) and,
# with -Open, confirms it. The path comes from a text file so that spaces and odd characters survive.
# usage: dialog_path.ps1 -Title Open -PathFile C:\...\path.txt [-Open]
param([Parameter(Mandatory = $true)][string]$Title, [Parameter(Mandatory = $true)][string]$PathFile, [switch]$Open)
Add-Type -AssemblyName System.Windows.Forms
$shell = New-Object -ComObject WScript.Shell
if (-not $shell.AppActivate($Title)) { 'no dialog'; exit 1 }
Start-Sleep -Milliseconds 700
if ($Open) {
    [System.Windows.Forms.SendKeys]::SendWait('%o')
    'opened'
} else {
    $path = (Get-Content -LiteralPath $PathFile -Raw).Trim()
    # (SendKeys reads + ^ % ~ ( ) { } [ ] as commands: each is wrapped in braces)
    $escaped = [regex]::Replace($path, '[+^%~(){}\[\]]', '{$0}')
    [System.Windows.Forms.SendKeys]::SendWait('%n')
    Start-Sleep -Milliseconds 400
    [System.Windows.Forms.SendKeys]::SendWait('^a')
    [System.Windows.Forms.SendKeys]::SendWait($escaped)
    'typed'
}
