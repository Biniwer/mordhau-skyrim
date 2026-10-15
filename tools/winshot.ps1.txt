# Dev: screenshot of an ordinary (non-game) window by process name, brought to the front first. Prints its screen rectangle.
# usage: winshot.ps1 -Proc TexGenx64 -Out E:\MordhauSkyrim\shots\now\tg.png
param([Parameter(Mandatory = $true)][string]$Proc, [Parameter(Mandatory = $true)][string]$Out)
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class WShot {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr h, System.Text.StringBuilder s, int n);
}
"@
[void][WShot]::SetProcessDPIAware()
$p = Get-Process $Proc -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
if (-not $p) { "no window for $Proc"; exit 1 }
# (a dialog of the same program in front of its main window: take whatever of it is in the foreground)
[void][WShot]::ShowWindow($p.MainWindowHandle, 5)
[void][WShot]::SetForegroundWindow($p.MainWindowHandle)
Start-Sleep -Milliseconds 500
$h = [WShot]::GetForegroundWindow()
$r = New-Object WShot+RECT
[void][WShot]::GetWindowRect($h, [ref]$r)
$w = $r.R - $r.L; $hh = $r.B - $r.T
if ($w -le 0 -or $hh -le 0) { 'window has no size'; exit 1 }
$bmp = New-Object System.Drawing.Bitmap $w, $hh
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($r.L, $r.T, 0, 0, (New-Object System.Drawing.Size $w, $hh))
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$t = New-Object System.Text.StringBuilder 300
[void][WShot]::GetWindowText($h, $t, 300)
"rect $($r.L) $($r.T) $w $hh  title: $t"
