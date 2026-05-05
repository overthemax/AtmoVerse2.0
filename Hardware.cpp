#include "Hardware.h"
#include <Arduino.h>
#include <SPI.h>
#include <SD.h> // Aggiunto per SD
#include <GxEPD2_BW.h> // Ripristinato a BW
#include <GxEPD2_3C.h>
#include "Debug.h"

// Instanziazione del display GxEPD2 B/N, 5.83" GDEW0583T8
GxEPD2_BW<GxEPD2_583_T8, GxEPD2_583_T8::HEIGHT> display(GxEPD2_583_T8(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

// Flag per tracciare lo stato della SD
static bool _sdInitialized = false;

// Inizializza la SD card
bool initSD() {
  if (_sdInitialized) return true;
  
  DEBUG_TRACE("initSD");
  
  // Configura il pin CS come output e portalo HIGH prima dell'inizializzazione
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  delay(10);
  
  // Assicuriamoci che SPI sia configurato per la SD
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  delay(500); // Attesa per stabilizzazione bus
  
  // Primo tentativo: 4MHz (velocità standard)
  // Serial.println("[SD-DEBUG] Tentativo 1: inizializzazione SD a 4MHz...");
  if (SD.begin(SD_CS, sdSPI, 4000000)) {
    _sdInitialized = true;
    // Serial.println("[HARDWARE] ✓ SD Card inizializzata a 4MHz (velocità ottimale)");
    
    uint8_t cardType = SD.cardType();
    // Serial.printf("[SD-INFO] Tipo card: %s\n", 
    //   cardType == CARD_MMC ? "MMC" : 
    //   cardType == CARD_SD ? "SDSC" : 
    //   cardType == CARD_SDHC ? "SDHC" : "SCONOSCIUTO");
    
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    // Serial.printf("[SD-INFO] Capacità: %llu MB\n", cardSize);
    
    return true;
  }
  
  // Secondo tentativo: 2MHz (ridotta per stabilità)
  // Serial.println("[SD-DEBUG] Tentativo 2: riprovo a 2MHz...");
  delay(200);
  if (SD.begin(SD_CS, sdSPI, 2000000)) {
    _sdInitialized = true;
    // Serial.println("[HARDWARE] ✓ SD Card inizializzata a 2MHz (velocità ridotta)");
    return true;
  }
  
  // Terzo tentativo: 1MHz (maggiore compatibilità)
  // Serial.println("[SD-DEBUG] Tentativo 3: riprovo a 1MHz...");
  delay(200);
  if (SD.begin(SD_CS, sdSPI, 1000000)) {
    _sdInitialized = true;
    // Serial.println("[HARDWARE] ✓ SD Card inizializzata a 1MHz (modalità compatibilità)");
    return true;
  }
  
  // Quarto tentativo: 400kHz (ultima possibilità)
  // Serial.println("[SD-DEBUG] Tentativo 4: riprovo a 400kHz...");
  delay(200);
  if (SD.begin(SD_CS, sdSPI, 400000)) {
    _sdInitialized = true;
    // Serial.println("[HARDWARE] ⚠️ SD Card inizializzata a 400kHz (modalità molto lenta)");
    // Serial.println("[HARDWARE] Possibile problema: card vecchia, collegamenti instabili o alimentazione insufficiente");
    return true;
  }
  
  // Quinto tentativo dopo reset del bus (4MHz)
  // Serial.println("[SD-DEBUG] Tentativo 5: reset bus e retry a 4MHz...");
  sdSPI.end();
  delay(500);
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  delay(100);
  
  if (SD.begin(SD_CS, sdSPI, 4000000)) {
    _sdInitialized = true;
    // Serial.println("[HARDWARE] ✓ SD Card inizializzata dopo reset bus (4MHz)");
    return true;
  }
  
  // Tutti i tentativi falliti
  // Serial.println("\n[HARDWARE] ❌ ERRORE: Impossibile inizializzare SD Card");
  // Serial.println("[HARDWARE] Verificare:");
  // Serial.println("  1. SD card correttamente inserita");
  // Serial.println("  2. Collegamenti hardware (CS, SCK, MISO, MOSI)");
  // Serial.println("  3. SD card formattata FAT32");
  // Serial.println("  4. Alimentazione stabile (3.3V)");
  // Serial.println("==============================\n");
  
  return false;
}

// Inizializza l'hardware
void initHardware() {
  DEBUG_TRACE();
  // Serial.begin è già chiamato nel setup(), evitiamo duplicati o reinizializzazioni che resettano USB
  if (!Serial) {
    Serial.begin(SERIAL_BAUD_RATE);
    while (!Serial && millis() < 3000); 
  }
  
  // Serial.println("\nInizializzazione hardware AtmoVerse 2.0...");
  
  // NOTA: SPI.begin() è già chiamato nel setup() prima di initDisplay()
  // Non duplicare l'inizializzazione SPI qui
  
  // Serial.println("Hardware inizializzato");
}
