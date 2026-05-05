#include "WeatherUtils.h"
#include "Config.h"
#include <HTTPClient.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <time.h>

// Stringhe costanti per classi icone meteo in PROGMEM
const char ICON_NIGHT_PREFIX[] PROGMEM = "night-";
const char ICON_STORM[] PROGMEM = "storm";
const char ICON_DRIZZLE[] PROGMEM = "drizzle";
const char ICON_RAIN[] PROGMEM = "rain";
const char ICON_SLEET[] PROGMEM = "sleet";
const char ICON_SHOWER[] PROGMEM = "shower";
const char ICON_SNOW[] PROGMEM = "snow";
const char ICON_FOG[] PROGMEM = "fog";
const char ICON_DUST[] PROGMEM = "dust";
const char ICON_HAZE[] PROGMEM = "haze";
const char ICON_VOLCANO[] PROGMEM = "volcano";
const char ICON_WIND[] PROGMEM = "wind";
const char ICON_TORNADO[] PROGMEM = "tornado";
const char ICON_NIGHT_CLEAR[] PROGMEM = "night-clear";
const char ICON_DAY_SUNNY[] PROGMEM = "day-sunny";
const char ICON_NIGHT_CLOUDY[] PROGMEM = "night-cloudy";
const char ICON_DAY_CLOUDY[] PROGMEM = "day-cloudy";
const char ICON_CLOUDY[] PROGMEM = "cloudy";
const char ICON_OVERCAST[] PROGMEM = "overcast";

// Dichiarazione di funzioni esterne definite in Weather.cpp
extern bool isWeatherDataValid();
extern bool isNightTime();

// Parsing dei dati meteo dal JSON - Versione per API 2.5
bool parseWeatherData(String& json) {
  // Definisci la capacità del documento JSON in base alle dimensioni della risposta
  const size_t capacity = 1024; // Ridotto ulteriormente da 1536 per risparmiare DRAM
  DynamicJsonDocument doc(capacity);
  
  // Parsing del JSON
  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    Serial.print(F("[WEATHER] Errore deserializeJson(): "));
    Serial.println(error.c_str());
    return false;
  }

  // Accesso diretto ai campi richiesti
  JsonObject main = doc["main"];
  if (!main) {
    Serial.println(F("[WEATHER] Errore: oggetto 'main' non trovato nel JSON"));
    return false;
  }

  // Oggetto meteo principale (contiene id, descrizione, icona)
  JsonArray weather = doc["weather"];
  if (!weather || weather.size() == 0) {
    Serial.println(F("[WEATHER] Errore: array 'weather' non trovato o vuoto"));
    return false;
  }

  // Accedi ai valori principali
  JsonObject weather0 = weather[0];
  
  // Riempi la struttura dati meteo
  extern WeatherData currentWeather;
  
  // Temperatura (in Kelvin dall'API, convertiamo in Celsius)
  currentWeather.temp = main["temp"].as<float>() - 273.15f;
  currentWeather.feels_like = main["feels_like"].as<float>() - 273.15f;
  
  // Altri dati atmosferici
  currentWeather.humidity = main["humidity"].as<int>();
  currentWeather.pressure = main["pressure"].as<int>();
  
  // Vento
  JsonObject wind = doc["wind"];
  if (wind) {
    currentWeather.wind_speed = wind["speed"].as<float>();
    currentWeather.wind_deg = wind["deg"].as<int>();
  } else {
    currentWeather.wind_speed = 0;
    currentWeather.wind_deg = 0;
  }
  
  // Dati meteo (condizione, descrizione, icona)
  currentWeather.weather_id = weather0["id"].as<int>();
  // Salva il puntatore alla stringa nel JSON (rimane valido finché il documento JSON esiste)
  currentWeather.description = weather0["description"].as<const char*>();
  // Nota: questo puntatore è valido solo durante questa funzione poiché il JSON document è locale
  // Creiamo una copia statica in PROGMEM
  static char descriptionBuffer[32];
  strlcpy(descriptionBuffer, currentWeather.description, sizeof(descriptionBuffer));
  currentWeather.description = descriptionBuffer; // Riassegna il puntatore alla copia statica
  strlcpy(currentWeather.icon, 
         weather0["icon"].as<const char*>(), 
         sizeof(currentWeather.icon));
  
  // Timestamp dell'aggiornamento
  currentWeather.last_update = time(NULL);
  
  return true;
}

// Funzione per ottenere il timestamp dell'ultimo aggiornamento meteo
time_t getLastUpdateTime() {
  extern WeatherData currentWeather;
  return currentWeather.last_update;
}

// Funzione per ottenere la classe dell'icona meteo
String getWeatherIconClass(int weatherId, bool isNight) {
  // Mappa gli ID delle condizioni meteo di OpenWeatherMap alle classi di icone
  static String iconClass;
  
  // Prefisso per le icone notturne
  String prefix;
  if (isNight) {
    prefix = FPSTR(ICON_NIGHT_PREFIX);
  } else {
    prefix = "";
  }
  
  // Classificazione in base all'ID (da API OpenWeatherMap)
  if (weatherId >= 200 && weatherId < 300) {
    iconClass = prefix + FPSTR(ICON_STORM);
  } else if (weatherId >= 300 && weatherId < 400) {
    iconClass = prefix + FPSTR(ICON_DRIZZLE);
  } else if (weatherId >= 500 && weatherId < 600) {
    iconClass = prefix + FPSTR(ICON_RAIN);
    // Eccezioni per alcuni tipi di pioggia
    if (weatherId == 511) {
      iconClass = FPSTR(ICON_SLEET); // Pioggia ghiacciata
    } else if (weatherId >= 520) {
      iconClass = prefix + FPSTR(ICON_SHOWER);
    }
  } else if (weatherId >= 600 && weatherId < 700) {
    iconClass = FPSTR(ICON_SNOW);
    if (weatherId >= 611 && weatherId <= 616) {
      iconClass = FPSTR(ICON_SLEET);
    }
  } else if (weatherId >= 700 && weatherId < 800) {
    if (weatherId == 701 || weatherId == 741) {
      iconClass = prefix + FPSTR(ICON_FOG);
    } else if (weatherId == 711 || weatherId == 731 || weatherId == 751 || weatherId == 761) {
      iconClass = FPSTR(ICON_DUST);
    } else if (weatherId == 721) {
      iconClass = FPSTR(ICON_HAZE);
    } else if (weatherId == 762) {
      iconClass = FPSTR(ICON_VOLCANO);
    } else if (weatherId == 771) {
      iconClass = FPSTR(ICON_WIND);
    } else if (weatherId == 781) {
      iconClass = FPSTR(ICON_TORNADO);
    } else {
      iconClass = FPSTR(ICON_DUST); // Default per altre condizioni atmosferiche
    }
  } else if (weatherId == 800) {
    iconClass = isNight ? FPSTR(ICON_NIGHT_CLEAR) : FPSTR(ICON_DAY_SUNNY);
  } else if (weatherId >= 801 && weatherId <= 804) {
    if (weatherId == 801) {
      iconClass = isNight ? FPSTR(ICON_NIGHT_CLOUDY) : FPSTR(ICON_DAY_CLOUDY);
    } else if (weatherId == 802) {
      iconClass = FPSTR(ICON_CLOUDY);
    } else if (weatherId == 803 || weatherId == 804) {
      iconClass = FPSTR(ICON_OVERCAST);
    }
  } else {
    // Default per ID non gestiti
    iconClass = isNight ? FPSTR(ICON_NIGHT_CLEAR) : FPSTR(ICON_DAY_SUNNY);
  }
  
  return iconClass;
}
