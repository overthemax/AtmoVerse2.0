// Backup implementation excluded from build
#if 0
#include "WebServer.h"
#include "NetworkUtils.h"
#include "Config.h"
#include "WeatherUtils.h"
#include "WebUIPages.h"
#include "WebMinimal.h"
#include "Hardware.h"
#include <SD.h>
#include <ArduinoJson.h>
#include <WiFi.h>

// Funzione per decodificare l'URL (traduce caratteri come %20 in spazi)
String urldecode(String str) {
  String ret = "";
  char ch;
  int i, len = str.length();
  
  for (i = 0; i < len; i++) {
    if (str[i] == '+') {
      ret += ' ';
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
      
      ret += (char)code;
      i += 2;
    } else {
      ret += str[i];
    }
  }
  return ret;
}

// Istanza del server
WiFiServer server(80);

// Inizializza il server
void setupServer() {
  // Verifica che la cartella www esista sulla SD
  if (!SD.exists("/www")) {
    // Non fare nulla se la cartella non esiste
    return;
  }
  
  // Avvia il server
  server.begin();
}

// Determina il tipo di contenuto in base all'estensione del file
String getContentType(String filename) {
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".jpg")) return "image/jpeg";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".svg")) return "image/svg+xml";
  else if (filename.endsWith(".json")) return "application/json";
  return "text/plain";
}

// Serve un file dalla SD card
bool serveFileFromSD(WiFiClient& client, String path) {
  // Verifica se la SD è presente e accessibile
  if (!SD.begin(SD_CS, sdSPI)) {
    return false;
  }
  
  // Gestione dei percorsi
  if (path.endsWith("/")) path += "index.html";
  
  // Non mostrare i file nascosti
  if (path.indexOf("/.") >= 0) return false;
  
  // Aggiungi prefisso /www al percorso
  if (!path.startsWith("/www")) {
    path = "/www" + path;
  }
  
  // Verifica se il file esiste
  if (!SD.exists(path)) {
    return false;
  }
  
  // Apri il file
  File file = SD.open(path, FILE_READ);
  if (!file) {
    return false;
  }
  
  // Determina il tipo di contenuto
  String contentType = getContentType(path);
  
  // Invia l'header della risposta
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: " + contentType);
  client.println("Connection: close");
  client.println();
  
  // Invia il file
  while (file.available()) {
    client.write(file.read());
  }
  
  // Chiudi il file e la connessione
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
  
  // Timeout per la richiesta
  unsigned long timeout = millis() + 5000;
  while (!client.available() && millis() < timeout) {
    delay(10);
  }
  
  // Se non ci sono dati disponibili, chiudi la connessione
  if (!client.available()) {
    client.stop();
    return;
  }
  
  // Leggi la prima riga della richiesta
  String request = client.readStringUntil('\r');
  client.readStringUntil('\n');
  
  // Estrai il metodo e il percorso
  String method = request.substring(0, request.indexOf(' '));
  String path = request.substring(request.indexOf(' ') + 1);
  path = path.substring(0, path.indexOf(' '));
  
  // Estrai i parametri URL se presenti
  String params = "";
  int qIndex = path.indexOf('?');
  if (qIndex != -1) {
    params = path.substring(qIndex + 1);
    path = path.substring(0, qIndex);
  }
  
  // Leggi le altre intestazioni e il corpo della richiesta
  String header = "";
  while (client.available()) {
    String line = client.readStringUntil('\r');
    client.readStringUntil('\n');
    if (line.length() == 0) break;
    header += line + "\n";
  }
  
  // --- API endpoints ---
  
  // Scansione WiFi
  if (path == "/api/wifi-scan" && method == "GET") {
    // Esegui la scansione e ritorna i risultati
    performWiFiScan(client);
    return;
  }
  
  // API per recuperare le impostazioni attuali
  if (path == "/api/settings" && method == "GET") {
    DynamicJsonDocument doc(1024);
    
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
    
    sendJsonResponse(client, json);
    return;
  }
  
  // API per il meteo
  if (path == "/api/weather" && method == "GET") {
    DynamicJsonDocument doc(2048);
    
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
    char localTimeStr[30];
    strftime(localTimeStr, sizeof(localTimeStr), "%H:%M - %d/%m/%Y", &timeinfo);
    time["local"] = localTimeStr;
    
    // Forza l'aggiornamento dei dati meteo se non validi
    if (!isWeatherDataValid()) {
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
    char lastUpdateStr[30];
    struct tm lastUpdateTime;
    localtime_r(&currentWeather.last_update, &lastUpdateTime);
    strftime(lastUpdateStr, sizeof(lastUpdateStr), "%H:%M - %d/%m/%Y", &lastUpdateTime);
    weather["last_update"] = lastUpdateStr;
    
    weather["valid"] = true;
    
    String json;
    serializeJson(doc, json);
    
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.println();
    serializeJson(doc, client);
    client.stop();
    return;
  }
  
  // Gestione configurazioni
  if (path == "/api/settings" && method == "POST") {
    // Leggi il corpo della richiesta
    String jsonBody = "";
    while (client.available()) {
      jsonBody += client.readString();
    }
    
    // Verifica che il body non sia vuoto
    if (jsonBody.length() == 0) {
      sendJsonResponse(client, "{\"success\":false,\"message\":\"Nessun dato ricevuto\"}");
      return;
    }
    
    DynamicJsonDocument doc(512);
    DeserializationError error;
    
    // Controlla se è un formato URL-encoded
    if (jsonBody.indexOf('=') > 0 && jsonBody.indexOf('&') > 0) {
      // Dividi la stringa in coppie chiave-valore
      String pairs[10]; // Massimo 10 parametri
      int pairCount = 0;
      int startPos = 0;
      int ampPos;
      
      // Estrai le coppie chiave-valore
      while ((ampPos = jsonBody.indexOf('&', startPos)) != -1 && pairCount < 10) {
        pairs[pairCount++] = jsonBody.substring(startPos, ampPos);
        startPos = ampPos + 1;
      }
      
      if (startPos < jsonBody.length() && pairCount < 10) {
        pairs[pairCount++] = jsonBody.substring(startPos);
      }
      
      // Processa ogni coppia e aggiungi al documento JSON
      for (int i = 0; i < pairCount; i++) {
        int eqPos = pairs[i].indexOf('=');
        if (eqPos != -1) {
          String key = pairs[i].substring(0, eqPos);
          String value = pairs[i].substring(eqPos + 1);
          
          // Decodifica URL-encoded
          value = urldecode(value);
          
          // Conversione di tipi in base alla chiave
          if (key == "latitude" || key == "longitude" || key == "timezone" || key == "dst") {
            float numValue = value.toFloat();
            doc[key] = numValue;
          } else if (key == "darkmode") {
            doc[key] = (value == "true" || value == "1");
          } else {
            doc[key] = value;
          }
        }
      }
      
      error = DeserializationError::Ok;
    } else {
      // Prova a fare il parsing del JSON
      error = deserializeJson(doc, jsonBody);
      if (error) {
        sendJsonResponse(client, "{\"success\":false,\"message\":\"Errore nel parsing JSON\"}");
        return;
      }
    }

    // Inizializzazione SD
    if (!SD.begin(5)) {
      sendJsonResponse(client, "{\"success\":false,\"message\":\"Errore nell'inizializzazione della SD\"}");
      return;
    }

    // Carica configurazione esistente
    loadConfig();
    
    // Aggiorna le impostazioni in base ai parametri ricevuti
    if (doc.containsKey("ssid")) {
      String ssid = doc["ssid"].as<String>();
      strlcpy(config.ssid, ssid.c_str(), sizeof(config.ssid));
    }

    if (doc.containsKey("password")) {
      String password = doc["password"].as<String>();
      strlcpy(config.password, password.c_str(), sizeof(config.password));
    }

    if (doc.containsKey("api_key") && doc["api_key"].as<String>().length() > 0) {
      String apiKey = doc["api_key"].as<String>();
      strlcpy(config.api_key, apiKey.c_str(), sizeof(config.api_key));
    }

    if (doc.containsKey("city")) {
      String city = doc["city"].as<String>();
      strlcpy(config.city, city.c_str(), sizeof(config.city));
    }

    if (doc.containsKey("timezone")) {
      config.gmtOffset_sec = doc["timezone"].as<int>() * 3600;
    }

    if (doc.containsKey("dst")) {
      config.daylightOffset_sec = doc["dst"].as<int>() * 3600;
    }

    // Parametri di risparmio energetico
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
    
    // Parametri di gestione degli errori di rete
    if (doc.containsKey("maxNetworkRetries")) {
      config.maxNetworkRetries = doc["maxNetworkRetries"].as<int>();
    }
    
    if (doc.containsKey("showLastDataOnError")) {
      config.showLastDataOnError = doc["showLastDataOnError"].as<bool>();
    }

    // Salva la configurazione aggiornata
    if (saveConfig()) {
      sendJsonResponse(client, "{\"success\":true,\"message\":\"Configurazione salvata con successo\"}");
      
      // Attendi per essere sicuro che la risposta venga inviata
      delay(5000);
      
      // Chiudi tutte le connessioni WiFi
      WiFi.disconnect(true);
      if (apMode) {
        WiFi.softAPdisconnect(true);
      }
      
      // Riavvia ESP32
      delay(1000);
      ESP.restart();
    } else {
      sendJsonResponse(client, "{\"success\":false,\"message\":\"Errore nel salvataggio della configurazione\"}");
    }
    return;
  }
  
  // Gestione specifica per la pagina /info
  if (path == "/info" && method == "GET") {
    // Utilizziamo direttamente il percorso info.html per coerenza
    serveFileFromSD(client, "/info.html");
    return;
  }
  
  // Servizio file dalla SD
  bool fileServed = false;
  
  // Flag per tracciare se il file è stato servito
  bool fileServed = false;

  // Gestione captive portal per Android e iOS (solo in modalità AP)
  if (apMode && (path.indexOf("generate_204") > 0 || path.indexOf("gen_204") > 0 || 
      path.indexOf("/chat") >= 0 || path.indexOf("/connectivitycheck") >= 0 ||
      path.indexOf("/hotspot-detect") >= 0)) {
    // Risposta per il captive portal
    client.println(F("HTTP/1.1 200 OK"));
    client.println(F("Content-Type: text/html"));
    client.println(F("Connection: close"));
    client.println();
    client.println(F("<html><head><meta http-equiv='refresh' content='0;url=/'></head><body></body></html>"));
    client.stop();
    Serial.print(F("Captive portal richiesto per path: "));
    Serial.println(path);
    return;
  }
  
  // Reindirizzamento alla pagina delle impostazioni se la URL è la root
  if (path == "/" || path == "") {
    // Reindirizza alla pagina delle impostazioni
    client.println(F("HTTP/1.1 302 Found"));
    client.println(F("Location: /www/settings.html"));
    client.println(F("Connection: close"));
    client.println();
    client.stop();
    Serial.println(F("Reindirizzamento a /www/settings.html"));
    return;
  }
  
  // Gestione richieste per /setup (compatibilità con vecchi collegamenti)
  if (path == "/setup") {
    client.println(F("HTTP/1.1 301 Moved Permanently"));
    client.println(F("Location: /www/settings.html"));
    client.println(F("Connection: close"));
    client.println();
    client.stop();
    return;
  }
  
  // Gestione chiamata API per scansione WiFi (comune a entrambe le modalità)
  if (path == "/api/wifi-scan") {
    performWiFiScan(client);
    return;
  }
    
    // Gestione delle impostazioni API
    if (path == "/api/settings") {
      if (header.indexOf("GET") >= 0) {
        // Leggi le impostazioni correnti dalla configurazione attuale
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: application/json");
        client.println("Connection: close");
        client.println();
        
        // Crea un JSON con tutti i campi di configurazione attuali
        String jsonResponse = "{\
          \"ssid\": \"" + String(config.ssid) + "\",\
          \"city\": \"" + String(config.city) + "\",\
          \"lat\": " + String(config.lat, 4) + ",\
          \"lon\": " + String(config.lon, 4) + ",\
          \"api_key\": \"" + String(config.api_key) + "\",\
          \"gmtOffset_sec\": " + String(config.gmtOffset_sec) + ",\
          \"daylightOffset_sec\": " + String(config.daylightOffset_sec) + ",\
          \"ntpServer\": \"" + String(config.ntpServer) + "\",\
          \"powerSavingEnabled\": " + String(config.powerSavingEnabled ? "true" : "false") + ",\
          \"powerSavingStartHour\": " + String(config.powerSavingStartHour) + ",\
          \"powerSavingEndHour\": " + String(config.powerSavingEndHour) + ",\
          \"normalUpdateInterval\": " + String(config.normalUpdateInterval) + ",\
          \"powerSavingUpdateInterval\": " + String(config.powerSavingUpdateInterval) + ",\
          \"maxNetworkRetries\": " + String(config.maxNetworkRetries) + ",\
          \"showLastDataOnError\": " + String(config.showLastDataOnError ? "true" : "false") + "\
        }";
        
        client.println(jsonResponse);
        Serial.println("Inviate impostazioni attuali al client");
      } else if (header.indexOf("POST") >= 0) {
        // Estrai il payload JSON dalla richiesta POST
        String jsonPayload = "";
        bool startedReading = false;
        bool isBodyComplete = false;
        // Leggi la richiesta fino a trovare il corpo JSON completo
        while (client.available() && !isBodyComplete) {
          String line = client.readStringUntil('\n');
          line.trim();
          
          // La linea vuota segna l'inizio del corpo della richiesta
          if (line.length() == 0 && !startedReading) {
            startedReading = true;
            continue;
          }
          
          // Riguadagna il corpo JSON
          if (startedReading) {
            jsonPayload += line;
            // Se la linea contiene la parentesi di chiusura, abbiamo finito
            if (line.indexOf("}") >= 0) {
              isBodyComplete = true;
            }
          }
        }
        
        Serial.print("Ricevuto JSON: ");
        Serial.println(jsonPayload);
        
        // Ora salviamo i dati
        if (jsonPayload.length() > 0) {
          DynamicJsonDocument doc(2048);
          DeserializationError error = deserializeJson(doc, jsonPayload);
          
          if (!error) {
            // Aggiorna i valori dalla configurazione
            if (doc.containsKey("ssid") && strlen(doc["ssid"]) > 0) {
              strlcpy(config.ssid, doc["ssid"], sizeof(config.ssid));
            }
            
            if (doc.containsKey("password") && strlen(doc["password"]) > 0) {
              strlcpy(config.password, doc["password"], sizeof(config.password));
            }
            
            if (doc.containsKey("city") && strlen(doc["city"]) > 0) {
              strlcpy(config.city, doc["city"], sizeof(config.city));
            }
            
            if (doc.containsKey("api_key") && strlen(doc["api_key"]) > 0) {
              strlcpy(config.api_key, doc["api_key"], sizeof(config.api_key));
            }
            
            if (doc.containsKey("ntpServer") && strlen(doc["ntpServer"]) > 0) {
              strlcpy(config.ntpServer, doc["ntpServer"], sizeof(config.ntpServer));
            }
            
            // Aggiorna i valori numerici
            if (doc.containsKey("lat")) config.lat = doc["lat"];
            if (doc.containsKey("lon")) config.lon = doc["lon"];
            if (doc.containsKey("gmtOffset_sec")) config.gmtOffset_sec = doc["gmtOffset_sec"];
            if (doc.containsKey("daylightOffset_sec")) config.daylightOffset_sec = doc["daylightOffset_sec"];
            if (doc.containsKey("powerSavingEnabled")) config.powerSavingEnabled = doc["powerSavingEnabled"];
            if (doc.containsKey("powerSavingStartHour")) config.powerSavingStartHour = doc["powerSavingStartHour"];
            if (doc.containsKey("powerSavingEndHour")) config.powerSavingEndHour = doc["powerSavingEndHour"];
            if (doc.containsKey("normalUpdateInterval")) config.normalUpdateInterval = doc["normalUpdateInterval"];
            if (doc.containsKey("powerSavingUpdateInterval")) config.powerSavingUpdateInterval = doc["powerSavingUpdateInterval"];
            if (doc.containsKey("maxNetworkRetries")) config.maxNetworkRetries = doc["maxNetworkRetries"];
            if (doc.containsKey("showLastDataOnError")) config.showLastDataOnError = doc["showLastDataOnError"];
            
            // Salva le impostazioni sulla SD card
            saveConfig();
            
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: application/json");
            client.println("Connection: close");
            client.println();
            client.println("{\"success\": true, \"message\": \"Configurazione salvata con successo\"}");
            
            Serial.println("Configurazione salvata con successo");
          } else {
            client.println("HTTP/1.1 400 Bad Request");
            client.println("Content-Type: application/json");
            client.println("Connection: close");
            client.println();
            client.println("{\"success\": false, \"error\": \"JSON non valido\"}");
            
            Serial.print("Errore nella deserializzazione JSON: ");
            Serial.println(error.c_str());
          }
        } else {
          client.println("HTTP/1.1 400 Bad Request");
          client.println("Content-Type: application/json");
          client.println("Connection: close");
          client.println();
          client.println("{\"success\": false, \"error\": \"Nessun dato ricevuto\"}");
          
          Serial.println("Nessun dato JSON ricevuto");
        }
      }
      client.stop();
      return;
    }

    // Verifica se il path richiesto contiene già /www/
    if (!path.startsWith("/www/") && !path.startsWith("/api/")) {
      // Aggiungi /www/ per cercare i file nella giusta directory
      path = "/www" + path;
      Serial.print("Path modificato: ");
      Serial.println(path);
    }
    
    // Prova a servire il file dalla SD card
    fileServed = serveFileFromSD(client, path);
    
    // Se il file non è trovato, prova con percorsi alternativi
    if (!fileServed) {
      // Prova percorsi alternativi per file comuni
      String altPath = "";
      
      if (path.endsWith(".css")) {
        altPath = "/www/css/style.css";
      } else if (path.endsWith(".js")) {
        altPath = "/www/js/setup.js";
      } else if (path.endsWith(".html")) {
        altPath = "/www/settings.html";
      }
      
      if (altPath.length() > 0) {
        Serial.print("Tentativo con path alternativo: ");
        Serial.println(altPath);
        fileServed = serveFileFromSD(client, altPath);
      }
      
      // File non trovato - risposta 404 con indicazioni utili
      client.println(F("HTTP/1.1 404 Not Found"));
      client.println(F("Content-Type: text/html"));
      client.println(F("Connection: close"));
      client.println();
      client.println(F("<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>404 - Pagina non trovata</title>"));
      client.println(F("<style>body{font-family:Arial;text-align:center;padding:40px;background:#f8f9fa;}</style></head>"));
      client.println(F("<body><h1>404 - Pagina non trovata</h1>"));
      client.println(F("<p>La pagina richiesta non è disponibile sulla stazione meteo AtmoVerse 2.0</p>"));
      client.println(F("<p>Sei in modalità configurazione (AP Mode)</p>"));
      client.println(F("<p><a href=\"/setup\">Vai alla pagina di configurazione</a></p></body></html>"));
      client.stop();
      Serial.print(F("Errore 404 per il path: "));
      Serial.println(path);
      return;
    }
  }
  
  // Tenta di servire il file dalla SD card
  fileServed = serveFileFromSD(client, path);
  
  // Se il file non è stato trovato, invia una risposta 404
  if (!fileServed) {
    client.println(F("HTTP/1.1 404 Not Found"));
    client.println(F("Content-Type: text/plain"));
    client.println(F("Connection: close"));
    client.println();
    client.println(F("404 - File not found"));
    client.stop();
  }
}
}

// Invia una risposta HTTP generica
void sendResponse(WiFiClient& client, const String& contentType, const String& content, int statusCode) {
  client.println("HTTP/1.1 " + String(statusCode) + " OK");
  client.println("Content-Type: " + contentType);
  client.println("Connection: close");
  client.println();
  client.println(content);
  client.stop();
}

// Invia una risposta JSON
void sendJsonResponse(WiFiClient& client, const String& jsonContent, int statusCode) {
  sendResponse(client, "application/json", jsonContent, statusCode);
}

// Invia un reindirizzamento HTTP 302
void sendRedirect(WiFiClient& client, const String& location) {
  client.println("HTTP/1.1 302 Found");
  client.println("Location: " + location);
  client.println("Connection: close");
  client.println();
  client.stop();
}

// Restituisce una stringa che descrive il tipo di crittografia WiFi
String getEncryptionTypeString(wifi_auth_mode_t encryptionType) {
  switch (encryptionType) {
    case WIFI_AUTH_OPEN:
      return "open";
    case WIFI_AUTH_WEP:
      return "WEP";
    case WIFI_AUTH_WPA_PSK:
      return "WPA";
    case WIFI_AUTH_WPA2_PSK:
      return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE:
      return "WPA2-Enterprise";
    case WIFI_AUTH_WPA3_PSK:
      return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:
      return "WPA2/WPA3";
    case WIFI_AUTH_WAPI_PSK:
      return "WAPI";
    default:
      return "Unknown";
  }
}

// Gestisce la scansione WiFi e restituisce i risultati in JSON
void performWiFiScan(WiFiClient& client) {
  // Prepara documento JSON per la risposta
  DynamicJsonDocument doc(4096);
  JsonArray networks = doc.createNestedArray("networks");
  
  // Avvia la scansione WiFi in modalità asincrona
  int numNetworks = WiFi.scanNetworks(/*async=*/false, /*show_hidden=*/false, /*passive=*/false, /*max_ms_per_chan=*/300);
  
  // Aggiungi ogni rete al documento JSON
  for (int i = 0; i < numNetworks; i++) {
    wifi_auth_mode_t encryptionType = (wifi_auth_mode_t)WiFi.encryptionType(i);
    String encryptionTypeStr = getEncryptionTypeString(encryptionType);
    
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
  String json;
  serializeJson(doc, json);
  
  sendJsonResponse(client, json);
}

#endif
