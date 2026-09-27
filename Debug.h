#pragma once
#include <Arduino.h>

// Global trace on (1) / off (0)
#ifndef ATMOVERSE_DEBUG
#define ATMOVERSE_DEBUG 0
#endif

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
