#include "Config.h"
#include "Hardware.h"
#include "Debug.h"
#include <SD.h>
#include <SPI.h>
#include <ArduinoJson.h>

// Definizione variabili globali
Config config;

// Constanti
const char* CONFIG_FILE = "/config.json";

// Carica la configurazione dalla SD
bool loadConfig() {
  DEBUG_TRACE();
  
  Serial.println("Caricamento configurazione...");
  
  // Verifica che la SD sia inizializzata
  if (!SD.begin(SD_CS, sdSPI)) {
    Serial.println("Impossibile inizializzare la SD card per il caricamento");
    return false;
  }
  
  // Apri il file di configurazione
  File file = SD.open(CONFIG_FILE, FILE_READ);
  if (!file) {
    Serial.println("File di configurazione non trovato, uso valori predefiniti");
    return false;
  }
  
  // Leggi il contenuto del file JSON
  size_t size = file.size();
  if (size > 1024) {
    Serial.println("File di configurazione troppo grande");
    file.close();
    return false;
  }
  
  // Alloca un buffer per contenere il file JSON
  char* json = new char[size + 1];
  if (!json) {
    Serial.println("Memoria insufficiente");
    file.close();
    return false;
  }
  
  // Leggi il file nel buffer
  file.readBytes(json, size);
  file.close();
  json[size] = '\0';
  
  // Deserializza il JSON
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, json);
  delete[] json;
  
  if (error) {
    Serial.print("Errore nella deserializzazione JSON: ");
    Serial.println(error.c_str());
    return false;
  }
  
  // Copia i valori dal documento JSON alla struttura di configurazione
  strlcpy(config.ssid, doc["ssid"] | "", sizeof(config.ssid));
  strlcpy(config.password, doc["password"] | "", sizeof(config.password));
  strlcpy(config.city, doc["city"] | "Rome", sizeof(config.city));
  config.lat = doc["lat"] | 41.9028;
  config.lon = doc["lon"] | 12.4964;
  strlcpy(config.api_key, doc["api_key"] | "", sizeof(config.api_key));
  config.gmtOffset_sec = doc["gmtOffset_sec"] | 3600;
  config.daylightOffset_sec = doc["daylightOffset_sec"] | 3600;
  strlcpy(config.ntpServer, doc["ntpServer"] | "pool.ntp.org", sizeof(config.ntpServer));
  
  // Nuove impostazioni risparmio energetico
  config.powerSavingEnabled = doc["powerSavingEnabled"] | true;
  config.powerSavingStartHour = doc["powerSavingStartHour"] | 22;
  config.powerSavingEndHour = doc["powerSavingEndHour"] | 7;
  config.normalUpdateInterval = doc["normalUpdateInterval"] | 30;
  config.powerSavingUpdateInterval = doc["powerSavingUpdateInterval"] | 120;
  
  // Impostazioni gestione errori
  config.maxNetworkRetries = doc["maxNetworkRetries"] | 3;
  config.showLastDataOnError = doc["showLastDataOnError"] | true;
  
  Serial.println("Configurazione caricata con successo");
  return true;
}

// Salva la configurazione sulla SD
bool saveConfig() {
  DEBUG_TRACE();
  
  Serial.println("Salvataggio configurazione...");
  
  // Verifica che la SD sia inizializzata
  if (!SD.begin(SD_CS, sdSPI)) {
    Serial.println("Impossibile inizializzare la SD card per il salvataggio");
    return false;
  }
  
  // Rimuovi il file se esiste già
  if (SD.exists(CONFIG_FILE)) {
    SD.remove(CONFIG_FILE);
  }
  
  // Apri il file per scrittura
  File file = SD.open(CONFIG_FILE, FILE_WRITE);
  if (!file) {
    Serial.println("Impossibile aprire il file di configurazione per il salvataggio");
    return false;
  }
  
  // Crea un documento JSON
  StaticJsonDocument<1024> doc;
  
  // Copia i valori dalla struttura di configurazione al documento JSON
  doc["ssid"] = config.ssid;
  doc["password"] = config.password;
  doc["city"] = config.city;
  doc["lat"] = config.lat;
  doc["lon"] = config.lon;
  doc["api_key"] = config.api_key;
  doc["gmtOffset_sec"] = config.gmtOffset_sec;
  doc["daylightOffset_sec"] = config.daylightOffset_sec;
  doc["ntpServer"] = config.ntpServer;
  
  // Nuove impostazioni risparmio energetico
  doc["powerSavingEnabled"] = config.powerSavingEnabled;
  doc["powerSavingStartHour"] = config.powerSavingStartHour;
  doc["powerSavingEndHour"] = config.powerSavingEndHour;
  doc["normalUpdateInterval"] = config.normalUpdateInterval;
  doc["powerSavingUpdateInterval"] = config.powerSavingUpdateInterval;
  
  // Impostazioni gestione errori
  doc["maxNetworkRetries"] = config.maxNetworkRetries;
  doc["showLastDataOnError"] = config.showLastDataOnError;
  
  // Serializza il JSON nel file
  if (serializeJson(doc, file) == 0) {
    Serial.println("Errore durante la serializzazione JSON");
    file.close();
    return false;
  }
  
  file.close();
  
  // Verifica che il file sia stato scritto correttamente
  if (!SD.exists(CONFIG_FILE)) {
    Serial.println("Errore: file non trovato dopo il salvataggio");
    return false;
  }
  
  Serial.println("Configurazione salvata con successo");
  return true;
}

// Ripristina la configurazione predefinita
bool resetConfig() {
  DEBUG_TRACE();
  
  Serial.println("Ripristino configurazione predefinita...");
  
  // Reimposta tutti i valori ai default
  strcpy(config.ssid, "");
  strcpy(config.password, "");
  strcpy(config.city, "Rome");
  config.lat = 41.9028;
  config.lon = 12.4964;
  strcpy(config.api_key, "");
  config.gmtOffset_sec = 3600;  // Default: GMT+1
  config.daylightOffset_sec = 3600;  // Default: 1 ora
  strcpy(config.ntpServer, "pool.ntp.org");
  
  // Inizializza le nuove impostazioni per il risparmio energetico
  config.powerSavingEnabled = true;         // Attiva per default
  config.powerSavingStartHour = 22;         // Dalle 22:00
  config.powerSavingEndHour = 7;            // Alle 7:00
  config.normalUpdateInterval = 30;         // Aggiornamento normale ogni 30 minuti
  config.powerSavingUpdateInterval = 120;   // In risparmio energetico ogni 120 minuti (2 ore)
  
  // Inizializza le impostazioni per la gestione degli errori di rete
  config.maxNetworkRetries = 3;            // 3 tentativi in caso di errore
  config.showLastDataOnError = true;        // Mostra l'ultimo dato disponibile in caso di errore
  
  return saveConfig();
}

// Verifica che la configurazione sia valida
bool checkConfigValidity() {
  // Verifica che i campi essenziali siano valorizzati
  bool isValid = true;
  
  // Verifica delle impostazioni di risparmio energetico
  if (config.powerSavingStartHour < 0 || config.powerSavingStartHour > 23) {
    config.powerSavingStartHour = 22; // Default
    isValid = false;
  }
  
  if (config.powerSavingEndHour < 0 || config.powerSavingEndHour > 23) {
    config.powerSavingEndHour = 7; // Default
    isValid = false;
  }
  
  if (config.normalUpdateInterval < 5) {
    config.normalUpdateInterval = 30; // Default
    isValid = false;
  }
  
  if (config.powerSavingUpdateInterval < 10) {
    config.powerSavingUpdateInterval = 120; // Default
    isValid = false;
  }
  
  return isValid;
}
