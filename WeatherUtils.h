#ifndef WEATHER_UTILS_H
#define WEATHER_UTILS_H

#include <Arduino.h>
#include <time.h>
#include "Config.h"

// Current weather
struct WeatherData {
  float temp;             // Temperatura in gradi Celsius
  float feels_like;       // Temperatura percepita in gradi Celsius
  float humidity;         // Humidity, percent
  float pressure;         // Pressione atmosferica in hPa
  float wind_speed;       // Wind speed (m/s, or mph with imperial units)
  int wind_deg;           // Wind direction in degrees
  int weather_id;         // Weather condition ID (OpenWeatherMap)
  char icon[8];           // Weather icon
  char description[64];   // Weather description
  float moon_phase;       // Fase lunare (0-1): 0=luna nuova, 0.25=primo quarto, 0.5=luna piena, 0.75=ultimo quarto
  time_t last_update;     // Timestamp dell'aggiornamento
  time_t sunrise;         // Sunrise and sunset (UTC), from OpenWeatherMap
  time_t sunset;
  bool valid;             // true if the data is valid
};

// Current weather data
extern WeatherData currentWeather;

// Weather functions
bool getWeatherData();
bool parseWeatherData(String& json);
String urlEncodeParam(const char* text);
bool isWeatherDataValid();
bool isNightTime();

// Wind speed in m/s whatever the units (OpenWeatherMap sends mph with imperial units)
float windSpeedMs();

// Beaufort thresholds (m/s): force 6 "strong breeze" and force 8 "gale"
const float WIND_STRONG_MS = 10.8f;  // 39 km/h
const float WIND_GALE_MS = 17.2f;    // 62 km/h

#endif // WEATHER_UTILS_H
