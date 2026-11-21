param([int]$ProcId, [string]$Out)

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Cap2 {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint f);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
    [DllImport("user32.dll")] public static extern IntPtr SetActiveWindow(IntPtr h);
}
"@

$h = (Get-Process -Id $ProcId).MainWindowHandle
if ($h -eq 0) { throw "no window" }
[Cap2]::ShowWindow($h, 9) | Out-Null
[Cap2]::SetWindowPos($h, [IntPtr](-1), 20, 20, 0, 0, 0x0001 -bor 0x0002) | Out-Null
[Cap2]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 1200

$r = New-Object Cap2+RECT
[Cap2]::GetWindowRect($h, [ref]$r) | Out-Null
$w = $r.R - $r.L; $ht = $r.B - $r.T
$bmp = New-Object System.Drawing.Bitmap $w, $ht
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($r.L, $r.T, 0, 0, $bmp.Size)
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
"captured ${w}x${ht}"
