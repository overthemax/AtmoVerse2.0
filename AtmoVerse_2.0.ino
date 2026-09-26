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
#include <ArduinoJson.h>
#include <time.h>

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
#include "RTCManager.h"
#include "Updater.h"
#include "Version.h"
#include "DisplayTask.h"

// Il loop esegue anche le connessioni HTTPS (meteo, aggiornamenti): lo
// stack predefinito da 8 KB è al limite durante l'handshake TLS
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

// Aggiornamenti automatici da GitHub (vedi Updater.h)
const unsigned long UPDATE_CHECK_MS     = 6UL * 60 * 60 * 1000;  // Controllo ogni 6 ore
const unsigned long UPDATE_RETRY_MS     = 10UL * 60 * 1000;      // Nuovo tentativo dopo un errore
const unsigned long FIRMWARE_HEALTHY_MS = 60UL * 1000;           // Dopo 60 s il firmware è confermato
const unsigned long AP_RETRY_MS         = 5UL * 60 * 1000;       // In AP: nuovo tentativo sulla rete configurata

// ---------------------------------------------------------------------------
// Batteria scarica: sonno profondo
// ---------------------------------------------------------------------------
// Al livello critico il display mostra la faccina stanca e la scheda dorme,
// risvegliandosi ogni 30 minuti solo per misurare la batteria. La variabile
// in memoria RTC sopravvive al sonno profondo.
const uint64_t BATTERY_SLEEP_US = 30ULL * 60 * 1000000;
RTC_DATA_ATTR bool sleepingForBattery = false;

void enterBatterySleep() {
  Serial.printf("[BATTERY] Batteria al %d%%: sonno profondo, nuovo controllo tra 30 minuti\n",
                battery.getPercentage());
  sleepingForBattery = true;
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  esp_sleep_enable_timer_wakeup(BATTERY_SLEEP_US);
  Serial.flush();
  esp_deep_sleep_start();
}

// Risveglio dal sonno per batteria: si misura e, se è ancora scarica e non in
// carica, si torna a dormire senza toccare display e WiFi (la faccina resta)
void checkBatteryAfterSleep() {
  if (!sleepingForBattery || esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_TIMER) {
    sleepingForBattery = false;
    return;
  }
  battery.begin();
  if (battery.isAvailable() && !battery.charging() &&
      battery.getPercentage() < BATTERY_CRITICAL_EXIT_PERCENT) {
    enterBatterySleep();
  }
  Serial.println("[BATTERY] Batteria ricaricata: avvio normale");
  sleepingForBattery = false;
}

// Richiesta di controllo aggiornamenti dalla pagina web (vedi WebServer.cpp)
volatile bool updateCheckRequested = false;
void requestUpdateCheck() {
  updateCheckRequested = true;
}

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
  // Riduce frequenza CPU a 80MHz per risparmio energetico (WiFi funziona fino a 80MHz)
  setCpuFrequencyMhz(80);

  // Inizializza Serial per debug
  Serial.begin(115200);
  delay(1000);

  // Dopo un sonno per batteria scarica: se lo è ancora si torna a dormire qui
  checkBatteryAfterSleep();
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
  initDisplay();          // Avvia anche il task del display sul core 0
  displayStartupScreen();

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

  // Rileva un eventuale rollback del firmware e completa l'aggiornamento dei
  // file della SD se era stato interrotto
  initUpdater();
  Serial.println("[SETUP] AtmoVerse " ATMOVERSE_VERSION);

  // Fuso orario subito: l'ora dell'RTC (UTC) viene mostrata correttamente
  // anche se non c'è internet
  applyTimezone();

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

  // Inizializzazione hardware
  initHardware();

  // Batteria: INA219 cercato sempre; se manca la batteria non viene mostrata
  battery.begin();

  // Inizializza RTC DS3231 - imposta subito il clock interno se disponibile
  rtcBegin();


  // --- Resto ---
  // Rete configurata ma non raggiungibile: modalità AP per la configurazione.
  // In AP il loop riprova la rete ogni 5 minuti (se nessuno è collegato all'AP).
  if (!setupWiFi()) {
    startAccessPoint(true);
    showAPModeInfo();
  }
  // Attendi sincronizzazione NTP (max 5s) poi aggiorna il DS3231
  // (il server NTP e il fuso orario sono impostati da connectToWiFi)
  if (!apMode && isWiFiConnected()) {
    struct tm ntpTime;
    if (getLocalTime(&ntpTime, 5000)) {
      syncToRTC();
    }
  }
  // Se NTP non disponibile e RTC presente, usa ora RTC come fallback
  if (rtcAvailable() && !rtcLostPower()) {
    syncFromRTC();
  }
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
  if (!getLocalTime(&timeinfo, 0)) {
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
  // Batteria bassa: meteo aggiornato meno spesso per risparmiare
  if (isPowerSavingMode() || (battery.isAvailable() && battery.getLevel() != BATTERY_LEVEL_OK)) {
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
  if (battery.isAvailable()) {
    battery.update();

    // Livello critico stabile per almeno un minuto (due letture): faccina e sonno
    static unsigned long criticalSince = 0;
    if (battery.getLevel() == BATTERY_LEVEL_CRITICAL) {
      if (criticalSince == 0) {
        criticalSince = currentMillis;
      } else if (currentMillis - criticalSince >= 60000UL) {
        showBatteryEmpty();
        waitDisplayIdle(15000);  // La schermata deve essere sul pannello prima di dormire
        enterBatterySleep();
      }
    } else {
      criticalSince = 0;
    }
  }
  
  // Sincronizzazione NTP periodica ogni 30 minuti per calibrare il DS3231
  static unsigned long lastNTPSync = 0;
  const unsigned long NTP_SYNC_INTERVAL_MS = 30UL * 60UL * 1000UL; // 30 minuti
  // Il primo aggiornamento del DS3231 avviene appena l'ora NTP è disponibile
  // (all'avvio può arrivare dopo i 5 s di attesa del setup), poi ogni 30 minuti.
  // Senza attese: l'ora di sistema è già sincronizzata in background da SNTP.
  static bool rtcSynced = false;
  if (!apMode && WiFi.status() == WL_CONNECTED && time(nullptr) > 1700000000 &&
      (!rtcSynced || (unsigned long)(currentMillis - lastNTPSync) >= NTP_SYNC_INTERVAL_MS)) {
    lastNTPSync = currentMillis;
    if (syncToRTC()) {
      rtcSynced = true;
      Serial.println("[RTC] RTC aggiornato con l'ora NTP");
    }
  }
  
  // Dopo 60 secondi senza crash il firmware è confermato: se una versione
  // appena installata va in crash prima, il bootloader torna alla precedente
  static bool firmwareConfirmed = false;
  if (!firmwareConfirmed && currentMillis >= FIRMWARE_HEALTHY_MS) {
    markFirmwareHealthy();
    firmwareConfirmed = true;
  }

  static bool updateDue = true;  // Primo controllo aggiornamenti appena possibile
  static unsigned long lastUpdateCheck = 0;
  static unsigned long updateWaitMs = UPDATE_CHECK_MS;

  // In AP con una rete configurata: nuovo tentativo ogni 5 minuti, solo se
  // nessun telefono è collegato all'AP (per non interrompere la configurazione)
  static unsigned long apSince = 0;
  if (apMode) {
    if (apSince == 0) apSince = currentMillis;
    if (strlen(config.ssid) > 0 && WiFi.softAPgetStationNum() == 0 &&
        currentMillis - apSince >= AP_RETRY_MS) {
      Serial.println("[WIFI] Modalità AP: nuovo tentativo sulla rete configurata");
      if (connectToWiFi(config.ssid, config.password)) {
        setupServer();
        getWeatherData();
        updateDisplay();
        updateDue = true;
      } else {
        startAccessPoint(true);
      }
      apSince = millis();
    }
  } else {
    apSince = 0;
  }

  // Aggiornamenti da GitHub: all'avvio (quindi anche subito dopo la prima
  // configurazione), poi ogni 6 ore; dopo un errore si riprova in 10 minuti.
  // Serve l'ora corretta (NTP o RTC) per verificare i certificati HTTPS.
  // Se viene installato un nuovo firmware, checkForUpdates() riavvia.
  if (!apMode && WiFi.status() == WL_CONNECTED && time(nullptr) > 1700000000 &&
      (updateDue || updateCheckRequested || currentMillis - lastUpdateCheck >= updateWaitMs)) {
    bool manualCheck = updateCheckRequested;  // Dalla pagina web: verifica completa dei file
    updateDue = false;
    updateCheckRequested = false;
    lastUpdateCheck = currentMillis;
    updateWaitMs = checkForUpdates(manualCheck) ? UPDATE_CHECK_MS : UPDATE_RETRY_MS;
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
    
    // Aggiorna il display allo scatto del minuto (non a 60 s dal boot, altrimenti
    // l'ora mostrata può restare indietro fino a quasi un minuto)
    static int lastShownMinute = -1;
    unsigned long refreshMs = getDisplayRefreshIntervalMs();
    bool refreshDue;
    struct tm nowTm;
    if (getLocalTime(&nowTm, 0)) {
      int minuteOfDay = nowTm.tm_hour * 60 + nowTm.tm_min;
      int stepMin = max(1, (int)(refreshMs / 60000UL));
      refreshDue = lastDisplayUpdate == 0 ||
                   (minuteOfDay != lastShownMinute &&
                    (minuteOfDay % stepMin == 0 ||
                     (unsigned long)(currentMillis - lastDisplayUpdate) >= refreshMs + 60000UL));
      if (refreshDue) lastShownMinute = minuteOfDay;
    } else {
      refreshDue = (unsigned long)(currentMillis - lastDisplayUpdate) >= refreshMs;
    }
    if (refreshDue) {
      lastDisplayUpdate = currentMillis ? currentMillis : 1;
      updateDisplay();
    }
  } else {
    // In modalità AP il display mostra la schermata statica di configurazione
    // Non aggiorniamo l'orario: senza NTP non è affidabile e setPartialWindow causa crash
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
