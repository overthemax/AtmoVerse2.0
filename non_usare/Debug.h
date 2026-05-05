#pragma once
#include <Arduino.h>

// Abilita (1) / Disabilita (0) il trace globale
#define ATMOVERSE_DEBUG 0  // Completamente disabilitato per ridurre la dimensione del codice

#if ATMOVERSE_DEBUG
  #define DEBUG_TRACE(msg)                                             \
    do {                                                           \
      Serial.print("[TRACE] ");                                   \
      Serial.print(__FILE__);                                      \
      Serial.print("::");                                         \
      Serial.print(__FUNCTION__);                                  \
      Serial.print(" (");                                         \
      Serial.print(__LINE__);                                      \
      Serial.print(") ");                                          \
      Serial.println(msg);                                         \
    } while (0)
#else
  #define DEBUG_TRACE(msg) do {} while (0)
#endif

#ifndef DEBUG_H
#define DEBUG_H

/**
 * @file Debug.h
 * @brief Sistema di debug asincrono per AtmoVerse 2.0
 * 
 * Questo file contiene il sistema di debug asincrono per AtmoVerse 2.0
 * che sfrutta la capacità dual-core dell'ESP32 per gestire i messaggi di debug
 * su un core separato, evitando di bloccare le operazioni principali
 */

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

// ==========================================
// Configurazione del sistema di debug
// ==========================================

// Dimensione della coda di messaggi e lunghezza massima del messaggio
#if defined(MINIMIZE_BUFFERS)
#define DEBUG_QUEUE_SIZE 10     // Numero ridotto di messaggi in coda quando MINIMIZE_BUFFERS è attivo
#define DEBUG_MESSAGE_SIZE 80   // Lunghezza ridotta di un messaggio in caratteri quando MINIMIZE_BUFFERS è attivo
#else
#define DEBUG_QUEUE_SIZE 30     // Numero massimo di messaggi in coda
#define DEBUG_MESSAGE_SIZE 150  // Lunghezza massima di un messaggio in caratteri
#endif

// Flag per abilitare/disabilitare categorie di debug
// Ogni categoria può essere attivata/disattivata indipendentemente
#define DEBUG_SYSTEM    true   // Info di sistema e inizializzazione
#define DEBUG_WIFI      true   // Connessioni WiFi, AP e comunicazione di rete
#define DEBUG_DISPLAY   true   // Operazioni sul display e-ink
#define DEBUG_SD        true   // Operazioni su scheda SD e file system
#define DEBUG_WEATHER   true   // Dati meteo e aggiornamenti API

// ==========================================
// Variabili globali (definite in Debug.cpp)
// ==========================================

/**
 * @brief Coda per i messaggi di debug
 * Contiene i messaggi in attesa di essere processati dal task di debug
 */
extern QueueHandle_t debugQueue;

/**
 * @brief Mutex per proteggere l'accesso alla Serial
 * Impedisce l'accesso concorrente alla Serial da parte di diversi task
 */
extern SemaphoreHandle_t serialMutex;

/**
 * @brief Handle del task di debug
 * Utilizzato per gestire e controllare lo stato del task di debug
 */
extern TaskHandle_t DebugTask;

/**
 * @brief Flag che indica se il sistema di debug è stato inizializzato
 * Volatile per garantire la visibilità tra diversi core/task
 */
extern volatile bool debugSystemInitialized;

// ==========================================
// Funzioni del sistema di debug
// ==========================================

/**
 * @brief Inizializza il sistema di debug
 * Configura Serial, crea mutex e coda dei messaggi
 */
void initDebugSystem();

/**
 * @brief Avvia il task di debug sul Core 1
 * Crea e configura il task che gestisce la stampa dei messaggi di debug
 */
void startDebugTask();

/**
 * @brief Funzione principale per l'invio di messaggi di debug
 * @param module Nome del modulo che genera il messaggio (es. "SYSTEM", "WIFI")
 * @param message Messaggio di debug da inviare
 */
void debugPrint(const char* module, const char* message);

/**
 * @brief Versione formattata della funzione di debug (simile a printf)
 * @param module Nome del modulo che genera il messaggio
 * @param format Stringa di formato (tipo printf)
 * @param ... Parametri variabili per il formato
 */
void debugPrintf(const char* module, const char* format, ...);

// ==========================================
// Macro per debug condizionale per categoria
// ==========================================

/**
 * Queste macro permettono di inviare messaggi di debug solo se la
 * relativa categoria è abilitata, riducendo l'overhead di debug
 * quando non necessario
 */
#define DEBUG_SYS(msg)     if(DEBUG_SYSTEM)  debugPrint("SYSTEM", msg)
#define DEBUG_WIFI(msg)    if(DEBUG_WIFI)    debugPrint("WIFI", msg)
#define DEBUG_DISP(msg)    if(DEBUG_DISPLAY) debugPrint("DISPLAY", msg)
#define DEBUG_SD(msg)      if(DEBUG_SD)      debugPrint("SD", msg)
#define DEBUG_WEATHER(msg) if(DEBUG_WEATHER) debugPrint("WEATHER", msg)

/**
 * Versioni formattate delle macro di debug per categoria
 * Utilizzano debugPrintf per supportare parametri variabili
 */
#define DEBUG_SYS_F(...)     if(DEBUG_SYSTEM)  debugPrintf("SYSTEM", __VA_ARGS__)
#define DEBUG_WIFI_F(...)    if(DEBUG_WIFI)    debugPrintf("WIFI", __VA_ARGS__)
#define DEBUG_DISP_F(...)    if(DEBUG_DISPLAY) debugPrintf("DISPLAY", __VA_ARGS__)
#define DEBUG_SD_F(...)      if(DEBUG_SD)      debugPrintf("SD", __VA_ARGS__)
#define DEBUG_WEATHER_F(...) if(DEBUG_WEATHER) debugPrintf("WEATHER", __VA_ARGS__)

/**
 * @brief Macro per compatibilità con il vecchio sistema di debug
 * Permette di mantenere compatibilità con il codice esistente
 */
#define DEBUG_TRACE(...) debugPrint("TRACE", "Function call")

// Per compatibilità con il codice esistente, non usare nella nuova implementazione
#ifndef DEBUG_TRACE
#define DEBUG_TRACE()
#endif

#endif // DEBUG_H
