#include "SystemMonitor.h"
#include "Hardware.h"

#include <driver/adc.h>

// Definizione delle variabili statiche dichiarate in SystemMonitor.h
StabilityMonitor* SystemMonitor::_componentMonitors[COMPONENT_COUNT];
bool SystemMonitor::_componentStatus[COMPONENT_COUNT];
unsigned long SystemMonitor::_lastReportTime = 0;

// Lettura tensione batteria tramite ADC sul pin definito in Hardware.h
float SystemMonitor::getBatteryVoltage() {
  // Se il pin non è valido, ritorna errore
  if (VBAT_PIN < 0) return -1.0f;

  // Configurazione ADC per ESP32
  analogReadResolution(12); // 0..4095
  analogSetPinAttenuation(VBAT_PIN, ADC_11db); // ~3.3V full-scale lato ADC

  // Media su più campioni per stabilità
  const int samples = 16;
  uint32_t acc = 0;
  for (int i = 0; i < samples; ++i) {
    acc += analogRead(VBAT_PIN);
    delay(2);
  }
  float raw = acc / float(samples);

  // Calcolo tensione: raw/4095 * 3.3V (FS con 11dB), poi moltiplica per partitore
  // Nota: 3.3V è una stima; per precisione usare calibrazione o Vref misurato
  const float adc_fs = 3.3f;
  float v_adc = (raw / 4095.0f) * adc_fs;
  float vbat = v_adc * VBAT_DIVIDER;
  return vbat;
}

int SystemMonitor::getBatteryPercent(float v) {
  // Mappatura lineare grezza 3.0V -> 0%, 4.2V -> 100%
  if (v <= 3.0f) return 0;
  if (v >= 4.2f) return 100;
  int pct = int((v - 3.0f) * (100.0f / 1.2f));
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  return pct;
}
