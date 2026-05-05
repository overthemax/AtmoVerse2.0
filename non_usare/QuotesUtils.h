#ifndef QUOTES_UTILS_H
#define QUOTES_UTILS_H

#include <Arduino.h>
#include <SD.h>
#include <ArduinoJson.h>

// Stringhe costanti per le citazioni di default - spostate in PROGMEM
extern const char DEFAULT_QUOTE1[] PROGMEM;
extern const char DEFAULT_QUOTE2[] PROGMEM;
extern const char DEFAULT_QUOTE3[] PROGMEM;
extern const char DEFAULT_QUOTE4[] PROGMEM;
extern const char DEFAULT_QUOTE5[] PROGMEM;
extern const char DEFAULT_QUOTE6[] PROGMEM;

// Ottiene una citazione casuale dalla categoria specificata
String getRandomQuote(String category);

#endif // QUOTES_UTILS_H
