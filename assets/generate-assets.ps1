# Generates the project images in assets/: logo.png, background.png, icon.png.
# Windows only (GDI+). Run from the repository root:
#   powershell -ExecutionPolicy Bypass -File assets/generate-assets.ps1
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$root = Split-Path -Parent $PSScriptRoot
$outDir = Join-Path $root 'assets'
New-Item -ItemType Directory -Path $outDir -Force | Out-Null

$accent = [System.Drawing.Color]::FromArgb(255, 76, 141, 255)
$accentLight = [System.Drawing.Color]::FromArgb(255, 140, 180, 255)
$text = [System.Drawing.Color]::FromArgb(255, 230, 234, 242)
$muted = [System.Drawing.Color]::FromArgb(255, 138, 147, 165)
$border = [System.Drawing.Color]::FromArgb(255, 42, 49, 64)

function New-RoundedPath([float]$x, [float]$y, [float]$w, [float]$h, [float]$radius) {
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $diameter = $radius * 2
    $path.AddArc($x, $y, $diameter, $diameter, 180, 90)
    $path.AddArc($x + $w - $diameter, $y, $diameter, $diameter, 270, 90)
    $path.AddArc($x + $w - $diameter, $y + $h - $diameter, $diameter, $diameter, 0, 90)
    $path.AddArc($x, $y + $h - $diameter, $diameter, $diameter, 90, 90)
    $path.CloseFigure()
    return $path
}

function Get-BoltPoints([float]$cx, [float]$cy, [float]$height) {
    $scale = $height / 100.0
    $shape = @(
        @(58.0, 2.0), @(24.0, 55.0), @(45.0, 55.0),
        @(38.0, 98.0), @(78.0, 42.0), @(54.0, 42.0)
    )
    $points = New-Object 'System.Drawing.PointF[]' $shape.Count
    for ($i = 0; $i -lt $shape.Count; $i++) {
        $points[$i] = New-Object System.Drawing.PointF(
            [float]($cx + ($shape[$i][0] - 50.0) * $scale),
            [float]($cy + ($shape[$i][1] - 50.0) * $scale))
    }
    return $points
}

function Draw-Bolt($graphics, [float]$cx, [float]$cy, [float]$height, $topColor, $bottomColor, [int]$glowAlpha) {
    if ($glowAlpha -gt 0) {
        $glowPath = New-Object System.Drawing.Drawing2D.GraphicsPath
        $glowPoints = Get-BoltPoints $cx $cy ($height * 1.6)
        $glowPath.AddPolygon($glowPoints)
        $glowBrush = New-Object System.Drawing.Drawing2D.PathGradientBrush($glowPath)
        $glowBrush.CenterColor = [System.Drawing.Color]::FromArgb($glowAlpha, $accent.R, $accent.G, $accent.B)
        $glowBrush.SurroundColors = @([System.Drawing.Color]::FromArgb(0, $accent.R, $accent.G, $accent.B))
        $glowBrush.SetSigmaBellShape([float]0.55)
        $graphics.FillPath($glowBrush, $glowPath)
        $glowBrush.Dispose()
        $glowPath.Dispose()
    }

    $points = Get-BoltPoints $cx $cy $height
    $rect = New-Object System.Drawing.RectangleF(
        [float]($cx - $height), [float]($cy - $height), [float]($height * 2), [float]($height * 2))
    $brush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
        $rect, $topColor, $bottomColor, [System.Drawing.Drawing2D.LinearGradientMode]::Vertical)
    $graphics.FillPolygon($brush, $points)
    $brush.Dispose()
}

function Draw-Spaced($graphics, [string]$value, $font, $brush, [float]$centerX, [float]$top, [float]$spacing) {
    $format = [System.Drawing.StringFormat]::GenericTypographic
    $width = 0.0
    foreach ($ch in $value.ToCharArray()) {
        $width += $graphics.MeasureString([string]$ch, $font, 1000, $format).Width + $spacing
    }
    $width -= $spacing

    $x = $centerX - $width / 2.0
    foreach ($ch in $value.ToCharArray()) {
        $size = $graphics.MeasureString([string]$ch, $font, 1000, $format)
        $graphics.DrawString([string]$ch, $font, $brush, [float]$x, [float]$top, $format)
        $x += $size.Width + $spacing
    }
}

function New-UiFont([string]$family, [float]$size) {
    return New-Object System.Drawing.Font(
        $family, $size, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
}

# ------------------------------------------------------------------ icon 283x300
$iconWidth = 283
$iconHeight = 300
$icon = New-Object System.Drawing.Bitmap($iconWidth, $iconHeight, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$graphics = [System.Drawing.Graphics]::FromImage($icon)
$graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit

$tileRect = New-Object System.Drawing.RectangleF(0, 0, $iconWidth, $iconHeight)
$tileBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
    $tileRect,
    [System.Drawing.Color]::FromArgb(255, 30, 37, 50),
    [System.Drawing.Color]::FromArgb(255, 13, 15, 20),
    [System.Drawing.Drawing2D.LinearGradientMode]::Vertical)
$tilePath = New-RoundedPath 1 1 ($iconWidth - 2) ($iconHeight - 2) 58
$graphics.FillPath($tileBrush, $tilePath)
$tileBrush.Dispose()

$edgePen = New-Object System.Drawing.Pen($border, 1.6)
$graphics.DrawPath($edgePen, $tilePath)
$edgePen.Dispose()
$tilePath.Dispose()

$glowRect = New-Object System.Drawing.RectangleF(40, 60, 203, 180)
$glow = New-Object System.Drawing.Drawing2D.GraphicsPath
$glow.AddEllipse($glowRect)
$glowBrush = New-Object System.Drawing.Drawing2D.PathGradientBrush($glow)
$glowBrush.CenterColor = [System.Drawing.Color]::FromArgb(70, $accent.R, $accent.G, $accent.B)
$glowBrush.SurroundColors = @([System.Drawing.Color]::FromArgb(0, $accent.R, $accent.G, $accent.B))
$graphics.FillPath($glowBrush, $glow)
$glowBrush.Dispose()
$glow.Dispose()

Draw-Bolt $graphics 141.5 150 148 $accentLight $accent 95

$sheen = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(24, 255, 255, 255))
$sheenPath = New-RoundedPath 1 1 ($iconWidth - 2) 126 58
$graphics.FillPath($sheen, $sheenPath)
$sheen.Dispose()
$sheenPath.Dispose()

$icon.Save((Join-Path $outDir 'icon.png'), [System.Drawing.Imaging.ImageFormat]::Png)
$graphics.Dispose()
$icon.Dispose()

# ------------------------------------------------------------------ logo 283x300
$logo = New-Object System.Drawing.Bitmap(283, 300, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$graphics = [System.Drawing.Graphics]::FromImage($logo)
$graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
$graphics.Clear([System.Drawing.Color]::Transparent)

Draw-Bolt $graphics 141.5 104 124 $accentLight $accent 72

$wordFont = New-UiFont 'Segoe UI Semibold' 44
# The logo sits on both light and dark surfaces, so the wordmark uses the accent
# colour and the tagline a mid grey instead of the UI's light text colour.
$wordBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 59, 123, 240))
Draw-Spaced $graphics 'DEIMOS' $wordFont $wordBrush 141.5 166 7.0
$wordBrush.Dispose()
$wordFont.Dispose()

$lineBrush = New-Object System.Drawing.SolidBrush($accent)
$graphics.FillRectangle($lineBrush, [float]91.5, [float]228, [float]100, [float]2.4)
$lineBrush.Dispose()

$tagFont = New-UiFont 'Segoe UI' 11.5
$tagBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 104, 114, 133))
Draw-Spaced $graphics 'DPI BYPASS CONTROL PLANE' $tagFont $tagBrush 141.5 245 2.6
$tagBrush.Dispose()
$tagFont.Dispose()

$logo.Save((Join-Path $outDir 'logo.png'), [System.Drawing.Imaging.ImageFormat]::Png)
$graphics.Dispose()
$logo.Dispose()

# ------------------------------------------------------------------ background 1200x300
$bgWidth = 1200
$bgHeight = 300
$background = New-Object System.Drawing.Bitmap($bgWidth, $bgHeight, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$graphics = [System.Drawing.Graphics]::FromImage($background)
$graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit

$bgRect = New-Object System.Drawing.RectangleF(0, 0, $bgWidth, $bgHeight)
$bgBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
    $bgRect,
    [System.Drawing.Color]::FromArgb(255, 20, 24, 32),
    [System.Drawing.Color]::FromArgb(255, 11, 13, 17),
    [System.Drawing.Drawing2D.LinearGradientMode]::Horizontal)
$graphics.FillRectangle($bgBrush, $bgRect)
$bgBrush.Dispose()

$haloRect = New-Object System.Drawing.RectangleF(620, -180, 720, 660)
$haloPath = New-Object System.Drawing.Drawing2D.GraphicsPath
$haloPath.AddEllipse($haloRect)
$haloBrush = New-Object System.Drawing.Drawing2D.PathGradientBrush($haloPath)
$haloBrush.CenterColor = [System.Drawing.Color]::FromArgb(64, $accent.R, $accent.G, $accent.B)
$haloBrush.SurroundColors = @([System.Drawing.Color]::FromArgb(0, $accent.R, $accent.G, $accent.B))
$graphics.FillPath($haloBrush, $haloPath)
$haloBrush.Dispose()
$haloPath.Dispose()

$dotBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(14, 255, 255, 255))
for ($x = 24; $x -lt $bgWidth; $x += 26) {
    for ($y = 20; $y -lt $bgHeight; $y += 26) {
        $graphics.FillEllipse($dotBrush, [float]$x, [float]$y, [float]1.6, [float]1.6)
    }
}
$dotBrush.Dispose()

$tileSize = 132
$tileTop = ($bgHeight - $tileSize) / 2.0
$tilePath = New-RoundedPath 76 $tileTop $tileSize $tileSize 30
$tileBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
    (New-Object System.Drawing.RectangleF(76, $tileTop, $tileSize, $tileSize)),
    [System.Drawing.Color]::FromArgb(255, 32, 40, 55),
    [System.Drawing.Color]::FromArgb(255, 13, 15, 20),
    [System.Drawing.Drawing2D.LinearGradientMode]::Vertical)
$graphics.FillPath($tileBrush, $tilePath)
$tileBrush.Dispose()
$edgePen = New-Object System.Drawing.Pen($border, 1.6)
$graphics.DrawPath($edgePen, $tilePath)
$edgePen.Dispose()
$tilePath.Dispose()
Draw-Bolt $graphics 142 150 74 $accentLight $accent 60

$titleFont = New-UiFont 'Segoe UI Semibold' 72
$titleBrush = New-Object System.Drawing.SolidBrush($text)
$graphics.DrawString('Deimos', $titleFont, $titleBrush, [float]252, [float]80, [System.Drawing.StringFormat]::GenericTypographic)
$titleBrush.Dispose()
$titleFont.Dispose()

$subtitleFont = New-UiFont 'Segoe UI' 23
$subtitleBrush = New-Object System.Drawing.SolidBrush($muted)
$graphics.DrawString('DPI bypass control plane for zapret', $subtitleFont, $subtitleBrush, [float]256, [float]166, [System.Drawing.StringFormat]::GenericTypographic)
$subtitleBrush.Dispose()
$subtitleFont.Dispose()

$chips = @('daemon', 'CLI', 'desktop')
$chipFont = New-UiFont 'Segoe UI' 15
$chipTextBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 170, 190, 220))
$chipBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(28, $accent.R, $accent.G, $accent.B))
$chipPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(90, $accent.R, $accent.G, $accent.B), 1.0)
$chipX = 256.0
foreach ($chip in $chips) {
    $size = $graphics.MeasureString($chip, $chipFont, 1000, [System.Drawing.StringFormat]::GenericTypographic)
    $chipWidth = $size.Width + 26.0
    $chipPath = New-RoundedPath $chipX 210 $chipWidth 30 9
    $graphics.FillPath($chipBrush, $chipPath)
    $graphics.DrawPath($chipPen, $chipPath)
    $graphics.DrawString($chip, $chipFont, $chipTextBrush, [float]($chipX + 13.0), [float]216, [System.Drawing.StringFormat]::GenericTypographic)
    $chipPath.Dispose()
    $chipX += $chipWidth + 12.0
}
$chipPen.Dispose()
$chipBrush.Dispose()
$chipTextBrush.Dispose()
$chipFont.Dispose()

$watermark = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(30, $accent.R, $accent.G, $accent.B), 2.0)
$watermarkPoints = Get-BoltPoints 1062 148 190
$graphics.DrawPolygon($watermark, $watermarkPoints)
$watermark.Dispose()

$background.Save((Join-Path $outDir 'background.png'), [System.Drawing.Imaging.ImageFormat]::Png)
$graphics.Dispose()
$background.Dispose()

Write-Host 'assets written:'
Get-ChildItem $outDir -Filter *.png | ForEach-Object {
    $image = [System.Drawing.Image]::FromFile($_.FullName)
    Write-Host ("  {0,-16} {1}x{2}  {3:N0} bytes" -f $_.Name, $image.Width, $image.Height, $_.Length)
    $image.Dispose()
}
