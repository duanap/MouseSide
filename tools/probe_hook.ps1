# Probe: install WH_MOUSE_LL in-process, pump messages, inject XBUTTON events,
# and log whether they reach the low-level hook chain. ASCII-only.
$ErrorActionPreference = 'Stop'
Add-Type @"
using System;
using System.Threading;
using System.Runtime.InteropServices;
public static class Probe {
  public delegate IntPtr HookProc(int nCode, IntPtr wp, IntPtr lp);
  [StructLayout(LayoutKind.Sequential)]
  public struct MSLL { public uint x, y; public uint mouseData, flags, time; public IntPtr extra; }
  [StructLayout(LayoutKind.Sequential)]
  public struct MSG { public IntPtr hwnd; public uint message, wParam, lParam; public uint time; public int ptX, ptY; }
  [DllImport("user32.dll", SetLastError=true)] public static extern IntPtr SetWindowsHookEx(int idHook, HookProc p, IntPtr hMod, uint tid);
  [DllImport("user32.dll")] public static extern bool UnhookWindowsHookEx(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr CallNextHookEx(IntPtr h, int nc, IntPtr wp, IntPtr lp);
  [DllImport("kernel32.dll")] public static extern IntPtr GetModuleHandle(string n);
  [DllImport("user32.dll")] public static extern bool PeekMessage(out MSG m, IntPtr h, uint mn, uint mx, uint remove);
  [DllImport("user32.dll")] public static extern bool TranslateMessage(ref MSG m);
  [DllImport("user32.dll")] public static extern IntPtr DispatchMessage(ref MSG m);
  [StructLayout(LayoutKind.Sequential)]
  public struct MOUSEINPUT { public int dx; public int dy; public uint mouseData; public uint dwFlags; public uint time; public IntPtr dwExtraInfo; }
  [StructLayout(LayoutKind.Sequential)]
  public struct INPUT { public uint type; public MOUSEINPUT mi; }
  [DllImport("user32.dll", SetLastError=true)] public static extern uint SendInput(uint n, INPUT[] inputs, int size);

  static HookProc cb;
  public static IntPtr hook = IntPtr.Zero;
  public static int hookErr;
  public static System.Collections.Generic.List<string> Log = new System.Collections.Generic.List<string>();

  public static IntPtr LLProc(int nc, IntPtr wp, IntPtr lp) {
    if (nc >= 0) {
      MSLL s = (MSLL)Marshal.PtrToStructure(lp, typeof(MSLL));
      Log.Add("msg=0x" + wp.ToString("X") + " xbHi=" + (s.mouseData >> 16));
    }
    return CallNextHookEx(hook, nc, wp, lp);
  }
  public static void Install() { cb = LLProc; hook = SetWindowsHookEx(14, cb, GetModuleHandle(null), 0); hookErr = Marshal.GetLastWin32Error(); }
  public static void Pump(int ms) {
    DateTime end = DateTime.UtcNow.AddMilliseconds(ms);
    MSG m;
    while (DateTime.UtcNow < end) {
      while (PeekMessage(out m, IntPtr.Zero, 0, 0, 1)) { TranslateMessage(ref m); DispatchMessage(ref m); }
      Thread.Sleep(10);
    }
  }
  public static void Inject(uint button) {
    INPUT down = new INPUT(); down.type = 0; down.mi.dwFlags = 0x80; down.mi.mouseData = button;
    INPUT up = new INPUT(); up.type = 0; up.mi.dwFlags = 0x100; up.mi.mouseData = button;
    INPUT[] arr = new INPUT[2]; arr[0] = down; arr[1] = up;
    Console.WriteLine("SendInput sent=" + SendInput(2, arr, Marshal.SizeOf(typeof(INPUT))));
  }
  public static void Uninstall() { if (hook != IntPtr.Zero) UnhookWindowsHookEx(hook); }
}
"@
[Probe]::Install()
Write-Output ("hook installed: " + ([Probe]::hook -ne [IntPtr]::Zero) + " err=" + [Probe]::hookErr)
[Probe]::Pump(400)
[Probe]::Inject(2)
[Probe]::Pump(600)
[Probe]::Inject(1)
[Probe]::Pump(600)
[Probe]::Uninstall()
Write-Output "---- probe hook log ----"
[Probe]::Log
Write-Output 'PROBE_DONE'
