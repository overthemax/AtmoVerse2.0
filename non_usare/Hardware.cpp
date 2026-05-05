#include "Hardware.h"
#include <Arduino.h>
#include <SPI.h>
#include "Debug.h"

// Instanziazione del display GxEPD2 B/N, 5.83" GDEW0583T8
// Riduzione dell'altezza della pagina da 120px a 32px per risparmiare memoria DRAM
GxEPD2_BW<PanelType, 32> display(PanelType(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)); // Using 32px page height to save DRAM

// Inizializzazione SPI per la SD card
SPIClass sdSPI(HSPI);

// Inizializza l'hardware con controlli di errore
bool initHardware() {
  DEBUG_TRACE();
  bool success = true;
  
  // Inizializzazione seriale
  Serial.begin(SERIAL_BAUD_RATE);
  static unsigned long startTime = millis();
  while (!Serial && (millis() - startTime) < 3000); // Attende fino a 3 secondi per la connessione seriale

  Serial.println(F("\nInizializzazione hardware AtmoVerse 2.0..."));

  // Verifica PSRAM (ESP32-WROVER e altri modelli con memoria esterna)
  #ifdef CONFIG_SPIRAM_SUPPORT
  if (psramInit()) {
    size_t psramSize = ESP.getPsramSize();
    Serial.print(F("PSRAM disponibile: "));
    Serial.print(psramSize / 1024);
    Serial.println(F(" KB"));
  } else {
    Serial.println(F("PSRAM non disponibile"));
  }
  #else
  Serial.println(F("PSRAM non supportata in questa build"));
  #endif

  // Inizializzazione SPI per display
  try {
    SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);
    Serial.println(F("SPI per display inizializzato"));
  } catch (...) {
    Serial.println(F("ERRORE: Impossibile inizializzare SPI per display"));
    success = false;
  }
  
  // Inizializzazione SPI per SD card
  try {
    sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    Serial.println(F("SPI per SD card inizializzato"));
  } catch (...) {
    Serial.println(F("ERRORE: Impossibile inizializzare SPI per SD card"));
    success = false;
  }

  // Configurazione pin per pulsanti
  pinMode(RESET_BUTTON_PIN, INPUT); // Il pull-up è esterno
  
  // Verifica finale e stampa dell'utilizzo della memoria
  if (success) {
    Serial.print(F("Heap libero: "));
    Serial.print(ESP.getFreeHeap() / 1024);
    Serial.println(F(" KB"));
    Serial.println(F("Hardware inizializzato con successo"));
  } else {
    Serial.println(F("ATTENZIONE: Alcuni componenti hardware non sono stati inizializzati correttamente"));
  }
  
  return success;
}
