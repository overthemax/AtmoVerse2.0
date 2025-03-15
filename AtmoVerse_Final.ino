// AtmoVerse_Final.ino
// Main application file for the AtmoVerse 2.0 - Weather display platform
// Optimized for ESP32 with memory constraints
// Using memory optimization techniques for e-ink display

/*
 * AtmoVerse 2.0 - Firmware principale
 * 
 * Questo file contiene la logica principale del progetto AtmoVerse, che integra il server web,
 * la gestione del display e-ink e l'acquisizione dei dati meteo. Il codice è ottimizzato per
 * l'ESP32, con particolare attenzione alla gestione della memoria e all'efficienza energetica.
 * 
 * Struttura del codice:
 * - setup(): Inizializza le periferiche e avvia il server web.
 * - loop(): Gestisce le operazioni cicliche, come l'aggiornamento del display e la gestione
 *   delle richieste HTTP.
 */

// *** Include necessari in ordine corretto per evitare conflitti ***
#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <SD.h>
#include <time.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>  // Include prima di qualsiasi nostro file che lo usa
#include "config.h"             // Include dopo ESPAsyncWebServer per evitare conflitti
#include "http_constants.h"     // File vuoto ora, solo per compatibilità
#include "weather.h"
#include "display.h"

// Definizioni pin
#define SD_CS 5
#ifndef CONFIG_BUTTON_PIN
#define CONFIG_BUTTON_PIN 0
#endif
#ifndef BUTTON_PIN
#define BUTTON_PIN CONFIG_BUTTON_PIN
#endif

// Variabili globali - dimensionate correttamente per ottimizzare la memoria
char ssid[SSID_BUFFER_SIZE + 1] = "";
char password[PASSWORD_BUFFER_SIZE + 1] = "";
char owm_api_key[OWM_API_KEY_LEN + 1] = "";
char owm_city[OWM_CITY_LEN + 1] = "Rome";
char owm_country[COUNTRY_BUFFER_SIZE] = "IT";
char timezone[TIMEZONE_BUFFER_SIZE + 1] = "CET-1CEST,M3.5.0,M10.5.0/3";

// Variabili di stato
bool dst_enabled = true;
bool inAPMode = false;
bool sdCardAvailable = false;

// Variabili globali per timestamp
unsigned long lastWeatherUpdate = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long webCheckTime = 0;

// Definizione strutture globali solo una volta
WeatherData currentWeather;
CustomQuotes customQuotes;

// Server DNS per captive portal
DNSServer dnsServer;

// Webserver
AsyncWebServer server(80);

// HTML Content
const char htmlContent[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
  <title>AtmoVerse</title>
</head>
<body>
  <h1>AtmoVerse 2.0</h1>
  <div id="weather"></div>
</body>
</html>
)=====";

const char settingsHtml[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
  <title>Settings</title>
</head>
<body>
  <h1>Settings</h1>
  <form action="/saveSettings" method="post">
    <!-- Settings form -->
  </form>
</body>
</html>
)=====";

const char quotesHtml[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
  <title>Quotes</title>
</head>
<body>
  <h1>Quotes</h1>
  <form action="/saveQuotes" method="post">
    <!-- Quotes form -->
  </form>
</body>
</html>
)=====";

/*
 * Funzione isConfigButtonPressed()
 * 
 * Controlla se il pulsante di configurazione è premuto o se i dati WiFi non sono configurati.
 * 
 * @return true se il pulsante di configurazione è premuto o se i dati WiFi non sono configurati,
 *         false altrimenti.
 */
bbool isConfigButtonPressed() {
  pinMode(CONFIG_BUTTON_PIN, INPUT_PULLUP);
  
  // Se il pulsante di configurazione è premuto o i dati WiFi non sono configurati
  if (digitalRead(CONFIG_BUTTON_PIN) == LOW || strlen(ssid) < 1) {
    return true;
  }
  
  return false;
}


/*
 * Funzione initSDCard()
 * 
 * Inizializza la scheda SD.
 * 
 * @return true se la scheda SD è stata inizializzata correttamente,
 *         false altrimenti.
 */
bool initSDCard() {
  Serial.println(F("Inizializzazione SD card..."));
  
  // Inizializza la SD con il pin CS
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);  // Disattiva temporaneamente la SD
  
  if (!SD.begin(SD_CS)) {
    Serial.println(F("Errore: SD card non trovata o non funzionante"));
    return false;
  }
  
  Serial.println(F("SD card inizializzata correttamente"));
  return true;
}

  
  Serial.println(F("SD card inizializzata correttamente"));
  return true;
}

/*
 * Funzione setupTime()
 * 
 * Configura il server NTP per il tempo.
 */
void setupTime() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  setenv("TZ", timezone, 1);
  tzset();
  
  Serial.println(F("NTP configurato"));
}


// Quotes functionality
bool loadQuotes() {
  // Implementazione mancante: caricamento delle citazioni da un file o database
  return true;
}


// Webserver functionality
void setupWebServer() {
  // Implementation moved from webserver.h
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", htmlContent);
  });
  
  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", settingsHtml);
  });
  
  server.on("/quotes", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", quotesHtml);
  });
  
  server.on("/saveSettings", HTTP_POST, [](AsyncWebServerRequest *request) {
    // Implementation moved from webserver.h
  });
  
  server.on("/saveQuotes", HTTP_POST, [](AsyncWebServerRequest *request) {
    // Implementation moved from webserver.h
  });
  
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
  });
}

void startWebServer() {
  // Implementation moved from webserver.h
  server.begin();
}

void startAPMode() {
  // Implementation moved from webserver.h
  WiFi.mode(WIFI_AP);
  WiFi.softAP("AtmoVerse");
  dnsServer.start(53, "*", WiFi.softAPIP());
  startWebServer();
}

String getContentType(String filename) {
  // Implementation moved from webserver.h
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  return "text/plain";
}

String getContentType(String filename) {
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  return "text/plain";
}

bool handleFileRead(String path) {
  // Implementazione mancante: lettura e gestione dei file
  if (path.endsWith("/")) path += "index.html";
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if (SD.exists(pathWithGz)) path += ".gz";
  if (SD.exists(path)) {
    File file = SD.open(path, "r");
    server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}


void handleRoot(AsyncWebServerRequest *request) {
  // Implementation moved from webserver.h
  if (!handleFileRead("/index.html")) request->send(404, "text/plain", "Not found");
}

void handleSettings(AsyncWebServerRequest *request) {
  // Implementation moved from webserver.h
  if (!handleFileRead("/settings.html")) request->send(404, "text/plain", "Not found");
}

void handleQuotes(AsyncWebServerRequest *request) {
  // Implementation moved from webserver.h
  if (!handleFileRead("/quotes.html")) request->send(404, "text/plain", "Not found");
}

void handleSaveSettings(AsyncWebServerRequest *request) {
  // Implementation moved from webserver.h
}

void handleSaveQuotes(AsyncWebServerRequest *request) {
  // Implementation moved from webserver.h
}

void saveConfig() {
  if (!SD.exists("/config")) {
    SD.mkdir("/config");
  }

  // Rimuovi il file esistente
  if (SD.exists("/config/config.json")) {
    SD.remove("/config/config.json");
  }

  File configFile = SD.open("/config/config.json", FILE_WRITE);
  if (!configFile) {
    Serial.println(F("Errore apertura config.json"));
    return;
  }

  // Utilizza una capacità ridotta per il documento JSON
  StaticJsonDocument<384> doc;

  // Aggiungi i parametri necessari
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


void loadConfig() {
  File configFile = SD.open("/config/config.json", FILE_READ);
  if (!configFile) {
    Serial.println(F("config.json non trovato"));
    return;
  }

  // Utilizza una capacità ridotta per il documento JSON
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


void updateEinkDisplay() {
  // Prepara i dati da visualizzare
  String weatherInfo = createWeatherDisplayString(currentWeather);

  // Imposta i margini del layout
  int x = DISP_MARGIN;
  int y = DISP_MARGIN;

  // Disegna la sezione con le informazioni meteo
  drawText(weatherInfo, x, y);

  // Mostra il display e-ink
  epd.display();
}

String createWeatherDisplayString(WeatherData weather) {
  String info = F("Citta': ") + weather.city + " | ";
  info += F("Paese: ") + weather.country + " | ";
  info += F("Temperatura: ") + String(weather.temperature, 2) + F("°C") + " | ";
  info += F("Umidita': ") + String(weather.humidity, 0) + "%" + " | ";
  info += "Pressione: " + String(weather.pressure, 1) + " hPa";

  
  return info;
}

void drawText(String text, int x, int y) {
  // Disegna il testo sul display e-ink
  display.drawString(text.c_str(), x, y, FreeSans9pt7b);
}


/*
 * Funzione setup()
 * 
 * Inizializza le periferiche e configura il server web. Questa funzione viene eseguita
 * una sola volta all'avvio del dispositivo.
 */
void setup() {
  // Inizializza la comunicazione seriale
  Serial.begin(115200);
  
  // Attendi che la seriale sia disponibile, con timeout
  uint8_t serialWaitCount = 0;
  while (!Serial && serialWaitCount < 50) { 
    delay(100);
    serialWaitCount++;
  }
  
  Serial.println(F("\nAtmoVerse 2.0 - Starting"));

  // Inizializza i pin
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Inizializza la scheda SD
  sdCardAvailable = initSDCard();
  
  // Imposta correttamente l'inizializzazione della SD card e il caricamento delle citazioni
  if (sdCardAvailable) {
    loadConfig();
    
    if (SD.begin(SD_CS)) {
      Serial.println(F("SD card inizializzata correttamente"));
      // Usa la funzione bool loadQuotes() per gestire l'errore
      if (!loadQuotes()) {
        Serial.println(F("Errore nel caricamento delle citazioni"));
      } else {
        Serial.println(F("Citazioni caricate correttamente"));
      }
    } else {
      Serial.println(F("Errore nell'inizializzazione della SD card"));
    }
  }
  
  
    
    // Avvia access point
    startAPMode();
    
    // Mostra messaggio sul display (utilizzo minimo di memoria)
    updateEinkDisplay();
  } else {
    Serial.println(F("Modalità normale attivata"));
    inAPMode = false;
    
    // Connetti al WiFi
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    // Aspetta connessione con timeout
    Serial.print(F("Connessione al WiFi"));
    uint8_t wifiWaitCount = 0;
    while (WiFi.status() != WL_CONNECTED && wifiWaitCount < 20) {
      delay(500);
      Serial.print(F("."));
      wifiWaitCount++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println(F("\nConnesso al WiFi"));
      Serial.print(F("Indirizzo IP: "));
      Serial.println(WiFi.localIP());
      
      // Configura il server NTP per il tempo
      setupTime();
      
      // Aggiorna i dati meteo
      updateWeather();
      
      // Aggiorna il display
      updateEinkDisplay();
      
      // Avvia server web
      setupWebServer();
      startWebServer();
    } else {
      Serial.println(F("\nImpossibile connettersi al WiFi"));
    }
  }
  
  // Riduzione del consumo energetico
  Serial.println(F("Ottimizzazione consumo energetico"));
  WiFi.setSleep(true);
}

/*
 * Funzione loop()
 * 
 * Gestisce le operazioni cicliche del dispositivo, come l'aggiornamento del display e
 * la gestione delle richieste HTTP. Questa funzione viene eseguita in modo continuo.
 */
void loop() {
  // Gestione del captive portal se in modalità configurazione
  if (inAPMode) {
    dnsServer.processNextRequest();
    // Pulizia minima della memoria
    yield();
    return;
  }
  
  // Controlla se è ora di aggiornare i dati meteo
  unsigned long currentMillis = millis();
  if (currentMillis - lastWeatherUpdate >= WEATHER_UPDATE_INTERVAL) {
    lastWeatherUpdate = currentMillis;
    updateWeather();
  }
  
  // Controlla se è ora di aggiornare il display
  if (currentMillis - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
    lastDisplayUpdate = currentMillis;
    updateEinkDisplay();
  }
  
  // Risparmio energetico - piccola pausa
  delay(100);
}
