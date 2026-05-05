<#
.SYNOPSIS
    Converte icone SVG in BMP monocromatici/grayscale usando Inkscape e .NET.
    Versione corretta V2.
#>

param(
    [string]$SourceDir = ".\sd_files\icons",
    [string]$DestDir = ".\icons_bmp",
    [int]$Size = 100
)

Write-Host "=== AtmoVerse Icon Converter V2 (PowerShell + Inkscape) ===" -ForegroundColor Cyan

# Verifica cartella sorgente
if (-not (Test-Path $SourceDir)) {
    Write-Error "Cartella sorgente non trovata: $SourceDir"
    exit
}

# Crea destinazione
if (-not (Test-Path $DestDir)) {
    New-Item -ItemType Directory -Path $DestDir | Out-Null
    Write-Host "Creata cartella destinazione: $DestDir" -ForegroundColor Green
}

# Cerca Inkscape
$InkscapePath = "C:\Program Files\Inkscape\bin\inkscape.exe"
if (-not (Test-Path $InkscapePath)) {
    $InkscapePath = Get-Command inkscape -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source
}

if (-not $InkscapePath) {
    Write-Error "Inkscape non trovato! Installa Inkscape per convertire SVG."
    exit
}

# Carica assembly .NET
Add-Type -AssemblyName System.Drawing

# Funzione per convertire
function Convert-SvgToBmp {
    param($svgFile, $destBmp, $size)

    $tempPng = "$destBmp.png"
    
    # 1. SVG -> PNG con Inkscape
    # Nota: Inkscape restituisce il controllo subito, ma a volte serve aspettare. 
    # Start-Process -Wait è più sicuro.
    $proc = Start-Process -FilePath $InkscapePath -ArgumentList "--export-type=png", "--export-filename=`"$tempPng`"", "--export-width=$size", "--export-height=$size", "`"$svgFile`"" -Wait -PassThru -NoNewWindow
    
    if (-not (Test-Path $tempPng)) {
        Write-Error "Errore creazione PNG da $svgFile"
        return
    }

    # 2. PNG -> BMP Grayscale con .NET
    try {
        # Carica PNG (blocca il file)
        $img = [System.Drawing.Bitmap]::FromFile($tempPng)
        
        # Crea nuova bitmap BMP (usiamo 24bpp RGB standard)
        # Fix per errore enum PowerShell: assegniamo prima il valore a una variabile tipizzata o usiamo il cast esplicito
        $pixelFormat = [System.Drawing.Imaging.PixelFormat]::Format24bppRgb
        $newBmp = New-Object System.Drawing.Bitmap $img.Width, $img.Height, $pixelFormat
        
        # Disegna in scala di grigi
        $g = [System.Drawing.Graphics]::FromImage($newBmp)
        
        # Matrice per scala di grigi
        $matrix = New-Object System.Drawing.Imaging.ColorMatrix
        $matrix.Matrix00 = 0.299; $matrix.Matrix01 = 0.299; $matrix.Matrix02 = 0.299
        $matrix.Matrix10 = 0.587; $matrix.Matrix11 = 0.587; $matrix.Matrix12 = 0.587
        $matrix.Matrix20 = 0.114; $matrix.Matrix21 = 0.114; $matrix.Matrix22 = 0.114
        
        $attrs = New-Object System.Drawing.Imaging.ImageAttributes
        $attrs.SetColorMatrix($matrix)
        
        # Sfondo bianco (per icone trasparenti)
        $g.Clear([System.Drawing.Color]::White)
        
        $g.DrawImage($img, [System.Drawing.Rectangle]::new(0, 0, $img.Width, $img.Height), 0, 0, $img.Width, $img.Height, [System.Drawing.GraphicsUnit]::Pixel, $attrs)
        
        $g.Dispose()
        $img.Dispose() # Rilascia il PNG originale
        
        # Salva BMP
        $newBmp.Save($destBmp, [System.Drawing.Imaging.ImageFormat]::Bmp)
        $newBmp.Dispose()
        
        Write-Host "OK: $(Split-Path $destBmp -Leaf)"
        
        # Ora possiamo cancellare il PNG perché $img è disposto
        Remove-Item $tempPng -Force
    }
    catch {
        Write-Error "Errore .NET su $tempPng : $_"
        if ($img) { $img.Dispose() }
        if ($newBmp) { $newBmp.Dispose() }
    }
}

# Loop principale
$files = Get-ChildItem "$SourceDir\*.svg"
$count = 0
foreach ($file in $files) {
    $destFile = Join-Path $DestDir ($file.BaseName + ".bmp")
    Convert-SvgToBmp $file.FullName $destFile $Size
    $count++
}

Write-Host "Finito! Convertiti $count file in $DestDir" -ForegroundColor Green

