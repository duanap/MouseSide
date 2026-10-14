# E2E variant for Button5=Disabled:
#  - XBUTTON2 must be swallowed: clipboard unchanged (no app copies),
#    textbox content unchanged (no navigation side effect checkable here).
#  - XBUTTON1 (paste) must still work.
# ASCII-only.
$ErrorActionPreference = 'Stop'

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class MouseInject {
  [StructLayout(LayoutKind.Sequential)]
  struct MOUSEINPUT { public int dx; public int dy; public uint mouseData; public uint dwFlags; public uint time; public IntPtr dwExtraInfo; }
  [StructLayout(LayoutKind.Explicit)]
  struct INPUU { [FieldOffset(0)] public MOUSEINPUT mi; }
  [StructLayout(LayoutKind.Sequential)]
  struct INPUT { public uint type; public INPUU u; }
  [DllImport("user32.dll", SetLastError=true)]
  static extern uint SendInput(uint n, INPUT[] inputs, int size);
  public static void XButton(uint button) {
    INPUT d = new INPUT(); d.type = 0; d.u.mi.dwFlags = 0x80; d.u.mi.mouseData = button;
    INPUT u = new INPUT(); u.type = 0; u.u.mi.dwFlags = 0x100; u.u.mi.mouseData = button;
    INPUT[] a = { d, u };
    if (SendInput(2, a, Marshal.SizeOf(typeof(INPUT))) != 2) throw new Exception("SendInput(mouse)");
  }
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
}
"@
Add-Type -AssemblyName System.Windows.Forms

$old = $null
try { $old = Get-Clipboard -Raw } catch {}
Set-Clipboard -Value 'PROBE-SENTINEL'

$f = New-Object System.Windows.Forms.Form
$f.Text = 'MouseSide Disabled Test'
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
        $tb.Text = 'SHOULD-NOT-COPY'; $tb.SelectAll()
      }
      1 { [MouseInject]::XButton(2) }     # disabled: must do nothing
      2 { $script:r['afterDisabled'] = [string](Get-Clipboard -Raw)
          $tb.Clear(); $tb.Focus() | Out-Null }
      3 { [MouseInject]::XButton(1) }     # paste still enabled
      4 { $script:r['paste'] = [string]$tb.Text
          $f.Close() }
    }
  } catch { $script:r['error'] = $_.Exception.Message; $f.Close() }
  $script:step++
})
$timer.Start()
[System.Windows.Forms.Application]::Run($f)
$timer.Stop()

Write-Output ("clipboard after XBUTTON2 (expect PROBE-SENTINEL): [" + $script:r['afterDisabled'] + "]")
Write-Output ("textbox after XBUTTON1  (expect PROBE-SENTINEL): [" + $script:r['paste'] + "]")
if ($script:r['error']) { Write-Output ("ERROR: " + $script:r['error']) }

if ($script:r['afterDisabled'] -eq 'PROBE-SENTINEL') { Write-Output 'RESULT-DISABLED: PASS' } else { Write-Output 'RESULT-DISABLED: FAIL' }
if ($script:r['paste'] -eq 'PROBE-SENTINEL') { Write-Output 'RESULT-PASTE-STILL-OK: PASS' } else { Write-Output 'RESULT-PASTE-STILL-OK: FAIL' }

if ($old) { Set-Clipboard -Value $old }
Write-Output 'E2E_DONE'
