param([string]$Output = "$PSScriptRoot/../build-codex/settings.png")
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public class MouseSideCapture {
 [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left,Top,Right,Bottom; }
 [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls,string title);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hwnd,out Rect rect);
 [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hwnd,IntPtr dc,uint flags);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd,uint message,IntPtr w,IntPtr l);
 [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
}
'@
$taskWindow = [MouseSideCapture]::FindWindow('MouseSide.Codex.Settings','MouseSide')
if ($taskWindow -eq [IntPtr]::Zero) { throw 'MouseSide window not running' }
[void][MouseSideCapture]::PostMessage($taskWindow,0x800B,[IntPtr]::Zero,[IntPtr]::Zero)
Start-Sleep -Milliseconds 300
$taskDpiContext = [MouseSideCapture]::SetThreadDpiAwarenessContext([IntPtr](-4))
$taskRect = New-Object MouseSideCapture+Rect
[void][MouseSideCapture]::GetWindowRect($taskWindow,[ref]$taskRect)
$taskBitmap = New-Object System.Drawing.Bitmap(($taskRect.Right-$taskRect.Left),($taskRect.Bottom-$taskRect.Top))
$taskGraphics = [System.Drawing.Graphics]::FromImage($taskBitmap)
$taskDc = $taskGraphics.GetHdc()
try { if(-not [MouseSideCapture]::PrintWindow($taskWindow,$taskDc,2)) { throw 'PrintWindow failed' } }
finally { $taskGraphics.ReleaseHdc($taskDc) }
$taskBitmap.Save([IO.Path]::GetFullPath($Output),[System.Drawing.Imaging.ImageFormat]::Png)
$taskGraphics.Dispose()
$taskBitmap.Dispose()
[void][MouseSideCapture]::SetThreadDpiAwarenessContext($taskDpiContext)
Write-Output ([IO.Path]::GetFullPath($Output))
