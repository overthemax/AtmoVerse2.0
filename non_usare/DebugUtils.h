#pragma once

// Ensure Arduino core types/macros (Serial, F, __FlashStringHelper, PROGMEM) are available
#include <Arduino.h>

// Definizione esplicita di DEBUG_MODE per abilitare/disabilitare il debug
// Disattivato il debug per ridurre il consumo di memoria
// #define DEBUG_MODE

// Definizione delle categorie di debug
#define DEBUG_CATEGORY_NONE       0
#define DEBUG_CATEGORY_SYSTEM     1
#define DEBUG_CATEGORY_WIFI       2
#define DEBUG_CATEGORY_SD         3
#define DEBUG_CATEGORY_WEATHER    4
#define DEBUG_CATEGORY_DISPLAY    5
#define DEBUG_CATEGORY_HTTP       6
#define DEBUG_CATEGORY_CONFIG     7
#define DEBUG_CATEGORY_POWER      8
#define DEBUG_CATEGORY_ALL        255

// Livelli di debug
#define DEBUG_LEVEL_ERROR         0
#define DEBUG_LEVEL_WARNING       1
#define DEBUG_LEVEL_INFO          2
#define DEBUG_LEVEL_DEBUG         3
#define DEBUG_LEVEL_VERBOSE       4

// Configurazione dei livelli di log per ogni categoria
// Modifica questi valori per controllare quali categorie e livelli mostrare
#define DEBUG_ACTIVE_CATEGORIES   (DEBUG_CATEGORY_NONE)
#define DEBUG_ACTIVE_LEVEL        DEBUG_LEVEL_ERROR

#ifdef DEBUG_MODE
  // Funzione per controllare se una categoria e un livello sono attivi
  #define DEBUG_IS_ACTIVE(category, level) \
      ((DEBUG_ACTIVE_CATEGORIES & category) && (level <= DEBUG_ACTIVE_LEVEL))
  
  // Macro per stampare un messaggio di debug
  #define DEBUG_LOG(category, level, format, ...) \
    do { \
      if (DEBUG_IS_ACTIVE(category, level)) { \
        Serial.print(millis()); \
        Serial.print(" - "); \
        \
        /* Prefisso categoria */ \
        switch (category) { \
          case DEBUG_CATEGORY_SYSTEM:  Serial.print("[SYSTEM] "); break; \
          case DEBUG_CATEGORY_WIFI:    Serial.print("[WIFI] "); break; \
          case DEBUG_CATEGORY_SD:      Serial.print("[SD] "); break; \
          case DEBUG_CATEGORY_WEATHER: Serial.print("[WEATHER] "); break; \
          case DEBUG_CATEGORY_DISPLAY: Serial.print("[DISPLAY] "); break; \
          case DEBUG_CATEGORY_HTTP:    Serial.print("[HTTP] "); break; \
          case DEBUG_CATEGORY_CONFIG:  Serial.print("[CONFIG] "); break; \
          case DEBUG_CATEGORY_POWER:   Serial.print("[POWER] "); break; \
          default:                     Serial.print("[LOG] "); break; \
        } \
        \
        /* Prefisso livello */ \
        switch (level) { \
          case DEBUG_LEVEL_ERROR:   Serial.print("ERROR: "); break; \
          case DEBUG_LEVEL_WARNING: Serial.print("WARN: "); break; \
          case DEBUG_LEVEL_INFO:    Serial.print("INFO: "); break; \
          case DEBUG_LEVEL_DEBUG:   Serial.print("DEBUG: "); break; \
          case DEBUG_LEVEL_VERBOSE: Serial.print("VERBOSE: "); break; \
          default: break; \
        } \
        \
        /* Messaggio formattato */ \
        Serial.printf(format, ##__VA_ARGS__); \
        Serial.println(); \
      } \
    } while(0)

  // Macro legacy per retrocompatibilità
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINTF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#else
  // Se il debug è disattivato, tutto diventa una macro vuota
  #define DEBUG_LOG(category, level, format, ...) ((void)0)
  #define DEBUG_IS_ACTIVE(category, level) (0)
  #define DEBUG_PRINT(x) ((void)0)
  #define DEBUG_PRINTLN(x) ((void)0)
  #define DEBUG_PRINTF(fmt, ...) ((void)0)
  // Elimina completamente le chiamate Serial
  #define Serial_print(...) ((void)0)
  #define Serial_println(...) ((void)0)
  #define Serial_printf(...) ((void)0)
  #define Serial_begin(...) ((void)0)
#endif

// Macro helper per ridurre l'occupazione di memoria con stringhe
#ifndef FPSTR
#define FPSTR(pstr_pointer) (reinterpret_cast<const __FlashStringHelper *>(pstr_pointer))
#endif
#ifndef PSTR
#define PSTR(s) (__extension__({static const char __c[] PROGMEM = (s); &__c[0];}))
#endif
