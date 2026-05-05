#include "WebServer.h"
#include "NetworkUtils.h"
#include "Config.h"
#include "WeatherUtils.h"
#include "WebUIPages.h"
#include "WebMinimal.h"
#include "Hardware.h"
#include "QuotesManager.h"
#include "Display.h"
#include "AtmoVerseConstants.h" // Aggiunto per costanti JSON se necessarie
#include <SD.h>
#include <ArduinoJson.h>
#include <WiFi.h>

// Variabile definita nel file principale per il controllo del refresh display
extern unsigned long lastDisplayUpdate;

// File citazioni su SD
static const char* QUOTES_JSON_PATH = "/quotes.json";
// File layout personalizzato su SD
static const char* LAYOUT_JSON_PATH = "/layout.json";

// Impostazioni runtime per tema e modalità display controllate dalla Web GUI
// Nota: non persistiamo su SD/NVS per semplicità; sono volatili fino al riavvio
String g_displayTheme = "default";   // "default" | "eink"
String g_displayMode  = "classic";   // "classic" | "focus" | "custom"

// Mappa le categorie lato UI a quelle usate dal firmware
static String mapUiCategoryToFirmware(const String& uiCat) {
  String c = uiCat;
  String lc = c; lc.toLowerCase();

  // Se arriva già una categoria firmware valida, restituiscila così com'è
  if (lc == "temporale" || lc == "pioggia_leggera" || lc == "pioggia" ||
      lc == "neve" || lc == "cielo_sereno" || lc == "poche_nuvole" ||
      lc == "nuvole_sparse" || lc == "nuvole_abbondanti" || lc == "nebbia" ||
      lc == "tempesta" || lc == "vento" || lc == "motivazione" ||
      lc == "mattina" || lc == "pomeriggio" || lc == "sera") {
    return lc;
  }

  // Alias storici / inglesi
  if (lc == "nuvoloso" || lc == "nuvole" || lc == "clouds") return "nuvole_sparse";
  if (lc == "sereno" || lc == "sole" || lc == "clear") return "cielo_sereno";

  if (lc == "rain" || lc == "pioggia") return "pioggia";
  if (lc == "drizzle" || lc == "pioggerella") return "pioggia_leggera";

  if (lc == "temporale" || lc == "thunderstorm") return "temporale";
  if (lc == "tempesta" || lc == "storm") return "tempesta";

  if (lc == "neve" || lc == "snow") return "neve";
  if (lc == "nebbia" || lc == "mist" || lc == "fog" || lc == "haze" || lc == "smoke") return "nebbia";
  if (lc == "vento" || lc == "wind" || lc == "windy") return "vento";

  if (lc == "any" || lc == "generiche" || lc == "motivazione") return "motivazione";
  if (lc == "mattina" || lc == "morning") return "mattina";
  if (lc == "pomeriggio" || lc == "afternoon") return "pomeriggio";
  if (lc == "sera" || lc == "evening") return "sera";

  return "motivazione";
}

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
  // Avvia sempre il server, anche se la SD o /www non sono presenti
  // In tal caso verranno restituite 404 o pagine minime
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
  // Verifica se la SD è presente e accessibile (inizializza una sola volta tramite initSD)
  if (!initSD()) {
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
  size_t fileSize = file.size();
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: " + contentType);
  client.println("Content-Length: " + String(fileSize));
  client.println("Accept-Ranges: none");
  // Cache aggressiva per asset statici non-HTML
  if (!path.endsWith(".html")) {
    client.println("Cache-Control: public, max-age=86400");
  } else {
    client.println("Cache-Control: no-cache");
  }
  client.println("Connection: close");
  client.println();
  
  // Invia il file usando un buffer per migliori performance
  uint8_t buffer[512];
  size_t totalSent = 0;
  while (file.available() && totalSent < fileSize) {
    size_t bytesRead = file.read(buffer, sizeof(buffer));
    size_t sent = client.write(buffer, bytesRead);
    totalSent += sent;
    // Verifica che il client sia ancora connesso
    if (!client.connected()) {
      file.close();
      return false;
    }
  }
  
  // Assicurati che tutti i dati siano inviati
  client.flush();
  delay(1);
  
  // Chiudi il file e la connessione
  file.close();
  client.stop();
  return true;
}

// Pagina di avviso quando mancano i file web o la SD non è disponibile
static void sendMissingAssetsPage(WiFiClient& client, bool sdOk, bool wwwExists) {
  String reason;
  if (!sdOk) {
    reason = "<p><strong>SD non inizializzata o non inserita.</strong></p>";
  } else if (!wwwExists) {
    reason = "<p><strong>Cartella /www assente sulla SD.</strong></p>";
  } else {
    reason = "<p><strong>File richiesto non trovato.</strong></p>";
  }

  String html =
    String("<!DOCTYPE html><html><head><meta charset=\"UTF-8\">")+
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"+
    "<title>AtmoVerse 2.0 - File Web Mancanti</title>"+
    "<style>body{font-family:Arial,Helvetica,sans-serif;background:#f8f9fa;color:#333;margin:0;padding:40px;}"+
    ".card{max-width:720px;margin:0 auto;background:#fff;border-radius:12px;box-shadow:0 6px 18px rgba(0,0,0,.08);padding:28px;}"+
    "h1{font-size:22px;margin:0 0 12px} p{line-height:1.5} code{background:#f1f3f5;padding:2px 6px;border-radius:6px}"+
    ".hint{background:#fff3cd;color:#856404;border:1px solid #ffeeba;border-radius:8px;padding:12px;margin-top:16px}"+
    ".list{margin:12px 0 0 18px} a{color:#0d6efd;text-decoration:none}</style></head><body>"+
    "<div class=\"card\">"+
    "<h1>Interfaccia web non disponibile</h1>"+
    reason+
    "<div class=\"hint\"><strong>Cosa fare:</strong> "+
      (sdOk ? "Copia la cartella <code>/www</code> nella root della SD con i file essenziali:" : "Inserisci e inizializza una scheda SD compatibile, poi copia i file web come indicato qui sotto:")+
    "</div>"+
    "<ul class=\"list\">"+
    "<li><code>/www/settings.html</code></li>"+
    "<li><code>/www/settings.js</code></li>"+
    "<li><code>/www/style.css</code></li>"+
    "<li><code>/www/app.js</code> (se usato)</li>"+
    "</ul>"+
    "<p>Percorso sorgente nel progetto: <code>data/www/</code>. Dopo aver copiato i file, riavvia il dispositivo oppure ricarica questa pagina.</p>"+
    "</div></body></html>";
  sendResponse(client, "text/html", html, 200);
}

// Gestione delle richieste dei client
void handleClientRequests() {
  // Gestisci il DNS server per captive portal se in modalità AP
  if (apMode) {
    dnsServer.processNextRequest();
  }
  
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
  
  // Gestione richieste di probe tipiche dei captive portal
  // Android: invece di 204, reindirizza per attivare il portale
  if (path == "/generate_204" || path == "/gen_204") {
    // Reindirizza ad settings per far apparire il captive portal
    client.println("HTTP/1.1 302 Found");
    String redirectUrl = "http://" + (apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString()) + "/settings.html";
    client.println("Location: " + redirectUrl);
    client.println("Connection: close");
    client.println();
    client.flush();
    delay(1);
    client.stop();
    return;
  }
  
  // Apple captive portal detection
  if (path == "/hotspot-detect.html" || path == "/library/test/success.html") {
    // Apple si aspetta una risposta HTML specifica
    const char* html = "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>";
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println("Content-Length: " + String(strlen(html)));
    client.println("Connection: close");
    client.println();
    client.print(html);
    client.flush();
    delay(1);
    client.stop();
    return;
  }
  
  if (path == "/ncsi.txt") {
    // Windows NCSI: rispondi con testo semplice
    sendResponse(client, "text/plain", "Microsoft NCSI", 200);
    return;
  }
  
  if (path == "/favicon.ico") {
    // Evita accesso SD per favicon non essenziale - 204 No Content
    client.println("HTTP/1.1 204 No Content");
    client.println("Connection: close");
    client.println();
    client.flush();
    delay(1);
    client.stop();
    return;
  }

  // Microsoft/Windows captive portal detection
  if (path == "/connecttest.txt") {
    const char* text = "Microsoft Connect Test";
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/plain");
    client.println("Content-Length: " + String(strlen(text)));
    client.println("Connection: close");
    client.println();
    client.print(text);
    client.flush();
    delay(1);
    client.stop();
    return;
  }
  
  // Altri probe comuni - reindirizza a settings
  if (path == "/redirect" || path == "/redirect.html" ||
      path == "/canonical.html" || path == "/success.txt" || path == "/success.html") {
    client.println("HTTP/1.1 302 Found");
    client.println("Location: http://192.168.4.1/settings.html");
    client.println("Connection: close");
    client.println();
    client.flush();
    delay(1);
    client.stop();
    return;
  }

  if (path == "/robots.txt") {
    sendResponse(client, "text/plain", "User-agent: *\nDisallow: /\n", 200);
    return;
  }

  // Health check semplice con info di diagnostica
  if (path == "/api/health" && method == "GET") {
    DynamicJsonDocument doc(1024);
    doc["status"] = "ok";
    doc["uptime_sec"] = (uint32_t)(millis() / 1000);
    doc["free_heap"] = (uint32_t)ESP.getFreeHeap();
    doc["apMode"] = apMode;
    doc["ip"] = apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
    long rssi = WiFi.RSSI();
    if (rssi == 0 || rssi == 31) {
      // valori non significativi quando non associati
      rssi = -127;
    }
    doc["rssi"] = (int)rssi;
    // Stato SD (best-effort) - Usa initSD centralizzato
    bool sdOk = initSD();
    doc["sd_ok"] = sdOk;

    String json;
    serializeJson(doc, json);
    sendJsonResponse(client, json);
    return;
  }
  
  // Alias compatibilità: GET /api/config (stesse info di settings, con chiavi attese dal frontend)
  if (path == "/api/config" && method == "GET") {
    DynamicJsonDocument doc(JSON_BUFFER_LARGE);
    doc["ssid"] = config.ssid;
    doc["city"] = config.city;
    doc["timezone"] = (int)(config.gmtOffset_sec / 3600);
    doc["daylightSaving"] = (config.daylightOffset_sec != 0);
    doc["ntpServer"] = config.ntpServer;
    doc["api_key"] = config.api_key;
    doc["use24hFormat"] = config.use24hFormat;
    doc["units"] = config.units;
    doc["language"] = config.language;
    // Power saving & retries
    doc["powerSavingEnabled"] = config.powerSavingEnabled;
    doc["powerSavingStartHour"] = config.powerSavingStartHour;
    doc["powerSavingEndHour"] = config.powerSavingEndHour;
    doc["normalUpdateInterval"] = config.normalUpdateInterval;
    doc["powerSavingUpdateInterval"] = config.powerSavingUpdateInterval;
    // Intervalli refresh display (secondi)
    doc["displayRefreshIntervalSec"] = config.displayRefreshIntervalSec;
    doc["displayRefreshIntervalSecPowerSaving"] = config.displayRefreshIntervalSecPowerSaving;
    doc["apTimeRefreshIntervalSec"] = config.apTimeRefreshIntervalSec;
    doc["maxNetworkRetries"] = config.maxNetworkRetries;
    // Intervalli meteo
    doc["weatherUpdateInterval"] = config.weatherUpdateInterval;
    doc["weatherPowerSavingUpdateInterval"] = config.weatherPowerSavingUpdateInterval;
    // Intervalli refresh display (secondi)
    doc["displayRefreshIntervalSec"] = config.displayRefreshIntervalSec;
    doc["displayRefreshIntervalSecPowerSaving"] = config.displayRefreshIntervalSecPowerSaving;
    doc["apTimeRefreshIntervalSec"] = config.apTimeRefreshIntervalSec;
    // Posizione citazione
    doc["quotePosX"] = config.quotePosX;
    doc["quotePosY"] = config.quotePosY;
    doc["apMode"] = apMode;
    doc["ipAddress"] = apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
    
    // Night Mode
    doc["nightModeEnabled"] = config.nightModeEnabled;
    doc["nightModeStartHour"] = config.nightModeStartHour;
    doc["nightModeEndHour"] = config.nightModeEndHour;
    
    // Battery Management
    doc["batteryMonitorEnabled"] = config.batteryMonitorEnabled;
    doc["batteryADCPin"] = config.batteryADCPin;
    doc["batteryVoltageDivider"] = config.batteryVoltageDivider;
    doc["batteryShowOnDisplay"] = config.batteryShowOnDisplay;
    
    // Weather Alerts
    doc["alertsEnabled"] = config.alertsEnabled;
    doc["alertTempHigh"] = config.alertTempHigh;
    doc["alertTempLow"] = config.alertTempLow;
    doc["alertWindHigh"] = config.alertWindHigh;
    doc["alertRain"] = config.alertRain;
    doc["alertSnow"] = config.alertSnow;
    doc["alertStorm"] = config.alertStorm;
    doc["alertShowOnDisplay"] = config.alertShowOnDisplay;
    doc["alertWebhookUrl"] = config.alertWebhookUrl;
    
    // Historical Statistics
    doc["historyEnabled"] = config.historyEnabled;
    doc["historyKeepDays"] = config.historyKeepDays;
    doc["historyShowOnDisplay"] = config.historyShowOnDisplay;

    String json;
    serializeJson(doc, json);
    sendJsonResponse(client, json);
    return;
  }

  // Alias compatibilità: POST /api/config (accetta i campi previsti dallo script)
  if (path == "/api/config" && method == "POST") {
    String body = "";
    while (client.available()) {
      body += client.readString();
    }
    if (body.length() == 0) {
      sendJsonResponse(client, "{\"success\":false,\"message\":\"Nessun dato ricevuto\"}");
      return;
    }

    DynamicJsonDocument doc(JSON_BUFFER_LARGE);
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
      sendJsonResponse(client, "{\"success\":false,\"message\":\"Errore nel parsing JSON\"}");
      return;
    }

    // Carica configurazione attuale
    loadConfig();

    // Applica aggiornamenti
    if (doc.containsKey("ssid")) strlcpy(config.ssid, doc["ssid"].as<String>().c_str(), sizeof(config.ssid));
    if (doc.containsKey("password")) strlcpy(config.password, doc["password"].as<String>().c_str(), sizeof(config.password));
    if (doc.containsKey("city")) strlcpy(config.city, doc["city"].as<String>().c_str(), sizeof(config.city));
    if (doc.containsKey("timezone")) config.gmtOffset_sec = doc["timezone"].as<int>() * 3600;
    if (doc.containsKey("daylightSaving")) config.daylightOffset_sec = doc["daylightSaving"].as<bool>() ? 3600 : 0;
    if (doc.containsKey("ntpServer")) strlcpy(config.ntpServer, doc["ntpServer"].as<String>().c_str(), sizeof(config.ntpServer));
    if (doc.containsKey("api_key")) strlcpy(config.api_key, doc["api_key"].as<String>().c_str(), sizeof(config.api_key));
    if (doc.containsKey("use24hFormat")) config.use24hFormat = doc["use24hFormat"].as<bool>();
    if (doc.containsKey("units")) strlcpy(config.units, doc["units"].as<String>().c_str(), sizeof(config.units));
    if (doc.containsKey("language")) strlcpy(config.language, doc["language"].as<String>().c_str(), sizeof(config.language));
    // theme rimosso - usa /layout.json per personalizzare
    if (doc.containsKey("powerSavingEnabled")) config.powerSavingEnabled = doc["powerSavingEnabled"].as<bool>();
    if (doc.containsKey("powerSavingStartHour")) config.powerSavingStartHour = doc["powerSavingStartHour"].as<int>();
    if (doc.containsKey("powerSavingEndHour")) config.powerSavingEndHour = doc["powerSavingEndHour"].as<int>();
    if (doc.containsKey("normalUpdateInterval")) config.normalUpdateInterval = doc["normalUpdateInterval"].as<int>();
    if (doc.containsKey("powerSavingUpdateInterval")) config.powerSavingUpdateInterval = doc["powerSavingUpdateInterval"].as<int>();
    if (doc.containsKey("maxNetworkRetries")) config.maxNetworkRetries = doc["maxNetworkRetries"].as<int>();
    // Intervalli meteo
    if (doc.containsKey("weatherUpdateInterval")) config.weatherUpdateInterval = doc["weatherUpdateInterval"].as<int>();
    if (doc.containsKey("weatherPowerSavingUpdateInterval")) config.weatherPowerSavingUpdateInterval = doc["weatherPowerSavingUpdateInterval"].as<int>();
    // Intervalli refresh display (secondi)
    if (doc.containsKey("displayRefreshIntervalSec")) config.displayRefreshIntervalSec = doc["displayRefreshIntervalSec"].as<int>();
    if (doc.containsKey("displayRefreshIntervalSecPowerSaving")) config.displayRefreshIntervalSecPowerSaving = doc["displayRefreshIntervalSecPowerSaving"].as<int>();
    if (doc.containsKey("apTimeRefreshIntervalSec")) config.apTimeRefreshIntervalSec = doc["apTimeRefreshIntervalSec"].as<int>();
    // Posizione citazione
    if (doc.containsKey("quotePosX")) config.quotePosX = doc["quotePosX"].as<int>();
    if (doc.containsKey("quotePosY")) config.quotePosY = doc["quotePosY"].as<int>();
    
    // Night Mode deprecato - mantieni per compatibilità
    if (doc.containsKey("nightModeEnabled")) config.nightModeEnabled = doc["nightModeEnabled"].as<bool>();
    if (doc.containsKey("nightModeStartHour")) config.nightModeStartHour = doc["nightModeStartHour"].as<int>();
    if (doc.containsKey("nightModeEndHour")) config.nightModeEndHour = doc["nightModeEndHour"].as<int>();
    // nightModeAutoTheme, dayTheme, nightTheme rimossi
    
    // Battery Management
    if (doc.containsKey("batteryMonitorEnabled")) config.batteryMonitorEnabled = doc["batteryMonitorEnabled"].as<bool>();
    if (doc.containsKey("batteryADCPin")) config.batteryADCPin = doc["batteryADCPin"].as<int>();
    if (doc.containsKey("batteryVoltageDivider")) config.batteryVoltageDivider = doc["batteryVoltageDivider"].as<float>();
    if (doc.containsKey("batteryShowOnDisplay")) config.batteryShowOnDisplay = doc["batteryShowOnDisplay"].as<bool>();
    
    // Weather Alerts
    if (doc.containsKey("alertsEnabled")) config.alertsEnabled = doc["alertsEnabled"].as<bool>();
    if (doc.containsKey("alertTempHigh")) config.alertTempHigh = doc["alertTempHigh"].as<float>();
    if (doc.containsKey("alertTempLow")) config.alertTempLow = doc["alertTempLow"].as<float>();
    if (doc.containsKey("alertWindHigh")) config.alertWindHigh = doc["alertWindHigh"].as<float>();
    if (doc.containsKey("alertRain")) config.alertRain = doc["alertRain"].as<bool>();
    if (doc.containsKey("alertSnow")) config.alertSnow = doc["alertSnow"].as<bool>();
    if (doc.containsKey("alertStorm")) config.alertStorm = doc["alertStorm"].as<bool>();
    if (doc.containsKey("alertShowOnDisplay")) config.alertShowOnDisplay = doc["alertShowOnDisplay"].as<bool>();
    if (doc.containsKey("alertWebhookUrl")) strlcpy(config.alertWebhookUrl, doc["alertWebhookUrl"].as<String>().c_str(), sizeof(config.alertWebhookUrl));
    
    // Historical Statistics
    if (doc.containsKey("historyEnabled")) config.historyEnabled = doc["historyEnabled"].as<bool>();
    if (doc.containsKey("historyKeepDays")) config.historyKeepDays = doc["historyKeepDays"].as<int>();
    if (doc.containsKey("historyShowOnDisplay")) config.historyShowOnDisplay = doc["historyShowOnDisplay"].as<bool>();

    if (saveConfig()) {
      Serial.println("\n=======================================");
      Serial.println("   CONFIGURAZIONE SALVATA CON SUCCESSO!");
      Serial.println("   Riavvio in corso...");
      Serial.println("=======================================\n");
      
      sendJsonResponse(client, "{\"success\":true,\"message\":\"Configurazione salvata con successo\"}");
      
      // Mostra conferma visiva sul display
      showConfigSaved();
      
      delay(5000);
      WiFi.disconnect(true);
      if (apMode) { WiFi.softAPdisconnect(true); }
      delay(1000);
      ESP.restart();
    } else {
      sendJsonResponse(client, "{\"success\":false,\"message\":\"Errore nel salvataggio della configurazione\"}");
    }
    return;
  }

  // Endpoint scansione WiFi - ritorna array diretto per compatibilità frontend
  if (path == "/api/wifi/scan" && method == "GET") {
    // Assicurati che la stazione sia attiva anche in AP
    wifi_mode_t prevMode = WiFi.getMode();
    bool restoreMode = false;
    if (prevMode == WIFI_MODE_AP) {
      WiFi.mode(WIFI_MODE_APSTA);
      restoreMode = true;
    } else if (prevMode == WIFI_MODE_NULL) {
      WiFi.mode(WIFI_MODE_STA);
      restoreMode = true;
    }

    // Esegui la scansione (bloccante)
    int num = WiFi.scanNetworks();

    // Crea array diretto (non oggetto wrapper) per compatibilità script.js
    DynamicJsonDocument doc(4096);
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < num && i < 20; i++) {
      JsonObject o = arr.createNestedObject();
      o["ssid"] = WiFi.SSID(i);
      o["rssi"] = WiFi.RSSI(i);
      o["channel"] = WiFi.channel(i);
      o["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }

    // Pulisci risultati della scansione in RAM
    WiFi.scanDelete();

    // Ripristina la modalità precedente se cambiata
    if (restoreMode) {
      WiFi.mode(prevMode);
    }

    String json;
    serializeJson(doc, json);
    sendJsonResponse(client, json);
    return;
  }

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
    // theme rimosso - usa /layout.json per personalizzare
    doc["use24hFormat"] = config.use24hFormat;
    doc["units"] = config.units;
    doc["language"] = config.language;
    
    // Aggiungi parametri di risparmio energetico
    doc["powerSavingEnabled"] = config.powerSavingEnabled;
    doc["powerSavingStartHour"] = config.powerSavingStartHour;
    doc["powerSavingEndHour"] = config.powerSavingEndHour;
    doc["normalUpdateInterval"] = config.normalUpdateInterval;
    doc["powerSavingUpdateInterval"] = config.powerSavingUpdateInterval;
    
    // Night Mode deprecato - mantieni per compatibilit\u00e0
    doc["nightModeEnabled"] = config.nightModeEnabled;
    doc["nightModeStartHour"] = config.nightModeStartHour;
    doc["nightModeEndHour"] = config.nightModeEndHour;
    // nightModeAutoTheme, dayTheme, nightTheme rimossi
    
    // Aggiungi parametri di gestione errori di rete
    doc["maxNetworkRetries"] = config.maxNetworkRetries;
    doc["showLastDataOnError"] = config.showLastDataOnError;
    // Posizione citazione
    doc["quotePosX"] = config.quotePosX;
    doc["quotePosY"] = config.quotePosY;
    
    String json;
    serializeJson(doc, json);
    
    sendJsonResponse(client, json);
    return;
  }
  
  // API per il meteo
  if (path == "/api/weather" && method == "GET") {
    DynamicJsonDocument doc(JSON_BUFFER_MEDIUM);
    
    extern WeatherData currentWeather;
    
    // Struttura del JSON come la richiede il frontend
    JsonObject weather = doc.createNestedObject("weather");
    JsonObject location = doc.createNestedObject("location");
    JsonObject time = doc.createNestedObject("time");
    
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
    weather["temp"] = currentWeather.temp > 0 ? currentWeather.temp : 15.0;
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
    client.println("Content-Length: " + String(json.length()));
    client.println("Accept-Ranges: none");
    client.println("Connection: close");
    client.println();
    client.print(json);
    client.flush();
    delay(1);
    client.stop();
    return;
  }
  
  // API citazioni - restituisce la citazione attualmente mostrata sul display
  if (path == "/api/quote/current" && method == "GET") {
    Quote q = getCurrentQuote();
    DynamicJsonDocument out(256);
    out["text"] = q.text;
    out["author"] = q.author;
    String json; serializeJson(out, json);
    sendJsonResponse(client, json);
    return;
  }

  // API citazioni - restituisce tutte le citazioni organizzate per categoria
  if (path == "/api/quotes/all" && method == "GET") {
    // Inizializza SD centralizzata
    if (!initSD()) {
      sendJsonResponse(client, "{\"success\":false,\"message\":\"SD non disponibile\"}");
      return;
    }

    // Apri file citazioni
    if (!SD.exists(QUOTES_JSON_PATH)) {
      // Se non esiste, ritorna struttura vuota
      sendJsonResponse(client, "{\"success\":true,\"quotes\":{}}");
      return;
    }

    File f = SD.open(QUOTES_JSON_PATH, FILE_READ);
    if (!f) {
      sendJsonResponse(client, "{\"success\":false,\"message\":\"Impossibile aprire quotes.json\"}");
      return;
    }

    // Carica JSON usando heap per evitare stack overflow
    DynamicJsonDocument* src = new DynamicJsonDocument(12288);
    DeserializationError err = deserializeJson(*src, f);
    f.close();
    if (err) {
      delete src;
      sendJsonResponse(client, "{\"success\":false,\"message\":\"Errore parsing JSON citazioni\"}");
      return;
    }

    // Costruisci risposta compatibile UI: array piatto di oggetti {text, author, category}
    DynamicJsonDocument* out = new DynamicJsonDocument(12288);
    JsonArray quotesArr = out->createNestedArray("quotes");

    for (JsonPair kv : src->as<JsonObject>()) {
      const char* cat = kv.key().c_str();
      JsonArray arr = kv.value().as<JsonArray>();
      for (JsonVariant v : arr) {
        const char* text = v["text"] | v["quote"] | "";
        const char* author = v["author"] | "";
        
        JsonObject quoteObj = quotesArr.createNestedObject();
        quoteObj["text"] = text;
        quoteObj["author"] = author;
        quoteObj["category"] = cat;
      }
    }
    delete src; // Libera memoria sorgente

    String json;
    serializeJson(*out, json);
    delete out; // Libera memoria output
    sendJsonResponse(client, json);
    return;
  }

  // API citazioni - GET tutte le citazioni (formato array per Web GUI)
  if (path == "/api/quotes" && method == "GET") {
    if (!initSD()) {
      sendJsonResponse(client, "[]");
      return;
    }
    
    if (!SD.exists(QUOTES_JSON_PATH)) {
      sendJsonResponse(client, "[]");
      return;
    }
    
    File f = SD.open(QUOTES_JSON_PATH, FILE_READ);
    if (!f) {
      sendJsonResponse(client, "[]");
      return;
    }
    
    DynamicJsonDocument* src = new DynamicJsonDocument(16384);
    DeserializationError err = deserializeJson(*src, f);
    f.close();
    
    if (err) {
      delete src;
      sendJsonResponse(client, "[]");
      return;
    }
    
    // Converti da formato oggetto {categoria: [{text, author}]} a array [{text, author, category}]
    DynamicJsonDocument* out = new DynamicJsonDocument(16384);
    JsonArray outArr = out->to<JsonArray>();
    
    for (JsonPair kv : src->as<JsonObject>()) {
      const char* cat = kv.key().c_str();
      JsonArray arr = kv.value().as<JsonArray>();
      for (JsonVariant v : arr) {
        JsonObject quoteObj = outArr.createNestedObject();
        quoteObj["text"] = v["text"] | "";
        quoteObj["author"] = v["author"] | "";
        quoteObj["category"] = cat;
        if (v.containsKey("time")) {
          quoteObj["time"] = v["time"];
        }
      }
    }
    delete src;
    
    String json;
    serializeJson(*out, json);
    delete out;
    sendJsonResponse(client, json);
    return;
  }
  
  // API citazioni - POST salva tutte le citazioni (da formato array Web GUI a formato oggetto firmware)
  if (path == "/api/quotes" && method == "POST") {
    String body = "";
    while (client.available()) {
      body += client.readString();
    }
    
    if (body.length() == 0) {
      sendJsonResponse(client, "{\"success\":false,\"message\":\"Nessun dato ricevuto\"}");
      return;
    }
    
    DynamicJsonDocument* src = new DynamicJsonDocument(16384);
    DeserializationError err = deserializeJson(*src, body);
    if (err) {
      delete src;
      sendJsonResponse(client, "{\"success\":false,\"message\":\"Errore parsing JSON\"}");
      return;
    }
    
    if (!initSD()) {
      delete src;
      sendJsonResponse(client, "{\"success\":false,\"message\":\"SD non disponibile\"}");
      return;
    }
    
    // Converti da formato array [{text, author, category}] a formato oggetto {categoria: [{text, author}]}
    DynamicJsonDocument* out = new DynamicJsonDocument(16384);
    
    JsonArray srcArr = src->as<JsonArray>();
    for (JsonVariant v : srcArr) {
      String cat = v["category"] | "motivazione";
      cat = mapUiCategoryToFirmware(cat);
      
      // Crea categoria se non esiste
      if (!out->containsKey(cat)) {
        out->createNestedArray(cat);
      }
      
      JsonArray catArr = (*out)[cat].as<JsonArray>();
      JsonObject quoteObj = catArr.createNestedObject();
      quoteObj["text"] = v["text"] | "";
      if (v.containsKey("author") && strlen(v["author"] | "") > 0) {
        quoteObj["author"] = v["author"];
      }
      if (v.containsKey("time") && strlen(v["time"] | "") > 0) {
        quoteObj["time"] = v["time"];
      }
    }
    delete src;
    
    // Salva su file
    File f = SD.open(QUOTES_JSON_PATH, FILE_WRITE);
    if (!f) {
      delete out;
      sendJsonResponse(client, "{\"success\":false,\"message\":\"Impossibile scrivere quotes.json\"}");
      return;
    }
    serializeJson(*out, f);
    f.close();
    delete out;
    
    Serial.println("[WEB] Citazioni salvate su /quotes.json (convertite in formato firmware)");
    sendJsonResponse(client, "{\"success\":true}");
    return;
  }

  // API citazioni - aggiorna una citazione (PUT /api/quotes/{index})
  if (path.startsWith("/api/quotes/") && method == "PUT") {
    // Estrai indice dall'URL
    int lastSlash = path.lastIndexOf('/');
    int index = path.substring(lastSlash + 1).toInt();
    
    String body = "";
    while (client.available()) {
      body += client.readString();
    }
    if (body.length() == 0) {
      sendJsonResponse(client, "{\"success\":false,\"message\":\"Nessun dato ricevuto\"}");
      return;
    }

    DynamicJsonDocument doc(1024);
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
      sendJsonResponse(client, "{\"success\":false,\"message\":\"Errore nel parsing JSON\"}");
      return;
    }

    String text = doc["text"].as<String>();
    String author = doc.containsKey("author") ? doc["author"].as<String>() : String("");
    String category = doc["category"].as<String>();
    
    text.trim();
    author.trim();
    category.trim();
    
    if (text.length() == 0 || category.length() == 0) {
      sendJsonResponse(client, "{\"success\":false,\"message\":\"Campi text e category obbligatori\"}");
      return;
    }

    if (!initSD()) {
      sendJsonResponse(client, "{\"success\":false,\"message\":\"SD non disponibile\"}");
      return;
    }

    if (!SD.exists(QUOTES_JSON_PATH)) {
      sendJsonResponse(client, "{\"success\":false,\"message\":\"File citazioni inesistente\"}");
      return;
    }

    // Carica file corrente
    DynamicJsonDocument* db = new DynamicJsonDocument(12288);
    File rf = SD.open(QUOTES_JSON_PATH, FILE_READ);
    if (!rf) {
      delete db;
      sendJsonResponse(client, "{\"success\":false,\"message\":\"Impossibile aprire quotes.json\"}");
      return;
    }
    deserializeJson(*db, rf);
    rf.close();

    // Trova e aggiorna la citazione all'indice specificato (conteggio globale)
    int currentIndex = 0;
    bool found = false;
    String foundCat = "";
    int catIndex = 0;

    for (JsonPair kv : db->as<JsonObject>()) {
      JsonArray arr = kv.value().as<JsonArray>();
      for (int i = 0; i < arr.size(); i++) {
        if (currentIndex == index) {
          found = true;
          foundCat = kv.key().c_str();
          catIndex = i;
          break;
        }
        currentIndex++;
      }
      if (found) break;
    }

    if (!found) {
      delete db;
      sendJsonResponse(client, "{\"success\":false,\"message\":\"Citazione non trovata\"}");
      return;
    }

    // Rimuovi dalla categoria vecchia
    JsonArray oldArr = (*db)[foundCat].as<JsonArray>();
    oldArr.remove(catIndex);

    // Aggiungi alla nuova categoria
    if (!db->containsKey(category) || !(*db)[category].is<JsonArray>()) {
      db->remove(category);
      db->createNestedArray(category);
    }
    JsonArray newArr = (*db)[category].as<JsonArray>();
    JsonObject o = newArr.createNestedObject();
    o["text"] = text;
    if (author.length() > 0) o["author"] = author;

    // Salva
    File wf = SD.open(QUOTES_JSON_PATH, FILE_WRITE);
    if (!wf) {
      delete db;
      sendJsonResponse(client, "{\"success\":false,\"message\":\"Impossibile scrivere quotes.json\"}");
      return;
    }
    serializeJson(*db, wf);
    wf.close();
    delete db;

    sendJsonResponse(client, "{\"success\":true,\"message\":\"Citazione aggiornata\"}");
    return;
  }

  // API citazioni - elimina una citazione (DELETE /api/quotes/{index})
  if (path.startsWith("/api/quotes/") && method == "DELETE") {
    // Estrai indice dall'URL
    int lastSlash = path.lastIndexOf('/');
    int index = path.substring(lastSlash + 1).toInt();

    if (!initSD()) {
      sendJsonResponse(client, "{\"success\":false,\"message\":\"SD non disponibile\"}");
      return;
    }

    if (!SD.exists(QUOTES_JSON_PATH)) {
      sendJsonResponse(client, "{\"success\":false,\"message\":\"File citazioni inesistente\"}");
      return;
    }

    // Carica file corrente
    DynamicJsonDocument* db = new DynamicJsonDocument(12288);
    File rf = SD.open(QUOTES_JSON_PATH, FILE_READ);
    if (!rf) {
      delete db;
      sendJsonResponse(client, "{\"success\":false,\"message\":\"Impossibile aprire quotes.json\"}");
      return;
    }
    deserializeJson(*db, rf);
    rf.close();

    // Trova e rimuovi la citazione all'indice specificato (conteggio globale)
    int currentIndex = 0;
    bool found = false;
    
    for (JsonPair kv : db->as<JsonObject>()) {
      JsonArray arr = kv.value().as<JsonArray>();
      for (int i = 0; i < arr.size(); i++) {
        if (currentIndex == index) {
          arr.remove(i);
          found = true;
          break;
        }
        currentIndex++;
      }
      if (found) break;
    }

    if (!found) {
      delete db;
      sendJsonResponse(client, "{\"success\":false,\"message\":\"Citazione non trovata\"}");
      return;
    }

    // Salva
    File wf = SD.open(QUOTES_JSON_PATH, FILE_WRITE);
    if (!wf) {
      delete db;
      sendJsonResponse(client, "{\"success\":false,\"message\":\"Impossibile scrivere quotes.json\"}");
      return;
    }
    serializeJson(*db, wf);
    wf.close();
    delete db;

    sendJsonResponse(client, "{\"success\":true,\"message\":\"Citazione eliminata\"}");
    return;
  }

  // API citazioni - elimina una citazione per indice (OLD endpoint - mantienilo per compatibilità)
  if (path == "/api/quotes/delete" && method == "POST") {
    String body = "";
    while (client.available()) {
      body += client.readString();
    }
    if (body.length() == 0) {
      sendJsonResponse(client, "{\"success\":false,\"message\":\"Nessun dato ricevuto\"}");
      return;
    }

    DynamicJsonDocument doc(512);
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
      sendJsonResponse(client, "{\"success\":false,\"message\":\"Errore nel parsing JSON\"}");
      return;
    }

    String uiCat = doc["category"].as<String>();
    String cat = mapUiCategoryToFirmware(uiCat);
    int index = doc["index"].as<int>();
    if (index < 0) {
      sendJsonResponse(client, "{\"success\":false,\"message\":\"Indice non valido\"}");
      return;
    }

    if (!initSD()) {
      sendJsonResponse(client, "{\"success\":false,\"message\":\"SD non disponibile\"}");
      return;
    }

    if (!SD.exists(QUOTES_JSON_PATH)) {
      sendJsonResponse(client, "{\"success\":false,\"message\":\"File citazioni inesistente\"}");
      return;
    }

    DynamicJsonDocument* db = new DynamicJsonDocument(12288);
    File rf = SD.open(QUOTES_JSON_PATH, FILE_READ);
    if (!rf) {
      delete db;
      sendJsonResponse(client, "{\"success\":false,\"message\":\"Impossibile aprire quotes.json\"}");
      return;
    }
    DeserializationError perr = deserializeJson(*db, rf);
    rf.close();
    if (perr) {
      delete db;
      sendJsonResponse(client, "{\"success\":false,\"message\":\"Errore parsing JSON citazioni\"}");
      return;
    }

    if (!db->containsKey(cat) || !(*db)[cat].is<JsonArray>()) {
      delete db;
      sendJsonResponse(client, "{\"success\":false,\"message\":\"Categoria non trovata\"}");
      return;
    }
    JsonArray arr = (*db)[cat].as<JsonArray>();
    if (index >= (int)arr.size()) {
      delete db;
      sendJsonResponse(client, "{\"success\":false,\"message\":\"Indice fuori range\"}");
      return;
    }

    // Rimuovi elemento
    arr.remove(index);

    // Salva su file
    File wf = SD.open(QUOTES_JSON_PATH, FILE_WRITE);
    if (!wf) {
      delete db;
      sendJsonResponse(client, "{\"success\":false,\"message\":\"Impossibile scrivere quotes.json\"}");
      return;
    }
    serializeJson(*db, wf);
    wf.close();
    delete db;

    sendJsonResponse(client, "{\"success\":true,\"message\":\"Citazione eliminata\"}");
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
    if (!initSD()) {
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
    // Intervalli refresh display (secondi)
    if (doc.containsKey("displayRefreshIntervalSec")) {
      config.displayRefreshIntervalSec = doc["displayRefreshIntervalSec"].as<int>();
    }
    if (doc.containsKey("displayRefreshIntervalSecPowerSaving")) {
      config.displayRefreshIntervalSecPowerSaving = doc["displayRefreshIntervalSecPowerSaving"].as<int>();
    }
    if (doc.containsKey("apTimeRefreshIntervalSec")) {
      config.apTimeRefreshIntervalSec = doc["apTimeRefreshIntervalSec"].as<int>();
    }
    // Posizione citazione
    if (doc.containsKey("quotePosX")) {
      config.quotePosX = doc["quotePosX"].as<int>();
    }
    if (doc.containsKey("quotePosY")) {
      config.quotePosY = doc["quotePosY"].as<int>();
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
      Serial.println("\n=======================================");
      Serial.println("   IMPOSTAZIONI SALVATE CON SUCCESSO!");
      Serial.println("   Riavvio in corso...");
      Serial.println("=======================================\n");
      
      sendJsonResponse(client, "{\"success\":true,\"message\":\"Configurazione salvata con successo\"}");
      
      // Mostra conferma visiva sul display
      showConfigSaved();
      
      // Attendi per essere sicuri che la risposta venga inviata
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
  
  // Gestione pagina Layout Editor
  if (path == "/layout" && method == "GET") {
    // Serve layout.html se esiste, altrimenti errore
    if (!serveFileFromSD(client, "/layout.html")) {
       sendResponse(client, "text/plain", "Layout editor file not found. Please copy layout.html to /www/ on SD card.", 404);
    }
    return;
  }

  // Gestione pagina Editor Citazioni
  if (path == "/quotes" && method == "GET") {
    // Serve quotes.html se esiste
    if (!serveFileFromSD(client, "/quotes.html")) {
       sendResponse(client, "text/plain", "Quotes editor file (quotes.html) not found on SD card.", 404);
    }
    return;
  }

  // API Layout: GET (Carica)
  if (path == "/api/layout" && method == "GET") {
      if (!initSD()) {
          sendJsonResponse(client, "{\"success\":false,\"message\":\"SD Error\"}");
          return;
      }
      if (SD.exists(LAYOUT_JSON_PATH)) {
          File f = SD.open(LAYOUT_JSON_PATH, FILE_READ);
          if (f) {
              String json = f.readString();
              f.close();
              sendJsonResponse(client, json);
          } else {
             sendJsonResponse(client, "{}");
          }
      } else {
          // Default layout JSON se non esiste - mostra layout di esempio
          String defaultLayout = R"({
  "grid": {
    "cols": 24,
    "row_height": 20,
    "margin": 4
  },
  "items": [
    {
      "id": "city_1",
      "type": "city",
      "x": 1,
      "y": 0,
      "w": 10,
      "h": 2,
      "z": 1,
      "visible": true,
      "font_size": 16,
      "align": "left"
    },
    {
      "id": "weather_icon_1",
      "type": "weather_icon",
      "x": 1,
      "y": 2,
      "w": 8,
      "h": 8,
      "z": 1,
      "visible": true,
      "icon_size": 160
    },
    {
      "id": "temperature_1",
      "type": "temperature",
      "x": 10,
      "y": 3,
      "w": 6,
      "h": 2,
      "z": 1,
      "visible": true,
      "font_size": 18
    },
    {
      "id": "humidity_1",
      "type": "humidity",
      "x": 10,
      "y": 6,
      "w": 5,
      "h": 2,
      "z": 1,
      "visible": true,
      "font_size": 14
    },
    {
      "id": "pressure_1",
      "type": "pressure",
      "x": 16,
      "y": 3,
      "w": 6,
      "h": 2,
      "z": 1,
      "visible": true,
      "font_size": 14
    },
    {
      "id": "wind_1",
      "type": "wind",
      "x": 16,
      "y": 6,
      "w": 6,
      "h": 2,
      "z": 1,
      "visible": true,
      "font_size": 14
    },
    {
      "id": "quote_1",
      "type": "quote",
      "x": 1,
      "y": 10,
      "w": 22,
      "h": 4,
      "z": 1,
      "visible": true,
      "font_size": 12
    },
    {
      "id": "footer_1",
      "type": "footer_bar",
      "x": 0,
      "y": 21,
      "w": 24,
      "h": 3,
      "z": 1,
      "visible": true,
      "font_size": 10
    }
  ]
})";
          sendJsonResponse(client, defaultLayout);
      }
      return;
  }

  // API Layout: POST (Salva)
  if (path == "/api/layout" && method == "POST") {
      String body = "";
      while (client.available()) body += client.readString();
      
      if (body.length() == 0) {
          sendJsonResponse(client, "{\"success\":false,\"message\":\"Empty body\"}");
          return;
      }

      // Parsing per validazione minima
      DynamicJsonDocument doc(2048);
      DeserializationError err = deserializeJson(doc, body);
      if (err) {
          sendJsonResponse(client, "{\"success\":false,\"message\":\"Invalid JSON\"}");
          return;
      }

      if (!initSD()) {
          sendJsonResponse(client, "{\"success\":false,\"message\":\"SD Error\"}");
          return;
      }

      // Salva su file
      File f = SD.open(LAYOUT_JSON_PATH, FILE_WRITE);
      if (f) {
          serializeJson(doc, f);
          f.close();
          
          // Attiva modalità custom
          g_displayMode = "custom";
          Serial.println("[WEB] Layout salvato. Modalità display impostata su 'custom'");
          
          // Forza aggiornamento display immediato
          lastDisplayUpdate = 0;
          
          sendJsonResponse(client, "{\"success\":true}");
      } else {
          sendJsonResponse(client, "{\"success\":false,\"message\":\"Write failed\"}");
      }
      return;
  }

  // Servizio file dalla SD
  bool fileServed = false;
  
  // In modalità AP, reindirizza alla pagina di configurazione
  if (apMode) {
    // Consenti asset statici anche se in root e con estensioni comuni
    bool isStaticAsset = path.startsWith("/css/") || path.startsWith("/js/") || path.startsWith("/img/") ||
                         path.endsWith(".css") || path.endsWith(".js") || path.endsWith(".svg") ||
                         path.endsWith(".png") || path.endsWith(".jpg") || path.endsWith(".jpeg") ||
                         path.endsWith(".ico") || path.endsWith(".webp") || path.endsWith(".gif");
    if (path != "/settings.html" && path != "/api/wifi-scan" && !isStaticAsset) {
      // Reindirizza alla pagina di configurazione esistente su SD
      client.println("HTTP/1.1 302 Found");
      client.println("Location: /settings.html");
      client.println("Connection: close");
      client.println();
      client.flush();
      delay(1);
      client.stop();
      return;
    }
    
    // Serve solo file dalla SD senza generazione dinamica
    fileServed = serveFileFromSD(client, path);
    if (!fileServed) {
      // Verifica stato SD e presenza cartella /www
      bool sdOk = initSD();
      bool wwwExists = sdOk && SD.exists("/www");
      bool isSettings = (path == "/settings.html" || path == "/");
      if (!wwwExists || isSettings) {
        sendMissingAssetsPage(client, sdOk, wwwExists);
      } else {
        // File non trovato - 404 base
        const char* msg = "404 - File not found";
        client.println("HTTP/1.1 404 Not Found");
        client.println("Content-Type: text/plain");
        client.println("Content-Length: " + String(strlen(msg)));
        client.println("Connection: close");
        client.println();
        client.print(msg);
        client.flush();
        delay(1);
        client.stop();
      }
    }
  } else {
    // Modalità normale, servi file dalla SD
    fileServed = serveFileFromSD(client, path);
    if (!fileServed) {
      // Verifica stato SD e presenza cartella /www
      bool sdOk = initSD();
      bool wwwExists = sdOk && SD.exists("/www");
      bool isHomeOrSettings = (path == "/" || path == "/index.html" || path == "/settings.html");
      if (!wwwExists || isHomeOrSettings) {
        sendMissingAssetsPage(client, sdOk, wwwExists);
      } else {
        // 404 semplice per altre risorse
        const char* msg = "404 - File not found";
        client.println("HTTP/1.1 404 Not Found");
        client.println("Content-Type: text/plain");
        client.println("Content-Length: " + String(strlen(msg)));
        client.println("Connection: close");
        client.println();
        client.print(msg);
        client.flush();
        delay(1);
        client.stop();
      }
    }
  }
}

// Invia una risposta HTTP generica
void sendResponse(WiFiClient& client, const String& contentType, const String& content, int statusCode) {
  client.println("HTTP/1.1 " + String(statusCode) + " OK");
  client.println("Content-Type: " + contentType);
  client.println("Content-Length: " + String(content.length()));
  client.println("Accept-Ranges: none");
  client.println("Connection: close");
  client.println();
  client.print(content);
  client.flush();
  delay(1);
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
  client.flush();
  delay(1);
  client.stop();
}

// Gestisce la scansione WiFi e restituisce i risultati in JSON
void performWiFiScan(WiFiClient& client) {
  // Prepara documento JSON per la risposta
  DynamicJsonDocument doc(4096);
  JsonArray networks = doc.createNestedArray("networks");

  // Se siamo in sola modalità AP, passiamo temporaneamente ad AP+STA per permettere la scansione
  wifi_mode_t prevMode = WiFi.getMode();
  bool switched = false;
  if (prevMode == WIFI_MODE_AP) {
    WiFi.mode(WIFI_MODE_APSTA);
    delay(100);
    switched = true;
  }

  // Avvia la scansione WiFi
  int numNetworks = WiFi.scanNetworks();

  // Ripristina la modalità precedente se è stata cambiata
  if (switched) {
    WiFi.mode(prevMode);
    delay(50);
  }

  if (numNetworks < 0) {
    // Nessun risultato o errore: rispondi con lista vuota
    String json;
    serializeJson(doc, json);
    sendJsonResponse(client, json);
    return;
  }

  // Aggiungi ogni rete al documento JSON (limite 20)
  for (int i = 0; i < numNetworks && i < 20; i++) {
    JsonObject network = networks.createNestedObject();
    network["ssid"] = WiFi.SSID(i);
    network["rssi"] = WiFi.RSSI(i);
    network["channel"] = WiFi.channel(i);
    network["encryption"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "secured" : "open";
  }

  // Invia la risposta
  String json;
  serializeJson(doc, json);
  sendJsonResponse(client, json);
}
