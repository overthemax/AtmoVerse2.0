// SD_Card_HW_Test.ino
// Test SD card con pin custom per ESP32
// Adatta i pin se necessario!

#include <SPI.h>
#include <SD.h>

// Pin definiti come nel tuo Hardware.h
#define SD_CS   14  // GPIO14 - chip select SD
#define SD_SCK  27  // GPIO27 - clock SD
#define SD_MOSI 26  // GPIO26 - mosi SD
#define SD_MISO 25  // GPIO25 - miso SD

SPIClass sdSPI(HSPI);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n===== SD Card Hardware Test =====");

  // Inizializza la SPI custom per la SD
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  // Prova a montare la SD
  if (!SD.begin(SD_CS, sdSPI)) {
    Serial.println("[ERRORE] SD card non trovata o errore hardware!");
    while (true) delay(1000);
  } else {
    Serial.println("[OK] SD card inizializzata correttamente!");
  }

  // Mostra i file nella root della SD
  File root = SD.open("/");
  if (!root) {
    Serial.println("[ERRORE] Impossibile aprire la root della SD!");
    while (true) delay(1000);
  }

  Serial.println("[INFO] File trovati nella root:");
  File file = root.openNextFile();
  if (!file) Serial.println("(Nessun file trovato)");
  while (file) {
    Serial.print("  ");
    Serial.print(file.name());
    if (file.isDirectory()) {
      Serial.println("/ (dir)");
    } else {
      Serial.print("  - ");
      Serial.print(file.size());
      Serial.println(" bytes");
    }
    file = root.openNextFile();
  }
  root.close();

  Serial.println("Test completato. Puoi ora spegnere o resettare.");
}

void loop() {
  // Nulla
}
