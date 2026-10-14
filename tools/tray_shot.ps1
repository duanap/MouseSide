# Capture the bottom-right corner of the primary screen (system tray area).
param([string]$OutPath = "$PSScriptRoot\..\test\tray.png")
$ErrorActionPreference = 'Stop'
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class TrayCap {
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
}
"@
[TrayCap]::SetProcessDPIAware() | Out-Null
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
$b = [System.Windows.Forms.SystemInformation]::VirtualScreen
$w = 420
$ht = 140
$x = $b.Right - $w
$y = $b.Bottom - $ht
$bmp = New-Object System.Drawing.Bitmap($w, $ht)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($x, $y, 0, 0, $bmp.Size)
$dir = Split-Path -Parent $OutPath
if (!(Test-Path $dir)) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
$bmp.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
Write-Output ("SAVED $OutPath")
