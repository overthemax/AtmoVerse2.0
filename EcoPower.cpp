/**
 * @file EcoPower.cpp
 * @brief Risparmio energetico a batteria (vedi EcoPower.h)
 */

#include "EcoPower.h"
#include <WiFi.h>
#include <sys/time.h>
#include <esp_sleep.h>
#include "BatteryManager.h"
#include "Config.h"
#include "NetworkUtils.h"
#include "WebServer.h"
#include "DisplayTask.h"

extern bool apMode;
extern unsigned long lastDisplayUpdate;

// Tasto a sfioramento: filo dal capocorda della vite in alto a destra (retro)
// al GPIO2, ingresso touch T2. Sull'ESP32 il valore letto scende col dito.
static const uint8_t TOUCH_PIN = 2;
static const unsigned long WEB_WINDOW_MS = 10UL * 60 * 1000;  // Pagina web dopo un tocco
static const unsigned long WIFI_MIN_ON_MS = 8000;             // Tempo per la sincronizzazione NTP
static const unsigned long WIFI_RETRY_MS = 15UL * 60 * 1000;  // Dopo una connessione fallita

static touch_value_t touchThreshold = 0;
static bool touchWakeEnabled = true;
static int falseTouches = 0;
static const int MAX_FALSE_TOUCHES = 3;
static bool webWindow = false;
static unsigned long webWindowUntil = 0;
static unsigned long wifiOnSince = 0;
static bool wifiFailed = false;
static unsigned long wifiFailedAt = 0;

// Il valore a riposo cambia molto tra USB (massa del PC) e batteria: la
// soglia si ricalcola a ogni cambio di alimentazione (vedi ecoPowerChanged)
void ecoBegin() {
  touchWakeEnabled = true;
  falseTouches = 0;
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) {
    sum += touchRead(TOUCH_PIN);
    delay(5);
  }
  touch_value_t rest = sum / 16;
  touchThreshold = rest * 2 / 3;
  Serial.printf("[ECO] Tasto a sfioramento su GPIO%d: riposo %u, soglia %u\n", TOUCH_PIN,
                (unsigned)rest, (unsigned)touchThreshold);
}

void ecoPowerChanged() {
  ecoBegin();
}

// Tocco vero: il valore resta sotto la soglia per più letture di fila
static bool touchConfirmed() {
  int below = 0;
  for (int i = 0; i < 5; i++) {
    if (touchRead(TOUCH_PIN) < touchThreshold) below++;
    delay(10);
  }
  return below >= 3;
}

bool ecoActive() {
  return battery.isAvailable() && !battery.charging() && !apMode && strlen(config.ssid) > 0;
}

bool ecoWebWindowOpen() {
  return webWindow && (long)(millis() - webWindowUntil) < 0;
}

void ecoOpenWebWindow() {
  webWindow = true;
  webWindowUntil = millis() + WEB_WINDOW_MS;
}

void ecoNoteWebActivity() {
  if (webWindow) webWindowUntil = millis() + WEB_WINDOW_MS;
}

bool ecoEnsureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  if (wifiFailed && millis() - wifiFailedAt < WIFI_RETRY_MS) return false;

  Serial.println("[ECO] Accendo il WiFi");
  if (!connectToWiFi(config.ssid, config.password)) {
    wifiFailed = true;
    wifiFailedAt = millis();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("[ECO] Rete non raggiungibile: nuovo tentativo tra 15 minuti");
    return false;
  }
  wifiFailed = false;
  wifiOnSince = millis();
  WiFi.setSleep(true);  // Finché resta acceso, il modem dorme tra un pacchetto e l'altro
  // Il server web riparte sulla nuova connessione
  server.end();
  setupServer();
  return true;
}

void ecoWiFiOffIfIdle() {
  if (WiFi.getMode() == WIFI_OFF) return;
  if (ecoWebWindowOpen()) return;
  if (WiFi.status() == WL_CONNECTED && millis() - wifiOnSince < WIFI_MIN_ON_MS) return;
  webWindow = false;
  Serial.println("[ECO] WiFi spento");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

void ecoSleep(unsigned long maxMs) {
  if (WiFi.getMode() != WIFI_OFF) return;
  waitDisplayIdle(15000);  // Il pannello deve aver finito prima di sospendere i core

  // Fino allo scatto del minuto successivo, più un piccolo margine
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  long msToMinute = (60 - (long)(tv.tv_sec % 60)) * 1000L - tv.tv_usec / 1000 + 300;
  unsigned long ms = min((unsigned long)max(msToMinute, 0L), maxMs);
  if (ms < 1000) return;

  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  esp_sleep_enable_timer_wakeup((uint64_t)ms * 1000ULL);
  if (touchWakeEnabled) {
    touchSleepWakeUpEnable(TOUCH_PIN, touchThreshold);
    esp_sleep_enable_touchpad_wakeup();
  }
  Serial.flush();
  esp_light_sleep_start();

  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TOUCHPAD) {
    if (touchConfirmed()) {
      falseTouches = 0;
      Serial.println("[ECO] Tocco: pagina web attiva per 10 minuti");
      ecoOpenWebWindow();
      lastDisplayUpdate = 0;  // Ridisegna subito: icona WiFi e indirizzo nel piè di pagina
    } else if (++falseTouches >= MAX_FALSE_TOUCHES) {
      touchWakeEnabled = false;
      Serial.println("[ECO] Troppi falsi tocchi: risveglio al tocco disattivato fino al prossimo cambio di alimentazione");
    } else {
      Serial.println("[ECO] Falso tocco ignorato");
    }
  }
}
