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
#include "AtmoVerseConstants.h" // JSON buffer sizes
#include <SD.h>
#include <ArduinoJson.h>
#include <WiFi.h>

// Reads the body of a POST request up to Content-Length (the body may
// arrive after the headers, in a later packet). Max 3 s.
static String readRequestBody(WiFiClient& client, int contentLength, int maxBody = 16384) {
  const int MAX_BODY = maxBody;
  String body;
  if (contentLength > MAX_BODY) contentLength = MAX_BODY;
  if (contentLength > 0 && !body.reserve(contentLength)) return body;  // Out of memory
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

// Defined in the main sketch, drives the display refresh
extern unsigned long lastDisplayUpdate;

// Value of a query string parameter ("h=08&fit=1"), empty if missing
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

// Literary clock: one file per hour, CLOCK_DIR/00.txt ... CLOCK_DIR/23.txt.
// The editor reads and saves one hour at a time (max ~64 KB), never all the
// files together: in total they are hundreds of KB, too much for the RAM.
static const int CLOCK_MAX_FILE = 65536;

// Valid hour "0".."23" -> file path, otherwise an empty string
static String clockPath(const String& h) {
  if (h.length() == 0 || h.length() > 2) return "";
  for (size_t i = 0; i < h.length(); i++) {
    if (!isDigit(h[i])) return "";
  }
  int hour = h.toInt();
  if (hour < 0 || hour > 23) return "";
  char path[20];
  snprintf(path, sizeof(path), CLOCK_DIR "/%02d.txt", hour);
  return String(path);
}

// GET /api/clock: quotes and covered minutes for every hour
static void sendClockSummary(WiFiClient& client) {
  String json = "{\"hours\":[";
  int totalQuotes = 0, totalMinutes = 0;
  for (int hour = 0; hour < 24; hour++) {
    char path[20];
    snprintf(path, sizeof(path), CLOCK_DIR "/%02d.txt", hour);
    bool minutes[60] = {false};
    int count = 0;
    File f = SD.open(path, FILE_READ);
    if (f) {
      // Only the first 3 characters of each line matter ("MM|")
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

// GET /api/clock?h=8: the hour's file as it is, sent in chunks without loading it into RAM
static void sendClockHour(WiFiClient& client, const String& path) {
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

// GET /api/clock?h=8&fit=1: for each line of the file, whether the display shows it in full
static void sendClockFit(WiFiClient& client, const String& path) {
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

// POST /api/quotes/raw: quotes.json already in the firmware format
// ({"section": [{text, author, ...}]}), written to the SD card as it arrives.
// The collection easily exceeds 40 KB: holding it in RAM and converting it
// ran out of memory. The file is checked (valid JSON with an object of
// sections) by reading it from the SD card without loading it, then it
// replaces the old one.
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

  // Check without loading the content: the filter keeps no field
  File in = SD.open(tmp, FILE_READ);
  JsonDocument filter;
  filter["__none__"] = true;
  JsonDocument check;
  DeserializationError err = in ? deserializeJson(check, in, DeserializationOption::Filter(filter))
                                : DeserializationError::InvalidInput;
  if (in) in.close();
  if (err || !check.is<JsonObject>()) {
    SD.remove(tmp);
    sendJsonResponse(client, "{\"success\":false,\"message\":\"JSON non valido\"}");
    return;
  }
  SD.remove("/quotes.json");  // QUOTES_JSON_PATH is defined further down
  if (!SD.rename(tmp, "/quotes.json")) {
    sendJsonResponse(client, "{\"success\":false,\"message\":\"Impossibile sostituire quotes.json\"}");
    return;
  }
  Serial.printf("[WEB] Quotes saved to /quotes.json (%d bytes)\n", received);
  lastDisplayUpdate = 0;
  sendJsonResponse(client, "{\"success\":true}");
}

// POST /api/clock?h=8: the body (text, lines "MM|text|Author, Work") goes
// straight to a temporary file, which replaces the hour's file only if it
// arrived in full
static void saveClockHour(WiFiClient& client, const String& path, int contentLength) {
  if (contentLength < 0 || contentLength > CLOCK_MAX_FILE) {
    sendJsonResponse(client, "{\"success\":false,\"message\":\"File troppo grande (max 64 KB per ora)\"}");
    return;
  }
  if (!SD.exists(CLOCK_DIR)) SD.mkdir(CLOCK_DIR);
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
    // The firmware splits lines on '\n': '\r' is dropped
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
  Serial.printf("[WEB] Literary clock saved: %s (%d bytes)\n", path.c_str(), received);
  lastDisplayUpdate = 0;  // If it is the current hour, the display uses it at once
  sendJsonResponse(client, "{\"success\":true}");
}

// Quotes file on the SD card
static const char* QUOTES_JSON_PATH = "/quotes.json";

// Hexadecimal character -> value
static int hexCharToValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
  if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
  return -1;  // Not a hexadecimal character
}

// Decodes a URL component (%20 -> space, + -> space)
String urldecode(const String& str) {
  String ret;
  size_t len = str.length();
  ret.reserve(len);  // Reserve the largest possible size
  
  for (size_t i = 0; i < len; i++) {
    if (str[i] == '+') {
      ret += ' ';
    } else if (str[i] == '%') {
      // At least 2 characters after %
      if (i + 2 >= len) {
        // Incomplete % sequence: kept as a normal character
        ret += str[i];
        continue;
      }
      
      int highNibble = hexCharToValue(str[i+1]);
      int lowNibble = hexCharToValue(str[i+2]);
      
      // Both characters must be hexadecimal
      if (highNibble >= 0 && lowNibble >= 0) {
        ret += (char)((highNibble << 4) | lowNibble);
        i += 2;  // Skip the two hexadecimal characters
      } else {
        // Invalid % sequence: % kept as a normal character
        ret += str[i];
      }
    } else {
      ret += str[i];
    }
  }
  return ret;
}

// Server instance
WiFiServer server(80);

// Starts the server
void setupServer() {
  // The server always starts, even without the SD card or /www:
  // then it answers with 404 or the minimal page
  server.begin();
}

// Content type from the file extension
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

// Serves a file from the SD card
bool serveFileFromSD(WiFiClient& client, String path) {
  // Is the SD card present and readable? (mounted once by initSD)
  if (!initSD()) {
    return false;
  }
  
  // Paths
  if (path.endsWith("/")) path += "index.html";
  
  // Hidden files are never served
  if (path.indexOf("/.") >= 0) return false;
  
  // Pages live under /www
  if (!path.startsWith("/www")) {
    path = "/www" + path;
  }
  
  // Does the file exist?
  if (!SD.exists(path)) {
    return false;
  }
  
  // Open the file
  File file = SD.open(path, FILE_READ);
  if (!file) {
    return false;
  }
  
  // Content type
  String contentType = getContentType(path);
  
  // Response headers
  size_t fileSize = file.size();
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: " + contentType);
  client.println("Content-Length: " + String(fileSize));
  client.println("Accept-Ranges: none");
  // Only images are cached: pages, scripts and styles change with the updates
  if (!path.endsWith(".html") && !path.endsWith(".js") && !path.endsWith(".css")) {
    client.println("Cache-Control: public, max-age=86400");
  } else {
    client.println("Cache-Control: no-cache");
  }
  client.println("Connection: close");
  client.println();
  
  // Send the file in chunks
  uint8_t buffer[512];
  size_t totalSent = 0;
  while (file.available() && totalSent < fileSize) {
    size_t bytesRead = file.read(buffer, sizeof(buffer));
    size_t sent = client.write(buffer, bytesRead);
    totalSent += sent;
    // Is the client still connected?
    if (!client.connected()) {
      file.close();
      return false;
    }
  }
  
  // Make sure everything was sent
  client.flush();
  delay(1);
  
  // Close the file and the connection
  file.close();
  client.stop();
  return true;
}

// Web pages missing (new, empty or missing SD card): the firmware's minimal
// setup page is shown. After the setup the full pages are downloaded
// from GitHub (see Updater.cpp), if there is an SD card.
static void sendMissingAssetsPage(WiFiClient& client, bool sdOk, bool wwwExists) {
  sendFallbackSetupPage(client);
}

// Client requests
void handleClientRequests() {
  // Captive portal DNS in AP mode
  if (apMode) {
    dnsServer.processNextRequest();
  }
  
  // WiFi switched off by power saving: the network structures have been
  // freed and polling the server would crash the board
  if (!apMode && WiFi.getMode() == WIFI_OFF) {
    return;
  }

  // Any client connecting?
  WiFiClient client = server.available();
  if (!client) {
    return;
  }
  ecoNoteWebActivity();  // On battery: the web page stays active while it is used

  // Request timeout
  unsigned long timeout = millis() + 5000;
  while (!client.available() && millis() < timeout) {
    delay(10);
  }
  
  // No data: close the connection
  if (!client.available()) {
    client.stop();
    return;
  }
  
  // Read the request line
  String request = client.readStringUntil('\r');
  client.readStringUntil('\n');
  
  // Method and path
  String method = request.substring(0, request.indexOf(' '));
  String path = request.substring(request.indexOf(' ') + 1);
  path = path.substring(0, path.indexOf(' '));
  
  // URL parameters, if any
  String params = "";
  int qIndex = path.indexOf('?');
  if (qIndex != -1) {
    params = path.substring(qIndex + 1);
    path = path.substring(0, qIndex);
  }
  
  // Read the other headers and the request body
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
  
  // Typical captive portal probes
  // Android: a redirect instead of 204 opens the portal
  if (path == "/generate_204" || path == "/gen_204") {
    // Redirect to the settings so the captive portal appears
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
    // Apple expects a specific HTML answer
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
    // Windows NCSI: plain text answer
    sendResponse(client, "text/plain", "Microsoft NCSI", 200);
    return;
  }
  
  if (path == "/favicon.ico") {
    // No SD access for the favicon - 204 No Content
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
  
  // Other common probes - redirect to the settings
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

  // Simple health check with diagnostic data
  // Update check requested by the web page (run by the loop)
  // Restart requested from the settings page
  if (path == "/api/restart" && method == "POST") {
    sendJsonResponse(client, "{\"success\":true}");
    showRestarting();
    markFirmwareHealthy();  // Wanted restart: the firmware works, no rollback
    delay(300);
    ESP.restart();
  }

  if (path == "/api/update/check" && method == "POST") {
    requestUpdateCheck();
    sendJsonResponse(client, "{\"success\":true,\"message\":\"Controllo aggiornamenti avviato\"}");
    return;
  }
  
  // Battery state (INA219), read-only
  if (path == "/api/battery" && method == "GET") {
    JsonDocument b;
    b["available"] = battery.isAvailable();
    if (battery.isAvailable()) {
      b["voltage"] = battery.getVoltage();
      b["current_mA"] = battery.getCurrent();      // positive = discharging
      b["percent"] = battery.getPercentage();
      b["charging"] = battery.charging();
      b["level"] = battery.getLevel() == BATTERY_LEVEL_CRITICAL ? "critical"
                 : battery.getLevel() == BATTERY_LEVEL_LOW ? "low" : "ok";
      b["remaining_min"] = battery.getEstimatedTimeRemaining();  // -1 = cannot be estimated
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
      // meaningless values when not connected
      rssi = -127;
    }
    doc["rssi"] = (int)rssi;
    // SD state (best effort) through the shared initSD
    bool sdOk = initSD();
    doc["sd_ok"] = sdOk;

    String json;
    serializeJson(doc, json);
    sendJsonResponse(client, json);
    return;
  }
  
  // Compatibility alias: GET /api/config (same data as settings, with the keys the pages expect)
  if (path == "/api/config" && method == "GET") {
    DynamicJsonDocument doc(JSON_BUFFER_LARGE);
    doc["ssid"] = config.ssid;
    doc["city"] = config.city;
    doc["timezone"] = (int)(config.gmtOffset_sec / 3600);
    doc["daylightSaving"] = (config.daylightOffset_sec != 0);
    doc["ntpServer"] = config.ntpServer;
    // The API key is never returned: the page only knows whether it is set
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
    // Display refresh intervals (seconds)
    doc["displayRefreshIntervalSec"] = config.displayRefreshIntervalSec;
    doc["displayRefreshIntervalSecPowerSaving"] = config.displayRefreshIntervalSecPowerSaving;
    doc["maxNetworkRetries"] = config.maxNetworkRetries;
    // Weather intervals
    // Display refresh intervals (seconds)
    doc["displayRefreshIntervalSec"] = config.displayRefreshIntervalSec;
    doc["displayRefreshIntervalSecPowerSaving"] = config.displayRefreshIntervalSecPowerSaving;
    doc["apMode"] = apMode;
    doc["ipAddress"] = apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
    
    // Power saving hours
    
    // Battery Management
    doc["batteryShowOnDisplay"] = config.batteryShowOnDisplay;
    
    

    String json;
    serializeJson(doc, json);
    sendJsonResponse(client, json);
    return;
  }

  // Compatibility alias: POST /api/config (accepts the fields the page sends)
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

    // Current settings
    loadConfig();

    // Apply the changes
    // Empty password = keep the stored one, except when the network changes
    // (then empty means an open network)
    String newSsid = doc["ssid"] | "";
    String newPassword = doc["password"] | "";
    bool ssidChanged = newSsid.length() > 0 && newSsid != config.ssid;
    if (newSsid.length() > 0) strlcpy(config.ssid, newSsid.c_str(), sizeof(config.ssid));
    if (newPassword.length() > 0 || ssidChanged) strlcpy(config.password, newPassword.c_str(), sizeof(config.password));
    if (doc.containsKey("city")) strlcpy(config.city, doc["city"].as<String>().c_str(), sizeof(config.city));
    if (doc.containsKey("timezone")) config.gmtOffset_sec = doc["timezone"].as<int>() * 3600;
    if (doc.containsKey("daylightSaving")) config.daylightOffset_sec = doc["daylightSaving"].as<bool>() ? 3600 : 0;
    if (doc.containsKey("ntpServer")) strlcpy(config.ntpServer, doc["ntpServer"].as<String>().c_str(), sizeof(config.ntpServer));
    // Empty API key = keep the stored one (it is no longer sent to the pages)
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
    // Weather intervals
    // Display refresh intervals (seconds)
    if (doc.containsKey("displayRefreshIntervalSec")) config.displayRefreshIntervalSec = doc["displayRefreshIntervalSec"].as<int>();
    if (doc.containsKey("displayRefreshIntervalSecPowerSaving")) config.displayRefreshIntervalSecPowerSaving = doc["displayRefreshIntervalSecPowerSaving"].as<int>();
    
    
    // Battery Management
    if (doc.containsKey("batteryShowOnDisplay")) config.batteryShowOnDisplay = doc["batteryShowOnDisplay"].as<bool>();
    
    

    if (saveConfig()) {
      Serial.println("[WEB] Settings saved, restarting");
      
      sendJsonResponse(client, "{\"success\":true,\"message\":\"Configurazione salvata con successo\"}");
      
      // Visual confirmation on the display
      showConfigSaved();
      
      delay(5000);
      WiFi.disconnect(true);
      if (apMode) { WiFi.softAPdisconnect(true); }
      delay(1000);
      // Wanted restart: the firmware works, no rollback
      markFirmwareHealthy();
      ESP.restart();
    } else {
      sendJsonResponse(client, "{\"success\":false,\"message\":\"Errore nel salvataggio della configurazione\"}");
    }
    return;
  }

  // WiFi scan endpoint: returns a plain array, as the pages expect
  if (path == "/api/wifi/scan" && method == "GET") {
    // The station interface must be active in AP mode too
    wifi_mode_t prevMode = WiFi.getMode();
    bool restoreMode = false;
    if (prevMode == WIFI_MODE_AP) {
      WiFi.mode(WIFI_MODE_APSTA);
      restoreMode = true;
    } else if (prevMode == WIFI_MODE_NULL) {
      WiFi.mode(WIFI_MODE_STA);
      restoreMode = true;
    }

    // Scan (blocking)
    int num = WiFi.scanNetworks();

    // Plain array (not a wrapper object), as the pages expect
    DynamicJsonDocument doc(4096);
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < num && i < 20; i++) {
      JsonObject o = arr.createNestedObject();
      o["ssid"] = WiFi.SSID(i);
      o["rssi"] = WiFi.RSSI(i);
      o["channel"] = WiFi.channel(i);
      o["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }

    // Free the scan results
    WiFi.scanDelete();

    // Restore the previous mode if it was changed
    if (restoreMode) {
      WiFi.mode(prevMode);
    }

    String json;
    serializeJson(doc, json);
    sendJsonResponse(client, json);
    return;
  }

  // WiFi scan
  if (path == "/api/wifi-scan" && method == "GET") {
    // Scan and return the results
    performWiFiScan(client);
    return;
  }
  
  // Current settings
  if (path == "/api/settings" && method == "GET") {
    DynamicJsonDocument doc(1024);
    
    // Every setting into the JSON document
    doc["ssid"] = config.ssid;
    doc["city"] = config.city;
    // The API key is never returned: the page only knows whether it is set
    doc["api_key_set"] = strlen(config.api_key) > 0;
    doc["version"] = ATMOVERSE_VERSION;
    doc["batteryShowOnDisplay"] = config.batteryShowOnDisplay;
    doc["updateStatus"] = getUpdateStatusText();
    doc["timezone"] = config.gmtOffset_sec / 3600;
    doc["dst"] = config.daylightOffset_sec / 3600;
    doc["use24hFormat"] = config.use24hFormat;
    doc["units"] = config.units;
    doc["language"] = config.language;
    
    // Power saving
    doc["powerSavingEnabled"] = config.powerSavingEnabled;
    doc["powerSavingStartHour"] = config.powerSavingStartHour;
    doc["powerSavingEndHour"] = config.powerSavingEndHour;
    doc["normalUpdateInterval"] = config.normalUpdateInterval;
    doc["powerSavingUpdateInterval"] = config.powerSavingUpdateInterval;
    
    
    // Network error handling
    doc["maxNetworkRetries"] = config.maxNetworkRetries;
    
    String json;
    serializeJson(doc, json);
    
    sendJsonResponse(client, json);
    return;
  }
  
  // Weather API
  if (path == "/api/weather" && method == "GET") {
    DynamicJsonDocument doc(JSON_BUFFER_MEDIUM);
    
    extern WeatherData currentWeather;
    
    // JSON layout expected by the pages
    JsonObject weather = doc.createNestedObject("weather");
    JsonObject location = doc.createNestedObject("location");
    JsonObject time = doc.createNestedObject("time");
    
    // Location
    location["city"] = config.city;
    location["country"] = "Italia";
    
    // Timestamp and local time
    struct tm timeinfo;
    getLocalTime(&timeinfo, 0);
    char localTimeStr[30];
    strftime(localTimeStr, sizeof(localTimeStr), "%H:%M - %d/%m/%Y", &timeinfo);
    time["local"] = localTimeStr;
    
    // Update the weather now if it is not valid
    if (!isWeatherDataValid()) {
      getWeatherData();
    }
    
    // Real weather data: if missing, "valid" is false and the page says so.
    // (Values used to be made up, and temperatures <= 0 became 15 °C)
    weather["temp"] = currentWeather.temp;
    weather["feels_like"] = currentWeather.feels_like;
    weather["humidity"] = currentWeather.humidity;
    weather["pressure"] = currentWeather.pressure;
    weather["wind_speed"] = windSpeedMs();  // Always m/s, whatever the units
    weather["units"] = config.units;
    weather["wind_deg"] = currentWeather.wind_deg;

    // Weather condition
    weather["condition"] = currentWeather.description;
    weather["icon"] = currentWeather.icon;
      
    // Time of the last update
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
  
  // Quotes API: the quote currently shown on the display
  if (path == "/api/quote/current" && method == "GET") {
    Quote q = getCurrentQuote();
    DynamicJsonDocument out(256);
    out["text"] = q.text;
    out["author"] = q.author;
    String json; serializeJson(out, json);
    sendJsonResponse(client, json);
    return;
  }

  // Literary clock: summary, reading and saving one hour
  if (path == "/api/clock") {
    if (!initSD()) {
      sendJsonResponse(client, "{\"success\":false,\"message\":\"SD non disponibile\"}", 503);
      return;
    }
    String h = queryParam(params, "h");
    if (h.length() == 0 && method == "GET") {
      sendClockSummary(client);
      return;
    }
    String file = clockPath(h);
    if (file.length() == 0) {
      sendJsonResponse(client, "{\"success\":false,\"message\":\"Ora non valida\"}", 400);
      return;
    }
    if (method == "GET") {
      if (queryParam(params, "fit") == "1") sendClockFit(client, file);
      else sendClockHour(client, file);
    } else if (method == "POST") {
      saveClockHour(client, file, contentLength);
    } else {
      sendJsonResponse(client, "{\"success\":false}", 405);
    }
    return;
  }

  // Quotes: the whole file in the firmware format, written to the SD card as it arrives
  if (path == "/api/quotes/raw" && method == "POST") {
    if (!initSD()) {
      sendJsonResponse(client, "{\"success\":false,\"message\":\"SD non disponibile\"}", 503);
      return;
    }
    saveQuotesRaw(client, contentLength);
    return;
  }

  // Settings
  if (path == "/api/settings" && method == "POST") {
    // Read the request body
    String jsonBody = readRequestBody(client, contentLength);
    
    // The body must not be empty
    if (jsonBody.length() == 0) {
      sendJsonResponse(client, "{\"success\":false,\"message\":\"Nessun dato ricevuto\"}");
      return;
    }
    
    DynamicJsonDocument doc(512);
    DeserializationError error;
    
    // URL-encoded form?
    if (jsonBody.indexOf('=') > 0 && jsonBody.indexOf('&') > 0) {
      // Split into key-value pairs
      String pairs[10]; // At most 10 parameters
      int pairCount = 0;
      int startPos = 0;
      int ampPos;
      
      // Extract the key-value pairs
      while ((ampPos = jsonBody.indexOf('&', startPos)) != -1 && pairCount < 10) {
        pairs[pairCount++] = jsonBody.substring(startPos, ampPos);
        startPos = ampPos + 1;
      }
      
      if (startPos < jsonBody.length() && pairCount < 10) {
        pairs[pairCount++] = jsonBody.substring(startPos);
      }
      
      // Every pair into the JSON document
      for (int i = 0; i < pairCount; i++) {
        int eqPos = pairs[i].indexOf('=');
        if (eqPos != -1) {
          String key = pairs[i].substring(0, eqPos);
          String value = pairs[i].substring(eqPos + 1);
          
          // URL decoding
          value = urldecode(value);
          
          // Type conversion by key
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
      // Otherwise parse it as JSON
      error = deserializeJson(doc, jsonBody);
      if (error) {
        sendJsonResponse(client, "{\"success\":false,\"message\":\"Errore nel parsing JSON\"}");
        return;
      }
    }

    // SD card
    if (!initSD()) {
      sendJsonResponse(client, "{\"success\":false,\"message\":\"Errore nell'inizializzazione della SD\"}");
      return;
    }

    // Current settings
    loadConfig();
    
    // Apply the received values
    // Empty password = keep the stored one, except when the network changes
    // (then empty means an open network)
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

    // Power saving
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
    // Display refresh intervals (seconds)
    if (doc.containsKey("displayRefreshIntervalSec")) {
      config.displayRefreshIntervalSec = doc["displayRefreshIntervalSec"].as<int>();
    }
    if (doc.containsKey("displayRefreshIntervalSecPowerSaving")) {
      config.displayRefreshIntervalSecPowerSaving = doc["displayRefreshIntervalSecPowerSaving"].as<int>();
    }
    
    // Network error handling
    if (doc.containsKey("maxNetworkRetries")) {
      config.maxNetworkRetries = doc["maxNetworkRetries"].as<int>();
    }
    

    // Save the settings
    if (saveConfig()) {
      Serial.println("[WEB] Settings saved, restarting");
      
      sendJsonResponse(client, "{\"success\":true,\"message\":\"Configurazione salvata con successo\"}");
      
      // Visual confirmation on the display
      showConfigSaved();
      
      // Wait so the answer is surely sent
      delay(5000);
      
      // Close every WiFi connection
      WiFi.disconnect(true);
      if (apMode) {
        WiFi.softAPdisconnect(true);
      }
      
      // Restart the ESP32
      delay(1000);
      // Wanted restart: the firmware works, no rollback
      markFirmwareHealthy();
      ESP.restart();
    } else {
      sendJsonResponse(client, "{\"success\":false,\"message\":\"Errore nel salvataggio della configurazione\"}");
    }
    return;
  }
  
  // The /info page
  if (path == "/info" && method == "GET") {
    // The old Info page is now part of the diagnostics page
    serveFileFromSD(client, "/diagnostics.html");
    return;
  }

  if ((path == "/quotes-editor.html" || path == "/quotes-editor") && method == "GET") {
    if (!serveFileFromSD(client, "/quotes-editor.html")) sendResponse(client, "text/plain", "quotes-editor.html not found on the SD card", 404);
    return;
  }
  
  // Files from the SD card
  bool fileServed = false;
  
  // In AP mode, redirect to the setup page
  if (apMode) {
    // Static assets are allowed, also in the root, with common extensions
    bool isStaticAsset = path.startsWith("/css/") || path.startsWith("/js/") || path.startsWith("/img/") ||
                         path.endsWith(".css") || path.endsWith(".js") || path.endsWith(".svg") ||
                         path.endsWith(".png") || path.endsWith(".jpg") || path.endsWith(".jpeg") ||
                         path.endsWith(".ico") || path.endsWith(".webp") || path.endsWith(".gif");
    if (path != "/settings.html" && path != "/api/wifi-scan" && !isStaticAsset) {
      // Redirect to the setup page on the SD card
      client.println("HTTP/1.1 302 Found");
      client.println("Location: /settings.html");
      client.println("Connection: close");
      client.println();
      client.flush();
      delay(1);
      client.stop();
      return;
    }
    
    // Files from the SD card only, nothing generated
    fileServed = serveFileFromSD(client, path);
    if (!fileServed) {
      // SD state and /www folder
      bool sdOk = initSD();
      bool wwwExists = sdOk && SD.exists("/www");
      bool isSettings = (path == "/settings.html" || path == "/");
      if (!wwwExists || isSettings) {
        sendMissingAssetsPage(client, sdOk, wwwExists);
      } else {
        // File not found - plain 404
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
    // Normal mode: files from the SD card
    fileServed = serveFileFromSD(client, path);
    if (!fileServed) {
      // SD state and /www folder
      bool sdOk = initSD();
      bool wwwExists = sdOk && SD.exists("/www");
      bool isHomeOrSettings = (path == "/" || path == "/index.html" || path == "/settings.html");
      if (!wwwExists || isHomeOrSettings) {
        sendMissingAssetsPage(client, sdOk, wwwExists);
      } else {
        // Plain 404 for other resources
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

// Sends a generic HTTP response
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

// Sends a JSON response
void sendJsonResponse(WiFiClient& client, const String& jsonContent, int statusCode) {
  sendResponse(client, "application/json", jsonContent, statusCode);
}

// WiFi scan, results as JSON
void performWiFiScan(WiFiClient& client) {
  // JSON document for the answer
  DynamicJsonDocument doc(4096);
  JsonArray networks = doc.createNestedArray("networks");

  // In AP-only mode, switch to AP+STA for the scan
  wifi_mode_t prevMode = WiFi.getMode();
  bool switched = false;
  if (prevMode == WIFI_MODE_AP) {
    WiFi.mode(WIFI_MODE_APSTA);
    delay(100);
    switched = true;
  }

  // Start the WiFi scan
  int numNetworks = WiFi.scanNetworks();

  // Restore the previous mode if it was changed
  if (switched) {
    WiFi.mode(prevMode);
    delay(50);
  }

  if (numNetworks < 0) {
    // No results or error: empty list
    String json;
    serializeJson(doc, json);
    sendJsonResponse(client, json);
    return;
  }

  // Every network into the JSON document (max 20)
  for (int i = 0; i < numNetworks && i < 20; i++) {
    JsonObject network = networks.createNestedObject();
    network["ssid"] = WiFi.SSID(i);
    network["rssi"] = WiFi.RSSI(i);
    network["channel"] = WiFi.channel(i);
    network["encryption"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "secured" : "open";
  }

  // Send the answer
  String json;
  serializeJson(doc, json);
  sendJsonResponse(client, json);
}
