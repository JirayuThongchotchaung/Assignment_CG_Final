# Generate the shop sign + Michelin 2040 board textures (Thai text rendered by GDI+).
# Usage:  powershell -ExecutionPolicy Bypass -File tools/make_signs.ps1 -ShopName "บะหมี่ของเรา"
param(
    [string]$ShopName = "บะหมี่ของเรา",
    [string]$SubTitle = "BA-MEE  SINCE 2026"
)

Add-Type -AssemblyName System.Drawing
$out = Join-Path $PSScriptRoot "..\Textures"

function New-Canvas($w, $h) {
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = "AntiAlias"
    $g.TextRenderingHint = "AntiAliasGridFit"
    return @($bmp, $g)
}

function Draw-Centered($g, $text, $font, $brush, $rect) {
    $fmt = New-Object System.Drawing.StringFormat
    $fmt.Alignment = "Center"
    $fmt.LineAlignment = "Center"
    $g.DrawString($text, $font, $brush, $rect, $fmt)
}

# ---- shop name banner (1024 x 256): red board, yellow Thai text ----
$bmp, $g = New-Canvas 1024 256
$g.Clear([System.Drawing.Color]::FromArgb(176, 28, 30))
$g.DrawRectangle((New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 210, 80)), 12), 10, 10, 1004, 236)
$fontBig = New-Object System.Drawing.Font "Leelawadee UI", 92, ([System.Drawing.FontStyle]::Bold), ([System.Drawing.GraphicsUnit]::Pixel)
$fontSmall = New-Object System.Drawing.Font "Segoe UI", 30, ([System.Drawing.FontStyle]::Bold), ([System.Drawing.GraphicsUnit]::Pixel)
$yellow = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 222, 90))
$white = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(250, 245, 230))
Draw-Centered $g $ShopName $fontBig $yellow (New-Object System.Drawing.RectangleF 0, 10, 1024, 180)
Draw-Centered $g $SubTitle $fontSmall $white (New-Object System.Drawing.RectangleF 0, 178, 1024, 60)
$bmp.Save((Join-Path $out "sign_shop.png"), [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()

# ---- Michelin Guide 2040 standing board (512 x 640) ----
$bmp, $g = New-Canvas 512 640
$g.Clear([System.Drawing.Color]::FromArgb(150, 18, 32))
$gold = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 205, 90))
$g.DrawRectangle((New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 205, 90)), 10), 14, 14, 484, 612)
$fStar = New-Object System.Drawing.Font "Segoe UI Symbol", 96, ([System.Drawing.FontStyle]::Regular), ([System.Drawing.GraphicsUnit]::Pixel)
$fTitle = New-Object System.Drawing.Font "Segoe UI", 78, ([System.Drawing.FontStyle]::Bold), ([System.Drawing.GraphicsUnit]::Pixel)
$fSub = New-Object System.Drawing.Font "Segoe UI", 46, ([System.Drawing.FontStyle]::Bold), ([System.Drawing.GraphicsUnit]::Pixel)
$fYear = New-Object System.Drawing.Font "Segoe UI", 120, ([System.Drawing.FontStyle]::Bold), ([System.Drawing.GraphicsUnit]::Pixel)
$fThai = New-Object System.Drawing.Font "Leelawadee UI", 40, ([System.Drawing.FontStyle]::Bold), ([System.Drawing.GraphicsUnit]::Pixel)
Draw-Centered $g ([string][char]0x2605 + " " + [char]0x2605) $fStar $gold (New-Object System.Drawing.RectangleF 0, 30, 512, 120)
Draw-Centered $g "MICHELIN" $fTitle $white (New-Object System.Drawing.RectangleF 0, 150, 512, 100)
Draw-Centered $g "GUIDE" $fSub $white (New-Object System.Drawing.RectangleF 0, 245, 512, 60)
Draw-Centered $g "2040" $fYear $gold (New-Object System.Drawing.RectangleF 0, 305, 512, 150)
Draw-Centered $g "ร้านแนะนำ" $fThai $white (New-Object System.Drawing.RectangleF 0, 470, 512, 70)
Draw-Centered $g "THAILAND" $fSub $white (New-Object System.Drawing.RectangleF 0, 540, 512, 60)
$bmp.Save((Join-Path $out "sign_michelin.png"), [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()

# ---- menu board with 2040 prices (512 x 384): dark board, warm text ----
$bmp, $g = New-Canvas 512 384
$g.Clear([System.Drawing.Color]::FromArgb(34, 30, 36))
$g.DrawRectangle((New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 205, 90)), 8), 8, 8, 496, 368)
$fHead = New-Object System.Drawing.Font "Leelawadee UI", 54, ([System.Drawing.FontStyle]::Bold), ([System.Drawing.GraphicsUnit]::Pixel)
$fItem = New-Object System.Drawing.Font "Leelawadee UI", 36, ([System.Drawing.FontStyle]::Bold), ([System.Drawing.GraphicsUnit]::Pixel)
$fFoot = New-Object System.Drawing.Font "Segoe UI", 24, ([System.Drawing.FontStyle]::Bold), ([System.Drawing.GraphicsUnit]::Pixel)
$cyan = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(120, 230, 255))
Draw-Centered $g "เมนู 2040" $fHead $gold (New-Object System.Drawing.RectangleF 0, 18, 512, 80)
$items = @(@("บะหมี่หมูแดง", "120.-"), @("เกี๊ยวน้ำ", "100.-"), @("บะหมี่ต้มยำ", "140.-"))
$left = New-Object System.Drawing.StringFormat
$right = New-Object System.Drawing.StringFormat
$right.Alignment = "Far"
for ($i = 0; $i -lt $items.Count; $i++) {
    $y = 110 + $i * 66
    $g.DrawString($items[$i][0], $fItem, $white, (New-Object System.Drawing.RectangleF 40, $y, 300, 60), $left)
    $g.DrawString($items[$i][1], $fItem, $gold, (New-Object System.Drawing.RectangleF 300, $y, 172, 60), $right)
}
Draw-Centered $g "SCAN TO PAY  |  DRONE DELIVERY" $fFoot $cyan (New-Object System.Drawing.RectangleF 0, 316, 512, 50)
$bmp.Save((Join-Path $out "sign_menu.png"), [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()

Write-Output "wrote sign_shop.png, sign_michelin.png, sign_menu.png"
