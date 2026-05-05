#include "WebServer.h"
#include "NetworkUtils.h"
#include "Config.h"
#include "WeatherUtils.h"
#include "WebUIPages.h"
#include "WebMinimal.h"
#include "Hardware.h"
#include "TimerUtils.h"
#include "SystemMonitor.h"
#include "DebugUtils.h"
#include "WebServerOpt.h" // Include file per ottimizzazioni
#include <SD.h>
#include <ArduinoJson.h>
#include <WiFi.h>

// Messaggi di risposta JSON in memoria PROGMEM
static const char JSON_SUCCESS_MSG[] PROGMEM = "{\"success\":true,\"message\":\"Configurazione salvata con successo. Il dispositivo si riavvierà automaticamente.\"}"; 
static const char JSON_ERROR_MSG[] PROGMEM = "{\"success\":false,\"message\":\"Errore nel salvataggio della configurazione\"}"; 
static const char JSON_NO_DATA_MSG[] PROGMEM = "{\"success\":false,\"message\":\"Nessun dato ricevuto\"}";

// Stringhe HTML comuni in PROGMEM
static const char HTTP_200_OK[] PROGMEM = "HTTP/1.1 200 OK";
static const char HTTP_302_FOUND[] PROGMEM = "HTTP/1.1 302 Found";
static const char HTTP_301_MOVED[] PROGMEM = "HTTP/1.1 301 Moved Permanently";
static const char HTTP_404_NOT_FOUND[] PROGMEM = "HTTP/1.1 404 Not Found";
static const char CONTENT_TYPE_HTML[] PROGMEM = "Content-Type: text/html";
static const char CONTENT_TYPE_JSON[] PROGMEM = "Content-Type: application/json";
static const char CONTENT_TYPE_PLAIN[] PROGMEM = "Content-Type: text/plain";
static const char CONNECTION_CLOSE[] PROGMEM = "Connection: close";
static const char LOCATION_SETTINGS[] PROGMEM = "Location: /www/settings.html";
static const char CAPTIVE_PORTAL_REDIRECT[] PROGMEM = "<html><head><meta http-equiv='refresh' content='0;url=/'></head><body></body></html>";

// Funzione per decodificare l'URL (traduce caratteri come %20 in spazi)
void urldecode(const char* str, char* result, size_t resultSize) {
  size_t i, j = 0;
  size_t len = strlen(str);
  
  for (i = 0; i < len && j < resultSize - 1; i++) {
    if (str[i] == '+') {
      result[j++] = ' ';
    } else if (str[i] == '%' && i + 2 < len) {
      int code = 0;
      if (str[i+1] >= '0' && str[i+1] <= '9') {
        code = (str[i+1] - '0') << 4;
      } else if (str[i+1] >= 'A' && str[i+1] <= 'F') {
        code = (10 + str[i+1] - 'A') << 4;
      } else if (str[i+1] >= 'a' && str[i+1] <= 'f') {
        code = (10 + str[i+1] - 'a') << 4;
      }
      
      if (str[i+2] >= '0' && str[i+2] <= '9') {
        code += (str[i+2] - '0');
      } else if (str[i+2] >= 'A' && str[i+2] <= 'F') {
        code += (10 + str[i+2] - 'A');
      } else if (str[i+2] >= 'a' && str[i+2] <= 'f') {
        code += (10 + str[i+2] - 'a');
      }
      
      result[j++] = (char)code;
      i += 2;
    } else {
      result[j++] = str[i];
    }
  }
  result[j] = '\0';
}

// Istanza del server
WiFiServer server(80);

// Inizializza il server
void setupServer() {
  // Verifica che la cartella www esista sulla SD
  if (!SD.exists("/www")) {
    DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_ERROR, "Impossibile avviare WebServer: cartella /www non trovata");
    SystemMonitor::recordError(COMPONENT_WEB_SERVER, "Cartella /www non trovata");
    return;
  }
  
  // Avvia il server
  server.begin();
  DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_INFO, "Web server avviato sulla porta 80");
  SystemMonitor::recordSuccess(COMPONENT_WEB_SERVER);
}

// Determina il tipo di contenuto in base all'estensione del file
void getContentType(const char* filename, char* result, size_t resultSize) {
  size_t len = strlen(filename);
  
  if (len > 5 && strcmp(filename + len - 5, ".html") == 0) {
    strlcpy(result, "text/html", resultSize);
  } else if (len > 4 && strcmp(filename + len - 4, ".css") == 0) {
    strlcpy(result, "text/css", resultSize);
  } else if (len > 3 && strcmp(filename + len - 3, ".js") == 0) {
    strlcpy(result, "application/javascript", resultSize);
  } else if (len > 4 && strcmp(filename + len - 4, ".png") == 0) {
    strlcpy(result, "image/png", resultSize);
  } else if (len > 4 && (strcmp(filename + len - 4, ".jpg") == 0 || strcmp(filename + len - 5, ".jpeg") == 0)) {
    strlcpy(result, "image/jpeg", resultSize);
  } else if (len > 4 && strcmp(filename + len - 4, ".ico") == 0) {
    strlcpy(result, "image/x-icon", resultSize);
  } else if (len > 4 && strcmp(filename + len - 4, ".svg") == 0) {
    strlcpy(result, "image/svg+xml", resultSize);
  } else if (len > 5 && strcmp(filename + len - 5, ".json") == 0) {
    strlcpy(result, "application/json", resultSize);
  } else {
    strlcpy(result, "text/plain", resultSize);
  }
}

// Flag per verificare se la SD è già inizializzata
bool sdCardInitialized = false;

// Inizializza la SD card se non è già inizializzata
bool ensureSDCardInitialized() {
  if (!sdCardInitialized) {
    // Aumentiamo la frequenza del bus SPI per migliorare le prestazioni
    sdSPI.setFrequency(25000000); // 25 MHz è un buon compromesso per la maggior parte delle SD card
    
    if (SD.begin(SD_CS, sdSPI)) {
      sdCardInitialized = true;
    } else {
      return false;
    }
  }
  return true;
}

// Serve un file dalla SD card con prestazioni ottimizzate
bool serveFileFromSD(WiFiClient& client, const char* pathParam) {
  // Verifica se la SD è presente e accessibile
  if (!ensureSDCardInitialized()) {
    DEBUG_LOG(DEBUG_CATEGORY_SD, DEBUG_LEVEL_ERROR, "Errore nell'inizializzazione della SD card durante serveFileFromSD");
    SystemMonitor::recordError(COMPONENT_SD_CARD, "SD non inizializzata");
    return false;
  }
  
  char path[64]; // Buffer ridotto per il percorso
  strlcpy(path, pathParam, sizeof(path));
  
  // Gestione dei percorsi
  size_t pathLen = strlen(path);
  if (pathLen > 0 && path[pathLen - 1] == '/') {
    if (pathLen + 11 < sizeof(path)) { // 11 = lunghezza di "index.html"+1
      strlcat(path, "index.html", sizeof(path));
    } else {
      return false; // Path troppo lungo
    }
  }
  
  // Non mostrare i file nascosti
  if (strstr(path, "/.") != NULL) return false;
  
  // Aggiungi prefisso /www al percorso se necessario
  char fullPath[48]; // Buffer ulteriormente ridotto per risparmiare DRAM
  if (strncmp(path, "/www", 4) != 0) {
    strlcpy(fullPath, "/www", sizeof(fullPath));
    strlcat(fullPath, path, sizeof(fullPath));
  } else {
    strlcpy(fullPath, path, sizeof(fullPath));
  }
  
  // Verifica se il file esiste
  if (!SD.exists(fullPath)) {
    DEBUG_LOG(DEBUG_CATEGORY_SD, DEBUG_LEVEL_WARNING, "File non trovato: %s", fullPath);
    SystemMonitor::recordError(COMPONENT_WEB_SERVER, "File richiesto non trovato");
    return false;
  }
  
  // Apri il file
  File file = SD.open(fullPath, FILE_READ);
  if (!file) {
    DEBUG_LOG(DEBUG_CATEGORY_SD, DEBUG_LEVEL_ERROR, "Impossibile aprire il file: %s", fullPath);
    SystemMonitor::recordError(COMPONENT_SD_CARD, "Errore apertura file");
    return false;
  }
  
  // Registra un successo nella lettura della SD
  SystemMonitor::recordSuccess(COMPONENT_SD_CARD);
  
  // Determina il tipo di contenuto
  char contentType[32]; // Buffer ridotto per il content type
  getContentType(fullPath, contentType, sizeof(contentType));
  
  // Ottieni la dimensione del file
  size_t fileSize = file.size();
  
  // Invia l'header della risposta
  client.println(F("HTTP/1.1 200 OK"));
  client.print(F("Content-Type: "));
  client.println(contentType);
  client.print(F("Content-Length: "));
  client.println(fileSize);
  client.println(F("Connection: close"));
  client.println();
  
  // Buffer per la lettura del file
  uint8_t buffer[256]; // Buffer ridotto a 256B per minimizzare l'uso della DRAM
  
  // Invia il file usando il buffer
  while (file.available()) {
    size_t bytesRead = file.read(buffer, sizeof(buffer));
    if (bytesRead > 0) {
      client.write(buffer, bytesRead);
    }
  }
  
  // Chiudi il file
  file.close();
  return true;
}

// Gestione delle richieste dei client - include DNS e web server
void handleClientRequests() {
  // Gestisci il DNS server per captive portal se in modalità AP
  if (apMode) {
    dnsServer.processNextRequest();
  }
  
  // Processa le richieste web
  processWebRequests();
}

// Versione ottimizzata che gestisce solo il web server (senza DNS)
void handleWebServerOnly() {
  // Processa solo le richieste web, senza gestire il DNS
  processWebRequests();
}

// Funzione comune per gestire le richieste web (estratta per evitare duplicazione)
void processWebRequests() {
  // Verifica se ci sono client che si connettono
  WiFiClient client = server.available();
  if (!client) {
    return;
  }
  
  // Registra una nuova richiesta web in arrivo
  static unsigned long requestCount = 0;
  requestCount++;
  DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_INFO, "Richiesta web #%lu ricevuta", requestCount);
  
  // Timeout per la richiesta
  unsigned long timeout = millis() + 5000;
  while (!client.available() && millis() < timeout) {
    // Utilizzo di yield() invece di delay(10) per permettere al sistema di gestire altri task
    // in un approccio completamente non bloccante sarebbe implementato con una macchina a stati
    yield();
  }
  
  // Se non ci sono dati disponibili, chiudi la connessione
  if (!client.available()) {
    DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_WARNING, "Timeout nella richiesta web - nessun dato ricevuto");
    SystemMonitor::recordError(COMPONENT_WEB_SERVER, "Timeout richiesta");
    client.stop();
    return;
  }
  
  // Registra un successo nella connessione web
  SystemMonitor::recordSuccess(COMPONENT_WEB_SERVER);
  
  // Buffer per metodo, path e parametri
  char methodBuffer[10];
  char pathBuffer[48]; // Ridotto da 64 per risparmiare DRAM
  char paramsBuffer[96] = ""; // Ridotto da 128 per risparmiare DRAM
  bool fileServed = false;
  
  // Leggi la prima riga della richiesta
  int index = 0;
  while (client.available()) {
    char c = client.read();
    if (c == ' ' || c == '\n' || c == '\r') {
      if (index == 0) {
        // Fine del metodo
        methodBuffer[index] = '\0';
        index = 0;
      } else if (index > 0) {
        // Fine del path
        pathBuffer[index] = '\0';
        break;
      }
    } else if (index == 0) {
      // Metodo
      if (index < sizeof(methodBuffer) - 1) {
        methodBuffer[index++] = c;
      }
    } else {
      // Path
      if (index < sizeof(pathBuffer) - 1) {
        pathBuffer[index++] = c;
      }
    }
  }
  
  // Cerca parametri nell'URL
  char* qMark = strchr(pathBuffer, '?');
  if (qMark != NULL) {
    size_t paramsLen = strlen(qMark + 1);
    if (paramsLen >= sizeof(paramsBuffer)) paramsLen = sizeof(paramsBuffer) - 1;
    memcpy(paramsBuffer, qMark + 1, paramsLen);
    paramsBuffer[paramsLen] = '\0';
    *qMark = '\0'; // Tronca il path all'interrogativo
  }


  // Leggi le altre intestazioni e scartale
  while (client.available()) {
    char c = client.read();
    if (c == '\r') {
      client.read(); // Consume '\n'
      char next = client.peek();
      if (next == '\r') {
        client.read(); // \r
        client.read(); // \n
        break; // End of headers
      }
    }
  }
  
  // Gestione captive portal per Android e iOS (solo in modalità AP)
  if (apMode && (strstr(pathBuffer, "generate_204") != NULL || strstr(pathBuffer, "gen_204") != NULL || 
      strstr(pathBuffer, "/chat") != NULL || strstr(pathBuffer, "/connectivitycheck") != NULL ||
      strstr(pathBuffer, "/hotspot-detect") != NULL)) {
    // Risposta per il captive portal
    client.println(FPSTR(HTTP_200_OK));
    client.println(FPSTR(CONTENT_TYPE_HTML));
    client.println(FPSTR(CONNECTION_CLOSE));
    client.println();
    client.println(FPSTR(CAPTIVE_PORTAL_REDIRECT));
    client.stop();
    Serial.print(F("Captive portal richiesto per path: "));
    Serial.println(pathBuffer);
    return;
  }
  
  // API per il meteo
  if (strcmp(pathBuffer, "/api/weather") == 0 && strcmp(methodBuffer, "GET") == 0) {
    DynamicJsonDocument doc(1024); // Ridotto da 2048 a 1024 per minimizzare l'uso della DRAM
    
    extern WeatherData currentWeather;
    
    // Struttura del JSON come la richiede il frontend
    JsonObject weather = doc["weather"].to<JsonObject>();
    JsonObject location = doc["location"].to<JsonObject>();
    JsonObject time = doc["time"].to<JsonObject>();
    
    // Dati località
    location["city"] = config.city;
    location["country"] = "Italia";
    
    // Timestamp e ora locale
    struct tm timeinfo;
    getLocalTime(&timeinfo);
    char localTimeStr[24]; // Ridotto da 30 a 24 byte per risparmiare DRAM
    strftime(localTimeStr, sizeof(localTimeStr), "%H:%M - %d/%m/%Y", &timeinfo);
    time["local"] = localTimeStr;
    
    // Limita gli aggiornamenti dei dati meteo a massimo uno ogni 5 minuti
    static unsigned long lastMeteoCheckTime = 0;
    unsigned long currentTime = millis();
    
    if (!isWeatherDataValid() && (currentTime - lastMeteoCheckTime > 5 * 60 * 1000)) {
      Serial.println("[WebServer] Aggiornamento dati meteo via web dopo verifica temporale");
      lastMeteoCheckTime = currentTime;
      getWeatherData();
    }
    
    // Dati meteo con struttura adatta al frontend
    weather["temperature"] = currentWeather.temp > 0 ? currentWeather.temp : 15.0;
    weather["feels_like"] = currentWeather.feels_like > 0 ? currentWeather.feels_like : 14.0;
    weather["humidity"] = currentWeather.humidity > 0 ? currentWeather.humidity : 60;
    weather["pressure"] = currentWeather.pressure > 0 ? currentWeather.pressure : 1013;
    weather["wind_speed"] = currentWeather.wind_speed;
    weather["wind_deg"] = currentWeather.wind_deg;
    
    // Condizione meteo
    weather["condition"] = strlen(currentWeather.description) > 0 ? 
                          currentWeather.description : "Nuvole Sparse";
    weather["icon"] = strlen(currentWeather.icon) > 0 ? 
                    currentWeather.icon : "03d";
      
    // Timestamp ultimo aggiornamento
    char lastUpdateStr[24]; // Ridotto da 30 a 24 byte per risparmiare DRAM
    struct tm lastUpdateTime;
    localtime_r(&currentWeather.last_update, &lastUpdateTime);
    strftime(lastUpdateStr, sizeof(lastUpdateStr), "%H:%M - %d/%m/%Y", &lastUpdateTime);
    weather["last_update"] = lastUpdateStr;
    
    weather["valid"] = true;
    
    String json;
    serializeJson(doc, json);
    
    sendJsonResponse(client, json.c_str());
    return;
  }
  
  // Reindirizzamento alla pagina delle impostazioni se la URL è la root
  if (strcmp(pathBuffer, "/") == 0 || strlen(pathBuffer) == 0) {
    // Reindirizza alla pagina delle impostazioni
    client.println(FPSTR(HTTP_302_FOUND));
    client.println(FPSTR(LOCATION_SETTINGS));
    client.println(FPSTR(CONNECTION_CLOSE));
    client.println();
    client.stop();
    Serial.println(F("Reindirizzamento a /www/settings.html"));
    return;
  }
  
  // Gestione richieste per /setup (compatibilità con vecchi collegamenti)
  if (strcmp(pathBuffer, "/setup") == 0) {
    client.println(FPSTR(HTTP_301_MOVED));
    client.println(FPSTR(LOCATION_SETTINGS));
    client.println(FPSTR(CONNECTION_CLOSE));
    client.println();
    client.stop();
    return;
  }
  
  // Gestione chiamata API per scansione WiFi (comune a entrambe le modalità)
  if (strcmp(pathBuffer, "/api/wifi-scan") == 0) {
    performWiFiScan(client);
    return;
  }
  
  // API per il monitoraggio dello stato del sistema
  if (strcmp(pathBuffer, "/api/system-status") == 0 && strcmp(methodBuffer, "GET") == 0) {
    DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_INFO, "API: Richiesta stato sistema");
    SystemMonitor::recordSuccess(COMPONENT_WEB_SERVER);
    
    // Creo documento JSON per la risposta
    DynamicJsonDocument doc(768);
    
    // Info generali di sistema
    doc["uptime"] = millis() / 1000; // in secondi
    doc["heap_free"] = ESP.getFreeHeap();
    
    // Stato dei componenti monitorati
    JsonObject components = doc.createNestedObject("components");
    
    // Aggiungo lo stato di ogni componente
    for (int i = 0; i < COMPONENT_COUNT; i++) {
      SystemComponent comp = static_cast<SystemComponent>(i);
      components[SystemMonitor::getComponentName(comp)] = SystemMonitor::isComponentStable(comp) ? "OK" : "INSTABILE";
    }
    
    // Stato generale del sistema
    SystemState state = SystemMonitor::getSystemState();
    doc["system_state"] = SystemMonitor::getStateName(state);
    
    // Invia risposta
    String jsonResponse;
    serializeJson(doc, jsonResponse);
    
    client.println(F("HTTP/1.1 200 OK"));
    client.println(F("Content-Type: application/json"));
    client.println(F("Connection: close"));
    client.println();
    client.print(jsonResponse);
    client.stop();
    return;
  }
  
  // API per recuperare le impostazioni attuali
  if (strcmp(pathBuffer, "/api/settings") == 0 && strcmp(methodBuffer, "GET") == 0) {
    DynamicJsonDocument doc(512); // Ridotto da 1024 a 512 per minimizzare l'uso della DRAM
    
    // Inserisci tutte le impostazioni nel documento JSON
    doc["ssid"] = config.ssid;
    doc["city"] = config.city;
    doc["api_key"] = config.api_key;
    doc["timezone"] = config.gmtOffset_sec / 3600;
    doc["dst"] = config.daylightOffset_sec / 3600;
    
    // Aggiungi parametri di risparmio energetico
    doc["powerSavingEnabled"] = config.powerSavingEnabled;
    doc["powerSavingStartHour"] = config.powerSavingStartHour;
    doc["powerSavingEndHour"] = config.powerSavingEndHour;
    doc["normalUpdateInterval"] = config.normalUpdateInterval;
    doc["powerSavingUpdateInterval"] = config.powerSavingUpdateInterval;
    
    // Aggiungi parametri di gestione errori di rete
    doc["maxNetworkRetries"] = config.maxNetworkRetries;
    doc["showLastDataOnError"] = config.showLastDataOnError;
    
    String json;
    serializeJson(doc, json);
    
    sendJsonResponse(client, json.c_str());
    return;
  }
  
  // API per il meteo
  if (strcmp(pathBuffer, "/api/weather") == 0 && strcmp(methodBuffer, "GET") == 0) {
    DynamicJsonDocument doc(1024); // Ridotto da 2048 a 1024 per minimizzare l'uso della DRAM
    
    extern WeatherData currentWeather;
    
    // Struttura del JSON come la richiede il frontend
    JsonObject weather = doc["weather"].to<JsonObject>();
    JsonObject location = doc["location"].to<JsonObject>();
    JsonObject time = doc["time"].to<JsonObject>();
    
    // Dati località
    location["city"] = config.city;
    location["country"] = "Italia";
    
    // Timestamp e ora locale
    struct tm timeinfo;
    getLocalTime(&timeinfo);
    char localTimeStr[24]; // Ridotto da 30 a 24 byte per risparmiare DRAM
    strftime(localTimeStr, sizeof(localTimeStr), "%H:%M - %d/%m/%Y", &timeinfo);
    time["local"] = localTimeStr;
    
    // Limita gli aggiornamenti dei dati meteo a massimo uno ogni 5 minuti
    static unsigned long lastMeteoCheckTime = 0;
    unsigned long currentTime = millis();
    
    if (!isWeatherDataValid() && (currentTime - lastMeteoCheckTime > 5 * 60 * 1000)) {
      Serial.println("[WebServer] Aggiornamento dati meteo via web dopo verifica temporale");
      lastMeteoCheckTime = currentTime;
      getWeatherData();
    }
    
    // Dati meteo con struttura adatta al frontend
    weather["temperature"] = currentWeather.temp > 0 ? currentWeather.temp : 15.0;
    weather["feels_like"] = currentWeather.feels_like > 0 ? currentWeather.feels_like : 14.0;
    weather["humidity"] = currentWeather.humidity > 0 ? currentWeather.humidity : 60;
    weather["pressure"] = currentWeather.pressure > 0 ? currentWeather.pressure : 1013;
    weather["wind_speed"] = currentWeather.wind_speed;
    weather["wind_deg"] = currentWeather.wind_deg;
    
    // Condizione meteo
    weather["condition"] = strlen(currentWeather.description) > 0 ? 
                          currentWeather.description : "Nuvole Sparse";
    weather["icon"] = strlen(currentWeather.icon) > 0 ? 
                    currentWeather.icon : "03d";
      
    // Timestamp ultimo aggiornamento
    char lastUpdateStr[24]; // Ridotto da 30 a 24 byte per risparmiare DRAM
    struct tm lastUpdateTime;
    localtime_r(&currentWeather.last_update, &lastUpdateTime);
    strftime(lastUpdateStr, sizeof(lastUpdateStr), "%H:%M - %d/%m/%Y", &lastUpdateTime);
    weather["last_update"] = lastUpdateStr;
    
    weather["valid"] = true;
    
    String json;
    serializeJson(doc, json);
    
    client.println(F("HTTP/1.1 200 OK"));
    client.println(F("Content-Type: application/json"));
    client.println(F("Connection: close"));
    client.println();
    serializeJson(doc, client);
    client.stop();
    return;
  }
  
  // Gestione configurazioni
  if (strcmp(pathBuffer, "/api/settings") == 0 && strcmp(methodBuffer, "POST") == 0) {
    DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_INFO, "API: Richiesta modifica configurazioni");
    SystemMonitor::recordSuccess(COMPONENT_WEB_SERVER);
    
    // Leggi il corpo della richiesta
    char jsonBody[384] = ""; // Buffer ulteriormente ridotto per risparmiare DRAM
    size_t jsonLen = 0;
    
    while (client.available() && jsonLen < sizeof(jsonBody) - 1) {
      jsonBody[jsonLen++] = client.read();
    }
    jsonBody[jsonLen] = '\0';
    
    // Verifica che il body non sia vuoto
    if (strlen(jsonBody) == 0) {
      sendJsonResponse(client, FPSTR(JSON_NO_DATA_MSG));
      return;
    }
    
    DynamicJsonDocument doc(512);
    DeserializationError error;
    
    // Controlla se è un formato URL-encoded
    if (strchr(jsonBody, '=') != NULL && strchr(jsonBody, '&') != NULL) {
      // Dividi la stringa in coppie chiave-valore
      char* pairs[10]; // Massimo 10 parametri
      int pairCount = 0;
      char* saveptr;
      char* pair = strtok_r(jsonBody, "&", &saveptr);
      
      // Estrai tutte le coppie separate da &
      while (pair != NULL && pairCount < 10) {
        pairs[pairCount++] = pair;
        pair = strtok_r(NULL, "&", &saveptr);
      }
      
      // Processa ogni coppia chiave-valore
      for (int i = 0; i < pairCount; i++) {
        char* eqPos = strchr(pairs[i], '=');
        if (eqPos != NULL) {
          *eqPos = '\0'; // Divide la stringa in key e value
          char* key = pairs[i];
          char* value = eqPos + 1;
          
          // Decodifica URL-encoded
          char decodedValue[64];
          urldecode(value, decodedValue, sizeof(decodedValue));
          
          // Conversione di tipi in base alla chiave
          if (strcmp(key, "latitude") == 0 || strcmp(key, "longitude") == 0 || strcmp(key, "timezone") == 0 || strcmp(key, "dst") == 0) {
            float numValue = atof(decodedValue);
            doc[key] = numValue;
          } else if (strcmp(key, "darkmode") == 0) {
            doc[key] = (strcmp(decodedValue, "true") == 0 || strcmp(decodedValue, "1") == 0);
          } else {
            doc[key] = decodedValue;
          }
        }
      }
    
    loadConfig();
    
    // Aggiorna i valori dalla request
    if (doc.containsKey("ssid")) {
      strlcpy(config.ssid, doc["ssid"], sizeof(config.ssid));
    }
    
    if (doc.containsKey("password")) {
      strlcpy(config.password, doc["password"], sizeof(config.password));
    }
    
    if (doc.containsKey("api_key")) {
      strlcpy(config.api_key, doc["api_key"], sizeof(config.api_key));
    }
    
    if (doc.containsKey("city")) {
      strlcpy(config.city, doc["city"], sizeof(config.city));
    }
    
    if (doc.containsKey("timezone")) {
      config.gmtOffset_sec = doc["timezone"].as<int>() * 3600;
    }
    
    if (doc.containsKey("dst")) {
      config.daylightOffset_sec = doc["dst"].as<int>() * 3600;
    }
    
    if (doc.containsKey("ntpServer")) {
      strlcpy(config.ntpServer, doc["ntpServer"], sizeof(config.ntpServer));
    }
    
    if (doc.containsKey("powerSavingEnabled")) {
      config.powerSavingEnabled = doc["powerSavingEnabled"].as<bool>();
    }
    
    if (doc.containsKey("powerSavingStartHour")) {
      config.powerSavingStartHour = doc["powerSavingStartHour"].as<int>();
    }
    
    if (doc.containsKey("powerSavingEndHour")) {
      config.powerSavingEndHour = doc["powerSavingEndHour"].as<int>();
    }
    
    if (doc.containsKey("normalUpdateInterval")) {
      config.normalUpdateInterval = doc["normalUpdateInterval"].as<int>();
    }
    
    if (doc.containsKey("powerSavingUpdateInterval")) {
      config.powerSavingUpdateInterval = doc["powerSavingUpdateInterval"].as<int>();
    }
    
    if (doc.containsKey("maxNetworkRetries")) {
      config.maxNetworkRetries = doc["maxNetworkRetries"].as<int>();
    }
    
    if (doc.containsKey("showLastDataOnError")) {
      config.showLastDataOnError = doc["showLastDataOnError"].as<bool>();
    }
    
    // Salva la configurazione aggiornata
    if (saveConfig()) {
      // Registra il successo nel sistema di monitoraggio
      DEBUG_LOG(DEBUG_CATEGORY_CONFIG, DEBUG_LEVEL_INFO, "Configurazione salvata con successo via API");
      SystemMonitor::recordSuccess(COMPONENT_WIFI);
      
      // Invia risposta di successo dalla memoria PROGMEM
      sendJsonResponse(client, FPSTR(JSON_SUCCESS_MSG));
      return;
      }
      
      // Disconnetti il WiFi in modo pulito
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      
      Serial.println(F("Configurazione salvata, riavvio in corso..."));
      
      // Schedula il riavvio dopo un breve ritardo per consentire il completamento di tutte le operazioni
      // Utilizzo Timer invece di delay(500)
      static Timer restartTimer(500, false);
      restartTimer.reset();
      restartTimer.enable();
      while (!restartTimer.isReady()) { yield(); }
      
      ESP.restart();
    } else {
      // Invia risposta di errore dalla memoria PROGMEM
      sendJsonResponse(client, FPSTR(JSON_ERROR_MSG));
    }
    
    return;
  }
  
  // Gestione specifica per la pagina /info
  if (strcmp(pathBuffer, "/info") == 0 && strcmp(methodBuffer, "GET") == 0) {
    // Utilizziamo direttamente il percorso info.html per coerenza
    serveFileFromSD(client, "/info.html");
    return;
  }
  
  // Verifica se il path richiesto contiene già /www/
  if (strncmp(pathBuffer, "/www/", 5) != 0 && strncmp(pathBuffer, "/api/", 5) != 0) {
    // Aggiungi /www/ per cercare i file nella giusta directory
    char newPath[96]; // Ridotto da 128 per risparmiare DRAM
    snprintf(newPath, sizeof(newPath), "/www%s", pathBuffer);
    strlcpy(pathBuffer, newPath, sizeof(pathBuffer));
    Serial.print(F("Path modificato: "));
    Serial.println(pathBuffer);
  }
  
  // Tenta di servire il file dalla SD card
  Serial.println(pathBuffer);
  serveFileFromSD(client, pathBuffer);
  
  // Se il file non è stato trovato, invia una risposta 404
  if (!fileServed) {
    client.println(FPSTR(HTTP_404_NOT_FOUND));
    client.println(FPSTR(CONTENT_TYPE_PLAIN));
    client.println(FPSTR(CONNECTION_CLOSE));
    client.println();
    client.println(F("404 - File not found"));
    client.stop();
  }
}

// Invia una risposta HTTP generica
void sendResponse(WiFiClient& client, const char* contentType, const char* content, int statusCode) {
  char statusLine[32];
  snprintf(statusLine, sizeof(statusLine), "HTTP/1.1 %d OK", statusCode);
  client.println(statusLine);
  client.print("Content-Type: ");
  client.println(contentType);
  client.println("Connection: close");
  client.println();
  client.println(content);
  client.stop();
}

// Invia una risposta HTTP generica con contenuto da PROGMEM
void sendResponse(WiFiClient& client, const char* contentType, const __FlashStringHelper* content, int statusCode) {
  char statusLine[32];
  snprintf(statusLine, sizeof(statusLine), "HTTP/1.1 %d OK", statusCode);
  client.println(statusLine);
  client.print("Content-Type: ");
  client.println(contentType);
  client.println("Connection: close");
  client.println();
  client.println(content);
  client.stop();
}

// Invia un reindirizzamento HTTP 302
void sendRedirect(WiFiClient& client, const char* location) {
  client.println(FPSTR(HTTP_302_FOUND));
  client.print(F("Location: "));
  client.println(location);
  client.println(FPSTR(CONNECTION_CLOSE));
  client.println();
  client.stop();
}

// Invia una risposta JSON
void sendJsonResponse(WiFiClient& client, const char* jsonContent, int statusCode) {
  sendResponse(client, "application/json", jsonContent, statusCode);
}

// Invia una risposta JSON con contenuto da PROGMEM
void sendJsonResponse(WiFiClient& client, const __FlashStringHelper* jsonContent, int statusCode) {
  sendResponse(client, "application/json", jsonContent, statusCode);
}

// getEncryptionTypeString è già definita in NetworkUtils.cpp

// Gestisce la scansione WiFi e restituisce i risultati in JSON
void performWiFiScan(WiFiClient& client) {
  // Prepara documento JSON per la risposta
  DynamicJsonDocument doc(1536); // Ridotto da 2048 a 1536 per minimizzare l'uso della DRAM
  JsonArray networks = doc.createNestedArray("networks");
  
  // Avvia la scansione WiFi in modalità asincrona
  int numNetworks = WiFi.scanNetworks(/*async=*/false, /*show_hidden=*/false, /*passive=*/false, /*max_ms_per_chan=*/300);
  
  // Aggiungi ogni rete al documento JSON
  for (int i = 0; i < numNetworks; i++) {
    wifi_auth_mode_t encryptionType = (wifi_auth_mode_t)WiFi.encryptionType(i);
    char encryptionTypeStr[16];
    getEncryptionTypeString(encryptionType, encryptionTypeStr, sizeof(encryptionTypeStr));
    
    // Salta reti nascoste senza SSID
    if (WiFi.SSID(i).length() == 0) {
      continue;
    }
    
    JsonObject network = networks.createNestedObject();
    network["ssid"] = WiFi.SSID(i);
    network["bssid"] = WiFi.BSSIDstr(i);
    network["rssi"] = WiFi.RSSI(i);
    network["channel"] = WiFi.channel(i);
    network["encryptionType"] = encryptionTypeStr;
    network["isSecure"] = (encryptionType != WIFI_AUTH_OPEN);
    
    // Limite di reti per non sovraccaricare il documento JSON
    if (networks.size() >= 15) break;
  }
  
  // Pulisci la scansione per liberare memoria
  WiFi.scanDelete();
  
  // Aggiungi stato della scansione
  doc["status"] = "success";
  doc["count"] = networks.size();
  
  // Invia la risposta
  char jsonBuffer[384]; // Buffer ulteriormente ridotto per risparmiare DRAM
  serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));
  
  sendJsonResponse(client, jsonBuffer);
}
