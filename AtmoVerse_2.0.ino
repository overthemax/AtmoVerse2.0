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
// Variabile usata per indicare se l'ultimo aggiornamento meteo è stato completato con successo
bool lastWeatherUpdateSuccess = false;

// Le variabili per il controllo assoluto degli aggiornamenti del display sono definite in Display.cpp

// Gestione multi-core con FreeRTOS
TaskHandle_t DisplayTask;
TaskHandle_t NetworkTask;
TaskHandle_t DebugTask;
SemaphoreHandle_t displayMutex; // Mutex per proteggere l'accesso al display

// Flag per la comunicazione tra i core
volatile bool requestDisplayRefresh = false;
volatile bool isSystemInitialized = false;

// Coda per i messaggi di debug
#define DEBUG_QUEUE_SIZE 20
#define DEBUG_MESSAGE_SIZE 128
QueueHandle_t debugQueue;
SemaphoreHandle_t serialMutex; // Mutex per l'accesso a Serial

// Funzione di debug asincrona che invia alla coda invece di stampare direttamente
void debugPrint(const char* module, const char* message) {
  if (!isSystemInitialized) {
    // Durante l'inizializzazione, stampa direttamente
    if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
      Serial.print("[");
      Serial.print(module);
      Serial.print("] ");
      Serial.println(message);
      xSemaphoreGive(serialMutex);
    }
    return;
  }
  
  // Crea il messaggio da inviare alla coda
  char fullMessage[DEBUG_MESSAGE_SIZE];
  snprintf(fullMessage, DEBUG_MESSAGE_SIZE, "[%s] %s", module, message);
  
  // Invia alla coda con timeout di 10ms (non bloccare se la coda è piena)
  if (xQueueSend(debugQueue, fullMessage, pdMS_TO_TICKS(10)) != pdPASS) {
    // Se la coda è piena, perdiamo il messaggio (senza bloccare il chiamante)
  }
}

// Versione che accetta anche parametri formattati (come printf)
void debugPrintf(const char* module, const char* format, ...) {
  char buffer[DEBUG_MESSAGE_SIZE];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, DEBUG_MESSAGE_SIZE, format, args);
  va_end(args);
  
  debugPrint(module, buffer);
}

// Task dedicato al debug che gira sul Core 1
void debugTaskFunction(void * parameter) {
  debugPrint("SYSTEM", "Task Debug avviato sul Core " + String(xPortGetCoreID()));
  
  // Buffer per il messaggio ricevuto dalla coda
  char message[DEBUG_MESSAGE_SIZE];
  
  // Loop principale del task debug
  while(true) {
    // Aspetta messaggi dalla coda di debug
    if (xQueueReceive(debugQueue, message, pdMS_TO_TICKS(1000)) == pdPASS) {
      // Abbiamo ricevuto un messaggio, stampiamolo
      if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
        Serial.println(message);
        xSemaphoreGive(serialMutex);
      }
    }
    
    // Piccolo delay per non monopolizzare la CPU
    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
}

// Funzione per eseguire l'aggiornamento fisico del display
// Questa funzione viene chiamata esclusivamente dal task del display
void execDisplayUpdate() {
  debugPrint("DISPLAY", "Esecuzione aggiornamento fisico del display");
  
  // Ottieni l'ora corrente
  struct tm timeinfo;
  getLocalTime(&timeinfo);
  
  // Aggiornamento completo del display
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    
    // Disegna i contenuti del display (data, meteo, ecc.)
    drawDisplayContent();
    
    // Se l'ultimo aggiornamento meteo ha avuto problemi, mostra un indicatore
    if (!lastWeatherUpdateSuccess) {
      // Disegna un'icona di avvertimento in alto a destra
      int iconX = display.width() - 30;
      int iconY = 15;
      
      // Triangolo di avvertimento
      display.fillTriangle(
        iconX, iconY + 20,           // Base sinistra
        iconX + 20, iconY + 20,      // Base destra
        iconX + 10, iconY,           // Punta
        GxEPD_BLACK
      );
      
      // Punto esclamativo all'interno
      display.fillRect(iconX + 9, iconY + 5, 2, 10, GxEPD_WHITE);
      display.fillRect(iconX + 9, iconY + 16, 2, 2, GxEPD_WHITE);
    }
  } while (display.nextPage());
  
  Serial.println("[DISPLAY] Aggiornamento fisico completato");
}

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
      // Modalità normale connessa: aggiorna i dati meteo secondo configurazione
      if ((unsigned long)(currentMillis - lastWeatherUpdate) >= config.normalUpdateInterval * 60 * 1000) {
        lastWeatherUpdate = currentMillis;
        lastWeatherUpdateSuccess = getWeatherData();
        
        if (!lastWeatherUpdateSuccess) {
          networkRetryCounter++;
          Serial.printf("[NETWORK TASK] Tentativo %d/%d fallito\n", networkRetryCounter, config.maxNetworkRetries);
        } else {
          networkRetryCounter = 0;
          // Richiedi un aggiornamento del display dopo aver aggiornato i dati meteo
          requestDisplayRefresh = true;
          Serial.println("[NETWORK TASK] Richiesto aggiornamento display dopo nuovo meteo");
        }
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

// =========================================================
// Variabili globali per il sistema dual-core
// =========================================================

/**
 * @brief Handle per il task di rete sul Core 0
 * Utilizzato per controllare e monitorare il task che gestisce WiFi, server web e API meteo
 */
TaskHandle_t NetworkTask;

/**
 * @brief Handle per il task del display sul Core 1
 * Utilizzato per controllare e monitorare il task che gestisce il display e-ink
 */
TaskHandle_t DisplayTask;

/**
 * @brief Mutex per proteggere l'accesso al display e-ink
 * Previene accessi concorrenti al display da parte di diversi task
 */
SemaphoreHandle_t displayMutex;

/**
 * @brief Flag per richiedere un aggiornamento del display
 * Quando impostato a true, il task del display aggiornerà il display alla prossima opportunità
 * Volatile per garantire la visibilità immediata tra diversi core/task
 */
volatile bool requestDisplayRefresh = false;

/**
 * @brief Flag che indica se il sistema è completamente inizializzato
 * I task attendono che questo flag sia true prima di iniziare le operazioni normali
 */
volatile bool isSystemInitialized = false;

/**
 * @brief Funzione principale del task per la gestione del display (Core 1)
 * 
 * Questo task si occupa esclusivamente della gestione del display e-ink,
 * eseguendo gli aggiornamenti quando richiesto e rispettando i vincoli
 * temporali necessari per preservare la durata del display.
 * Viene eseguito sul Core 1 per separare le operazioni di visualizzazione
 * dalle operazioni di rete e logica principale.
 * 
 * @param parameter Parametri del task (non utilizzati)
 */
void displayTaskFunction(void * parameter) {
  // Log dell'avvio del task con informazione sul core in uso
  DEBUG_DISP("Task Display avviato sul Core " + String(xPortGetCoreID()));
  
  // Loop principale del task display - viene eseguito per tutta la durata del sistema
  while(true) {
    // Attende che il sistema sia completamente inizializzato prima di procedere
    // Questo evita aggiornamenti prematuri del display durante la fase di avvio
    if (!isSystemInitialized) {
      vTaskDelay(1000 / portTICK_PERIOD_MS); // Attesa di 1 secondo
      continue; // Salta il resto del ciclo e ricontrolla
    }
    
    // Verifica se è necessario aggiornare il display in base al flag di richiesta
    if (requestDisplayRefresh) {
      unsigned long currentMillis = millis();
      static unsigned long lastDisplayPhysicalUpdate = 0;
      
      // Limitiamo gli aggiornamenti fisici del display e-ink a uno ogni 5 minuti
      // Questo è fondamentale per preservare la durata del display e ridurre il consumo energetico
      if (currentMillis - lastDisplayPhysicalUpdate >= 5 * 60 * 1000) {
        DEBUG_DISP("Esecuzione aggiornamento display");
        
        // Acquisizione del mutex per garantire l'accesso esclusivo al display
        // Importante: il mutex protegge il display da accessi concorrenti da parte di altri task
        if (xSemaphoreTake(displayMutex, portMAX_DELAY) == pdTRUE) {
          // Esecuzione dell'aggiornamento completo del display e-ink
          // Utilizziamo l'aggiornamento a pagine supportato dalla libreria GxEPD2
          display.setFullWindow();  // Imposta l'aggiornamento su tutto il display
          display.firstPage();      // Inizia la sequenza di aggiornamento
          do {
            display.fillScreen(GxEPD_WHITE);  // Pulisce il display con sfondo bianco
            drawDisplayContent();             // Disegna il contenuto attuale (meteo, orario, ecc.)
          } while (display.nextPage());       // Continua finché tutte le pagine sono aggiornate
          
          // Rilascio del mutex dopo aver completato l'aggiornamento
          // Questo permette ad altri task di accedere al display se necessario
          xSemaphoreGive(displayMutex);
          
          // Aggiornamento del timestamp per tenere traccia dell'ultimo refresh fisico
          lastDisplayPhysicalUpdate = currentMillis;
          
          // Reset del flag di richiesta dopo aver completato l'aggiornamento
          requestDisplayRefresh = false;
          
          DEBUG_DISP("Aggiornamento display completato con successo");
        }
      } else {
        // Se non è ancora trascorso l'intervallo minimo, loghiamo periodicamente
        // ma limitiamo i log a uno ogni 30 secondi per non intasare la console
        static unsigned long lastLogTime = 0;
        if (currentMillis - lastLogTime > 30000) { // Log massimo ogni 30 secondi 
          lastLogTime = currentMillis;
          DEBUG_DISP_F("Aggiornamento posticipato: intervallo minimo non rispettato. Tempo trascorso: %d secondi", 
                      (currentMillis - lastDisplayPhysicalUpdate) / 1000);
        }
      }
    }
    
    // Delay per evitare che il task monopolizzi la CPU
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

/**
 * @brief Funzione del task per la gestione della rete sul Core 0
 * 
 * Questo task si occupa di tutte le operazioni di rete: gestione WiFi,
 * server web, aggiornamento dati meteo da API esterne e sincronizzazione NTP.
 * Viene eseguito sul Core 0 per separare le operazioni di rete da quelle
 * di visualizzazione, evitando interferenze e migliorando le prestazioni.
 * 
 * @param parameter Parametri del task (non utilizzati)
 */
void networkTaskFunction(void * parameter) {
  // Log dell'avvio del task con informazione sul core in uso
  DEBUG_SYS("Task Network avviato sul Core " + String(xPortGetCoreID()));
  
  // Variabili locali per tracciare gli intervalli di tempo tra le operazioni
  unsigned long lastWebServerCheck = 0;    // Timestamp ultimo controllo del server web
  unsigned long lastWeatherUpdate = 0;     // Timestamp ultimo aggiornamento dati meteo
  unsigned long lastDisplayUpdate = 0;     // Timestamp ultima richiesta di aggiornamento display
  
  // Loop principale del task di rete - continua per tutta la durata del sistema
  while(true) {
    // Timestamp corrente per i controlli temporali
    unsigned long currentMillis = millis();
    
    // Attende che il sistema sia completamente inizializzato prima di procedere
    // Questo evita operazioni di rete premature durante la fase di avvio
    if (!isSystemInitialized) {
      vTaskDelay(500 / portTICK_PERIOD_MS); // Attesa di 500ms
      continue; // Salta il resto del ciclo e ricontrolla
    }
    
    // =====================================================
    // Gestione della modalità operativa (AP o WiFi Station)
    // =====================================================
    if (apMode) {
      /**
       * MODALITÀ ACCESS POINT
       * In questa modalità, il dispositivo funziona come un access point
       * che serve un portale captive per permettere la configurazione
       * tramite interfaccia web senza necessità di connessione WiFi.
       */
      
      // Gestione delle richieste HTTP al portale captive
      handleClientRequests();
      
      // Aggiornamento periodico del display in modalità AP (ogni 5 minuti)
      // Solo richiesta - l'esecuzione dell'aggiornamento è gestita dal task display sul Core 1
      if ((unsigned long)(currentMillis - lastDisplayUpdate) >= 5 * 60 * 1000) {
        lastDisplayUpdate = currentMillis;
        requestDisplayRefresh = true; // Flag per richiedere l'aggiornamento
        DEBUG_SYS("Richiesto aggiornamento display in modalità AP");
      }
    } 
    else if (WiFi.status() == WL_CONNECTED) {
      /**
       * MODALITÀ STATION (CONNESSO A WIFI)
       * In questa modalità, il dispositivo è connesso a una rete WiFi
       * e può accedere a Internet per ottenere dati meteo e orario.
       */
      
      // Aggiornamento dati meteo secondo l'intervallo configurato dall'utente
      if ((unsigned long)(currentMillis - lastWeatherUpdate) >= config.normalUpdateInterval * 60 * 1000) {
        lastWeatherUpdate = currentMillis;
        DEBUG_WEATHER("Richiedo aggiornamento dati meteo");
        
        // Chiamata all'API meteo e gestione risultato
        bool weatherSuccess = getWeatherData();
        
        if (!weatherSuccess) {
          // Gestione errore nell'aggiornamento dati meteo
          DEBUG_WEATHER("Errore aggiornamento dati meteo");
        } else {
          DEBUG_WEATHER("Dati meteo aggiornati con successo");
          // Richiedi un aggiornamento del display dopo aver ricevuto nuovi dati meteo
          requestDisplayRefresh = true;
        }
      }
      
      // Controllo periodico del server web (ogni 3 secondi)
      // per gestire eventuali richieste dell'interfaccia di configurazione
      if (currentMillis - lastWebServerCheck >= 3000) {
        lastWebServerCheck = currentMillis;
        if (server.hasClient()) {
          handleClientRequests();
        }
      }
      
      // Richiesta di aggiornamento periodico del display (ogni 10 minuti)
      // per aggiornare l'orario e altre informazioni che cambiano nel tempo
      if ((unsigned long)(currentMillis - lastDisplayUpdate) >= 10 * 60 * 1000) {
        lastDisplayUpdate = currentMillis;
        requestDisplayRefresh = true;
        DEBUG_SYS("Richiesto aggiornamento periodico display (refresh orario)");
      }
    }
    
    // Delay per non sovraccaricare la CPU
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

// Setup iniziale
void setup() {
  // Inizializzazione base per Serial e sistema di debug
  Serial.begin(115200);
  delay(500); // Breve delay per stabilizzazione
  
  // Logo iniziale
  Serial.println("\n====== AtmoVerse 2.0 DUAL-CORE ======\n");
  Serial.println("Calendario meteo con display e-ink");
  Serial.println("Versione firmware: " + String(FIRMWARE_VERSION));
  Serial.println("Data build: " + String(__DATE__) + " " + String(__TIME__));
  Serial.println("Core attuale: " + String(xPortGetCoreID()));
  Serial.println("================================\n");
  
  // Inizializzazione sistema di debug sul Core 1
  initDebugSystem();
  
  // Inizializzazione hardware
  initHardware();
  
  // Inizializzazione pulsanti
  initButtons();
  
  // Inizializzazione display e-ink
  initDisplay();
  displayStartupScreen();
  
  // Inizializzazione SD card
  if (!SD.begin(SD_CS, sdSPI)) {
    Serial.println("Errore nell'inizializzazione della SD card");
    displayError("Errore SD Card");
    delay(2000);
  } else {
    Serial.println("SD card inizializzata correttamente");
  }
  
  // Caricamento e validazione della configurazione
  Serial.println("Tentativo di caricamento configurazione...");
  bool configOk = false;
  
  if (!loadConfig()) {
    Serial.println("Configurazione non trovata, utilizzo valori predefiniti");
    saveConfig(); // Salva la configurazione predefinita
  } else {
    Serial.println("Configurazione caricata con successo!");
    Serial.print("SSID: '");
    Serial.print(config.ssid);
    Serial.println("'");
    Serial.print("Città: '");
    Serial.print(config.city);
    Serial.println("'");
    
    // Controlliamo esplicitamente che la configurazione sia valida
    configOk = checkConfigValidity();
    
    if (!configOk) {
      Serial.println("\nATTENZIONE: La configurazione caricata non è valida!");
      Serial.println("Il dispositivo partirà in modalità AP per permetterti di configurarlo.");
    } else {
      Serial.println("\nConfigurazione valida, procedo con la connessione WiFi.");
    }
  }
  
  // Inizializzazione SVG e icone meteo
  initSVGHelper();
  initWeatherIcons();
  // Controlla se configurare o meno la modalità AP
  apMode = !configOk || (strlen(config.ssid) == 0);

  // Se la modalità non è AP, prova a connettersi al WiFi
  // Inizializzazione modulo citazioni
  if (!QuotesManager::begin()) {
    Serial.println("Errore nell'inizializzazione gestore citazioni");
  }

  // Decidiamo se procedere con la connessione WiFi o avviare la modalità AP
  if (!configOk) {
    Serial.println("----- FASE: Avvio modalità Access Point -----");
    Serial.println("Configurazione non valida, avvio in modalità AP...");
    startAccessPoint(true); // Forza l'avvio in modalità AP
    showAPModeInfo();
  Serial.print(config.ssid);
  Serial.println("'");
  
  // Tentativo di connessione con timeout di 30 secondi e migliore gestione degli errori
  Serial.println("Tento connessione WiFi con timeout esteso (30 secondi)...");
  
  // Primo tentativo di connessione
  if (connectToWiFi(config.ssid, config.password, 30000)) {
    Serial.println("OK: Connesso alla rete WiFi!");
    
    // Debug: segnala l'inizio della sincronizzazione NTP
    Serial.println("----- FASE: Sincronizzazione orario -----");
    setupTimeServer();  // Configura il server NTP
    
    Serial.println("Tentativo di ottenere l'orario dal server NTP...");
    // Qui non c'è un vero controllo sul successo della sincronizzazione NTP
    // lo faremo in futuro...
    
    // Debug: segnala l'inizio del setup del server web
    Serial.println("----- FASE: Inizializzazione server web -----");
    
    // Inizializzazione del server web
    setupServer();
    
    // Aggiornamento display con informazioni di connessione
    Serial.println("----- FASE: Aggiornamento display con info connessione -----");
    String ipAddress = WiFi.localIP().toString();
    displayConnectionInfo(ipAddress.c_str());
    Serial.println("OK: Server web inizializzato");
    
    // Prima scarica i dati meteo
    Serial.println("----- FASE: Scaricamento dati meteo -----");
    bool weatherSuccess = false;
    try {
      weatherSuccess = getWeatherData();
      lastWeatherUpdateSuccess = weatherSuccess;
      Serial.print("Aggiornamento meteo: ");
      Serial.println(weatherSuccess ? "OK" : "FALLITO");
    } catch(const std::exception& e) {
      Serial.print("Errore durante il recupero dei dati meteo: ");
      Serial.println(e.what());
    } catch(...) {
      Serial.println("Errore sconosciuto durante il recupero dei dati meteo");
    }
    
    // Breve pausa per consentire al sistema di stabilizzarsi
    delay(500);
    
    // Ora aggiorna il display con i dati ottenuti
    Serial.println("----- FASE: Aggiornamento display finale -----");
    delay(100); // Pausa per stabilizzare il sistema
    
    // Controlla se ci sono dati meteo disponibili prima di aggiornare il display
    if (weatherSuccess) {
      Serial.println("Aggiornamento display con dati meteo");
      updateDisplay();
      Serial.println("OK: Display aggiornato con successo");
    } else {
      Serial.println("Nessun dato meteo disponibile, mostro info connessione");
      displayConnectionInfo(ipAddress.c_str());
      Serial.println("Display aggiornato con info connessione");
    }
  } else {
    // La connessione WiFi è fallita anche dopo diversi tentativi
    Serial.println("ATTENZIONE: Connessione WiFi fallita dopo ripetuti tentativi");
    Serial.println("Controllo se si tratta di un problema temporaneo...");
    
    // Breve attesa prima di riprovare - potrebbe essere un problema temporaneo
    delay(2000);
    
    // ULTIMO TENTATIVO di connessione WiFi con un timeout più lungo
    Serial.println("Ultimo tentativo di connessione con timeout esteso...");
    if (connectToWiFi(config.ssid, config.password, 45000)) { // Timeout extra lungo per l'ultimo tentativo
      Serial.println("OK: Connesso alla rete WiFi al secondo tentativo!");
      // Debug: segnala l'inizio della sincronizzazione NTP
      Serial.println("----- FASE: Sincronizzazione orario -----");
      setupTimeServer();
      Serial.println("----- FASE: Setup callback client web -----");
      
      // Inizializza il server web
      setupServer();
      String ipAddress = WiFi.localIP().toString();
      displayConnectionInfo(ipAddress.c_str());
      
      // Aggiorna il display finale
      updateDisplay();
    } else {
      // Tutti i tentativi sono falliti, avvio in modalità Access Point
      Serial.println("\n============================================");
      Serial.println("TUTTI I TENTATIVI DI CONNESSIONE WIFI FALLITI");
      Serial.println("Avvio in modalità Access Point per permettere la configurazione");
      Serial.println("============================================\n");
      
      // Mostra istruzioni dettagliate per l'utente
      Serial.println("Per configurare il dispositivo:");
      Serial.println("1. Connettiti alla rete WiFi 'AtmoVerse-Setup'");
      Serial.println("2. Apri il browser e vai all'indirizzo: 192.168.4.1");
      Serial.println("3. Inserisci le credenziali corrette della tua rete WiFi");
      
      // Avvia l'access point
      startAccessPoint();
      showAPModeInfo();
      
      // Aggiorna il display con informazioni sulla modalità AP
      displayAPModeInfo();
    }
  }
  
  // Mostra display aggiornato
  updateDisplay();
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
        resetConfiguration();
        
        // Mostra un messaggio di conferma sul display
        displayError("Configurazione resettata");
        delay(2000); // Breve attesa per mostrare il messaggio
        
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
    if (eTaskGetState(DisplayTask) == eDeleted) {
      DEBUG_SYS("ERRORE CRITICO: DisplayTask non attivo o bloccato!");
      DEBUG_SYS("Esecuzione riavvio di emergenza...");
      ESP.restart(); // Riavvia il dispositivo come misura di recupero
    }
    
    // Verifica lo stato del task network sul Core 0
    if (eTaskGetState(NetworkTask) == eDeleted) {
      DEBUG_SYS("ERRORE CRITICO: NetworkTask non attivo o bloccato!");
      DEBUG_SYS("Esecuzione riavvio di emergenza...");
      ESP.restart(); // Riavvia il dispositivo come misura di recupero
    }
    
    // Verifica lo stato del task debug sul Core 1
    // Nota: se il task debug è inattivo, utilizziamo Serial direttamente
    // poiché le macro di debug potrebbero non funzionare
    if (eTaskGetState(DebugTask) == eDeleted) {
      Serial.println("[SYSTEM] ERRORE CRITICO: DebugTask non attivo o bloccato!");
      Serial.println("[SYSTEM] Esecuzione riavvio di emergenza...");
      ESP.restart(); // Riavvia il dispositivo come misura di recupero
    }
    
    DEBUG_SYS("Controllo watchdog completato: tutti i task attivi");
  }
  
  // Delay per risparmiare energia
  delay(10);
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
  
  // Gestione separata per modalità AP e modalità normale
  static unsigned long lastWebServerCheck = 0;
  static unsigned long lastDNSCheck = 0;
  
  if (apMode) {
    // In modalità AP, gestisci il server DNS più frequentemente
    if (currentMillis - lastDNSCheck >= 100) { // Controlla il DNS ogni 100ms
      lastDNSCheck = currentMillis;
      // Processa il DNS senza bisogno di controllo (il flag apMode è già true)
      dnsServer.processNextRequest();
    }
    
    // Gestisci il web server circa ogni 800ms
    if (currentMillis - lastWebServerCheck >= 800) {
      lastWebServerCheck = currentMillis;
      handleWebServerOnly(); // Funzione modificata che gestisce solo il web server, non il DNS
    }
    
    // Un delay più breve in modalità AP per garantire una migliore reattività
    delay(100);
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
    
    // Delay più lungo in modalità stazione per ridurre il consumo
    delay(1000);
  }
}
