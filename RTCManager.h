#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H

#include <Arduino.h>

// Initializes the DS3231 and, if present, sets the ESP32 internal clock
bool rtcBegin();

// Sets the ESP32 internal clock from the DS3231 time
bool syncFromRTC();

// Updates the DS3231 with the current ESP32 internal clock (after an NTP sync)
bool syncToRTC();

// Returns true if the DS3231 is present and has a valid time
bool rtcAvailable();

// Returns true if the DS3231 lost power (time not reliable)
bool rtcLostPower();

#endif // RTC_MANAGER_H
