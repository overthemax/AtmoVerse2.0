#ifndef QUOTES_UTILS_H
#define QUOTES_UTILS_H

#include <Arduino.h>
#include <SD.h>
#include <ArduinoJson.h>

// Ottiene una citazione casuale dalla categoria specificata
String getRandomQuote(String category);

#endif // QUOTES_UTILS_H
