# Capture the MouseSide settings window to a PNG. ASCII-only.
param([string]$OutPath = "$PSScriptRoot\..\test\ui.png")
$ErrorActionPreference = 'Stop'
Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class WinCap {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  public struct RECT { public int L; public int T; public int R; public int B; }
  public static IntPtr FindByClass(string cls) {
    IntPtr found = IntPtr.Zero;
    EnumProc cb = delegate(IntPtr h, IntPtr l) {
      StringBuilder sb = new StringBuilder(256);
      GetClassName(h, sb, 256);
      if (sb.ToString() == cls && IsWindowVisible(h)) { found = h; return false; }
      return true;
    };
    EnumWindows(cb, IntPtr.Zero);
    return found;
  }
}
"@
[WinCap]::SetProcessDPIAware() | Out-Null
$h = [WinCap]::FindByClass('MouseSideUI')
if ($h -eq [IntPtr]::Zero) { throw 'MouseSideUI window not found' }
[WinCap]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 500
$r = New-Object WinCap+RECT
[WinCap]::GetWindowRect($h, [ref]$r) | Out-Null
$w = $r.R - $r.L
$ht = $r.B - $r.T
if ($w -le 0 -or $ht -le 0) { throw 'window has zero size (hidden?)' }
Add-Type -AssemblyName System.Drawing
$bmp = New-Object System.Drawing.Bitmap($w, $ht)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($r.L, $r.T, 0, 0, $bmp.Size)
$dir = Split-Path -Parent $OutPath
if (!(Test-Path $dir)) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
$bmp.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
Write-Output ("SAVED $OutPath ($w x $ht)")
