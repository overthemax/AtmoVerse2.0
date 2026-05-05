/*
 * AtmoVerse 2.0 - Versione ottimizzata
 * Sistema meteo con ESP32, display e-ink e interfaccia web
 * Calendario integrato per visualizzare la data corrente
 */

// Librerie essenziali
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <SD.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiServer.h>
#include <DNSServer.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <time.h>

#include <ArduinoIoTCloud.h>
#include <Arduino_ConnectionHandler.h>
#include "CloudSecrets.h"

// Librerie per E-Ink Display (GxEPD2)
#include <GxEPD2_BW.h>
#include <Fonts/FreeSans9pt7b.h>

// Includere i moduli del progetto
#include "Hardware.h"
#include "Config.h"
#include "NetworkUtils.h"  // Nuovo modulo per la rete
#include "WeatherUtils.h"  // Nuovo modulo per i dati meteo
#include "WebServer.h"     // Nuovo modulo per il server web
#include "WebUIPages.h"    // Modulo per le interfacce web essenziali
#include "WebMinimal.h"    // Modulo per le interfacce web minimali
#include "SVGHelper.h"     // Modulo per gestione SVG
#include "WeatherIcons.h"  // Modulo per icone OpenWeatherMap
#include "Display.h"
#include "Calendar.h"
#include "AtmoVerseConstants.h"
#include "AtmoSerialLogger.h"  // Nuovo sistema di logging
#include "QuotesManager.h"    // Modulo per la gestione delle citazioni
#include "BatteryManager.h"

// Pin per il pulsante di reset configurazione e contatore di pressioni
#define RESET_BUTTON_PIN 35  // Pin del pulsante di RESET esterno
#define RESET_HOLD_TIME 5000      // Tempo di pressione continua per il reset (5 secondi)

// Timestamp dell'ultimo aggiornamento meteo
unsigned long lastWeatherUpdate = 0;

// Timestamp dell'ultimo aggiornamento del display
unsigned long lastDisplayUpdate = 0;

// Contatore tentativi di connessione alla rete meteo
int networkRetryCounter = 0;

// Flag per indicare l'ultimo stato dell'aggiornamento meteo
bool lastWeatherUpdateSuccess = true;

// Istanza del logger
AtmoSerialLogger Logger(sdSPI, SD_CS);

// Variabili per il rilevamento delle pressioni multiple e reset lungo
unsigned long lastResetPressTime = 0;
unsigned long resetPressStartTime = 0;
bool resetButtonPressed = false;

WiFiConnectionHandler* cloudConnection = nullptr;

void initCloudProperties() {
  ArduinoCloud.setBoardId(CLOUD_DEVICE_ID);
  ArduinoCloud.setSecretDeviceKey(CLOUD_DEVICE_SECRET);
}

// Mostra un messaggio sul display e-ink

void setupOTA() {
  ArduinoOTA.setHostname("AtmoVerse");
  ArduinoOTA.begin();
}

// Funzione per resettare la configurazione e entrare in modalità AP
void resetConfigAndEnterAP() {
  // Rimozione del log Serial per risparmiare memoria
  
  // Cancella direttamente il file di configurazione
  if (initSD()) {
    if (SD.exists("conf.json")) {
      if (SD.remove("conf.json")) {
        // File di configurazione cancellato con successo
      } else {
        // Errore durante la cancellazione del file di configurazione
      }
    } else {
      // File di configurazione non trovato
    }
  }
  
  // Resetta la configurazione in memoria
  resetConfig();
  
  // Entra in modalità AP per permettere la riconfigurazione
  startAccessPoint(true);

  // Mostra schermata AP grafica sul display
  showAPModeInfo();

  String apSSID = WiFi.softAPSSID();
  showStatusOnDisplay(String("RESET OK\nAP: " + apSSID + "\n192.168.4.1").c_str());
  delay(AP_INFO_DISPLAY_DURATION_MS);
  showAPModeInfo();
}


// Setup iniziale
void setup() {
  // Inizializza Serial per debug
  Serial.begin(115200);
  delay(1000);
  // Serial.println("\n\n=== AtmoVerse 2.0 Startup ===");
  
  // Attesa per stabilizzazione sistema prima di inizializzare SD
  delay(500);
  
  // --- SD CARD su HSPI - INIZIALIZZO PRIMA DELLA DISPLAY ---
  // Inizializzazione centralizzata SD PRIMA per evitare conflitti SPI
  // Serial.println("[SETUP] Inizializzazione SD card...");
  bool sdAvailable = initSD();
  
  // --- DISPLAY: Inizializza e mostra schermata di boot DOPO SD ---
  delay(BOOT_DELAY_MS);

  // Serial.println("[SETUP] Inizializzazione display...");
  SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS); // VSPI per display
  initDisplay();
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(NULL);
    display.setCursor(10, 60);
    display.print("AtmoVerse 2.0");
    display.setCursor(10, 100);
    display.print("Caricamento...");
  } while (display.nextPage());

  delay(BOOT_SPLASH_DURATION_MS);
  
  // --- VERIFICA SD ---
  if (!sdAvailable) {
    // Serial.println("ATTENZIONE: SD Card non rilevata o errore inizializzazione!");
    // Serial.println("Il sistema funzionerà in modalità AP per la configurazione iniziale.");
  }

  // Verifica se esiste il file di configurazione (solo se SD disponibile)
  bool configFileExists = false;
  if (sdAvailable) {
    configFileExists = SD.exists("/conf.json") || SD.exists("conf.json");
    // Serial.printf("[SETUP] File configurazione esiste sulla SD: %s\n", configFileExists ? "SI" : "NO");
  }

  // Carica la configurazione
  bool configLoaded = loadConfig();
  
  // Verifica se la configurazione ha SSID impostato
  bool hasSSID = strlen(config.ssid) > 0;
  // Serial.printf("[SETUP] SSID presente in config: %s\n", hasSSID ? "SI" : "NO");

  config.batteryMonitorEnabled = true;
  config.batteryShowOnDisplay = true;
  
  // Se esiste il file e la configurazione è stata caricata, ma non è valida,
  // potrebbe esserci un errore di lettura. Proviamo a rileggerla fino a 3 volte.
  if (configFileExists && !hasSSID && sdAvailable) {
    // Serial.println("[SETUP] File esiste ma SSID vuoto, tento rilettura...");
    // Prova a rileggere fino a CONFIG_READ_MAX_RETRIES volte prima di dare per persa la configurazione
    for (int i = 0; i < CONFIG_READ_MAX_RETRIES; i++) {
      // Serial.printf("[SETUP] Tentativo di rilettura #%d\n", i+1);
      delay(CONFIG_RETRY_DELAY_MS);
      
      // Ricarica la configurazione
      configLoaded = loadConfig();
      hasSSID = strlen(config.ssid) > 0;
      
      if (hasSSID) {
        // Serial.println("[SETUP] Configurazione caricata con successo al retry");
        break;
      }
    }
  }

  // Verifica finale di validità della configurazione
  if (!checkConfigValidity()) {
    // Configurazione non valida, avvio AP
    // Serial.println("[SETUP] ⚠️ Config non valida - avvio AP mode per configurazione");
    startAccessPoint(true);
    // Serial.println("[SETUP] startAccessPoint completato");
    showAPModeInfo();
    // Serial.println("[SETUP] showAPModeInfo completato - fine setup");
    return;
  } else {
    // Serial.println("[SETUP] ✓ Configurazione valida, procedo con connessione WiFi");
  }

  // Se non esisteva il file di configurazione ma è stato creato il default, salvalo
  if (!configFileExists) {
    saveConfig();
  }

  // Inizializza il generatore casuale con rumore ADC + hardware RNG
  randomSeed(analogRead(0) ^ (esp_random() & 0xFFFF));

  // DEBUG TEMPORANEO: forza aggiornamento meteo ogni 2 minuti
  config.normalUpdateInterval = 2;

  // Inizializzazione hardware
  initHardware();

  if (config.batteryMonitorEnabled) {
    battery.begin(config.batteryADCPin, config.batteryVoltageDivider);
  }

  displayStartupScreen();

  // --- Resto ---
  setupWiFi();
  if (!apMode && isWiFiConnected()) {
    setupOTA();
    if (cloudConnection == nullptr) {
      cloudConnection = new WiFiConnectionHandler(config.ssid, config.password);
    }
    initCloudProperties();
    ArduinoCloud.begin(*cloudConnection);
  }
  configTime(config.gmtOffset_sec, config.daylightOffset_sec, config.ntpServer);
  setupServer();
  getWeatherData();
  updateDisplay();
}

// Contatore per il loop principale
long loopCounter = 0;

// Determina se siamo in modalità risparmio energetico
bool isPowerSavingMode() {
  if (!config.powerSavingEnabled) {
    return false; // Risparmio energetico disabilitato
  }

  // Ottieni l'ora corrente
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return false; // Errore nel recupero dell'ora, assume modalità normale
  }
  
  int currentHour = timeinfo.tm_hour;
  
  // Gestisci il caso in cui l'orario di inizio sia maggiore dell'orario di fine
  // (es. dalle 22:00 alle 7:00 del giorno successivo)
  if (config.powerSavingStartHour > config.powerSavingEndHour) {
    return (currentHour >= config.powerSavingStartHour || currentHour < config.powerSavingEndHour);
  } else {
    return (currentHour >= config.powerSavingStartHour && currentHour < config.powerSavingEndHour);
  }
}

// Calcola l'intervallo di aggiornamento in base alla modalità
unsigned long getUpdateInterval() {
  if (isPowerSavingMode()) {
    return config.powerSavingUpdateInterval * 60 * 1000; // Converti minuti in millisecondi
  } else {
    return config.normalUpdateInterval * 60 * 1000; // Converti minuti in millisecondi
  }
}

// Calcola l'intervallo di refresh display in ms (modalità client)
unsigned long getDisplayRefreshIntervalMs() {
  if (isPowerSavingMode()) {
    return (unsigned long)config.displayRefreshIntervalSecPowerSaving * 1000UL;
  } else {
    return (unsigned long)config.displayRefreshIntervalSec * 1000UL;
  }
}

// Loop principale
void loop() {
  unsigned long currentMillis = millis();
  ArduinoOTA.handle();
  ArduinoCloud.update();
  if (config.batteryMonitorEnabled) {
    battery.update();
  }
  
  // Esegui aggiornamenti solo quando non siamo in modalità AP
  if (!apMode) {
    // Determina l'intervallo di aggiornamento basato sulla modalità di risparmio energetico
    unsigned long updateInterval = getUpdateInterval();
    
    // Aggiorna i dati meteo in base all'intervallo calcolato
    if ((unsigned long)(currentMillis - lastWeatherUpdate) >= updateInterval) {
      lastWeatherUpdate = currentMillis;
      
      // Tenta di aggiornare i dati meteo
      if (getWeatherData()) {
        // Aggiornamento riuscito, resetta il contatore di tentativi
        networkRetryCounter = 0;
        lastWeatherUpdateSuccess = true;
        // Non aggiorniamo il display qui - lasciamo che sia fatto solo dall'aggiornamento a intervallo fisso
      } else {
        // Errore nell'aggiornamento, gestisci i tentativi
        networkRetryCounter++;
        
        if (networkRetryCounter <= config.maxNetworkRetries) {
          // Tenta di nuovo fra un minuto
          lastWeatherUpdate = currentMillis - updateInterval + (60 * 1000);
        } else {
          // Numero massimo di tentativi raggiunto
          lastWeatherUpdateSuccess = false;
          // Non aggiorniamo il display qui - lasciamo che sia fatto solo dall'aggiornamento a intervallo fisso
          // Resetta il contatore e riprova al prossimo intervallo
          networkRetryCounter = 0;
        }
      }
    }
    
    // Aggiorna il display in base all'intervallo configurato
    if ((unsigned long)(currentMillis - lastDisplayUpdate) >= getDisplayRefreshIntervalMs()) {
      lastDisplayUpdate = currentMillis;
      updateDisplay(); // Aggiornamento completo del display una volta al minuto
    }
  } else {
    // In modalità AP, aggiorna solo l'orario in base all'intervallo configurato
    if ((unsigned long)(currentMillis - lastDisplayUpdate) >= (unsigned long)config.apTimeRefreshIntervalSec * 1000UL) {
      lastDisplayUpdate = currentMillis;
      updateTimeOnly();
    }
  }
  
  
  // Gestisci le richieste web e il captive portal ma limitalo in frequenza
  static unsigned long lastWebServerCheck = 0;
  if (currentMillis - lastWebServerCheck >= WEB_SERVER_CHECK_INTERVAL_MS) {
    lastWebServerCheck = currentMillis;
    handleClientRequests(); // Gestisce sia server che DNS captive portal
  }
  
  // DISABILITATO: Controlla la connessione WiFi e passa in modalità AP se necessario
  // checkWiFiConnection();
  
  // Delay ridotto per mantenere reattività del server web
  // Il delay originale di 500ms rendeva l'interfaccia lenta
  delay(10); 
}
