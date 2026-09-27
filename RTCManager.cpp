#include "RTCManager.h"
#include <RTClib.h>
#include <Wire.h>
#include <time.h>
#include <sys/time.h>

// DS3231 collegato con D (SDA) su GPIO13 e C (SCL) su GPIO15.
// Usa il secondo bus I2C dell'ESP32 (Wire1): il primo (Wire) è già avviato
// dall'INA219 sui pin 32/33, e un secondo Wire.begin() con altri pin verrebbe
// ignorato dal core, lasciando l'RTC irraggiungibile.
#define RTC_SDA_PIN 13
#define RTC_SCL_PIN 15
static TwoWire& rtcWire = Wire1;

static RTC_DS3231 rtc;
static bool _rtcAvailable = false;

bool rtcBegin() {
    Serial.printf("[RTC] Init I2C on SDA=%d SCL=%d\n", RTC_SDA_PIN, RTC_SCL_PIN);
    rtcWire.begin(RTC_SDA_PIN, RTC_SCL_PIN);
    rtcWire.setClock(100000);  // 100kHz standard mode
    
    // Prova inizializzazione con retry
    for (int attempt = 1; attempt <= 3; attempt++) {
        Serial.printf("[RTC] Attempt %d/3 to init DS3231...\n", attempt);
        if (rtc.begin(&rtcWire)) {
            _rtcAvailable = true;
            Serial.println("[RTC] DS3231 trovato!");
            break;
        }
        Serial.printf("[RTC] Attempt %d failed, retrying...\n", attempt);
        delay(100);
    }
    
    if (!_rtcAvailable) {
        Serial.println("[RTC] DS3231 non trovato dopo 3 tentativi");
        return false;
    }

    if (rtc.lostPower()) {
        Serial.println("[RTC] DS3231 ha perso alimentazione - ora non affidabile, attendere sync NTP");
        return true; // Disponibile ma ora non valida
    }

    // Sincronizza subito il clock interno ESP32 dal DS3231
    syncFromRTC();
    return true;
}

bool syncFromRTC() {
    if (!_rtcAvailable) return false;
    if (rtc.lostPower()) return false;

    // Il DS3231 contiene l'ora UTC: la conversione al fuso orario la fa il
    // sistema (TZ), quindi l'ora è corretta anche prima di configTime()
    DateTime now = rtc.now();
    struct timeval tv;
    tv.tv_sec  = (time_t)now.unixtime();
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);

    Serial.printf("[RTC] Ora UTC letta da DS3231: %02d/%02d/%04d %02d:%02d:%02d\n",
                  now.day(), now.month(), now.year(),
                  now.hour(), now.minute(), now.second());
    return true;
}

bool syncToRTC() {
    if (!_rtcAvailable) return false;

    // Salva l'ora UTC (non quella locale): vedi syncFromRTC()
    time_t nowUtc = time(nullptr);
    if (nowUtc < 1700000000) {  // Orologio non ancora sincronizzato (prima del 2023)
        Serial.println("[RTC] syncToRTC: ora di sistema non valida");
        return false;
    }

    DateTime dt((uint32_t)nowUtc);
    rtc.adjust(dt);
    Serial.printf("[RTC] DS3231 aggiornato (UTC): %02d/%02d/%04d %02d:%02d:%02d\n",
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
