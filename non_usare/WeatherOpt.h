#pragma once

// Include this file in Weather.cpp to enable optimized functions
// that reduce sketch size when OPTIMIZE_SIZE is defined

#ifdef OPTIMIZE_SIZE

#include <Arduino.h>
#include <time.h>
#include <pgmspace.h>

// Versione ottimizzata di getWeatherData() che restituisce valori predefiniti
// senza eseguire chiamate API
inline bool getWeatherDataOpt() {
  // Valori predefiniti per modalità ottimizzata
  currentWeather.temp = 20.5;
  currentWeather.feels_like = 21.0;
  currentWeather.humidity = 65;
  currentWeather.pressure = 1013;
  currentWeather.wind_speed = 4.2;
  currentWeather.wind_deg = 180;
  currentWeather.weather_id = 800; // Cielo sereno
  strlcpy(currentWeather.icon, "01d", sizeof(currentWeather.icon));
  
  static const char DEFAULT_DESC[] PROGMEM = "cielo sereno";
  static char descBuffer[16]; // Ridotto a 16 bytes
  // Copia da PROGMEM in modo compatibile ESP32, assicurando il terminatore
  strncpy_P(descBuffer, DEFAULT_DESC, sizeof(descBuffer) - 1);
  descBuffer[sizeof(descBuffer) - 1] = '\0';
  currentWeather.description = descBuffer;
  
  // Calcola la fase lunare semplificata
  time_t now;
  time(&now);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  currentWeather.moon_phase = ((float)timeinfo.tm_mday / 30.0);
  
  // Aggiorna il timestamp
  currentWeather.last_update = now;
  currentWeather.valid = true;
  
  return true;
}

// Versione ottimizzata di getWeatherIconName
inline String getWeatherIconNameOpt(int weatherId, bool isNight) {
  return isNight ? "01n" : "01d"; // Default: cielo sereno
}

#define GET_WEATHER_DATA getWeatherDataOpt
#define GET_WEATHER_ICON_NAME getWeatherIconNameOpt

#else

#define GET_WEATHER_DATA getWeatherData
#define GET_WEATHER_ICON_NAME getWeatherIconName

#endif
