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
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <time.h>

// Moduli del progetto
#include "Hardware.h"
#include "Config.h"
#include "Display.h"
#include "WebServer.h"
#include "WebUIPages.h"
#include "WeatherUtils.h"
#include "NetworkUtils.h"
#include "SystemUtils.h"
#include "SVGHelper.h"
#include "WeatherIcons.h"
#include "Debug.h"
#include "Calendar.h"
#include "AtmoVerseConstants.h"
#include "QuotesManager.h"
#include "ButtonUtils.h"

// Definizione pin per il pulsante di reset configurazione e contatore di pressioni
#define RESET_BUTTON_PIN 35  // Pin del pulsante di RESET esterno
#define RESET_HOLD_TIME 5000 // Tempo di pressione continua per il reset (5 secondi)

// Variabili globali
unsigned long lastWeatherUpdate = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastNetworkCheck = 0;
int networkRetryCounter = 0;
bool lastUpdateSuccess = false;

// Setup iniziale
void setup() {
  // Inizializzazione serial monitor
  Serial.begin(SERIAL_BAUD_RATE);
  Serial.println("\n\n=== AtmoVerse 2.0 - Avvio ===");
  
  // Inizializzazione hardware
  initHardware();
  
  // Inizializzazione display e-ink
  initDisplay();
  displayStartupScreen();
  
  // Caricamento configurazione
  if (!loadConfig()) {
    Serial.println("Configurazione non trovata, utilizzo valori predefiniti");
    saveConfig(); // Salva la configurazione predefinita
  }
  
  // Inizializzazione SD card
  if (!SD.begin(SD_CS, sdSPI)) {
    Serial.println("Errore nell'inizializzazione della SD card");
    displayError("Errore SD Card");
    delay(2000);
  } else {
    Serial.println("SD card inizializzata");
  }
  
  // Inizializzazione SVG e icone meteo
  if (!SVGHelper::begin()) {
    Serial.println("Errore nell'inizializzazione SVG");
  }
  
  if (!WeatherIcons::begin()) {
    Serial.println("Errore nell'inizializzazione icone meteo");
  }
  
  // Inizializzazione modulo citazioni
  if (!QuotesManager::begin()) {
    Serial.println("Errore nell'inizializzazione gestore citazioni");
  }
  
  // Tentativo di connessione WiFi
  if (connectToWiFi(config.ssid, config.password, 20000)) {
    Serial.println("Connesso alla rete WiFi");
    
    // Sincronizzazione orario NTP
    configTime(config.gmtOffset_sec, config.daylightOffset_sec, "pool.ntp.org", "time.nist.gov");
    
    // Aggiorna meteo appena connesso
    getWeatherData();
    
    // Inizializza il server web in modalità station
    setupServer();
  } else {
    Serial.println("Impossibile connettersi alla rete WiFi, avvio in modalità AP");
    startAccessPoint();
    showAPModeInfo();
  }
  
  // Mostra display aggiornato
  updateDisplay();
}

// Loop principale ottimizzato per ridurre il consumo energetico
void loop() {
  unsigned long currentMillis = millis();
  
  // Aggiorna meteo ogni intervallo (se in modalità station)
  if (WiFi.status() == WL_CONNECTED) {
    if ((unsigned long)(currentMillis - lastWeatherUpdate) >= config.normalUpdateInterval * 60 * 1000) {
      lastWeatherUpdate = currentMillis;
      lastUpdateSuccess = getWeatherData();
      
      if (!lastUpdateSuccess) {
        networkRetryCounter++;
        Serial.printf("Tentativo %d/%d fallito\n", networkRetryCounter, config.maxNetworkRetries);
      } else {
        networkRetryCounter = 0;
      }
    }
    
    // Aggiorna il display ogni minuto
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
  
  // Aggiungiamo un delay significativo per ridurre drasticamente l'uso della CPU
  // Aumentato a 500ms - questo riduce drasticamente la frequenza del ciclo principale
  delay(500);
}
