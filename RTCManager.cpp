#include "RTCManager.h"
#include <RTClib.h>
#include <Wire.h>
#include <time.h>
#include <sys/time.h>

// DS3231 wired with D (SDA) on GPIO13 and C (SCL) on GPIO15.
// Uses the ESP32's second I2C bus (Wire1): the first one (Wire) is already
// started by the INA219 on pins 32/33, and a second Wire.begin() with other
// pins would be ignored by the core, leaving the RTC unreachable.
#define RTC_SDA_PIN 13
#define RTC_SCL_PIN 15
static TwoWire& rtcWire = Wire1;

static RTC_DS3231 rtc;
static bool _rtcAvailable = false;

bool rtcBegin() {
    Serial.printf("[RTC] Init I2C on SDA=%d SCL=%d\n", RTC_SDA_PIN, RTC_SCL_PIN);
    rtcWire.begin(RTC_SDA_PIN, RTC_SCL_PIN);
    rtcWire.setClock(100000);  // 100kHz standard mode
    
    // Initialization with retries
    for (int attempt = 1; attempt <= 3; attempt++) {
        Serial.printf("[RTC] Attempt %d/3 to init DS3231...\n", attempt);
        if (rtc.begin(&rtcWire)) {
            _rtcAvailable = true;
            Serial.println("[RTC] DS3231 found");
            break;
        }
        Serial.printf("[RTC] Attempt %d failed, retrying...\n", attempt);
        delay(100);
    }
    
    if (!_rtcAvailable) {
        Serial.println("[RTC] DS3231 not found after 3 attempts");
        return false;
    }

    if (rtc.lostPower()) {
        Serial.println("[RTC] DS3231 lost power: time not reliable until the NTP sync");
        return true; // Present, but the time is not valid
    }

    // Sets the ESP32 internal clock from the DS3231 at once
    syncFromRTC();
    return true;
}

bool syncFromRTC() {
    if (!_rtcAvailable) return false;
    if (rtc.lostPower()) return false;

    // The DS3231 holds UTC time: the system (TZ) converts it to the time
    // zone, so the time is right even before configTime()
    DateTime now = rtc.now();
    struct timeval tv;
    tv.tv_sec  = (time_t)now.unixtime();
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);

    Serial.printf("[RTC] UTC time from the DS3231: %02d/%02d/%04d %02d:%02d:%02d\n",
                  now.day(), now.month(), now.year(),
                  now.hour(), now.minute(), now.second());
    return true;
}

bool syncToRTC() {
    if (!_rtcAvailable) return false;

    // Stores UTC time (not local time): see syncFromRTC()
    time_t nowUtc = time(nullptr);
    if (nowUtc < 1700000000) {  // Clock not synced yet (before 2023)
        Serial.println("[RTC] syncToRTC: system time not valid");
        return false;
    }

    DateTime dt((uint32_t)nowUtc);
    rtc.adjust(dt);
    Serial.printf("[RTC] DS3231 updated (UTC): %02d/%02d/%04d %02d:%02d:%02d\n",
                  dt.day(), dt.month(), dt.year(), dt.hour(), dt.minute(), dt.second());
    return true;
}

bool rtcAvailable() {
    return _rtcAvailable;
}

bool rtcLostPower() {
    if (!_rtcAvailable) return true;
    return rtc.lostPower();
}
