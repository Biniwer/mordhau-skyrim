# Dev: a half-size picture of the whole desktop (all screens). usage: screen_grab.ps1 -Out E:\...\screen.png
param([Parameter(Mandatory = $true)][string]$Out)
Add-Type -AssemblyName System.Drawing, System.Windows.Forms
$b = [System.Windows.Forms.SystemInformation]::VirtualScreen
$bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($b.X, $b.Y, 0, 0, $bmp.Size)
$small = New-Object System.Drawing.Bitmap $bmp, ([int]($b.Width / 2)), ([int]($b.Height / 2))
$small.Save($Out)
"$($b.X),$($b.Y) $($b.Width)x$($b.Height)"
