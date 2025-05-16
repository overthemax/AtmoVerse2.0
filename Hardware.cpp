#include "Hardware.h"
#include <Arduino.h>
#include <SPI.h>
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include "Debug.h"

// Instanziazione del display GxEPD2 B/N, 5.83" GDEW0583T8
GxEPD2_BW<GxEPD2_583_T8, GxEPD2_583_T8::HEIGHT> display(GxEPD2_583_T8(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

// Inizializza l'hardware
void initHardware() {
  DEBUG_TRACE();
  Serial.begin(SERIAL_BAUD_RATE);
  while (!Serial && millis() < 3000); // Attende fino a 3 secondi per la connessione seriale
  
  Serial.println("\nInizializzazione hardware AtmoVerse 2.0...");
  
  // Inizializzazione SPI
  SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);
  
  Serial.println("Hardware inizializzato");
}
