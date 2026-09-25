#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H

#include <Arduino.h>

// Inizializza il DS3231 e, se disponibile, sincronizza il clock interno ESP32
bool rtcBegin();

// Imposta il clock interno ESP32 dall'ora del DS3231
bool syncFromRTC();

// Aggiorna il DS3231 con l'ora corrente del clock interno ESP32 (dopo sync NTP)
bool syncToRTC();

// Restituisce true se il DS3231 è disponibile e ha un'ora valida
bool rtcAvailable();

// Restituisce true se il DS3231 ha perso alimentazione (ora non affidabile)
bool rtcLostPower();

#endif // RTC_MANAGER_H
