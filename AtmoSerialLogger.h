#ifndef ATMO_SERIAL_LOGGER_H
#define ATMO_SERIAL_LOGGER_H

#include <Arduino.h>

// Classe per disabilitare i log e risparmiare memoria
class AtmoSerialLogger {
public:
  // Costruttore semplificato che non usa SD
  AtmoSerialLogger(SPIClass& spi, int cs) {
    // Non fa nulla
  }
  
  // Inizializza Serial solo se necessario (consigliabile rimuovere)
  void begin(unsigned long baudRate) {
    // Inizializzazione opzionale di Serial per debug critico
    // Serial.begin(baudRate);
  }
  
  // Questi metodi ora non fanno nulla, per risparmiare memoria
  size_t print(const char* message) { return 0; }
  size_t print(String message) { return 0; }
  size_t print(int value) { return 0; }
  size_t print(unsigned int value) { return 0; }
  size_t print(long value) { return 0; }
  size_t print(unsigned long value) { return 0; }
  size_t print(float value, int decimalPlaces = 2) { return 0; }

  size_t println(const char* message) { return 0; }
  size_t println(String message) { return 0; }
  size_t println(int value) { return 0; }
  size_t println(unsigned int value) { return 0; }
  size_t println(long value) { return 0; }
  size_t println(unsigned long value) { return 0; }
  size_t println(float value, int decimalPlaces = 2) { return 0; }
  size_t println() { return 0; }
};

#endif // ATMO_SERIAL_LOGGER_H
