# Script per copiare i file necessari sulla SD card di AtmoVerse 2.0
# Autore: Sistema AtmoVerse
# Data: 2025-01-20

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  AtmoVerse 2.0 - Copia File SD Card" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Richiedi la lettera dell'unità SD
Write-Host "Inserisci la lettera dell'unità SD card (es: E, F, G): " -NoNewline -ForegroundColor Yellow
$sdDrive = Read-Host
$sdDrive = $sdDrive.ToUpper() -replace ':', ''

# Verifica che l'unità esista
if (-not (Test-Path "${sdDrive}:\")) {
    Write-Host "ERRORE: Unità ${sdDrive}:\ non trovata!" -ForegroundColor Red
    Write-Host "Verifica che la SD card sia inserita e premi un tasto per uscire..." -ForegroundColor Red
    $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
    exit 1
}

Write-Host ""
Write-Host "Unità SD trovata: ${sdDrive}:\" -ForegroundColor Green
Write-Host ""

# Percorsi di origine
$projectPath = $PSScriptRoot
$quotesSource = Join-Path $projectPath "sd_files\quotes.json"
$iconsSource = Join-Path $projectPath "sd_files\icons"

# Percorsi di destinazione
$quotesDestination = "${sdDrive}:\quotes.json"
$iconsDestination = "${sdDrive}:\icons"

# Verifica file di origine
if (-not (Test-Path $quotesSource)) {
    Write-Host "ERRORE: File quotes.json non trovato in: $quotesSource" -ForegroundColor Red
    Write-Host "Premi un tasto per uscire..." -ForegroundColor Red
    $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
    exit 1
}

if (-not (Test-Path $iconsSource)) {
    Write-Host "ERRORE: Cartella icons non trovata in: $iconsSource" -ForegroundColor Red
    Write-Host "Premi un tasto per uscire..." -ForegroundColor Red
    $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
    exit 1
}

Write-Host "Operazioni da eseguire:" -ForegroundColor Cyan
Write-Host "  1. Copia quotes.json -> ${sdDrive}:\quotes.json" -ForegroundColor White
Write-Host "  2. Copia cartella icons -> ${sdDrive}:\icons\" -ForegroundColor White
Write-Host ""
Write-Host "Vuoi procedere? (S/N): " -NoNewline -ForegroundColor Yellow
$confirm = Read-Host
if ($confirm -ne 'S' -and $confirm -ne 's') {
    Write-Host "Operazione annullata." -ForegroundColor Yellow
    exit 0
}

Write-Host ""
Write-Host "Avvio copia file..." -ForegroundColor Cyan
Write-Host ""

# Copia quotes.json
try {
    Write-Host "[1/2] Copia quotes.json..." -NoNewline -ForegroundColor White
    Copy-Item -Path $quotesSource -Destination $quotesDestination -Force
    Write-Host " OK" -ForegroundColor Green
    
    # Verifica dimensione file
    $sourceSize = (Get-Item $quotesSource).Length
    $destSize = (Get-Item $quotesDestination).Length
    Write-Host "      Dimensione: $([math]::Round($destSize/1KB, 2)) KB" -ForegroundColor Gray
    
    if ($sourceSize -ne $destSize) {
        Write-Host "      ATTENZIONE: Dimensioni diverse!" -ForegroundColor Yellow
    }
} catch {
    Write-Host " ERRORE" -ForegroundColor Red
    Write-Host "      Dettagli: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

# Crea cartella icons se non esiste
if (-not (Test-Path $iconsDestination)) {
    try {
        Write-Host "[2/2] Creazione cartella icons..." -NoNewline -ForegroundColor White
        New-Item -ItemType Directory -Path $iconsDestination -Force | Out-Null
        Write-Host " OK" -ForegroundColor Green
    } catch {
        Write-Host " ERRORE" -ForegroundColor Red
        Write-Host "      Dettagli: $($_.Exception.Message)" -ForegroundColor Red
        exit 1
    }
}

# Copia file SVG
try {
    Write-Host "      Copia file SVG..." -ForegroundColor White
    $svgFiles = Get-ChildItem -Path $iconsSource -Filter "*.svg"
    $copiedCount = 0
    
    foreach ($file in $svgFiles) {
        $destPath = Join-Path $iconsDestination $file.Name
        Copy-Item -Path $file.FullName -Destination $destPath -Force
        Write-Host "      - $($file.Name)" -ForegroundColor Gray
        $copiedCount++
    }
    
    Write-Host "      Copiati $copiedCount file SVG" -ForegroundColor Green
    
} catch {
    Write-Host "      ERRORE nella copia file SVG" -ForegroundColor Red
    Write-Host "      Dettagli: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "  COPIA COMPLETATA CON SUCCESSO!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""

# Riepilogo file sulla SD
Write-Host "File presenti sulla SD:" -ForegroundColor Cyan
Write-Host "  - quotes.json ($([math]::Round((Get-Item $quotesDestination).Length/1KB, 2)) KB)" -ForegroundColor White
Write-Host "  - icons\ ($copiedCount file SVG)" -ForegroundColor White

# Lista file icons
$iconsList = Get-ChildItem -Path $iconsDestination -Filter "*.svg" | Select-Object -ExpandProperty Name
foreach ($icon in $iconsList) {
    Write-Host "    - $icon" -ForegroundColor Gray
}

Write-Host ""
Write-Host "Prossimi passi:" -ForegroundColor Yellow
Write-Host "  1. Rimuovi la SD card dal PC in sicurezza" -ForegroundColor White
Write-Host "  2. Inserisci la SD nell'ESP32" -ForegroundColor White
Write-Host "  3. Riavvia l'ESP32" -ForegroundColor White
Write-Host "  4. Verifica che le icone appaiano sul display" -ForegroundColor White
Write-Host "  5. Controlla la web GUI per le citazioni" -ForegroundColor White
Write-Host ""
Write-Host "Premi un tasto per uscire..." -ForegroundColor Cyan
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
