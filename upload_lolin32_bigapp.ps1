# Compila e carica AtmoVerse sulla LOLIN32 via USB.
# Uso: .\upload_lolin32_bigapp.ps1            (prima porta COM trovata)
#      .\upload_lolin32_bigapp.ps1 -Port COM4
#
# La tabella partizioni viene da partitions.csv nella cartella dello sketch:
# due aree firmware da 1,9 MB per l'aggiornamento automatico da GitHub.
# upload.maximum_size deve corrispondere a quella dimensione (0x1E0000).
param([string]$Port = "")

$ArduinoCli = "C:\Program Files\Arduino CLI\arduino-cli.exe"
$Fqbn = "esp32:esp32:lolin32"
$Sketch = $PSScriptRoot

if ($Port -eq "") {
  $ports = [System.IO.Ports.SerialPort]::GetPortNames()
  if ($ports.Count -eq 0) {
    Write-Error "Nessuna porta COM rilevata. Collega la LOLIN32 via USB e riprova."
    exit 1
  }
  $Port = $ports[0]
}

Write-Host "Uso porta $Port con partizioni OTA (2 x 1,9 MB)"
& $ArduinoCli compile -j 0 --fqbn $Fqbn --build-property upload.maximum_size=1966080 $Sketch
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $ArduinoCli upload -p $Port --fqbn $Fqbn $Sketch
exit $LASTEXITCODE
