# Dev: put other mashups' loaders back into Skyrim's folder after a test session (the test starter sets them aside).
# usage: other_mashups.ps1          (puts back every *.asi.mordhau-off)
$skyrim = 'E:\steam object\steamapps\common\Skyrim Special Edition'
Get-ChildItem -Path $skyrim -Filter '*.asi.mordhau-off' -ErrorAction SilentlyContinue | ForEach-Object {
    $back = $_.FullName -replace '\.mordhau-off$', ''
    if (-not (Test-Path $back)) { Rename-Item -Path $_.FullName -NewName (Split-Path $back -Leaf) } else { Remove-Item $_.FullName -Force }
    "put back: $(Split-Path $back -Leaf)"
}
