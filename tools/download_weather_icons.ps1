# Downloads Weather Icons PNGs (Erik Flowers) from GitHub.
# Needs PowerShell only. Convert them afterwards with tools/convert_icons_to_1bit.py.

$baseUrl = "https://raw.githubusercontent.com/erikflowers/weather-icons/master/png/256"
$outputDir = "sd_files\icons_png"

# Icons to download (local name -> GitHub name)
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

Write-Host "Downloading Weather Icons from GitHub to $outputDir" -ForegroundColor Cyan
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null

$downloaded = 0
$failed = 0

foreach ($icon in $icons.GetEnumerator()) {
    $fileName = "$($icon.Value).png"
    $url = "$baseUrl/$fileName"
    $outputPath = Join-Path $outputDir "$($icon.Key).png"

    try {
        Invoke-WebRequest -Uri $url -OutFile $outputPath -ErrorAction Stop
        Write-Host "  ok     $fileName" -ForegroundColor Green
        $downloaded++
    } catch {
        Write-Host "  failed $fileName ($_)" -ForegroundColor Red
        $failed++
    }
}

Write-Host ""
Write-Host "Downloaded: $downloaded/$($icons.Count)" -ForegroundColor Green
if ($failed -gt 0) {
    Write-Host "Failed: $failed" -ForegroundColor Red
}
Write-Host "Next step: python tools/convert_icons_to_1bit.py" -ForegroundColor Yellow
