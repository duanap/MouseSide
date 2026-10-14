# E2E test v3: pumped WinForms rig + TopMost + foreground verification at
# injection time + GetAsyncKeyState probe. ASCII-only.
$ErrorActionPreference = 'Stop'

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class MouseInject {
  [StructLayout(LayoutKind.Sequential)]
  struct KEYBDINPUT { public ushort wVk; public ushort wScan; public uint dwFlags; public uint time; public IntPtr dwExtraInfo; }
  [StructLayout(LayoutKind.Sequential)]
  struct MOUSEINPUT { public int dx; public int dy; public uint mouseData; public uint dwFlags; public uint time; public IntPtr dwExtraInfo; }
  [StructLayout(LayoutKind.Explicit)]
  struct INPUU { [FieldOffset(0)] public MOUSEINPUT mi; [FieldOffset(0)] public KEYBDINPUT ki; }
  [StructLayout(LayoutKind.Sequential)]
  struct INPUT { public uint type; public INPUU u; }
  [DllImport("user32.dll", SetLastError=true)]
  static extern uint SendInput(uint n, INPUT[] inputs, int size);
  static INPUT Key(ushort vk, bool up) {
    INPUT i = new INPUT(); i.type = 1; i.u.ki.wVk = vk; i.u.ki.dwFlags = up ? (uint)2 : (uint)0;
    return i;
  }
  public static void CtrlCombo(ushort vk) {
    INPUT[] a = { Key(0x11, false), Key(vk, false), Key(vk, true), Key(0x11, true) };
    if (SendInput(4, a, Marshal.SizeOf(typeof(INPUT))) != 4) throw new Exception("SendInput(kbd)");
  }
  public static void XButton(uint button) {
    INPUT d = new INPUT(); d.type = 0; d.u.mi.dwFlags = 0x80; d.u.mi.mouseData = button;
    INPUT u = new INPUT(); u.type = 0; u.u.mi.dwFlags = 0x100; u.u.mi.mouseData = button;
    INPUT[] a = { d, u };
    if (SendInput(2, a, Marshal.SizeOf(typeof(INPUT))) != 2) throw new Exception("SendInput(mouse)");
  }
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern short GetAsyncKeyState(int vk);
  public static bool KeySeen(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }
}
"@
Add-Type -AssemblyName System.Windows.Forms

function Probe-Keys([int]$ms, [int]$vk) {
  $seen = $false
  $end = [DateTime]::UtcNow.AddMilliseconds($ms)
  while ([DateTime]::UtcNow -lt $end) {
    if ([MouseInject]::KeySeen($vk) -or [MouseInject]::KeySeen(0x11)) { $seen = $true }
    Start-Sleep -Milliseconds 15
  }
  # keep polling a bit more since injection may land late
  return $seen
}

$old = $null
try { $old = Get-Clipboard -Raw } catch {}
Write-Output ("CLIPBOARD-BEFORE: [" + $old + "]")

$f = New-Object System.Windows.Forms.Form
$f.Text = 'MouseSide E2E Target'
$f.Size = New-Object System.Drawing.Size(560, 380)
$f.StartPosition = 'Manual'
$f.Location = New-Object System.Drawing.Point(80, 80)
$f.TopMost = $true
$tb = New-Object System.Windows.Forms.TextBox
$tb.Multiline = $true
$tb.SetBounds(10, 10, 520, 300)
$f.Controls.Add($tb)

$script:step = 0
$script:r = @{}
$timer = New-Object System.Windows.Forms.Timer
$timer.Interval = 900
$timer.Add_Tick({
  try {
    switch ($script:step) {
      0 {
        [MouseInject]::SetForegroundWindow($f.Handle) | Out-Null
        $f.Activate(); $tb.Focus() | Out-Null
        $tb.Text = 'CONTROL-OK'; $tb.SelectAll()
        $script:r['fg0'] = ([MouseInject]::GetForegroundWindow() -eq $f.Handle)
      }
      1 {
        $script:r['fg1'] = ([MouseInject]::GetForegroundWindow() -eq $f.Handle)
        [MouseInject]::CtrlCombo(0x43)      # control: direct Ctrl+C
      }
      2 { $script:r['control'] = [string](Get-Clipboard -Raw)
          $tb.Text = 'SIDE-COPY'; $tb.SelectAll()
          [MouseInject]::SetForegroundWindow($f.Handle) | Out-Null
          $tb.Focus() | Out-Null }
      3 { $script:r['fg3'] = ([MouseInject]::GetForegroundWindow() -eq $f.Handle)
          [MouseInject]::XButton(2) }       # our app -> Ctrl+C
      4 { $script:r['copy'] = [string](Get-Clipboard -Raw)
          $tb.Clear(); $tb.Focus() | Out-Null }
      5 { [MouseInject]::XButton(1) }       # our app -> Ctrl+V
      6 { $script:r['paste'] = [string]$tb.Text
          $f.Close() }
    }
  } catch { $script:r['error'] = $_.Exception.Message; $f.Close() }
  $script:step++
})
$timer.Start()
[System.Windows.Forms.Application]::Run($f)
$timer.Stop()

Write-Output ("FG@step0/1/3: " + $script:r['fg0'] + "/" + $script:r['fg1'] + "/" + $script:r['fg3'])
Write-Output ("CONTROL (direct Ctrl+C): [" + $script:r['control'] + "]")
Write-Output ("XBUTTON2 -> our Ctrl+C:  [" + $script:r['copy'] + "]")
Write-Output ("XBUTTON1 -> our Ctrl+V:  [" + $script:r['paste'] + "]")
if ($script:r['error']) { Write-Output ("ERROR: " + $script:r['error']) }

if ($script:r['control'] -eq 'CONTROL-OK') { Write-Output 'RESULT-RIG: PASS' } else { Write-Output 'RESULT-RIG: FAIL' }
if ($script:r['copy'] -eq 'SIDE-COPY') { Write-Output 'RESULT-COPY: PASS' } else { Write-Output 'RESULT-COPY: FAIL' }
if ($script:r['paste'] -eq 'SIDE-COPY') { Write-Output 'RESULT-PASTE: PASS' } else { Write-Output 'RESULT-PASTE: FAIL' }

if ($old) { Set-Clipboard -Value $old }
$failed = ($script:r['control'] -ne 'CONTROL-OK') -or
          ($script:r['copy'] -ne 'SIDE-COPY') -or
          ($script:r['paste'] -ne 'SIDE-COPY') -or
          ($null -ne $script:r['error'])
Write-Output 'E2E_DONE'
if ($failed) { exit 1 } else { exit 0 }
