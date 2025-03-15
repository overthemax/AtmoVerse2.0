/*
 * AtmoVerse 2.0 - Implementazione Gestione Citazioni
 * 
 * Ottimizzato per la memoria dell'ESP32
 */

#include "quotes.h"
#include "config.h"
#include <ArduinoJson.h>
#include <SD.h>

// Funzione per salvare le citazioni su SD
bool saveQuotes() {
  // Rimuovi file esistente
  if (SD.exists("/quotes/quotes.json")) {
    SD.remove("/quotes/quotes.json");
  }
  
  // Crea cartella quotes se non esiste
  if (!SD.exists("/quotes")) {
    SD.mkdir("/quotes");
  }
  
  File file = SD.open("/quotes/quotes.json", FILE_WRITE);
  if (!file) {
    Serial.println(F("Errore: impossibile aprire il file delle citazioni"));
    return false;
  }

  JsonDocument doc;
  JsonArray thunderstorm = doc.createNestedArray("thunderstorm");
  for (int i = 0; i < customQuotes.thunderstormCount; i++) {
    thunderstorm.add(customQuotes.thunderstorm[i]);
  }

  JsonArray rain = doc.createNestedArray("rain");
  for (int i = 0; i < customQuotes.rainCount; i++) {
    rain.add(customQuotes.rain[i]);
  }

  JsonArray snow = doc.createNestedArray("snow");
  for (int i = 0; i < customQuotes.snowCount; i++) {
    snow.add(customQuotes.snow[i]);
  }

  JsonArray clearSky = doc.createNestedArray("clearSky");
  for (int i = 0; i < customQuotes.clearSkyCount; i++) {
    clearSky.add(customQuotes.clearSky[i]);
  }

  JsonArray brokenClouds = doc.createNestedArray("brokenClouds");
  for (int i = 0; i < customQuotes.brokenCloudsCount; i++) {
    brokenClouds.add(customQuotes.brokenClouds[i]);
  }

  if (serializeJson(doc, file) == 0) {
    Serial.println(F("Errore nella scrittura delle citazioni"));
    file.close();
    return false;
  }
  
  file.close();
  Serial.println(F("Citazioni salvate con successo"));
  return true;
}

// Funzione per caricare le citazioni da SD
bool loadQuotes() {
  if (!SD.exists("/quotes/quotes.json")) {
    Serial.println(F("File delle citazioni non trovato"));
    
    // Imposta citazioni predefinite
    strcpy(customQuotes.clearSky[0], "La vita è come un cielo sereno: bella, luminosa e piena di possibilità.");
    customQuotes.clearSkyCount = 1;
    
    strcpy(customQuotes.brokenClouds[0], "Come nuvole passeggere, i problemi vanno e vengono.");
    customQuotes.brokenCloudsCount = 1;
    
    strcpy(customQuotes.rain[0], "La pioggia ci ricorda che dopo ogni tempesta, torna sempre il sereno.");
    customQuotes.rainCount = 1;
    
    strcpy(customQuotes.snow[0], "Ogni fiocco di neve è diverso, proprio come ogni momento della vita.");
    customQuotes.snowCount = 1;
    
    strcpy(customQuotes.thunderstorm[0], "I temporali, come le difficoltà, ci rendono più forti.");
    customQuotes.thunderstormCount = 1;
    
    return true;
  }
  
  File quotesFile = SD.open("/quotes/quotes.json", FILE_READ);
  if (!quotesFile) {
    Serial.println(F("Errore: impossibile aprire il file delle citazioni"));
    return false;
  }
  
  // Riduce la dimensione del documento JSON a 384 bytes
  StaticJsonDocument<384> doc;
  
  DeserializationError error = deserializeJson(doc, quotesFile);
  if (error) {
    Serial.println(F("Errore nel parsing del file delle citazioni"));
    quotesFile.close();
    return false;
  }
  
  // Inizializza contatori
  customQuotes.thunderstormCount = 0;
  customQuotes.rainCount = 0;
  customQuotes.snowCount = 0;
  customQuotes.clearSkyCount = 0;
  customQuotes.brokenCloudsCount = 0;
  
  // Carica le citazioni per ogni categoria
  if (doc.containsKey("thunderstorm")) {
    JsonArray thunderstorm = doc["thunderstorm"];
    customQuotes.thunderstormCount = min((size_t)MAX_QUOTES_PER_CATEGORY, thunderstorm.size());
    for (uint8_t i = 0; i < customQuotes.thunderstormCount; i++) {
      strcpy(customQuotes.thunderstorm[i], thunderstorm[i].as<String>().c_str());
    }
  }
  
  if (doc.containsKey("rain")) {
    JsonArray rain = doc["rain"];
    customQuotes.rainCount = min((size_t)MAX_QUOTES_PER_CATEGORY, rain.size());
    for (uint8_t i = 0; i < customQuotes.rainCount; i++) {
      strcpy(customQuotes.rain[i], rain[i].as<String>().c_str());
    }
  }
  
  if (doc.containsKey("snow")) {
    JsonArray snow = doc["snow"];
    customQuotes.snowCount = min((size_t)MAX_QUOTES_PER_CATEGORY, snow.size());
    for (uint8_t i = 0; i < customQuotes.snowCount; i++) {
      strcpy(customQuotes.snow[i], snow[i].as<String>().c_str());
    }
  }
  
  if (doc.containsKey("clearSky")) {
    JsonArray clearSky = doc["clearSky"];
    customQuotes.clearSkyCount = min((size_t)MAX_QUOTES_PER_CATEGORY, clearSky.size());
    for (uint8_t i = 0; i < customQuotes.clearSkyCount; i++) {
      strcpy(customQuotes.clearSky[i], clearSky[i].as<String>().c_str());
    }
  }
  
  if (doc.containsKey("brokenClouds")) {
    JsonArray brokenClouds = doc["brokenClouds"];
    customQuotes.brokenCloudsCount = min((size_t)MAX_QUOTES_PER_CATEGORY, brokenClouds.size());
    for (uint8_t i = 0; i < customQuotes.brokenCloudsCount; i++) {
      strcpy(customQuotes.brokenClouds[i], brokenClouds[i].as<String>().c_str());
    }
  }
  
  quotesFile.close();
  Serial.println(F("Citazioni caricate con successo"));
  return true;
}

// Funzione per ottenere una citazione in base alla categoria meteo
String getWeatherQuote(String weatherCategory) {
  // Controllo preventivo per verificare se esistono citazioni per la categoria
  // Ottimizzato per evitare chiamate multiple a random() e prevenire divisione per zero
  if ((weatherCategory == F("thunderstorm") && customQuotes.thunderstormCount == 0) ||
      (weatherCategory == F("rain") && customQuotes.rainCount == 0) ||
      (weatherCategory == F("snow") && customQuotes.snowCount == 0) ||
      (weatherCategory == F("clearSky") && customQuotes.clearSkyCount == 0) ||
      (weatherCategory == F("brokenClouds") && customQuotes.brokenCloudsCount == 0)) {
    return F("Non ci sono citazioni disponibili per questa condizione meteo.");
  }
  
  uint8_t quoteIndex = 0;
  
  // Genera un indice solo se ci sono citazioni disponibili e restituisce la citazione corrispondente
  if (weatherCategory == F("thunderstorm") && customQuotes.thunderstormCount > 0) {
    quoteIndex = random(0, customQuotes.thunderstormCount);
    return String(customQuotes.thunderstorm[quoteIndex]);
  } 
  else if (weatherCategory == F("rain") && customQuotes.rainCount > 0) {
    quoteIndex = random(0, customQuotes.rainCount);
    return String(customQuotes.rain[quoteIndex]);
  } 
  else if (weatherCategory == F("snow") && customQuotes.snowCount > 0) {
    quoteIndex = random(0, customQuotes.snowCount);
    return String(customQuotes.snow[quoteIndex]);
  } 
  else if (weatherCategory == F("clearSky") && customQuotes.clearSkyCount > 0) {
    quoteIndex = random(0, customQuotes.clearSkyCount);
    return String(customQuotes.clearSky[quoteIndex]);
  } 
  else if (weatherCategory == F("brokenClouds") && customQuotes.brokenCloudsCount > 0) {
    quoteIndex = random(0, customQuotes.brokenCloudsCount);
    return String(customQuotes.brokenClouds[quoteIndex]);
  }
  
  // Caso di fallback nel caso in cui la categoria non sia riconosciuta
  return F("Oggi è un buon giorno per riflettere sul tempo.");
}
