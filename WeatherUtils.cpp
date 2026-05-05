#include "WeatherUtils.h"
#include "Config.h"
#include <HTTPClient.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <time.h>

// Variabile globale per i dati meteo
WeatherData currentWeather;

// Funzione per ottenere i dati meteo da OpenWeatherMap (versione 2.5, gratuita)
bool getWeatherData() {
  // Serial.println("[WEATHER] Richiesta dati meteo in corso...");
  
  // Verifica se abbiamo una configurazione valida
  if (!loadConfig()) {
    // Serial.println("[WEATHER] Errore: Configurazione non valida");
    return false;
  }
  
  // Verifica se API key e città sono configurate
  if (strlen(config.api_key) == 0 || strlen(config.city) == 0) {
    // Serial.println("[WEATHER] Errore: API key o città non configurate");
    // Serial.print("[WEATHER] API key: ");
    // Serial.println(config.api_key);
    // Serial.print("[WEATHER] Città: ");
    // Serial.println(config.city);
    return false;
  }
  
  // Conserviamo le coordinate per futuri usi con API più avanzate
  if (config.lat == 0 && config.lon == 0) {
    // Serial.println("[WEATHER] Info: Coordinate non impostate per " + String(config.city));
    // Inizializza coordinate di default in base alla città - Check ottimizzato con strstr
    if (strstr(config.city, "Milano") != NULL) {
      config.lat = 45.4642; 
      config.lon = 9.1900;
    } else if (strstr(config.city, "Roma") != NULL) {
      config.lat = 41.9028;
      config.lon = 12.4964;
    } else if (strstr(config.city, "Napoli") != NULL) {
      config.lat = 40.8518;
      config.lon = 14.2681;
    } else if (strstr(config.city, "Torino") != NULL) {
      config.lat = 45.0703;
      config.lon = 7.6869;
    } else {
      // Coordinate di default per l'Italia se la città non è riconosciuta
      config.lat = 42.5;
      config.lon = 12.5;
    }
    // Salva le coordinate per uso futuro
    saveConfig();
  }
  
  // Torniamo all'API gratuita 2.5 per i dati meteo base
  static const char base_url[] PROGMEM = "https://api.openweathermap.org/data/2.5/weather?q=";
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
  
  HTTPClient http;
  http.begin(url);
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
      
      // Parsing dei dati JSON
      bool success = parseWeatherData(payload);
      // if (success) {
      //   Serial.println("[WEATHER] Dati meteo aggiornati con successo!");
      // } else {
      //   Serial.println("[WEATHER] Errore nel parsing dei dati meteo");
      // }
      return success;
    } else {
      // Serial.print("[WEATHER] Errore HTTP: ");
      // Serial.println(httpCode);
    }
  } else {
    // Serial.print("[WEATHER] Errore connessione: ");
    // Serial.println(http.errorToString(httpCode));
  }
  
  http.end();
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
    // Serial.print("[WEATHER] Errore deserializeJson: ");
    // Serial.println(error.c_str());
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
  
  // Calcola la fase lunare in base al giorno del mese (approssimazione)
  // Dato che non abbiamo più l'API OneCall, usiamo un metodo approssimativo
  time_t now = time(NULL);
  struct tm *timeinfo = localtime(&now);
  
  // Converti il giorno del mese in un valore da 0 a 1 per la fase lunare
  // Questa è solo un'approssimazione, non tiene conto del ciclo lunare reale
  currentWeather.moon_phase = (float)(timeinfo->tm_mday - 1) / 29.5;
  // Serial.print("[WEATHER] Fase lunare approssimata: ");
  // Serial.println(currentWeather.moon_phase);
  
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
