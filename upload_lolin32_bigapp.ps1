$ArduinoCli = "C:\Program Files\Arduino CLI\arduino-cli.exe"
$Fqbn = "esp32:esp32:lolin32:PartitionScheme=no_ota"
$Sketch = $PSScriptRoot

$ports = [System.IO.Ports.SerialPort]::GetPortNames()
if ($ports.Count -eq 0) {
  Write-Error "Nessuna porta COM rilevata. Collega la LOLIN32 via USB e riprova."
  exit 1
}

$Port = $ports[0]
Write-Host "Uso porta $Port con partizione No OTA (Large APP)"
& $ArduinoCli compile --fqbn $Fqbn $Sketch
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $ArduinoCli upload -p $Port --fqbn $Fqbn $Sketch
exit $LASTEXITCODE
