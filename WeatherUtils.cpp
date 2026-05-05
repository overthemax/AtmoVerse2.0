#include "WeatherUtils.h"
#include "Config.h"
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <time.h>

// Variabile globale per i dati meteo
WeatherData currentWeather;

// Funzione per ottenere i dati meteo da OpenWeatherMap (versione 2.5, gratuita)
bool getWeatherData() {
  Serial.println("[WEATHER] Richiesta dati meteo in corso...");
  
  // Verifica se abbiamo una configurazione valida
  if (!loadConfig()) {
    Serial.println("[WEATHER] Errore: Configurazione non valida");
    return false;
  }
  
  // Verifica se API key e città sono configurate
  if (strlen(config.api_key) == 0 || strlen(config.city) == 0) {
    Serial.println("[WEATHER] Errore: API key o città non configurate");
    Serial.print("[WEATHER] API key: "); Serial.println(config.api_key);
    Serial.print("[WEATHER] Citta: "); Serial.println(config.city);
    return false;
  }
  
  // NOTA: L'API 2.5 di OpenWeatherMap accetta direttamente il nome città.
  // Le coordinate (lat/lon) sono opzionali e possono essere configurate manualmente
  // se si vuole usare l'API 3.0 OneCall in futuro.
  
  // Torniamo all'API gratuita 2.5 per i dati meteo base
  static const char base_url[] PROGMEM = "http://api.openweathermap.org/data/2.5/weather?q=";
  String url = FPSTR(base_url);
  url += config.city;
  url += "&units=";
  url += (strlen(config.units) ? config.units : "metric");
  url += "&lang=";
  url += (strlen(config.language) ? config.language : "it");
  url += "&appid=";
  url += config.api_key;
  
  // Serial.print("[WEATHER] URL richiesta: ");
  // Serial.println(url);
  
  WiFiClient client;
  HTTPClient http;
  http.begin(client, url);
  http.setTimeout(10000); // Timeout di 10 secondi
  
  // Effettua la richiesta GET
  // Serial.println("[WEATHER] Invio richiesta GET...");
  int httpCode = http.GET();
  // Serial.print("[WEATHER] Codice risposta HTTP: ");
  // Serial.println(httpCode);
  
  // Controlla il codice di risposta
  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      // Serial.println("[WEATHER] Risposta ricevuta! Parsing JSON...");
      // Serial.print("[WEATHER] Payload: ");
      // Serial.println(payload);
      http.end();
      client.stop();
      
      // Parsing dei dati JSON
      bool success = parseWeatherData(payload);
      if (success) {
        Serial.print("[WEATHER] OK - id:"); Serial.print(currentWeather.weather_id);
        Serial.print(" temp:"); Serial.print(currentWeather.temp);
        Serial.print(" city:"); Serial.println(config.city);
      } else {
        Serial.println("[WEATHER] Errore nel parsing dei dati meteo");
      }
      return success;
    } else {
      Serial.print("[WEATHER] Errore HTTP: ");
      Serial.println(httpCode);
    }
  } else {
    Serial.print("[WEATHER] Errore connessione: ");
    Serial.println(http.errorToString(httpCode));
  }
  
  http.end();
  client.stop();
  return false;
}

// Parsing dei dati meteo dal JSON - Versione per API 2.5
bool parseWeatherData(String& json) {
  // Alloca un documento JSON dinamico 
  DynamicJsonDocument doc(2048);
  
  // Esegui il parsing del JSON
  DeserializationError error = deserializeJson(doc, json);
  
  // Verifica errori di parsing
  if (error) {
    Serial.print("[WEATHER] Errore deserializeJson: ");
    Serial.println(error.c_str());
    return false;
  }
  
  // Verifica che il documento contenga i campi necessari
  if (!doc.containsKey("main") || !doc.containsKey("weather")) {
    // Serial.println("[WEATHER] Errore: JSON non contiene campi 'main' o 'weather'");
    return false;
  }
  
  // Estrai e salva i dati meteo nelle variabili globali
  currentWeather.temp = doc["main"]["temp"];
  currentWeather.feels_like = doc["main"]["feels_like"];
  currentWeather.humidity = doc["main"]["humidity"];
  currentWeather.pressure = doc["main"]["pressure"];
  currentWeather.wind_speed = doc["wind"]["speed"];
  currentWeather.wind_deg = doc["wind"]["deg"];
  
  // Estrai informazioni sul tempo
  JsonObject weather = doc["weather"][0];
  currentWeather.weather_id = weather["id"];
  strlcpy(currentWeather.icon, weather["icon"], sizeof(currentWeather.icon));
  strlcpy(currentWeather.description, weather["description"], sizeof(currentWeather.description));
  
  // Calcola la fase lunare usando l'algoritmo corretto
  // Basato su conteggio giorni dalla luna nuova del 6 gennaio 2000
  // Il ciclo lunare è di circa 29.53059 giorni
  time_t now = time(NULL);
  
  // Data di riferimento: 6 gennaio 2000 18:14 UTC (luna nuova conosciuta)
  // In timestamp: 947182440 secondi dal 1/1/1970
  const time_t referenceNewMoon = 947182440;
  const float lunarCycle = 29.53059;  // Periodo orbitale lunare in giorni
  
  // Calcola giorni trascorsi dalla luna nuova di riferimento
  float daysSinceRef = (float)(now - referenceNewMoon) / 86400.0f;
  
  // Calcola la fase (0.0 = luna nuova, 0.5 = luna piena, 1.0 = luna nuova)
  float phase = fmod(daysSinceRef / lunarCycle, 1.0f);
  if (phase < 0) phase += 1.0f;  // Normalizza valori negativi
  
  currentWeather.moon_phase = phase;
  
  // Aggiorna il timestamp
  currentWeather.last_update = time(NULL);
  currentWeather.valid = true;
  
  return true;
}

// Verifica se i dati meteo sono validi
bool isWeatherDataValid() {
  // Serial.println("[WEATHER] Verifica validità dati meteo...");
  
  // Se non ci sono dati o il flag valid è falso
  if (!currentWeather.valid) {
    // Serial.println("[WEATHER] Dati meteo non validi: flag valid = false");
    return false;
  }
  
  // Se l'ultimo aggiornamento è troppo vecchio (più di 3 ore)
  time_t now = time(NULL);
  if (now - currentWeather.last_update > 3 * 3600) {
    // Serial.print("[WEATHER] Dati meteo troppo vecchi. Ultimo aggiornamento: ");
    // Serial.print(currentWeather.last_update);
    // Serial.print(", Ora: ");
    // Serial.println(now);
    return false;
  }
  
  // Verifica che i dati essenziali siano presenti
  if (currentWeather.temp == 0 && currentWeather.humidity == 0 && currentWeather.pressure == 0) {
    // Serial.println("[WEATHER] Dati meteo non validi: valori essenziali tutti a zero");
    return false;
  }
  
  // Serial.println("[WEATHER] Dati meteo validi!");
  
  return true;
}

// Restituisce il timestamp dell'ultimo aggiornamento meteo
time_t getLastUpdateTime() {
  return currentWeather.last_update;
}

// Funzione per determinare se è notte
bool isNightTime() {
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  
  // Considera notte dalle 19:00 alle 7:00
  int currentHour = timeinfo.tm_hour;
  return (currentHour >= 19 || currentHour < 7);
}

// Ottiene la classe dell'icona meteo in base all'ID della condizione
String getWeatherIconClass(int weatherId, bool isNight) {
  // Converti ID OpenWeatherMap in classe FontAwesome
  
  // Temporale (200-299)
  if (weatherId >= 200 && weatherId < 300) {
    return "fas fa-bolt";
  }
  
  // Pioggerella (300-399)
  else if (weatherId >= 300 && weatherId < 400) {
    return "fas fa-cloud-rain";
  }
  
  // Pioggia (500-599)
  else if (weatherId >= 500 && weatherId < 600) {
    if (weatherId == 511) { // Pioggia ghiacciata
      return "fas fa-cloud-meatball";
    }
    return "fas fa-cloud-showers-heavy";
  }
  
  // Neve (600-699)
  else if (weatherId >= 600 && weatherId < 700) {
    return "fas fa-snowflake";
  }
  
  // Atmosfera - nebbia, caligine (700-799)
  else if (weatherId >= 700 && weatherId < 800) {
    return "fas fa-smog";
  }
  
  // Cielo sereno (800)
  else if (weatherId == 800) {
    return isNight ? "fas fa-moon" : "fas fa-sun";
  }
  
  // Nuvoloso (801-899)
  else if (weatherId > 800 && weatherId < 900) {
    if (weatherId == 801) { // Poche nuvole
      return isNight ? "fas fa-cloud-moon" : "fas fa-cloud-sun";
    }
    else if (weatherId == 802) { // Nubi sparse
      return isNight ? "fas fa-cloud-moon" : "fas fa-cloud-sun";
    }
    return "fas fa-cloud";
  }
  
  // Default
  return "fas fa-cloud";
}
