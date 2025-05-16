#ifndef WEATHER_UTILS_H
#define WEATHER_UTILS_H

#include <Arduino.h>
#include <time.h>
#include "Config.h"

// Struttura dati per le informazioni meteo attuali
struct WeatherData {
  float temp;             // Temperatura in gradi Celsius
  float feels_like;       // Temperatura percepita in gradi Celsius
  float humidity;         // Umidità percentuale
  float pressure;         // Pressione atmosferica in hPa
  float wind_speed;       // Velocità del vento in m/s
  int wind_deg;           // Direzione del vento in gradi
  int weather_id;         // ID condizione meteo (da OpenWeatherMap)
  char icon[8];           // Icona meteo
  char description[64];   // Descrizione meteo
  float moon_phase;       // Fase lunare (0-1): 0=luna nuova, 0.25=primo quarto, 0.5=luna piena, 0.75=ultimo quarto
  time_t last_update;     // Timestamp dell'aggiornamento
  bool valid;             // Flag che indica se i dati sono validi
};

// Dati meteo correnti
extern WeatherData currentWeather;

// Intervallo di aggiornamento meteo (30 minuti)
const unsigned long WEATHER_UPDATE_INTERVAL = 30 * 60 * 1000;

// Funzioni per la gestione dei dati meteo
bool getWeatherData();
bool parseWeatherData(String& json);
bool isWeatherDataValid();
bool isNightTime();
time_t getLastUpdateTime();
String getWeatherIconClass(int weatherId, bool isNight);

#endif // WEATHER_UTILS_H
