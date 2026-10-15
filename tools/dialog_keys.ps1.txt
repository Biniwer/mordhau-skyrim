# Dev: sends keys (SendKeys notation) to an open Windows file dialog, found by its window title.
# usage: dialog_keys.ps1 -Title "File Upload" -Keys "+{TAB}^a"
param([Parameter(Mandatory = $true)][string]$Title, [Parameter(Mandatory = $true)][string]$Keys)
Add-Type -AssemblyName System.Windows.Forms
$shell = New-Object -ComObject WScript.Shell
if (-not $shell.AppActivate($Title)) { 'no dialog'; exit 1 }
Start-Sleep -Milliseconds 600
[System.Windows.Forms.SendKeys]::SendWait($Keys)
'sent'
