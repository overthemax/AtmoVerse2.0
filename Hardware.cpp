#include "Hardware.h"
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <GxEPD2_BW.h>
#include "Debug.h"
#include <esp_system.h>

// GxEPD2 black and white display, 5.83" GDEW0583T8
GxEPD2_BW<GxEPD2_583_T8, GxEPD2_583_T8::HEIGHT> display(GxEPD2_583_T8(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

static bool _sdInitialized = false;

// Mounts the SD card once. Slower SPI clocks are tried for old cards or
// unstable wiring, then the bus is reset for a last attempt at 4 MHz.
bool initSD() {
  if (_sdInitialized) return true;

  DEBUG_TRACE("initSD");

  // CS high before the first transaction
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  delay(10);

  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  delay(500); // Let the bus settle

  static const uint32_t CLOCKS[] = {4000000, 2000000, 1000000, 400000};
  for (size_t i = 0; i < sizeof(CLOCKS) / sizeof(CLOCKS[0]); i++) {
    if (i > 0) delay(200);
    if (SD.begin(SD_CS, sdSPI, CLOCKS[i])) {
      _sdInitialized = true;
      if (i > 0) Serial.printf("[SD] Card mounted at %lu kHz\n", (unsigned long)(CLOCKS[i] / 1000));
      return true;
    }
  }

  // Last attempt after a bus reset
  sdSPI.end();
  delay(500);
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  delay(100);
  if (SD.begin(SD_CS, sdSPI, 4000000)) {
    _sdInitialized = true;
    return true;
  }

  static bool reported = false;  // initSD() is called often: report once
  if (!reported) Serial.println("[SD] Cannot mount the SD card: check that it is inserted and FAT32");
  reported = true;
  return false;
}

void initHardware() {
  DEBUG_TRACE();
  // Serial is normally started in setup(): starting it again would reset the USB link
  if (!Serial) {
    Serial.begin(SERIAL_BAUD_RATE);
    while (!Serial && millis() < 3000);
  }
  // SPI for the display is started in setup(), before initDisplay()
}

// Why the board restarted last time, for the log and the diagnostics page
const char* resetReasonText() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:  return "power on";
    case ESP_RST_EXT:      return "reset pin";
    case ESP_RST_SW:       return "restart by the firmware";
    case ESP_RST_PANIC:    return "crash (panic)";
    case ESP_RST_INT_WDT:  return "interrupt watchdog";
    case ESP_RST_TASK_WDT: return "task watchdog";
    case ESP_RST_WDT:      return "watchdog";
    case ESP_RST_DEEPSLEEP: return "wake from deep sleep";
    case ESP_RST_BROWNOUT: return "brownout (supply voltage too low)";
    case ESP_RST_SDIO:     return "SDIO";
    default:               return "unknown";
  }
}
