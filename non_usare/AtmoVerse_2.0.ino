/**
 * AtmoVerse 2.0 - Calendario meteo con display e-ink
 * 
 * @file AtmoVerse_2.0.ino
 * @brief Applicazione principale per AtmoVerse 2.0 - sistema dual-core di gestione meteo
 * 
 * Questa applicazione implementa un sistema dual-core che mostra informazioni meteo 
 * e un calendario su un display e-ink, con aggiornamenti automatici.
 * Utilizza i due core dell'ESP32 per separare le operazioni di rete e display,
 * garantendo maggiore responsività e affidabilità.
 * 
 * - Core 0: gestione della rete, server web e API meteo
 * - Core 1: gestione del display e-ink e sistema di debug
 * 
 * @hardware ESP32, display e-ink GxEPD2, RTC DS3231, SD card, pulsanti
 * @author AtmoVerse Team
 * @version 2.0.1
 * @date 2023-05-23
 */

/*
 * AtmoVerse 2.0 - Versione ottimizzata
 * Sistema meteo con ESP32, display e-ink e interfaccia web
 * Calendario integrato per visualizzare la data corrente
 */

// Inclusione delle ottimizzazioni di build (rimosso: build_config.h superfluo)

// Librerie essenziali
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <SD.h>
#include <WiFi.h>
#include <DNSServer.h>
#include "Debug.h"
#include "DebugUtils.h"

// Variabile per il controllo periodico del server web
unsigned long lastWebServerCheck = 0;
#include <ArduinoJson.h>
#include <time.h>

// Moduli del progetto
#include "Hardware.h"
#include "Config.h"
#include "Display.h"
#include "WebServer.h"
#include "WebUIPages.h"
#include "WeatherUtils.h"
#include "Weather.h"
#include "NetworkUtils.h"
#include "SystemUtils.h"
#include "SVGHelper.h"
#include "WeatherIcons.h"
#include "Calendar.h"
#include "AtmoVerseConstants.h"
#include "QuotesManager.h"
#include "ButtonUtils.h"
#include "TimerUtils.h"
#include "SystemMonitor.h"
#include "RetryUtils.h"

// Variabili globali - ottimizzate
static unsigned long lastWeatherUpdate = 0; // static per limitare lo scope
static unsigned long lastDisplayUpdate = 0; // static per limitare lo scope
static unsigned long lastNetworkCheck = 0; // static per limitare lo scope
static unsigned long lastWebLogTime = 0; // static per limitare lo scope
static uint8_t networkRetryCounter = 0; // cambiato da int a uint8_t per ridurre memoria
// Variabile usata per indicare se l'ultimo aggiornamento meteo è stato completato con successo
bool lastWeatherUpdateSuccess = false; // mantenuta globale perché usata in altri file

// Le variabili per il controllo assoluto degli aggiornamenti del display sono definite in Display.cpp

// Gestione multi-core con FreeRTOS
TaskHandle_t DisplayTask;
TaskHandle_t NetworkTask;
extern TaskHandle_t DebugTask; // Definito in Debug.cpp
SemaphoreHandle_t displayMutex; // Mutex per proteggere l'accesso al display

// Flag per la comunicazione tra i core
volatile bool requestDisplayRefresh = false;
volatile bool isSystemInitialized = false;

// Coda per i messaggi di debug - definita in Debug.cpp
extern QueueHandle_t debugQueue;
extern SemaphoreHandle_t serialMutex; // Mutex per l'accesso a Serial

// Le funzioni debugPrint e debugPrintf sono definite in Debug.cpp
// Qui le dichiariamo come extern per poterle utilizzare
extern void debugPrint(const char* module, const char* message);
extern void debugPrintf(const char* module, const char* format, ...);

// La funzione debugTaskFunction è definita in Debug.cpp
extern void debugTaskFunction(void * parameter);

// Funzione per eseguire l'aggiornamento fisico del display (definita in Display.cpp)
// Questa funzione viene chiamata esclusivamente dal task del display
extern void execDisplayUpdate();

// Task che gestisce esclusivamente il display (Core 1)
void displayTaskFunction(void * parameter) {
  Serial.println("[SYSTEM] Task Display avviato sul Core " + String(xPortGetCoreID()));
  
  // Loop principale del task display
  while(true) {
    // Aspetta che il sistema sia inizializzato prima di procedere
    if (!isSystemInitialized) {
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      continue;
    }
    
    // Verifica se è necessario aggiornare il display e se è trascorso l'intervallo minimo
    if (requestDisplayRefresh) {
      unsigned long currentMillis = millis();
      static unsigned long lastDisplayPhysicalUpdate = 0;
      
      // Limitiamo gli aggiornamenti fisici a uno ogni 5 minuti
      if (currentMillis - lastDisplayPhysicalUpdate >= 5 * 60 * 1000) {
        Serial.println("\n[DISPLAY TASK] Esecuzione aggiornamento display sul Core " + String(xPortGetCoreID()));
        
        // Prendi il mutex per l'accesso esclusivo al display
        if (xSemaphoreTake(displayMutex, portMAX_DELAY) == pdTRUE) {
          // Esegui l'aggiornamento del display
          execDisplayUpdate();
          
          // Rilascia il mutex
          xSemaphoreGive(displayMutex);
          
          // Aggiorna il timestamp dell'ultimo aggiornamento
          lastDisplayPhysicalUpdate = currentMillis;
          
          // Resetta il flag di richiesta
          requestDisplayRefresh = false;
          
          Serial.println("[DISPLAY TASK] Aggiornamento display completato");
        }
      } else {
        Serial.print("[DISPLAY TASK] Intervallo minimo non rispettato. Tempo trascorso: ");
        Serial.print((currentMillis - lastDisplayPhysicalUpdate) / 1000);
        Serial.println(" secondi");
        
        // Anche se non aggiorniamo, resettiamo il flag dopo un certo tempo
        static unsigned long lastFlagResetTime = 0;
        if (currentMillis - lastFlagResetTime >= 30000) { // 30 secondi
          lastFlagResetTime = currentMillis;
          requestDisplayRefresh = false;
          Serial.println("[DISPLAY TASK] Flag di richiesta resettato per timeout");
        }
      }
    }
    
    // Delay per evitare che il task monopolizzi la CPU
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

// Task che gestisce la rete e altri processi (Core 0)
void networkTaskFunction(void * parameter) {
  Serial.println("[SYSTEM] Task Network avviato sul Core " + String(xPortGetCoreID()));
  
  // Variabili locali per il task
  unsigned long lastWebServerCheck = 0;
  unsigned long currentMillis;
  
  // Loop principale del task di rete
  while(true) {
    currentMillis = millis();
    
    // Aspetta che il sistema sia inizializzato prima di procedere
    if (!isSystemInitialized) {
      vTaskDelay(500 / portTICK_PERIOD_MS);
      continue;
    }
    
    // Gestione della modalità operativa
    if (apMode) {
      // In modalità AP, gestisci le richieste del captive portal
      handleClientRequests();
      
      // Aggiornamento orario (solo richiesta, l'esecuzione è gestita dal task display)
      if ((unsigned long)(currentMillis - lastDisplayUpdate) >= 5 * 60 * 1000) {
        lastDisplayUpdate = currentMillis;
        requestDisplayRefresh = true;
        Serial.println("[NETWORK TASK] Richiesto aggiornamento display in modalità AP");
      }
    } 
    else if (WiFi.status() == WL_CONNECTED) {
      // Modalità normale connessa: gestione avanzata aggiornamento meteo
      bool shouldUpdateWeather = false;
      
      // Caso 1: è tempo di aggiornamento regolare
      if ((unsigned long)(currentMillis - lastWeatherUpdate) >= config.normalUpdateInterval * 60 * 1000) {
        shouldUpdateWeather = true;
      }
      // Caso 2: è tempo di ritentare dopo un fallimento precedente
      else if (!lastWeatherUpdateSuccess && isTimeToRetryWeather()) {
        DEBUG_LOG(DEBUG_CATEGORY_SYSTEM, DEBUG_LEVEL_INFO, 
                "Tentativo di recupero dati meteo falliti in corso...");
        shouldUpdateWeather = true;
      }
      
      // Esegui l'aggiornamento meteo se necessario
      if (shouldUpdateWeather) {
        lastWeatherUpdate = currentMillis;
        lastWeatherUpdateSuccess = getWeatherData();
        
        if (!lastWeatherUpdateSuccess) {
          networkRetryCounter++;
          String retryStatus = getWeatherRetryStatus();
          DEBUG_LOG(DEBUG_CATEGORY_SYSTEM, DEBUG_LEVEL_WARNING, 
                "Aggiornamento meteo fallito: %s", 
                retryStatus.length() > 0 ? retryStatus.c_str() : "nessun dettaglio");
          
          // Registra l'errore nel monitoraggio di sistema
          SystemMonitor::recordError(COMPONENT_WEATHER_API, retryStatus.c_str());
          
          // Se superato numero massimo di tentativi, log più grave
          if (networkRetryCounter >= config.maxNetworkRetries) {
            DEBUG_LOG(DEBUG_CATEGORY_SYSTEM, DEBUG_LEVEL_ERROR, 
                  "LIMITE MASSIMO TENTATIVI RAGGIUNTO - Verificare connessione e API key");
            // Genera un report completo dello stato del sistema
            SystemMonitor::printStatusReport(true);
          }
        } else {
          networkRetryCounter = 0;
          DEBUG_LOG(DEBUG_CATEGORY_SYSTEM, DEBUG_LEVEL_INFO, 
                "Aggiornamento dati meteo completato con successo");
          
          // Registra il successo nel monitoraggio di sistema
          SystemMonitor::recordSuccess(COMPONENT_WEATHER_API);
        }
        
        // Richiede l'aggiornamento del display dopo ogni tentativo (riuscito o meno)
        requestDisplayRefresh = true;
      }
      
      // Controlla periodicamente il webserver
      if (currentMillis - lastWebServerCheck >= 3000) { // Ogni 3 secondi
        lastWebServerCheck = currentMillis;
        if (server.hasClient()) {
          handleClientRequests();
        }
      }
      
      // Richiedi un aggiornamento periodico del display
      if ((unsigned long)(currentMillis - lastDisplayUpdate) >= 10 * 60 * 1000) { // Ogni 10 minuti
        lastDisplayUpdate = currentMillis;
        requestDisplayRefresh = true;
        Serial.println("[NETWORK TASK] Richiesto aggiornamento periodico display");
      }
    }
    
    // Delay per non sovraccaricare la CPU
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

// Le variabili globali per il sistema dual-core sono già state definite all'inizio del file
// Questa sezione duplicata è stata rimossa per evitare errori di compilazione

// La funzione execDisplayUpdate() è già definita nel file Display.cpp
// Non definiamo nuovamente questa funzione qui per evitare errori di compilazione

/**
 * @brief Funzione di setup - inizializzazione del sistema
 * 
 * Questa funzione viene chiamata una sola volta all'avvio e inizializza
 * tutto il sistema, compresa la creazione dei task per i due core.
 */
void setup() {
  // Inizializzazione della comunicazione seriale
  Serial.begin(115200);
  Serial.println("\n\nAtmoVerse 2.0 - Inizializzazione sistema");
  
  // Inizializzazione del sistema di debug
  initDebugSystem();
  
  // Mutex per proteggere l'accesso al display
  displayMutex = xSemaphoreCreateMutex();
  
  // Inizializzazione del sistema di monitoraggio (prima dell'hardware)
  SystemMonitor::init();
  
  // Inizializzazione hardware con verifica
  bool hwSuccess = initHardware();
  if (hwSuccess) {
    SystemMonitor::recordSuccess(COMPONENT_DISPLAY);
    SystemMonitor::recordSuccess(COMPONENT_SD_CARD);
  } else {
    SystemMonitor::recordError(COMPONENT_DISPLAY, "Errore inizializzazione hardware");
    DEBUG_LOG(DEBUG_CATEGORY_SYSTEM, DEBUG_LEVEL_ERROR, "Errore inizializzazione hardware");
  }
  
// Se abbiamo una configurazione valida, tenta la connessione WiFi
if (strlen(config.ssid) > 0) {
  bool wifiSuccess = connectToWiFi(config.ssid, config.password, 10000);
  if (wifiSuccess) {
    SystemMonitor::recordSuccess(COMPONENT_WIFI);
    DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_INFO, "WiFi connesso a %s", WiFi.SSID().c_str());
  } else {
    SystemMonitor::recordError(COMPONENT_WIFI, "Impossibile connettersi a WiFi");
    DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_WARNING, "Connessione WiFi fallita");
  }
}
  
// Avvia il task di debug sul Core 1
startDebugTask();
  
// Avvia i task principali
xTaskCreatePinnedToCore(
  displayTaskFunction,   // Funzione da eseguire
  "DisplayTask",         // Nome del task
#if defined(MINIMIZE_BUFFERS)
  3584,                  // Stack size ulteriormente ridotto da 4096 a 3584 bytes
#else
  4096,                  // Stack size ridotto da 8192 a 4096 bytes
#endif
  NULL,                  // Parametri
  1,                     // Priorità (0-24, 24 = massima)
  &DisplayTask,          // Handle del task
  1                      // Core (1 = secondo core)
);
  
xTaskCreatePinnedToCore(
  networkTaskFunction,   // Funzione da eseguire
  "NetworkTask",         // Nome del task
#if defined(MINIMIZE_BUFFERS)
  4096,                  // Stack size ulteriormente ridotto da 5120 a 4096 bytes
#else
  5120,                  // Stack size ridotto da 8192 a 5120 bytes
#endif
  NULL,                  // Parametri
  1,                     // Priorità (0-24, 24 = massima)
  &NetworkTask,          // Handle del task
  0                      // Core (0 = primo core)
);
  
Serial.println("Inizializzazione completata");
isSystemInitialized = true;
}

/**
 * @brief Loop principale minimo per il sistema dual-core
 * 
 * Nel sistema dual-core, il loop principale viene utilizzato principalmente per il monitoraggio
 * e la gestione di eventi critici come il reset del dispositivo. La maggior parte della logica
 * applicativa è ora distribuita nei task dedicati sui due core dell'ESP32.
 * Il loop principale funziona anche come watchdog software per verificare la salute dei task.
 */
void loop() {
  // =====================================================
  // Monitoraggio e operazioni essenziali non delegabili ai task
  // =====================================================
  
  // Variabili statiche per il monitoraggio del pulsante di reset
  static unsigned long lastResetButtonCheck = 0;
  static unsigned long resetButtonPressStart = 0;
  static bool resetButtonPressed = false;
  
  unsigned long currentMillis = millis();
  
  // Controllo del pulsante di reset (ogni 100ms per evitare il rimbalzo)
  // Questa operazione viene mantenuta nel loop principale per garantire
  // che sia sempre possibile resettare il dispositivo anche in caso di
  // problemi con i task FreeRTOS
  if (currentMillis - lastResetButtonCheck >= 100) {
    lastResetButtonCheck = currentMillis;
    
    // Verifica se il pulsante RESET è premuto (logica invertita con pull-up)
    if (digitalRead(RESET_BUTTON_PIN) == LOW) {
      // Prima pressione del pulsante rilevata
      if (!resetButtonPressed) {
        resetButtonPressed = true;
        resetButtonPressStart = currentMillis;
        DEBUG_SYS("Pulsante RESET premuto - tenere premuto per reset");
      } 
      // Controllo se il pulsante è stato tenuto premuto abbastanza a lungo
      else if (currentMillis - resetButtonPressStart >= RESET_HOLD_TIME) {
        // Pulsante tenuto premuto per il tempo necessario al reset
        DEBUG_SYS("**** RESET CONFIGURAZIONE ****");
        DEBUG_SYS("Cancellazione configurazione in corso...");
        
        // Esegue la cancellazione della configurazione
        resetConfig(); // Corretto nome della funzione
        
        // Mostra un messaggio di conferma sul display
        displayError("Configurazione resettata");
        
        // Utilizzo Timer invece di delay(2000)
        static Timer resetConfirmationTimer(2000, false);
        resetConfirmationTimer.reset();
        resetConfirmationTimer.enable();
        while (!resetConfirmationTimer.isReady()) { yield(); } // Consente altri processi
        
        // Riavvia il dispositivo per applicare il reset
        DEBUG_SYS("Riavvio in corso...");
        ESP.restart();
      }
    } 
    // Il pulsante è stato rilasciato prima del tempo necessario per il reset
    else if (resetButtonPressed) {
      resetButtonPressed = false;
      DEBUG_SYS("Pulsante RESET rilasciato - reset annullato");
    }
  }
  
  // =====================================================
  // Sistema di watchdog software per monitorare i task
  // =====================================================
  
  /**
   * Questo watchdog controlla periodicamente lo stato dei task FreeRTOS
   * e riavvia il dispositivo se uno di essi risulta inattivo o bloccato.
   * È un meccanismo di sicurezza fondamentale in un sistema dual-core
   * per garantire che i task critici continuino a funzionare correttamente.
   */
  static unsigned long lastWatchdogCheck = 0;
  
  // Esegue il controllo dei task ogni 30 secondi
  if (currentMillis - lastWatchdogCheck >= 30000) {
    lastWatchdogCheck = currentMillis;
    DEBUG_SYS("Controllo watchdog: verifica stato task...");
    
    // Verifica lo stato del task display sul Core 1
    eTaskState displayTaskStatus = eTaskGetState(DisplayTask);
    if (displayTaskStatus == eDeleted || displayTaskStatus == eSuspended) {
      DEBUG_SYS("ERRORE CRITICO: DisplayTask non attivo o bloccato!");
      SystemMonitor::recordError(COMPONENT_DISPLAY, "Task eliminato o sospeso");
      
      // Verifica se il sistema è in stato critico
      if (SystemMonitor::getSystemState() >= SYSTEM_CRITICAL) {
        DEBUG_SYS("Sistema in stato CRITICO, esecuzione riavvio di emergenza...");
        SystemMonitor::printStatusReport(true);
        ESP.restart(); // Riavvia il dispositivo come misura di recupero
      }
    } else {
      SystemMonitor::recordSuccess(COMPONENT_DISPLAY);
    }
    
    // Verifica lo stato del task network sul Core 0
    eTaskState networkTaskStatus = eTaskGetState(NetworkTask);
    if (networkTaskStatus == eDeleted || networkTaskStatus == eSuspended) {
      DEBUG_SYS("ERRORE CRITICO: NetworkTask non attivo o bloccato!");
      SystemMonitor::recordError(COMPONENT_WIFI, "Task eliminato o sospeso");
      
      // Verifica se il sistema è in stato critico
      if (SystemMonitor::getSystemState() >= SYSTEM_CRITICAL) {
        DEBUG_SYS("Sistema in stato CRITICO, esecuzione riavvio di emergenza...");
        SystemMonitor::printStatusReport(true);
        ESP.restart(); // Riavvia il dispositivo come misura di recupero
      }
    } else {
      // Non aggiorniamo qui lo stato WiFi perché è già gestito altrove
    }
    
    // Verifica lo stato del task debug (se configurato)
    eTaskState debugTaskStatus = eTaskGetState(DebugTask);
    if (debugTaskStatus == eDeleted || debugTaskStatus == eSuspended) {
      // Nota: se il task debug è inattivo, utilizziamo Serial direttamente
      // poiché le macro di debug potrebbero non funzionare
      Serial.println("[SYSTEM] ERRORE CRITICO: DebugTask non attivo o bloccato!");
      // Non usiamo SystemMonitor qui perché dipende dal debug
      
      // Riavvia direttamente il dispositivo
      Serial.println("[SYSTEM] Esecuzione riavvio di emergenza...");
      ESP.restart(); // Riavvia il dispositivo come misura di recupero
    }
    
    // Genera un report di stato periodico
    SystemMonitor::printStatusReport();
    
    DEBUG_SYS("Controllo watchdog completato: tutti i task attivi");
  }
  
  // Controllo del pulsante reset e messaggi di log web - gestione ottimizzata
  static unsigned long lastButtonCheck = 0;
  
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
  
  // Gestione separata per modalità AP e modalità normale
  static unsigned long lastDNSCheck = 0;
  static unsigned long webServerCheckTime = 0; // Rinominata per evitare conflitti con la variabile nel task di rete
  
  if (apMode) {
    // In modalità AP, gestisci il server DNS più frequentemente
    if (currentMillis - lastDNSCheck >= 100) { // Controlla il DNS ogni 100ms
      lastDNSCheck = currentMillis;
      // Processa il DNS senza bisogno di controllo (il flag apMode è già true)
      dnsServer.processNextRequest();
    }
    
    // Gestisci il web server circa ogni 800ms
    if (currentMillis - webServerCheckTime >= 800) {
      webServerCheckTime = currentMillis;
      handleWebServerOnly(); // Funzione modificata che gestisce solo il web server, non il DNS
    }
    
    // Nessun delay in modalità AP per garantire una migliore reattività
    // Il loop verrà eseguito naturalmente al ritmo giusto
    yield(); // Consente ad altri task di essere eseguiti
  } else {
    // In modalità stazione, gestisci le richieste web con frequenza ridotta
    // Aumentato intervallo da 800ms a 3 secondi per ridurre stress sul display e-ink
    if (currentMillis - lastWebServerCheck >= 3000) {
      lastWebServerCheck = currentMillis;
      
      // Verifica se ci sono client attivi prima di chiamare handleClientRequests
      // Questo evita chiamate non necessarie che possono causare aggiornamenti del display
      if (WiFi.status() == WL_CONNECTED && server.hasClient()) {
        handleClientRequests(); // Gestisce sia server che DNS captive portal
      }
    }
    
    // Evito delay bloccante in modalità stazione
    // Il ritardo è gestito in modo non bloccante dal sistema di task
    yield(); // Consente ad altri task di essere eseguiti
  }
}
