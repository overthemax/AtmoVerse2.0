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

// Mostra un messaggio sul display e-ink

// Funzione per resettare la configurazione e entrare in modalità AP
void resetConfigAndEnterAP() {
  // Rimozione del log Serial per risparmiare memoria
  
  // Cancella direttamente il file di configurazione
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (SD.begin(SD_CS, sdSPI)) {
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

  // Rimozione di tutti i log Serial per risparmiare memoria
  String apSSID = WiFi.softAPSSID();
  showStatusOnDisplay(String("RESET OK\nAP: " + apSSID + "\n192.168.4.1").c_str());
  delay(3000); // Mostra la schermata per 3 secondi
  showAPModeInfo();
}

// Funzione per controllare il reset della configurazione tramite pressione lunga
// TEMPORANEAMENTE DISABILITATA per debug
void checkResetButton() {
  // Funzione disabilitata temporaneamente per evitare reset accidentali
  // durante il debug
  
  // Se hai bisogno di ripristinarla, rimuovi questo commento e riattiva il codice originale
  
  /*
  static int lastButtonState = HIGH;
  static unsigned long lastDebounceTime = 0;
  static bool resetTriggered = false;
  const unsigned long debounceDelay = 100;  // Increased debounce delay to 100 ms

  int currentButtonState = digitalRead(RESET_BUTTON_PIN);

  if (currentButtonState != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (currentButtonState == LOW) {
      if (!resetButtonPressed) {
        resetButtonPressed = true;
        resetPressStartTime = millis();
        resetTriggered = false;  // New press, clear trigger flag
        // Reset button pressed message removed to save memory
      } else {
        if (!resetTriggered && (millis() - resetPressStartTime >= RESET_HOLD_TIME)) {
          resetTriggered = true;
          resetConfigAndEnterAP();
        }
      }
    } else {
      resetButtonPressed = false;
      resetTriggered = false;
    }
  }
  lastButtonState = currentButtonState;
  */
  
  // Versione disabilitata - non fa nulla
  return;
}

// Setup iniziale
void setup() {
  // --- DISPLAY: Inizializza e mostra schermata di boot PRIMA DI TUTTO ---
  // Logger disabilitato per risparmiare memoria
  delay(2000);

  // Log rimosso per risparmiare memoria
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
  // Log rimosso per risparmiare memoria

  delay(2000); // Dare il tempo alla porta seriale di connettersi
  
  // Header log rimosso per risparmiare memoria
  
  // Configurazione pin per il pulsante di reset
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
  
  // --- SD CARD su HSPI ---
  // Inizializzazione SD card (rimossi i messaggi di log)
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  SD.begin(SD_CS, sdSPI);

  // Verifica se esiste il file di configurazione
  bool configFileExists = SD.exists("/conf.json") || SD.exists("conf.json");
  // Log di configurazione rimosso

  // Carica la configurazione
  bool configLoaded = loadConfig();
  // Log di caricamento configurazione rimosso
  
  // Verifica se la configurazione ha SSID impostato
  bool hasSSID = strlen(config.ssid) > 0;
  // Log SSID rimosso
  
  // Se esiste il file e la configurazione è stata caricata, ma non è valida,
  // potrebbe esserci un errore di lettura. Proviamo a rileggerla fino a 3 volte.
  if (configFileExists && !hasSSID) {
    // Log tentativo di rilettura rimosso
    
    // Prova a rileggere fino a 3 volte prima di dare per persa la configurazione
    for (int i = 0; i < 3; i++) {
      Serial.print("[SETUP] Tentativo di rilettura #"); Serial.println(i+1);
      delay(500); // Attendi prima di riprovare
      
      // Ricarica la configurazione
      configLoaded = loadConfig();
      hasSSID = strlen(config.ssid) > 0;
      
      if (hasSSID) {
        // Rilettura riuscita
        break;
      }
    }
  }

  // Verifica finale di validità della configurazione
  if (!checkConfigValidity()) {
    // Configurazione non valida, avvio AP
    // Per debug: Non cancellare il file esistente ma solo entrare in AP mode
    startAccessPoint(true);
    showAPModeInfo();
    return;
  }

  // Se non esisteva il file di configurazione ma è stato creato il default, salvalo
  if (!configFileExists) {
    // Creazione file di configurazione predefinito
    saveConfig();
  }

  // Inizializzazione hardware
  initHardware();

  displayStartupScreen();

  // --- Resto ---
  setupWiFi();
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

// Loop principale
void loop() {
  unsigned long currentMillis = millis();
  
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
    
    // Aggiorna il display esattamente ogni minuto (60 secondi) - questo è l'UNICO punto di aggiornamento
    if ((unsigned long)(currentMillis - lastDisplayUpdate) >= 60 * 1000) {
      lastDisplayUpdate = currentMillis;
      updateDisplay(); // Aggiornamento completo del display una volta al minuto
    }
  } else {
    // In modalità AP, aggiorna solo l'orario ogni 60 secondi
    if ((unsigned long)(currentMillis - lastDisplayUpdate) >= 60 * 1000) {
      lastDisplayUpdate = currentMillis;
      updateTimeOnly();
    }
  }
  
  // Controllo del pulsante reset e messaggi di log web - gestione ottimizzata
  static unsigned long lastButtonCheck = 0;
  static unsigned long lastWebLogTime = 0;
  
  // Controlla il reset della configurazione solo ogni 300ms (ridotto significativamente)
  if (currentMillis - lastButtonCheck >= 300) {
    lastButtonCheck = currentMillis;
    checkResetButton();
    
    // Registra il messaggio di log solo ogni 30 secondi (molto ridotto)
    if (currentMillis - lastWebLogTime >= 30000) {
      lastWebLogTime = currentMillis;
      // Rimozione log periodici
    }
  }
  
  // Gestisci le richieste web e il captive portal ma limitalo in frequenza
  // Questa funzione viene chiamata meno frequentemente per ridurre l'impatto sul display
  static unsigned long lastWebServerCheck = 0;
  if (currentMillis - lastWebServerCheck >= 800) { // Circa una volta ogni 800ms anziché 150ms
    lastWebServerCheck = currentMillis;
    handleClientRequests(); // Gestisce sia server che DNS captive portal
  }
  
  // DISABILITATO: Controlla la connessione WiFi e passa in modalità AP se necessario
  // checkWiFiConnection();
  
  // Aggiungiamo un delay significativo per ridurre drasticamente l'uso della CPU
  // Aumentato a 500ms - questo riduce drasticamente la frequenza del ciclo principale
  delay(500);
}
