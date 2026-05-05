# Script PowerShell per preparare icone BMP (400x400) per AtmoVerse
# 1) Opzionale: converte gli SVG in PNG con Inkscape
# 2) Converte tutti i PNG/JPG in BMP 8-bit scala di grigi 400x400

param(
    [switch]$Help
)

if ($Help) {
    Write-Host "Uso: .\Prepara_Icone_BMP_fixed.ps1" -ForegroundColor Cyan
    Write-Host "" 
    Write-Host "Questo script:" -ForegroundColor White
    Write-Host "  1. Legge SVG/PNG/JPG dalla cartella 'svg'" -ForegroundColor White
    Write-Host "  2. (Se Inkscape è presente) converte gli SVG in PNG" -ForegroundColor White
    Write-Host "  3. Converte PNG/JPG in BMP 400x400 in 'sd_files\\icons_bmp'" -ForegroundColor White
    exit 0
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Preparazione Icone BMP per AtmoVerse" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Parametri
$inputDir  = "svg"                    # Sorgenti SVG/PNG/JPG
$outputDir = "sd_files\icons_bmp"    # BMP di output
$iconSize  = 400                       # 400x400 px

# Carica System.Drawing
Add-Type -AssemblyName System.Drawing

# Verifica cartella input
if (-not (Test-Path $inputDir)) {
    Write-Host "❌ Cartella non trovata: $inputDir" -ForegroundColor Red
    exit 1
}

# Crea cartella output se manca
if (-not (Test-Path $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir | Out-Null
}

Write-Host "📁 Input:  $inputDir" -ForegroundColor White
Write-Host "📁 Output: $outputDir" -ForegroundColor White
Write-Host "📐 Dimensione: ${iconSize}x${iconSize} px" -ForegroundColor White
Write-Host ""

# Percorso opzionale di Inkscape
$inkscapePath = "C:\\Program Files\\Inkscape\\bin\\inkscape.exe"

# 1) SVG -> PNG (se Inkscape è disponibile)
$svgFiles = Get-ChildItem -Path $inputDir -Filter *.svg -File -ErrorAction SilentlyContinue
if ($svgFiles.Count -gt 0) {
    if (Test-Path $inkscapePath) {
        Write-Host "🖼  Trovati $($svgFiles.Count) file SVG, li converto in PNG con Inkscape..." -ForegroundColor Yellow
        foreach ($svg in $svgFiles) {
            $pngPath = [System.IO.Path]::ChangeExtension($svg.FullName, ".png")
            Write-Host "  🔄 $($svg.Name) -> $(Split-Path $pngPath -Leaf)" -ForegroundColor Cyan
            & $inkscapePath `
                "$($svg.FullName)" `
                --export-type=png `
                --export-width=$iconSize `
                --export-height=$iconSize `
                --export-filename="$pngPath" `
                2>$null
        }
        Write-Host "" 
    } else {
        Write-Host "⚠️  SVG trovati ma Inkscape non è installato o il percorso non è corretto:" -ForegroundColor Yellow
        Write-Host "    $inkscapePath" -ForegroundColor Yellow
        Write-Host "    Gli SVG verranno ignorati; verranno convertiti solo PNG/JPG già presenti." -ForegroundColor Yellow
        Write-Host "" 
    }
}

# 2) PNG/JPG -> BMP
$imageFiles = Get-ChildItem -Path $inputDir -Include *.png,*.jpg,*.jpeg -File

if ($imageFiles.Count -eq 0) {
    Write-Host "❌ Nessun file PNG/JPG trovato in $inputDir" -ForegroundColor Red
    exit 1
}

Write-Host "Trovati $($imageFiles.Count) file immagine" -ForegroundColor Green
Write-Host ""

$converted = 0

foreach ($file in $imageFiles) {
    $bmpName   = [System.IO.Path]::GetFileNameWithoutExtension($file.Name) + ".bmp"
    $outputPath = Join-Path $outputDir $bmpName

    Write-Host "🔄 $($file.Name)" -ForegroundColor Cyan

    # Carica immagine sorgente
    $img = [System.Drawing.Image]::FromFile($file.FullName)

    # Crea bitmap ridimensionata
    $resized  = New-Object System.Drawing.Bitmap($iconSize, $iconSize)
    $graphics = [System.Drawing.Graphics]::FromImage($resized)

    # Qualità alta
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.SmoothingMode     = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $graphics.PixelOffsetMode   = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

    # Disegna immagine ridimensionata
    $graphics.DrawImage($img, 0, 0, $iconSize, $iconSize)

    # Bitmap in scala di grigi 8-bit
    $grayBitmap = New-Object System.Drawing.Bitmap($iconSize, $iconSize, [System.Drawing.Imaging.PixelFormat]::Format8bppIndexed)

    # Palette 0-255
    $palette = $grayBitmap.Palette
    for ($i = 0; $i -lt 256; $i++) {
        $palette.Entries[$i] = [System.Drawing.Color]::FromArgb($i, $i, $i)
    }
    $grayBitmap.Palette = $palette

    # Copia pixel in scala di grigi
    for ($y = 0; $y -lt $iconSize; $y++) {
        for ($x = 0; $x -lt $iconSize; $x++) {
            $pixel = $resized.GetPixel($x, $y)
            $gray  = [int]($pixel.R * 0.299 + $pixel.G * 0.587 + $pixel.B * 0.114)
            $grayBitmap.SetPixel($x, $y, [System.Drawing.Color]::FromArgb($gray, $gray, $gray))
        }
    }

    # Salva come BMP
    $grayBitmap.Save($outputPath, [System.Drawing.Imaging.ImageFormat]::Bmp)

    # Cleanup
    $graphics.Dispose()
    $resized.Dispose()
    $grayBitmap.Dispose()
    $img.Dispose()

    $fileSize = (Get-Item $outputPath).Length / 1KB
    Write-Host "  ✓ Salvato: $outputPath ($([math]::Round($fileSize, 1)) KB)" -ForegroundColor Green
    $converted++

    Write-Host ""
}

Write-Host "========================================" -ForegroundColor Green
Write-Host "✓ Convertiti: $converted/$($imageFiles.Count)" -ForegroundColor Green
Write-Host "📋 Prossimi passi:" -ForegroundColor Yellow
Write-Host ("  1. Copia {0} sulla SD card" -f $outputDir) -ForegroundColor White
Write-Host "  2. Ricompila il firmware" -ForegroundColor White
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "Premi un tasto per uscire..." -ForegroundColor Cyan
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
