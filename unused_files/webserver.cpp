/*
 * AtmoVerse 2.0 - Implementazione Server Web
 * 
 * Questo file contiene la logica per il server web utilizzato nel progetto AtmoVerse.
 * Ottimizzato per la memoria dell'ESP32, utilizza la libreria ESPAsyncWebServer per gestire le richieste HTTP.
 */

#include "webserver.h"
#include "config.h"
#include "html_content.h"
#include "quotes.h"
#include "display.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <SD.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <WebServer.h>  // File di costanti HTTP personalizzato
#include "http_constants.h"  

// Istanze globali
AsyncWebServer server(80);  // Istanza del server web sulla porta 80

/*
 * Configura il server web con i gestori di rotte
 * Questa funzione imposta le rotte principali e i gestori di callback per le richieste HTTP.
 */
void setupWebServer() {
  // Configura le rotte principali con gestori di callback ottimizzati
  server.on("/", HTTP_GET, handleRoot);  // Gestore per la pagina principale
  server.on("/settings", HTTP_GET, handleSettings);  // Gestore per la pagina delle impostazioni
  server.on("/quotes", HTTP_GET, handleQuotes);  // Gestore per la pagina delle citazioni
  server.on("/save_settings", HTTP_POST, handleSaveSettings);  // Gestore per salvare le impostazioni
  server.on("/save_quotes", HTTP_POST, handleSaveQuotes);  // Gestore per salvare le citazioni

  // Gestore per le rotte non trovate - ottimizzato per consumo memoria
  server.onNotFound([](AsyncWebServerRequest *request) {
    String path = request->url();
    if (handleFileRead(path)) {  // Verifica se il file esiste
      String contentType = getContentType(path);
      request->send(SD, path, contentType);  // Invia il file richiesto
    } else {
      request->send(404, F("text/plain"), F("Not Found"));  // Risposta per errore 404
    }
  });
}

/*
 * Avvia il server web
 * Questa funzione avvia il server web e stampa un messaggio di conferma.
 */
void startWebServer() {
  server.begin();
  Serial.println(F("Server web avviato"));
}

/*
 * Verifica l'esistenza di un file
 * Questa funzione controlla se un file esiste sulla SD card.
 * @param path: Il percorso del file da verificare
 * @return: true se il file esiste, false altrimenti
 */
bool handleFileRead(String path) {
  if (path.endsWith("/")) path += F("index.html");  // Aggiunge index.html se il percorso è una directory
  return SD.exists(path);  // Verifica l'esistenza del file
}

/*
 * Determina il tipo di contenuto di un file
 * Questa funzione restituisce il tipo MIME in base all'estensione del file.
 * @param filename: Il nome del file
 * @return: Il tipo MIME del file
 */
String getContentType(String filename) {
  if (filename.endsWith(".html")) return F("text/html");
  else if (filename.endsWith(".css")) return F("text/css");
  else if (filename.endsWith(".js")) return F("application/javascript");
  else if (filename.endsWith(".ico")) return F("image/x-icon");
  else if (filename.endsWith(".json")) return F("application/json");
  else if (filename.endsWith(".png")) return F("image/png");
  else if (filename.endsWith(".jpg")) return F("image/jpeg");
  else if (filename.endsWith(".gif")) return F("image/gif");
  else if (filename.endsWith(".txt")) return F("text/plain");
  return F("application/octet-stream");  // Tipo predefinito
}

/*
 * Funzione per ottenere una stringa da PROGMEM
 * Questa funzione converte una stringa memorizzata in PROGMEM in una String.
 * @param str: La stringa in PROGMEM
 * @return: La stringa convertita
 */
String PROGMEM_getstring(const char* str) {
  return String(str);
}

/*
 * Avvia la modalità Access Point
 * Questa funzione configura l'ESP32 come Access Point e avvia il DNS server per il captive portal.
 */
void startAPMode() {
  Serial.println(F("Avvio modalità AP"));
  inAPMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP("AtmoVerse", "");
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
}

/*
 * Gestisce la richiesta della pagina principale
 * Questa funzione genera la pagina HTML principale con i dati meteo e le citazioni.
 * @param request: La richiesta HTTP
 */
void handleRoot(AsyncWebServerRequest *request) {
  String html = PROGMEM_getstring(MAIN_PAGE);
  // Sostituzioni per i dati meteo e le citazioni
  html.replace(F("%CITY%"), currentWeather.cityName);
  html.replace(F("%COUNTRY%"), currentWeather.countryCode);
  html.replace(F("%TEMPERATURE%"), String(currentWeather.temperature, 1));
  html.replace(F("%HUMIDITY%"), String(currentWeather.humidity, 1));
  html.replace(F("%WIND_SPEED%"), String(currentWeather.windSpeed, 1));
  html.replace(F("%PRESSURE%"), String(currentWeather.pressure, 0));
  html.replace(F("%WEATHER_CONDITION%"), currentWeather.weatherCondition);
  const char* weatherIcon = "cloud";
  if (currentWeather.weatherId >= 200 && currentWeather.weatherId < 300) weatherIcon = "flash_on";
  else if (currentWeather.weatherId >= 300 && currentWeather.weatherId < 600) weatherIcon = "water";
  else if (currentWeather.weatherId >= 600 && currentWeather.weatherId < 700) weatherIcon = "ac_unit";
  else if (currentWeather.weatherId >= 700 && currentWeather.weatherId < 800) weatherIcon = "dehaze";
  else if (currentWeather.weatherId == 800) weatherIcon = "wb_sunny";
  html.replace(F("%WEATHER_ICON%"), weatherIcon);
  String quote = getWeatherQuote(currentWeather.weatherCategory);
  html.replace(F("%QUOTE_CONTENT%"), quote);
  html.replace(F("%QUOTE_CATEGORY%"), currentWeather.weatherCategory);
  html.replace(F("%WIFI_STATUS%"), WiFi.status() == WL_CONNECTED ? F("Connesso") : F("Disconnesso"));
  html.replace(F("%IP_ADDRESS%"), WiFi.localIP().toString());
  html.replace(F("%FREE_HEAP%"), String(ESP.getFreeHeap()));
  html.replace(F("%LAST_UPDATE%"), currentWeather.lastUpdate);
  request->send(200, F("text/html"), html);
  html = String();  // Libera la memoria
}

/*
 * Gestisce la richiesta della pagina delle impostazioni
 * Questa funzione genera la pagina HTML per le impostazioni.
 * @param request: La richiesta HTTP
 */
void handleSettings(AsyncWebServerRequest *request) {
  String html = PROGMEM_getstring(SETTINGS_PAGE);
  html.replace(F("%SSID%"), ssid);
  html.replace(F("%PASSWORD%"), password);
  html.replace(F("%API_KEY%"), owm_api_key);
  html.replace(F("%CITY%"), owm_city);
  html.replace(F("%COUNTRY%"), owm_country);
  html.replace(F("%TIMEZONE%"), timezone);
  html.replace(F("%DST_CHECKED%"), dst_enabled ? F("checked") : F(""));
  request->send(200, F("text/html"), html);
  html = String();  // Libera la memoria
}

/*
 * Gestisce la richiesta della pagina delle citazioni
 * Questa funzione genera la pagina HTML per le citazioni.
 * @param request: La richiesta HTTP
 */
void handleQuotes(AsyncWebServerRequest *request) {
  String html = PROGMEM_getstring(QUOTES_PAGE);
  if (customQuotes.clearSkyCount > 0) html.replace(F("%CLEAR_QUOTE_1%"), customQuotes.clearSky[0]);
  else html.replace(F("%CLEAR_QUOTE_1%"), F(""));
  if (customQuotes.brokenCloudsCount > 0) html.replace(F("%CLOUDS_QUOTE_1%"), customQuotes.brokenClouds[0]);
  else html.replace(F("%CLOUDS_QUOTE_1%"), F(""));
  if (customQuotes.rainCount > 0) html.replace(F("%RAIN_QUOTE_1%"), customQuotes.rain[0]);
  else html.replace(F("%RAIN_QUOTE_1%"), F(""));
  if (customQuotes.snowCount > 0) html.replace(F("%SNOW_QUOTE_1%"), customQuotes.snow[0]);
  else html.replace(F("%SNOW_QUOTE_1%"), F(""));
  if (customQuotes.thunderstormCount > 0) html.replace(F("%THUNDERSTORM_QUOTE_1%"), customQuotes.thunderstorm[0]);
  else html.replace(F("%THUNDERSTORM_QUOTE_1%"), F(""));
  request->send(200, F("text/html"), html);
  html = String();  // Libera la memoria
}

/*
 * Gestisce la richiesta di salvataggio delle impostazioni
 * Questa funzione salva le nuove impostazioni inviate dal client.
 * @param request: La richiesta HTTP
 */
void handleSaveSettings(AsyncWebServerRequest *request) {
  bool paramsChanged = false;
  
  // Utilizziamo variabili temporanee per evitare allocazioni multiple
  String newValue;
  
  if (request->hasParam("ssid", true)) {
    newValue = request->getParam("ssid", true)->value();
    if (strncmp(newValue.c_str(), ssid, sizeof(ssid)) != 0) {
      strncpy(ssid, newValue.c_str(), sizeof(ssid) - 1);
      ssid[sizeof(ssid) - 1] = '\0';
      paramsChanged = true;
    }
  }
  
  if (request->hasParam("password", true)) {
    newValue = request->getParam("password", true)->value();
    if (strncmp(newValue.c_str(), password, sizeof(password)) != 0) {
      strncpy(password, newValue.c_str(), sizeof(password) - 1);
      password[sizeof(password) - 1] = '\0';
      paramsChanged = true;
    }
  }
  
  if (request->hasParam("api_key", true)) {
    newValue = request->getParam("api_key", true)->value();
    if (strncmp(newValue.c_str(), owm_api_key, sizeof(owm_api_key)) != 0) {
      strncpy(owm_api_key, newValue.c_str(), sizeof(owm_api_key) - 1);
      owm_api_key[sizeof(owm_api_key) - 1] = '\0';
      paramsChanged = true;
    }
  }
  
  if (request->hasParam("city", true)) {
    newValue = request->getParam("city", true)->value();
    if (strncmp(newValue.c_str(), owm_city, sizeof(owm_city)) != 0) {
      strncpy(owm_city, newValue.c_str(), sizeof(owm_city) - 1);
      owm_city[sizeof(owm_city) - 1] = '\0';
      paramsChanged = true;
    }
  }
  
  if (request->hasParam("country", true)) {
    newValue = request->getParam("country", true)->value();
    if (strncmp(newValue.c_str(), owm_country, sizeof(owm_country)) != 0) {
      strncpy(owm_country, newValue.c_str(), sizeof(owm_country) - 1);
      owm_country[sizeof(owm_country) - 1] = '\0';
      paramsChanged = true;
    }
  }
  
  if (request->hasParam("timezone", true)) {
    newValue = request->getParam("timezone", true)->value();
    if (strncmp(newValue.c_str(), timezone, sizeof(timezone)) != 0) {
      strncpy(timezone, newValue.c_str(), sizeof(timezone) - 1);
      timezone[sizeof(timezone) - 1] = '\0';
      paramsChanged = true;
    }
  }
  
  if (request->hasParam("dst_enabled", true)) {
    bool newDst = true;
    if (newDst != dst_enabled) {
      dst_enabled = newDst;
      paramsChanged = true;
    }
  } else {
    bool newDst = false;
    if (newDst != dst_enabled) {
      dst_enabled = newDst;
      paramsChanged = true;
    }
  }
  
  // Salva i parametri se sono cambiati
  if (paramsChanged) {
    saveConfig();
    
    // Se in modalità AP e sono stati configurati SSID e password, riavvia
    if (inAPMode && strlen(ssid) > 0 && strlen(password) > 0) {
      request->send(200, F("text/html"), F("<html><body><h1>Configurazione salvata</h1><p>Il dispositivo si riavvierà...</p></body></html>"));
      delay(3000);
      ESP.restart();
      return;
    }
  }
  
  // Reindirizza alla pagina delle impostazioni
  request->redirect("/settings");
}

/*
 * Gestisce la richiesta di salvataggio delle citazioni
 * Questa funzione salva le nuove citazioni inviate dal client.
 * @param request: La richiesta HTTP
 */
void handleSaveQuotes(AsyncWebServerRequest *request) {
  if (request->hasParam("clear_quote_1", true)) {
    String newValue = request->getParam("clear_quote_1", true)->value();
    strncpy(customQuotes.clearSky[0], newValue.c_str(), QUOTE_BUFFER_SIZE - 1);
    customQuotes.clearSky[0][QUOTE_BUFFER_SIZE - 1] = '\0';
  }
  if (request->hasParam("clouds_quote_1", true)) {
    String newValue = request->getParam("clouds_quote_1", true)->value();
    strncpy(customQuotes.brokenClouds[0], newValue.c_str(), QUOTE_BUFFER_SIZE - 1);
    customQuotes.brokenClouds[0][QUOTE_BUFFER_SIZE - 1] = '\0';
  }
  if (request->hasParam("rain_quote_1", true)) {
    String newValue = request->getParam("rain_quote_1", true)->value();
    strncpy(customQuotes.rain[0], newValue.c_str(), QUOTE_BUFFER_SIZE - 1);
    customQuotes.rain[0][QUOTE_BUFFER_SIZE - 1] = '\0';
  }
  if (request->hasParam("snow_quote_1", true)) {
    String newValue = request->getParam("snow_quote_1", true)->value();
    strncpy(customQuotes.snow[0], newValue.c_str(), QUOTE_BUFFER_SIZE - 1);
    customQuotes.snow[0][QUOTE_BUFFER_SIZE - 1] = '\0';
  }
  if (request->hasParam("thunderstorm_quote_1", true)) {
    String newValue = request->getParam("thunderstorm_quote_1", true)->value();
    strncpy(customQuotes.thunderstorm[0], newValue.c_str(), QUOTE_BUFFER_SIZE - 1);
    customQuotes.thunderstorm[0][QUOTE_BUFFER_SIZE - 1] = '\0';
  }
  saveConfig();
  request->send(200, "text/plain", "Quotes saved successfully");
}

/*
 * Salva la configurazione su SD
 * Questa funzione salva le impostazioni e le citazioni su una file JSON sulla SD card.
 */
void saveConfig() {
  if (!SD.exists("/config")) {
    SD.mkdir("/config");
  }
  
  // Rimuovi file esistente
  if (SD.exists("/config.json")) {
    SD.remove("/config.json");
  }
  
  File configFile = SD.open("/config.json", FILE_WRITE);
  if (!configFile) {
    Serial.println(F("Errore apertura config.json"));
    return;
  }
  
  // Utilizzo una capacità ridotta per il documento JSON
  StaticJsonDocument<384> doc;
  
  // Aggiungi solo i parametri necessari
  doc["ssid"] = ssid;
  doc["password"] = password;
  doc["owm_api_key"] = owm_api_key;
  doc["owm_city"] = owm_city;
  doc["owm_country"] = owm_country;
  doc["timezone"] = timezone;
  doc["dst_enabled"] = dst_enabled;
  
  if (serializeJson(doc, configFile) == 0) {
    Serial.println(F("Errore scrittura config.json"));
  }
  
  configFile.close();
}

/*
 * Carica la configurazione da SD
 * Questa funzione carica le impostazioni e le citazioni da un file JSON sulla SD card.
 */
void loadConfig() {
  File configFile = SD.open("/config.json", FILE_READ);
  if (!configFile) {
    Serial.println(F("config.json non trovato"));
    return;
  }
  
  // Utilizzo una capacità ridotta per il documento JSON
  StaticJsonDocument<384> doc;
  
  DeserializationError error = deserializeJson(doc, configFile);
  if (error) {
    Serial.println(F("Errore parsing config.json"));
    configFile.close();
    return;
  }
  
  // Carica i parametri con controlli di sicurezza
  if (doc.containsKey("ssid")) {
    strncpy(ssid, doc["ssid"], sizeof(ssid) - 1);
    ssid[sizeof(ssid) - 1] = '\0';
  }
  
  if (doc.containsKey("password")) {
    strncpy(password, doc["password"], sizeof(password) - 1);
    password[sizeof(password) - 1] = '\0';
  }
  
  if (doc.containsKey("owm_api_key")) {
    strncpy(owm_api_key, doc["owm_api_key"], sizeof(owm_api_key) - 1);
    owm_api_key[sizeof(owm_api_key) - 1] = '\0';
  }
  
  if (doc.containsKey("owm_city")) {
    strncpy(owm_city, doc["owm_city"], sizeof(owm_city) - 1);
    owm_city[sizeof(owm_city) - 1] = '\0';
  }
  
  if (doc.containsKey("owm_country")) {
    strncpy(owm_country, doc["owm_country"], sizeof(owm_country) - 1);
    owm_country[sizeof(owm_country) - 1] = '\0';
  }
  
  if (doc.containsKey("timezone")) {
    strncpy(timezone, doc["timezone"], sizeof(timezone) - 1);
    timezone[sizeof(timezone) - 1] = '\0';
  }
  
  if (doc.containsKey("dst_enabled")) {
    dst_enabled = doc["dst_enabled"];
  }
  
  configFile.close();
}
