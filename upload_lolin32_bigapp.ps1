# Builds AtmoVerse and uploads it to the LOLIN32 over USB.
# Usage: .\upload_lolin32_bigapp.ps1            (first COM port found)
#        .\upload_lolin32_bigapp.ps1 -Port COM4
#
# The partition table comes from partitions.csv in the sketch folder:
# two 1.9 MB firmware slots for the automatic update from GitHub.
# upload.maximum_size must match that size (0x1E0000).
param([string]$Port = "")

$ArduinoCli = "C:\Program Files\Arduino CLI\arduino-cli.exe"
$Fqbn = "esp32:esp32:lolin32"
$Sketch = $PSScriptRoot

if ($Port -eq "") {
  $ports = [System.IO.Ports.SerialPort]::GetPortNames()
  if ($ports.Count -eq 0) {
    Write-Error "No COM port found. Connect the LOLIN32 over USB and try again."
    exit 1
  }
  $Port = $ports[0]
}

Write-Host "Using port $Port with OTA partitions (2 x 1.9 MB)"
& $ArduinoCli compile -j 0 --fqbn $Fqbn --build-property upload.maximum_size=1966080 $Sketch
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $ArduinoCli upload -p $Port --fqbn $Fqbn $Sketch
exit $LASTEXITCODE
