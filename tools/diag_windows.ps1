# Diagnose settings window state and message wiring. ASCII-only.
$ErrorActionPreference = 'Stop'
Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class Diag {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
  [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h, uint m, UIntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr SendMessageW(IntPtr h, uint m, UIntPtr w, IntPtr l);
  public static IntPtr uiHwnd = IntPtr.Zero;
  public static IntPtr hostHwnd = IntPtr.Zero;
  public static void Find() {
    EnumProc cb = delegate(IntPtr h, IntPtr l) {
      StringBuilder sb = new StringBuilder(256);
      GetClassName(h, sb, 256);
      string c = sb.ToString();
      if (c == "MouseSideUI") uiHwnd = h;
      if (c == "MouseSideHost") hostHwnd = h;
      return true;
    };
    EnumWindows(cb, IntPtr.Zero);
  }
}
"@
$ErrorActionPreference = 'Continue'
[Diag]::Find()
$ui = [Diag]::uiHwnd
$hw = [Diag]::hostHwnd
Write-Output ("ui=" + $ui + " host=" + $hw)
if ($ui -eq [IntPtr]::Zero) { Write-Output 'NO-UI-WINDOW'; exit 1 }
[uint32]$p1 = 0; [Diag]::GetWindowThreadProcessId($ui, [ref]$p1) | Out-Null
[uint32]$p2 = 0; [Diag]::GetWindowThreadProcessId($hw, [ref]$p2) | Out-Null
Write-Output ("pids: ui=" + $p1 + " host=" + $p2 + "  visible=" + [Diag]::IsWindowVisible($ui) + " iconic=" + [Diag]::IsIconic($ui))

Write-Output '--- direct ShowWindow(SW_SHOW=5) ---'
[Diag]::ShowWindow($ui, 5) | Out-Null
Start-Sleep -Milliseconds 500
Write-Output ('visible now: ' + [Diag]::IsWindowVisible($ui))

Write-Output '--- hide again, then post WM_APP_SHOWUI (0x8003) to host ---'
[Diag]::ShowWindow($ui, 0) | Out-Null
Start-Sleep -Milliseconds 300
Write-Output ('visible now: ' + [Diag]::IsWindowVisible($ui))
$ok = [Diag]::PostMessageW($hw, 0x8003, [UIntPtr]::Zero, [IntPtr]::Zero)
Start-Sleep -Milliseconds 800
Write-Output ('post returned ' + $ok + '; visible now: ' + [Diag]::IsWindowVisible($ui))

Write-Output '--- hide again, then post WM_APP_TRAY (0x8002) lParam=WM_LBUTTONDBLCLK (0x203) ---'
[Diag]::ShowWindow($ui, 0) | Out-Null
Start-Sleep -Milliseconds 300
$ok = [Diag]::PostMessageW($hw, 0x8002, [UIntPtr]::Zero, [IntPtr]0x203)
Start-Sleep -Milliseconds 800
Write-Output ('post returned ' + $ok + '; visible now: ' + [Diag]::IsWindowVisible($ui))
Write-Output 'DIAG_DONE'
