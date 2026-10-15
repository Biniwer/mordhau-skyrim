# Dev: puts a text file's contents on the clipboard. usage: clip.ps1 -File <path>
param([Parameter(Mandatory = $true)][string]$File)
Get-Content -Raw -Encoding UTF8 -LiteralPath $File | Set-Clipboard
