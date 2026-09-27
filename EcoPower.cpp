/**
 * @file EcoPower.cpp
 * @brief Power saving on battery (see EcoPower.h)
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

// Touch button: wire from the lug of the top-right screw (back)
// to GPIO2, touch input T2. On the ESP32 the reading drops under a finger.
static const uint8_t TOUCH_PIN = 2;
static const unsigned long WEB_WINDOW_MS = 10UL * 60 * 1000;  // Web page after a touch
static const unsigned long WIFI_MIN_ON_MS = 8000;             // Time for the NTP sync
static const unsigned long WIFI_RETRY_MS = 15UL * 60 * 1000;  // After a failed connection

static touch_value_t touchThreshold = 0;
static bool touchWakeEnabled = true;
static int falseTouches = 0;
static const int MAX_FALSE_TOUCHES = 3;
static bool webWindow = false;
static unsigned long webWindowUntil = 0;
static unsigned long wifiOnSince = 0;
static bool wifiFailed = false;
static unsigned long wifiFailedAt = 0;

// The idle value changes a lot between USB (PC ground) and battery: the
// threshold is recomputed at every power change (see ecoPowerChanged)
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
  Serial.printf("[ECO] Touch button on GPIO%d: idle %u, threshold %u\n", TOUCH_PIN,
                (unsigned)rest, (unsigned)touchThreshold);
}

void ecoPowerChanged() {
  ecoBegin();
}

// Real touch: the value stays under the threshold for several readings in a row
static bool touchConfirmed() {
  int below = 0;
  for (int i = 0; i < 5; i++) {
    if (touchRead(TOUCH_PIN) < touchThreshold) below++;
    delay(10);
  }
  return below >= 3;
}

// With the charger plugged in a touch does nothing (everything is already
// on), but it is logged: that way the wiring can be tested over USB
void ecoLogTouch() {
  static unsigned long lastCheck = 0;
  static bool touched = false;
  if (touchThreshold == 0 || millis() - lastCheck < 200) return;
  lastCheck = millis();
  touch_value_t value = touchRead(TOUCH_PIN);
  if (value >= touchThreshold) {
    touched = false;
  } else if (!touched && touchConfirmed()) {
    touched = true;
    Serial.printf("[ECO] Touch on GPIO%d: value %u, threshold %u (on the charger: no action)\n",
                  TOUCH_PIN, (unsigned)value, (unsigned)touchThreshold);
  }
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

  Serial.println("[ECO] Switching WiFi on");
  if (!connectToWiFi(config.ssid, config.password)) {
    wifiFailed = true;
    wifiFailedAt = millis();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("[ECO] Network unreachable: next attempt in 15 minutes");
    return false;
  }
  wifiFailed = false;
  wifiOnSince = millis();
  WiFi.setSleep(true);  // While it stays on, the modem sleeps between packets
  // The web server restarts on the new connection
  server.end();
  setupServer();
  return true;
}

void ecoWiFiOffIfIdle() {
  if (WiFi.getMode() == WIFI_OFF) return;
  if (ecoWebWindowOpen()) return;
  if (WiFi.status() == WL_CONNECTED && millis() - wifiOnSince < WIFI_MIN_ON_MS) return;
  webWindow = false;
  Serial.println("[ECO] WiFi off");
  server.end();  // Closes the server before the network goes down
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

void ecoSleep(unsigned long maxMs) {
  if (WiFi.getMode() != WIFI_OFF) return;
  waitDisplayIdle(15000);  // The panel must be done before the cores are suspended

  // Until the next minute, plus a small margin
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
      Serial.println("[ECO] Touch: web page active for 10 minutes");
      ecoOpenWebWindow();
      lastDisplayUpdate = 0;  // Redraw at once: WiFi icon and address in the footer
    } else if (++falseTouches >= MAX_FALSE_TOUCHES) {
      touchWakeEnabled = false;
      Serial.println("[ECO] Too many false touches: touch wake-up disabled until the next power change");
    } else {
      Serial.println("[ECO] False touch ignored");
    }
  }
}
