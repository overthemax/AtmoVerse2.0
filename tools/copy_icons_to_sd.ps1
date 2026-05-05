# Script per copiare le icone 1-bit sulla SD card
# Uso: .\copy_icons_to_sd.ps1 F:

param(
    [Parameter(Mandatory=$false)]
    [string]$SDDrive = "F:"
)

# Verifica che il drive SD esista
if (-not (Test-Path $SDDrive)) {
    Write-Host "ERRORE: Drive SD '$SDDrive' non trovato!" -ForegroundColor Red
    Write-Host ""
    Write-Host "Drive disponibili:" -ForegroundColor Yellow
    Get-PSDrive -PSProvider FileSystem | Where-Object { $_.Root -match "^\w:\\" } | ForEach-Object {
        Write-Host "  $($_.Name):" -ForegroundColor Cyan
    }
    Write-Host ""
    Write-Host "Uso: .\copy_icons_to_sd.ps1 <drive>" -ForegroundColor Yellow
    Write-Host "Esempio: .\copy_icons_to_sd.ps1 F:" -ForegroundColor Yellow
    exit 1
}

# Percorsi
$sourceDir = Join-Path $PSScriptRoot "..\icons_bmp_1bit"
$destDir = Join-Path $SDDrive "icons"

# Verifica che la directory sorgente esista
if (-not (Test-Path $sourceDir)) {
    Write-Host "ERRORE: Directory sorgente non trovata: $sourceDir" -ForegroundColor Red
    Write-Host "Esegui prima: python convert_icons_to_1bit.py" -ForegroundColor Yellow
    exit 1
}

Write-Host ""
Write-Host "=== COPIA ICONE SULLA SD CARD ===" -ForegroundColor Cyan
Write-Host ""
Write-Host "Sorgente:     $sourceDir" -ForegroundColor White
Write-Host "Destinazione: $destDir" -ForegroundColor White
Write-Host ""

# Crea la directory icons sulla SD se non esiste
if (-not (Test-Path $destDir)) {
    Write-Host "Creazione directory 'icons' sulla SD..." -ForegroundColor Yellow
    New-Item -ItemType Directory -Path $destDir -Force | Out-Null
}

# Conta i file
$files = Get-ChildItem -Path $sourceDir -Filter "*.bmp"
$totalFiles = $files.Count

Write-Host "File da copiare: $totalFiles" -ForegroundColor Green
Write-Host ""
Write-Host "Copia in corso..." -ForegroundColor Yellow

# Copia i file
$copied = 0
$failed = 0

foreach ($file in $files) {
    try {
        $destPath = Join-Path $destDir $file.Name
        Copy-Item -Path $file.FullName -Destination $destPath -Force
        $copied++
        
        # Mostra progresso ogni 20 file
        if ($copied % 20 -eq 0) {
            $percent = [math]::Round(($copied / $totalFiles) * 100)
            Write-Host "  Progresso: $copied/$totalFiles ($percent%)" -ForegroundColor Gray
        }
    }
    catch {
        Write-Host "  ERRORE copiando $($file.Name): $_" -ForegroundColor Red
        $failed++
    }
}

Write-Host ""
Write-Host "=== COPIA COMPLETATA ===" -ForegroundColor Cyan
Write-Host "  ✓ Copiati: $copied" -ForegroundColor Green
if ($failed -gt 0) {
    Write-Host "  ✗ Falliti: $failed" -ForegroundColor Red
}

# Calcola lo spazio totale
$totalSize = (Get-ChildItem -Path $destDir -Filter "*.bmp" | Measure-Object -Property Length -Sum).Sum
$totalSizeMB = [math]::Round($totalSize / 1MB, 2)

Write-Host ""
Write-Host "Spazio occupato: $totalSizeMB MB" -ForegroundColor White
Write-Host ""
Write-Host "Le icone sono ora disponibili in: $destDir" -ForegroundColor Green
Write-Host ""
