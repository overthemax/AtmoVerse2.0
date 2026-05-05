/*
  Minimal test sketch for GxEPD2 5.83" e-ink display (GDEW0583T8) on ESP32
  - Collega i pin secondo i define qui sotto!
  - Apri il Serial Monitor a 115200 baud
*/

#include <GxEPD2_BW.h>
#include <SPI.h>

// === DEFINIZIONE PIN ===
#define EPD_BUSY    4    // GPIO04 - busy
#define EPD_RST     16   // GPIO16 - res (reset)
#define EPD_DC      17   // GPIO17 - d/c (data/command)
#define EPD_CS      5    // GPIO05 - cs (chip select)
#define EPD_SCK     18   // GPIO18 - sck (clock)
#define EPD_MOSI    23   // GPIO23 - sdi (data in/MOSI)

// === OGGETTO DISPLAY ===
GxEPD2_BW<GxEPD2_583_T8, GxEPD2_583_T8::HEIGHT> display(GxEPD2_583_T8(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n===== TEST DISPLAY E-INK 5.83\" =====");

  // Inizializza la SPI per il display
  SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);

  // Inizializza il display
  display.init(115200);
  display.setRotation(0);
  display.setTextColor(GxEPD_BLACK);
  display.setFullWindow();
  Serial.println("Display inizializzato!");

  // Mostra schermata di test
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setFont(NULL);
    display.setCursor(10, 60);
    display.print("TEST E-INK");
    display.setCursor(10, 120);
    display.print("AtmoVerse 2.0");
    display.setCursor(10, 180);
    display.print("OK!");
  } while (display.nextPage());
  Serial.println("Test completato. Se vedi il testo, il display funziona!");
}

void loop() {
  // Non fa nulla
}
