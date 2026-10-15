# Dev: a picture of the Skyrim window as it is on screen, without bringing it forward or sending it anything.
# usage: grab_skyrim.ps1 -Out E:\MordhauSkyrim\shots\mm\x.png     (prints the window's size; nothing is saved while it is minimised)
param([Parameter(Mandatory = $true)][string]$Out)
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class GrabSky {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
}
"@
[GrabSky]::SetProcessDPIAware() | Out-Null
$p = Get-Process SkyrimSE -ErrorAction Stop | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
$r = New-Object GrabSky+RECT
[GrabSky]::GetWindowRect($p.MainWindowHandle, [ref]$r) | Out-Null
$w = $r.R - $r.L; $h = $r.B - $r.T
if ([GrabSky]::IsIconic($p.MainWindowHandle) -or $w -lt 800) { "minimised ($w x $h): nothing saved"; exit 1 }
$bmp = New-Object System.Drawing.Bitmap $w, $h
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($r.L, $r.T, 0, 0, $bmp.Size)
$bmp.Save($Out)
"$w x $h saved"
