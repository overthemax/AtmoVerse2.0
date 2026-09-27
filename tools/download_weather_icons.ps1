# Scarica icone Weather Icons da GitHub e converte in BMP
# Richiede: Solo PowerShell (nessuna dipendenza)

$baseUrl = "https://raw.githubusercontent.com/erikflowers/weather-icons/master/png/256"
$outputDir = "sd_files\icons_png"
$bmpDir = "sd_files\icons_bmp"

# Mappa icone (nome file → nome GitHub)
$icons = @{
    "wi-fog" = "wi-fog"
    "wi-day-sunny" = "wi-day-sunny"
    "wi-day-cloudy" = "wi-day-cloudy"
    "wi-day-thunderstorm" = "wi-day-thunderstorm"
    "wi-rain" = "wi-rain"
    "wi-sprinkle" = "wi-sprinkle"
    "wi-thunderstorm" = "wi-thunderstorm"
    "wi-storm-showers" = "wi-storm-showers"
    "wi-rain-mix" = "wi-rain-mix"
    "wi-sleet" = "wi-sleet"
    "wi-snow" = "wi-snow"
    "wi-snowflake-cold" = "wi-snowflake-cold"
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Download Weather Icons da GitHub" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Crea directory
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
New-Item -ItemType Directory -Path $bmpDir -Force | Out-Null

Write-Host "📁 Download in: $outputDir" -ForegroundColor White
Write-Host ""

$downloaded = 0
$failed = 0

foreach ($icon in $icons.GetEnumerator()) {
    $fileName = "$($icon.Value).png"
    $url = "$baseUrl/$fileName"
    $outputPath = Join-Path $outputDir "$($icon.Key).png"
    
    Write-Host "⬇️  $fileName" -ForegroundColor Cyan
    
    try {
        Invoke-WebRequest -Uri $url -OutFile $outputPath -ErrorAction Stop
        Write-Host "   ✓ Salvato" -ForegroundColor Green
        $downloaded++
    } catch {
        Write-Host "   ❌ Errore: $_" -ForegroundColor Red
        $failed++
    }
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "✓ Scaricati: $downloaded/$($icons.Count)" -ForegroundColor Green

if ($failed -gt 0) {
    Write-Host "❌ Falliti: $failed" -ForegroundColor Red
}

Write-Host ""
Write-Host "📋 Prossimo passo:" -ForegroundColor Yellow
Write-Host "   Esegui: python convert_icons_simple.py" -ForegroundColor White
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "Premi un tasto per continuare..." -ForegroundColor Cyan
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
