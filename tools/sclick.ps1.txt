# Dev: click at screen coordinates (physical pixels), for ordinary program windows. usage: sclick.ps1 x y [x y ...] (0.6 s apart)
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class SClick {
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
}
"@
[void][SClick]::SetProcessDPIAware()
for ($i = 0; $i + 1 -lt $args.Count; $i += 2) {
  [void][SClick]::SetCursorPos([int]$args[$i], [int]$args[$i + 1]); Start-Sleep -Milliseconds 150
  [SClick]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 60; [SClick]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 600
}
'clicked'
