// SD_Card_HW_Test.ino
// SD card test with custom pins on the ESP32
// Change the pins if needed.

#include <SPI.h>
#include <SD.h>

// Same pins as Hardware.h
#define SD_CS   14  // GPIO14 - chip select SD
#define SD_SCK  27  // GPIO27 - clock SD
#define SD_MOSI 26  // GPIO26 - mosi SD
#define SD_MISO 25  // GPIO25 - miso SD

SPIClass sdSPI(HSPI);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n===== SD Card Hardware Test =====");

  // Custom SPI for the SD card
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  // Mount the SD card
  if (!SD.begin(SD_CS, sdSPI)) {
    Serial.println("[ERROR] SD card not found or hardware error");
    while (true) delay(1000);
  } else {
    Serial.println("[OK] SD card mounted");
  }

  // List the files in the root folder
  File root = SD.open("/");
  if (!root) {
    Serial.println("[ERROR] Cannot open the SD root folder");
    while (true) delay(1000);
  }

  Serial.println("[INFO] Files in the root folder:");
  File file = root.openNextFile();
  if (!file) Serial.println("(no files)");
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

  Serial.println("Test done. You can now power off or reset.");
}

void loop() {
}
