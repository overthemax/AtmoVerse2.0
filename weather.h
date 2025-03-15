/*
 * AtmoVerse 2.0 - Funzioni Meteo
 * 
 * Funzioni per ottenere e aggiornare i dati meteorologici
 */

#ifndef WEATHER_H
#define WEATHER_H

#include "config.h"
#include <Arduino.h>

// Prototipi
void updateWeather();
void setupTime();
void processWeatherData(const String& json);
String getWeatherQuote(const String& weatherCategory);

struct WeatherData {
  float temperature;
  float humidity;
  float pressure;
  String weatherCondition;
  int weatherId;
  String weatherCategory;
  float windSpeed;
  String cityName;
  String countryCode;
  String lastUpdate;
};

// Weather functions implementation
void setupTime() {
  // Configura il server NTP e imposta il fuso orario
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  setenv("TZ", timezone, 1);
  tzset();
  
  // Attendi che il tempo sia impostato
  Serial.println(F("Attesa per la sincronizzazione NTP..."));
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println();
  
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print(F("Timestamp attuale: "));
  Serial.println(asctime(&timeinfo));
}

void updateWeather() {
  // Weather update implementation
}

void processWeatherData(const String& json) {
  // Weather data processing implementation
}

#endif // WEATHER_H
