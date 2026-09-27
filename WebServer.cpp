#include "WebServer.h"
#include "NetworkUtils.h"
#include "Config.h"
#include "WeatherUtils.h"
#include "WebMinimal.h"
#include "Hardware.h"
#include "QuotesManager.h"
#include "Display.h"
#include "Updater.h"
#include "BatteryManager.h"
#include "Version.h"
#include "EcoPower.h"
#include "AtmoVerseConstants.h" // Aggiunto per costanti JSON se necessarie
#include <SD.h>
#include <ArduinoJson.h>
#include <WiFi.h>

// Legge il corpo di una richiesta POST fino a Content-Length (il corpo può
// arrivare dopo le intestazioni, in un pacchetto successivo). Max 3 s.
static String readRequestBody(WiFiClient& client, int contentLength, int maxBody = 16384) {
  const int MAX_BODY = maxBody;
  String body;
  if (contentLength > MAX_BODY) contentLength = MAX_BODY;
  if (contentLength > 0 && !body.reserve(contentLength)) return body;  // Memoria insufficiente
  unsigned long deadline = millis() + 3000;
  while (client.connected() && millis() < deadline) {
    while (client.available()) {
      body += (char)client.read();
      if (contentLength > 0 && (int)body.length() >= contentLength) return body;
      if ((int)body.length() >= MAX_BODY) return body;
    }
    if (contentLength <= 0 && body.length() > 0) break;
    delay(5);
  }
  return body;
}

// Variabile definita nel file principale per il controllo del refresh display
extern unsigned long lastDisplayUpdate;

// Valore di un parametro della query string ("h=08&fit=1"), vuoto se assente
static String queryParam(const String& params, const char* name) {
  String key = String(name) + "=";
  int start = 0;
  while (start < (int)params.length()) {
    int end = params.indexOf('&', start);
    if (end < 0) end = params.length();
    if (params.substring(start, start + key.length()) == key) {
      return params.substring(start + key.length(), end);
    }
    start = end + 1;
  }
  return "";
}

// Orologio letterario: un file per ora, /orari/00.txt ... /orari/23.txt.
// L'editor legge e salva un'ora alla volta (max ~64 KB), mai tutti i file
// insieme: in totale sono centinaia di KB, troppi per la RAM della scheda.
static const int ORARI_MAX_FILE = 65536;

// Ora valida "0".."23" -> percorso del file, altrimenti stringa vuota
static String orariPath(const String& h) {
  if (h.length() == 0 || h.length() > 2) return "";
  for (size_t i = 0; i < h.length(); i++) {
    if (!isDigit(h[i])) return "";
  }
  int hour = h.toInt();
  if (hour < 0 || hour > 23) return "";
  char path[16];
  snprintf(path, sizeof(path), "/orari/%02d.txt", hour);
  return String(path);
}

// GET /api/orari: per ogni ora quante citazioni e quanti minuti coperti
static void sendOrariSummary(WiFiClient& client) {
  String json = "{\"hours\":[";
  int totalQuotes = 0, totalMinutes = 0;
  for (int hour = 0; hour < 24; hour++) {
    char path[16];
    snprintf(path, sizeof(path), "/orari/%02d.txt", hour);
    bool minutes[60] = {false};
    int count = 0;
    File f = SD.open(path, FILE_READ);
    if (f) {
      // Basta leggere i primi 3 caratteri di ogni riga ("MM|")
      bool lineStart = true;
      char mm[3];
      int pos = 0;
      while (f.available()) {
        char c = f.read();
        if (c == '\n') { lineStart = true; pos = 0; continue; }
        if (!lineStart) continue;
        if (pos < 2) { mm[pos++] = c; continue; }
        lineStart = false;
        if (c == '|' && isDigit(mm[0]) && isDigit(mm[1])) {
          int m = (mm[0] - '0') * 10 + (mm[1] - '0');
          if (m < 60) { minutes[m] = true; count++; }
        }
      }
      f.close();
    }
    int covered = 0;
    for (int m = 0; m < 60; m++) covered += minutes[m];
    totalQuotes += count;
    totalMinutes += covered;
    if (hour) json += ",";
    json += "{\"h\":" + String(hour) + ",\"count\":" + String(count) + ",\"minutes\":" + String(covered) + "}";
  }
  json += "],\"total\":" + String(totalQuotes) + ",\"covered\":" + String(totalMinutes) + "}";
  sendJsonResponse(client, json);
}

// GET /api/orari?h=8: il file dell'ora così com'è, inviato a pezzi senza caricarlo in RAM
static void sendOrariHour(WiFiClient& client, const String& path) {
  File f = SD.open(path, FILE_READ);
  size_t size = f ? f.size() : 0;
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/plain; charset=utf-8");
  client.println("Content-Length: " + String(size));
  client.println("Cache-Control: no-store");
  client.println("Connection: close");
  client.println();
  if (f) {
    uint8_t buf[512];
    while (f.available()) {
      int n = f.read(buf, sizeof(buf));
      if (n <= 0) break;
      client.write(buf, n);
    }
    f.close();
  }
  client.flush();
  delay(1);
  client.stop();
}

// GET /api/orari?h=8&fit=1: per ogni riga del file, se il display la mostra per intero
static void sendOrariFit(WiFiClient& client, const String& path) {
  String json = "{\"fit\":[";
  File f = SD.open(path, FILE_READ);
  bool first = true;
  if (f) {
    while (f.available()) {
      String line = f.readStringUntil('\n');
      line.trim();
      if (line.length() == 0) continue;
      int sep = line.indexOf('|', 3);
      bool ok = line.length() > 3 && line[2] == '|' && sep > 3 &&
                clockQuoteFits(line.substring(3, sep), line.substring(sep + 1));
      json += first ? "" : ",";
      json += ok ? "true" : "false";
      first = false;
    }
    f.close();
  }
  json += "]}";
  sendJsonResponse(client, json);
}

// POST /api/quotes/raw: quotes.json già nel formato del firmware
// ({"categoria": [{text, author, ...}]}), scritto sulla SD mentre arriva.
// La raccolta supera facilmente i 40 KB: tenerla in RAM e convertirla
// esauriva la memoria. Il file viene verificato (JSON valido con un oggetto
// di categorie) leggendolo dalla SD senza caricarlo, poi sostituisce quello vecchio.
static const int QUOTES_MAX_FILE = 256 * 1024;

static void saveQuotesRaw(WiFiClient& client, int contentLength) {
  if (contentLength <= 0 || contentLength > QUOTES_MAX_FILE) {
    sendJsonResponse(client, "{\"success\":false,\"message\":\"File troppo grande o vuoto\"}");
    return;
  }
  const char* tmp = "/quotes.tmp";
  SD.remove(tmp);
  File f = SD.open(tmp, FILE_WRITE);
  if (!f) {
    sendJsonResponse(client, "{\"success\":false,\"message\":\"Impossibile scrivere sulla SD\"}");
    return;
  }
  int received = 0;
  bool writeOk = true;
  uint8_t buf[512];
  unsigned long lastData = millis();
  while (received < contentLength && millis() - lastData < 5000) {
    int avail = client.available();
    if (avail <= 0) {
      if (!client.connected()) break;
      delay(2);
      continue;
    }
    int n = client.read(buf, min((int)sizeof(buf), min(avail, contentLength - received)));
    if (n <= 0) continue;
    if (f.write(buf, n) != (size_t)n) writeOk = false;
    received += n;
    lastData = millis();
  }
  f.close();
  if (!writeOk || received != contentLength) {
    SD.remove(tmp);
    sendJsonResponse(client, writeOk ? "{\"success\":false,\"message\":\"Dati incompleti\"}"
                                     : "{\"success\":false,\"message\":\"SD piena o non scrivibile\"}");
    return;
  }

  // Verifica senza caricare il contenuto: il filtro non tiene nessun campo
  File in = SD.open(tmp, FILE_READ);
  JsonDocument filter;
  filter["__nessuno__"] = true;
  JsonDocument check;
  DeserializationError err = in ? deserializeJson(check, in, DeserializationOption::Filter(filter))
                                : DeserializationError::InvalidInput;
  if (in) in.close();
  if (err || !check.is<JsonObject>()) {
    SD.remove(tmp);
    sendJsonResponse(client, "{\"success\":false,\"message\":\"JSON non valido\"}");
    return;
  }
  SD.remove("/quotes.json");  // QUOTES_JSON_PATH è definito più avanti nel file
  if (!SD.rename(tmp, "/quotes.json")) {
    sendJsonResponse(client, "{\"success\":false,\"message\":\"Impossibile sostituire quotes.json\"}");
    return;
  }
  Serial.printf("[WEB] Citazioni salvate su /quotes.json (%d byte)\n", received);
  lastDisplayUpdate = 0;
  sendJsonResponse(client, "{\"success\":true}");
}

// POST /api/orari?h=8: il corpo (testo, righe "MM|testo|Autore, Opera") va
// direttamente su un file temporaneo e sostituisce quello dell'ora solo se
// è arrivato per intero
static void saveOrariHour(WiFiClient& client, const String& path, int contentLength) {
  if (contentLength < 0 || contentLength > ORARI_MAX_FILE) {
    sendJsonResponse(client, "{\"success\":false,\"message\":\"File troppo grande (max 64 KB per ora)\"}");
    return;
  }
  if (!SD.exists("/orari")) SD.mkdir("/orari");
  String tmp = path.substring(0, path.length() - 4) + ".tmp";
  SD.remove(tmp);
  File f = SD.open(tmp, FILE_WRITE);
  if (!f) {
    sendJsonResponse(client, "{\"success\":false,\"message\":\"Impossibile scrivere sulla SD\"}");
    return;
  }
  int received = 0;
  bool writeOk = true;
  uint8_t buf[512];
  unsigned long lastData = millis();
  while (received < contentLength && millis() - lastData < 5000) {
    int avail = client.available();
    if (avail <= 0) {
      if (!client.connected()) break;
      delay(2);
      continue;
    }
    int n = client.read(buf, min((int)sizeof(buf), min(avail, contentLength - received)));
    if (n <= 0) continue;
    // Il separatore di riga del firmware è '\n': i '\r' si scartano
    for (int i = 0; i < n; i++) {
      if (buf[i] != '\r' && f.write(buf[i]) != 1) writeOk = false;
    }
    received += n;
    lastData = millis();
  }
  f.close();
  if (!writeOk || received != contentLength) {
    SD.remove(tmp);
    sendJsonResponse(client, writeOk ? "{\"success\":false,\"message\":\"Dati incompleti\"}"
                                     : "{\"success\":false,\"message\":\"SD piena o non scrivibile\"}");
    return;
  }
  SD.remove(path);
  if (!SD.rename(tmp, path)) {
    sendJsonResponse(client, "{\"success\":false,\"message\":\"Impossibile sostituire il file\"}");
    return;
  }
  Serial.printf("[WEB] Orologio letterario salvato: %s (%d byte)\n", path.c_str(), received);
  lastDisplayUpdate = 0;  // Se è l'ora corrente, il display la usa subito
  sendJsonResponse(client, "{\"success\":true}");
}

// File citazioni su SD
static const char* QUOTES_JSON_PATH = "/quotes.json";

// Funzione helper per convertire carattere esadecimale in valore
static int hexCharToValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
  if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
  return -1;  // Carattere non valido
}

// Funzione per decodificare l'URL (traduce caratteri come %20 in spazi)
String urldecode(const String& str) {
  String ret;
  size_t len = str.length();
  ret.reserve(len);  // Pre-alloca per massima dimensione possibile
  
  for (size_t i = 0; i < len; i++) {
    if (str[i] == '+') {
      ret += ' ';
    } else if (str[i] == '%') {
      // Verifica che ci siano almeno 2 caratteri dopo %
      if (i + 2 >= len) {
        // Sequenza % incompleta, tratta come carattere normale
        ret += str[i];
        continue;
      }
      
      int highNibble = hexCharToValue(str[i+1]);
      int lowNibble = hexCharToValue(str[i+2]);
      
      // Verifica che entrambi i caratteri siano esadecimali validi
      if (highNibble >= 0 && lowNibble >= 0) {
        ret += (char)((highNibble << 4) | lowNibble);
        i += 2;  // Salta i due caratteri esadecimali
      } else {
        // Sequenza % non valida, tratta % come carattere normale
        ret += str[i];
      }
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

// Pagine web assenti (SD nuova, vuota o non inserita): si mostra la pagina di
// configurazione minima del firmware. Dopo la configurazione le pagine
// complete vengono scaricate da GitHub (vedi Updater.cpp), se c'è la SD.
static void sendMissingAssetsPage(WiFiClient& client, bool sdOk, bool wwwExists) {
  sendFallbackSetupPage(client);
}

// Gestione delle richieste dei client
void handleClientRequests() {
  // Gestisci il DNS server per captive portal se in modalità AP
  if (apMode) {
    dnsServer.processNextRequest();
  }
  
  // WiFi spento dal risparmio energetico: le strutture di rete sono state
  // liberate e interrogare il server manderebbe in crash la scheda
  if (!apMode && WiFi.getMode() == WIFI_OFF) {
    return;
  }

  // Verifica se ci sono client che si connettono
  WiFiClient client = server.available();
  if (!client) {
    return;
  }
  ecoNoteWebActivity();  // A batteria: la pagina web resta attiva finché la si usa

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
  int contentLength = 0;
  while (client.available()) {
    String line = client.readStringUntil('\r');
    client.readStringUntil('\n');
    if (line.length() == 0) break;
    if (line.length() > 15 && line.substring(0, 15).equalsIgnoreCase("Content-Length:")) {
      contentLength = line.substring(15).toInt();
    }
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

  if (path == "/quotes.json" && method == "GET") {
    if (!initSD()) {
      sendJsonResponse(client, "{\"error\":\"SD non disponibile\"}", 503);
      return;
    }
    if (!SD.exists(QUOTES_JSON_PATH)) {
      sendJsonResponse(client, "{\"error\":\"quotes.json non trovato\"}", 404);
      return;
    }
    File f = SD.open(QUOTES_JSON_PATH, FILE_READ);
    if (!f) {
      sendJsonResponse(client, "{\"error\":\"Impossibile aprire quotes.json\"}", 500);
      return;
    }
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Content-Length: " + String(f.size()));
    client.println("Cache-Control: no-store");
    client.println("Connection: close");
    client.println();
    uint8_t buffer[512];
    while (f.available()) {
      size_t n = f.read(buffer, sizeof(buffer));
      client.write(buffer, n);
    }
    f.close();
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
  // Controllo aggiornamenti richiesto dalla pagina web (eseguito dal loop)
  if (path == "/api/update/check" && method == "POST") {
    requestUpdateCheck();
    sendJsonResponse(client, "{\"success\":true,\"message\":\"Controllo aggiornamenti avviato\"}");
    return;
  }
  
  // Stato della batteria (INA219), di sola lettura
  if (path == "/api/battery" && method == "GET") {
    JsonDocument b;
    b["available"] = battery.isAvailable();
    if (battery.isAvailable()) {
      b["voltage"] = battery.getVoltage();
      b["current_mA"] = battery.getCurrent();      // positiva = scarica
      b["percent"] = battery.getPercentage();
      b["charging"] = battery.charging();
      b["level"] = battery.getLevel() == BATTERY_LEVEL_CRITICAL ? "critical"
                 : battery.getLevel() == BATTERY_LEVEL_LOW ? "low" : "ok";
      b["remaining_min"] = battery.getEstimatedTimeRemaining();  // -1 = non stimabile
    }
    b["showOnDisplay"] = config.batteryShowOnDisplay;
    String json;
    serializeJson(b, json);
    sendJsonResponse(client, json);
    return;
  }

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
    // L API key non viene mai restituita: la pagina sa solo se è impostata
    doc["api_key_set"] = strlen(config.api_key) > 0;
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
    doc["maxNetworkRetries"] = config.maxNetworkRetries;
    // Intervalli meteo
    // Intervalli refresh display (secondi)
    doc["displayRefreshIntervalSec"] = config.displayRefreshIntervalSec;
    doc["displayRefreshIntervalSecPowerSaving"] = config.displayRefreshIntervalSecPowerSaving;
    doc["apMode"] = apMode;
    doc["ipAddress"] = apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
    
    // Night Mode
    
    // Battery Management
    doc["batteryShowOnDisplay"] = config.batteryShowOnDisplay;
    
    

    String json;
    serializeJson(doc, json);
    sendJsonResponse(client, json);
    return;
  }

  // Alias compatibilità: POST /api/config (accetta i campi previsti dallo script)
  if (path == "/api/config" && method == "POST") {
    String body = readRequestBody(client, contentLength);
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
    // Password vuota = mantieni quella salvata, tranne quando cambia la rete
    // (allora vuota significa rete aperta)
    String newSsid = doc["ssid"] | "";
    String newPassword = doc["password"] | "";
    bool ssidChanged = newSsid.length() > 0 && newSsid != config.ssid;
    if (newSsid.length() > 0) strlcpy(config.ssid, newSsid.c_str(), sizeof(config.ssid));
    if (newPassword.length() > 0 || ssidChanged) strlcpy(config.password, newPassword.c_str(), sizeof(config.password));
    if (doc.containsKey("city")) strlcpy(config.city, doc["city"].as<String>().c_str(), sizeof(config.city));
    if (doc.containsKey("timezone")) config.gmtOffset_sec = doc["timezone"].as<int>() * 3600;
    if (doc.containsKey("daylightSaving")) config.daylightOffset_sec = doc["daylightSaving"].as<bool>() ? 3600 : 0;
    if (doc.containsKey("ntpServer")) strlcpy(config.ntpServer, doc["ntpServer"].as<String>().c_str(), sizeof(config.ntpServer));
    // API key vuota = mantieni quella salvata (non viene più inviata alle pagine)
    if ((doc["api_key"] | "")[0] != '\0') strlcpy(config.api_key, doc["api_key"].as<String>().c_str(), sizeof(config.api_key));
    if (doc.containsKey("use24hFormat")) config.use24hFormat = doc["use24hFormat"].as<bool>();
    if (doc.containsKey("units")) strlcpy(config.units, doc["units"].as<String>().c_str(), sizeof(config.units));
    if (doc.containsKey("language")) strlcpy(config.language, doc["language"].as<String>().c_str(), sizeof(config.language));
    if (doc.containsKey("powerSavingEnabled")) config.powerSavingEnabled = doc["powerSavingEnabled"].as<bool>();
    if (doc.containsKey("powerSavingStartHour")) config.powerSavingStartHour = doc["powerSavingStartHour"].as<int>();
    if (doc.containsKey("powerSavingEndHour")) config.powerSavingEndHour = doc["powerSavingEndHour"].as<int>();
    if (doc.containsKey("normalUpdateInterval")) config.normalUpdateInterval = doc["normalUpdateInterval"].as<int>();
    if (doc.containsKey("powerSavingUpdateInterval")) config.powerSavingUpdateInterval = doc["powerSavingUpdateInterval"].as<int>();
    if (doc.containsKey("maxNetworkRetries")) config.maxNetworkRetries = doc["maxNetworkRetries"].as<int>();
    // Intervalli meteo
    // Intervalli refresh display (secondi)
    if (doc.containsKey("displayRefreshIntervalSec")) config.displayRefreshIntervalSec = doc["displayRefreshIntervalSec"].as<int>();
    if (doc.containsKey("displayRefreshIntervalSecPowerSaving")) config.displayRefreshIntervalSecPowerSaving = doc["displayRefreshIntervalSecPowerSaving"].as<int>();
    
    
    // Battery Management
    if (doc.containsKey("batteryShowOnDisplay")) config.batteryShowOnDisplay = doc["batteryShowOnDisplay"].as<bool>();
    
    

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
      // Riavvio voluto: il firmware funziona, niente rollback
      markFirmwareHealthy();
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
    // L API key non viene mai restituita: la pagina sa solo se è impostata
    doc["api_key_set"] = strlen(config.api_key) > 0;
    doc["version"] = ATMOVERSE_VERSION;
    doc["batteryShowOnDisplay"] = config.batteryShowOnDisplay;
    doc["updateStatus"] = getUpdateStatusText();
    doc["timezone"] = config.gmtOffset_sec / 3600;
    doc["dst"] = config.daylightOffset_sec / 3600;
    doc["use24hFormat"] = config.use24hFormat;
    doc["units"] = config.units;
    doc["language"] = config.language;
    
    // Aggiungi parametri di risparmio energetico
    doc["powerSavingEnabled"] = config.powerSavingEnabled;
    doc["powerSavingStartHour"] = config.powerSavingStartHour;
    doc["powerSavingEndHour"] = config.powerSavingEndHour;
    doc["normalUpdateInterval"] = config.normalUpdateInterval;
    doc["powerSavingUpdateInterval"] = config.powerSavingUpdateInterval;
    
    
    // Aggiungi parametri di gestione errori di rete
    doc["maxNetworkRetries"] = config.maxNetworkRetries;
    
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
    getLocalTime(&timeinfo, 0);
    char localTimeStr[30];
    strftime(localTimeStr, sizeof(localTimeStr), "%H:%M - %d/%m/%Y", &timeinfo);
    time["local"] = localTimeStr;
    
    // Forza l'aggiornamento dei dati meteo se non validi
    if (!isWeatherDataValid()) {
      getWeatherData();
    }
    
    // Dati meteo reali: se mancano "valid" è false e la pagina lo mostra.
    // (Prima venivano inventati valori, e le temperature <= 0 diventavano 15 °C)
    weather["temp"] = currentWeather.temp;
    weather["feels_like"] = currentWeather.feels_like;
    weather["humidity"] = currentWeather.humidity;
    weather["pressure"] = currentWeather.pressure;
    weather["wind_speed"] = currentWeather.wind_speed;  // m/s (units=metric)
    weather["wind_deg"] = currentWeather.wind_deg;

    // Condizione meteo
    weather["condition"] = currentWeather.description;
    weather["icon"] = currentWeather.icon;
      
    // Timestamp ultimo aggiornamento
    char lastUpdateStr[30];
    struct tm lastUpdateTime;
    localtime_r(&currentWeather.last_update, &lastUpdateTime);
    strftime(lastUpdateStr, sizeof(lastUpdateStr), "%H:%M - %d/%m/%Y", &lastUpdateTime);
    weather["last_update"] = lastUpdateStr;
    
    weather["valid"] = currentWeather.valid;

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

  // Orologio letterario: riepilogo, lettura e salvataggio di un'ora
  if (path == "/api/orari") {
    if (!initSD()) {
      sendJsonResponse(client, "{\"success\":false,\"message\":\"SD non disponibile\"}", 503);
      return;
    }
    String h = queryParam(params, "h");
    if (h.length() == 0 && method == "GET") {
      sendOrariSummary(client);
      return;
    }
    String file = orariPath(h);
    if (file.length() == 0) {
      sendJsonResponse(client, "{\"success\":false,\"message\":\"Ora non valida\"}", 400);
      return;
    }
    if (method == "GET") {
      if (queryParam(params, "fit") == "1") sendOrariFit(client, file);
      else sendOrariHour(client, file);
    } else if (method == "POST") {
      saveOrariHour(client, file, contentLength);
    } else {
      sendJsonResponse(client, "{\"success\":false}", 405);
    }
    return;
  }

  // Citazioni: file intero nel formato del firmware, scritto sulla SD mentre arriva
  if (path == "/api/quotes/raw" && method == "POST") {
    if (!initSD()) {
      sendJsonResponse(client, "{\"success\":false,\"message\":\"SD non disponibile\"}", 503);
      return;
    }
    saveQuotesRaw(client, contentLength);
    return;
  }

  // Gestione configurazioni
  if (path == "/api/settings" && method == "POST") {
    // Leggi il corpo della richiesta
    String jsonBody = readRequestBody(client, contentLength);
    
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
    // Password vuota = mantieni quella salvata, tranne quando cambia la rete
    // (allora vuota significa rete aperta)
    String newSsid = doc["ssid"] | "";
    String newPassword = doc["password"] | "";
    bool ssidChanged = newSsid.length() > 0 && newSsid != config.ssid;
    if (newSsid.length() > 0) {
      strlcpy(config.ssid, newSsid.c_str(), sizeof(config.ssid));
    }

    if (newPassword.length() > 0 || ssidChanged) {
      strlcpy(config.password, newPassword.c_str(), sizeof(config.password));
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
    
    // Parametri di gestione degli errori di rete
    if (doc.containsKey("maxNetworkRetries")) {
      config.maxNetworkRetries = doc["maxNetworkRetries"].as<int>();
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
      // Riavvio voluto: il firmware funziona, niente rollback
      markFirmwareHealthy();
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

  if ((path == "/quotes-editor.html" || path == "/quotes-editor") && method == "GET") {
    if (!initSD()) {
      sendResponse(client, "text/plain", "SD non disponibile", 503);
      return;
    }
    File f = SD.open("/www/quotes-editor.html", FILE_READ);
    if (!f) {
      sendResponse(client, "text/plain", "quotes-editor.html non trovato su SD", 404);
      return;
    }
    String html = f.readString();
    f.close();
    html.replace(
      "const response = await fetch('/quotes.json');\r\n        if (response.ok) {\r\n          quotes = await response.json();",
      "const response = await fetch('/quotes.json?raw=1');\r\n        if (response.ok) {\r\n          const data = await response.json();\r\n          quotes = Array.isArray(data) ? data : Object.entries(data).flatMap(([category, items]) =>\r\n            Array.isArray(items) ? items.map(item => ({\r\n              text: item.text || item.quote || '',\r\n              author: item.author || '',\r\n              category,\r\n              time: item.time || '',\r\n              season: item.season || ''\r\n            })) : []\r\n          );"
    );
    html.replace(
      "const response = await fetch('/quotes.json');\n        if (response.ok) {\n          quotes = await response.json();",
      "const response = await fetch('/quotes.json?raw=1');\n        if (response.ok) {\n          const data = await response.json();\n          quotes = Array.isArray(data) ? data : Object.entries(data).flatMap(([category, items]) =>\n            Array.isArray(items) ? items.map(item => ({\n              text: item.text || item.quote || '',\n              author: item.author || '',\n              category,\n              time: item.time || '',\n              season: item.season || ''\n            })) : []\n          );"
    );
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html; charset=utf-8");
    client.println("Content-Length: " + String(html.length()));
    client.println("Cache-Control: no-store");
    client.println("Connection: close");
    client.println();
    client.print(html);
    client.flush();
    delay(1);
    client.stop();
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
