# Generate MouseSide tray/app icons (normal + paused) as .ico files.
# ASCII-only; draws with System.Drawing, writes ICO containers with PNG payloads.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$assets = Join-Path (Split-Path -Parent $root) 'assets'
New-Item -ItemType Directory -Force -Path $assets | Out-Null

function Add-RoundRect([System.Drawing.Drawing2D.GraphicsPath]$p,
                       [float]$x, [float]$y, [float]$w, [float]$h, [float]$r, [float]$s) {
  $x *= $s; $y *= $s; $w *= $s; $h *= $s; $r *= $s
  $d = 2 * $r
  $p.AddArc($x, $y, $d, $d, 180, 90)
  $p.AddArc($x + $w - $d, $y, $d, $d, 270, 90)
  $p.AddArc($x + $w - $d, $y + $h - $d, $d, $d, 0, 90)
  $p.AddArc($x, $y + $h - $d, $d, $d, 90, 90)
  $p.CloseFigure()
}

function New-IconBitmap([int]$size, [System.Drawing.Color]$bg,
                        [System.Drawing.Color]$body, [System.Drawing.Color]$wheel,
                        [System.Drawing.Color]$split) {
  $b = New-Object System.Drawing.Bitmap($size, $size)
  $g = [System.Drawing.Graphics]::FromImage($b)
  $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
  $s = $size / 256.0

  # rounded-square background
  $bgPath = New-Object System.Drawing.Drawing2D.GraphicsPath
  Add-RoundRect $bgPath 16 16 224 224 52 $s
  $bgBrush = New-Object System.Drawing.SolidBrush($bg)
  $g.FillPath($bgBrush, $bgPath)

  # mouse body
  $mPath = New-Object System.Drawing.Drawing2D.GraphicsPath
  Add-RoundRect $mPath 84 56 88 144 42 $s
  $bodyBrush = New-Object System.Drawing.SolidBrush($body)
  $g.FillPath($bodyBrush, $mPath)

  # button splitter line
  $pen = New-Object System.Drawing.Pen($split, (5.0 * $s))
  $g.DrawLine($pen, (88 * $s), (118 * $s), (168 * $s), (118 * $s))

  # scroll wheel
  $wPath = New-Object System.Drawing.Drawing2D.GraphicsPath
  Add-RoundRect $wPath 120 72 16 38 8 $s
  $wBrush = New-Object System.Drawing.SolidBrush($wheel)
  $g.FillPath($wBrush, $wPath)

  $g.Dispose()
  return $b
}

function Save-Ico([System.Drawing.Bitmap]$src, [string]$file, [int[]]$sizes) {
  $ms = New-Object System.IO.MemoryStream
  $bw = New-Object System.IO.BinaryWriter($ms)
  $bw.Write([uint16]0); $bw.Write([uint16]1); $bw.Write([uint16]$sizes.Count)

  $pngs = @()
  foreach ($sz in $sizes) {
    $r = New-Object System.Drawing.Bitmap($sz, $sz)
    $rg = [System.Drawing.Graphics]::FromImage($r)
    $rg.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $rg.DrawImage($src, 0, 0, $sz, $sz)
    $rg.Dispose()
    $pm = New-Object System.IO.MemoryStream
    $r.Save($pm, [System.Drawing.Imaging.ImageFormat]::Png)
    $r.Dispose()
    $pngs += , $pm.ToArray()
  }

  $offset = 6 + 16 * $sizes.Count
  for ($i = 0; $i -lt $sizes.Count; $i++) {
    $sz = $sizes[$i]
    $bw.Write([byte]($(if ($sz -ge 256) { 0 } else { $sz })))  # width
    $bw.Write([byte]($(if ($sz -ge 256) { 0 } else { $sz })))  # height
    $bw.Write([byte]0)          # palette
    $bw.Write([byte]0)          # reserved
    $bw.Write([uint16]1)        # planes
    $bw.Write([uint16]32)       # bpp
    $bw.Write([uint32]$pngs[$i].Length)
    $bw.Write([uint32]$offset)
    $offset += $pngs[$i].Length
  }
  foreach ($p in $pngs) { $bw.Write($p) }
  $bw.Flush()
  [System.IO.File]::WriteAllBytes($file, $ms.ToArray())
  $bw.Dispose(); $ms.Dispose()
  Write-Output ("written: " + $file)
}

$sizes = 16, 20, 24, 32, 48, 64, 256

$normal = New-IconBitmap 256 `
  -bg ([System.Drawing.Color]::FromArgb(47, 111, 237)) `
  -body ([System.Drawing.Color]::White) `
  -wheel ([System.Drawing.Color]::FromArgb(154, 166, 178)) `
  -split ([System.Drawing.Color]::FromArgb(201, 210, 219))
Save-Ico $normal (Join-Path $assets 'mouseside.ico') $sizes
$normal.Dispose()

$paused = New-IconBitmap 256 `
  -bg ([System.Drawing.Color]::FromArgb(138, 145, 156)) `
  -body ([System.Drawing.Color]::FromArgb(232, 234, 237)) `
  -wheel ([System.Drawing.Color]::FromArgb(168, 175, 184)) `
  -split ([System.Drawing.Color]::FromArgb(196, 201, 207))
Save-Ico $paused (Join-Path $assets 'mouseside_paused.ico') $sizes
$paused.Dispose()

Write-Output 'ICON_DONE'
