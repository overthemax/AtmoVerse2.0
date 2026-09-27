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

// For 3 minutes after the charger is unplugged the board stays awake and
// reads the touch button continuously: the idle value and the drop under a
// finger change a lot on battery, and this is the only way to see them
// (see ecoTouchStats, shown on the diagnostics page)
static const unsigned long TOUCH_TEST_MS = 3UL * 60 * 1000;
static unsigned long touchTestUntil = 0;

// Touch statistics since boot, for the diagnostics page
static touch_value_t batteryIdle = 0, batteryThreshold = 0;  // Last calibration on battery
static touch_value_t currentIdle = 0;                         // Last calibration
static touch_value_t lowestReading = 0;                       // Since the last calibration
static uint16_t touchWakeups = 0, touchesConfirmed = 0, touchesFalse = 0;

static touch_value_t touchThreshold = 0;
static bool touchWakeEnabled = true;
static int falseTouches = 0;
static const int MAX_FALSE_TOUCHES = 3;
static bool webWindow = false;
static unsigned long webWindowUntil = 0;
static unsigned long wifiOnSince = 0;
static bool wifiFailed = false;
static unsigned long wifiFailedAt = 0;

// Nothing to do in the interrupt: touches are read by ecoPollTouch and ecoSleep
static void IRAM_ATTR onTouchInterrupt() {}

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
  // The wake-up from light sleep uses the channel's own threshold: on the
  // ESP32 the touch driver of core 3.x ignores the one passed to
  // touchSleepWakeUpEnable and keeps its default, 1.5% below the reading at
  // boot. On battery the idle value is lower than that (boot is usually on
  // USB), so the board woke up continuously, every wake-up was a false touch
  // and after three the touch wake-up was switched off. Attaching an
  // interrupt is the driver's way to set the channel threshold.
  touchAttachInterrupt(TOUCH_PIN, onTouchInterrupt, touchThreshold);
  currentIdle = rest;
  lowestReading = rest;
  if (battery.isAvailable() && !battery.charging()) {
    batteryIdle = rest;
    batteryThreshold = touchThreshold;
    touchTestUntil = millis() + TOUCH_TEST_MS;
  }
  Serial.printf("[ECO] Touch button on GPIO%d: idle %u, threshold %u\n", TOUCH_PIN,
                (unsigned)rest, (unsigned)touchThreshold);
}

bool ecoTouchTestActive() {
  return touchTestUntil != 0 && (long)(millis() - touchTestUntil) < 0;
}

void ecoTouchStats(JsonObject out) {
  out["pin"] = TOUCH_PIN;
  out["idle"] = (unsigned)currentIdle;
  out["threshold"] = (unsigned)touchThreshold;
  out["lowest"] = (unsigned)lowestReading;
  out["battery_idle"] = (unsigned)batteryIdle;
  out["battery_threshold"] = (unsigned)batteryThreshold;
  out["wakeups"] = touchWakeups;
  out["confirmed"] = touchesConfirmed;
  out["false"] = touchesFalse;
  out["wake_enabled"] = touchWakeEnabled;
  out["test_active"] = ecoTouchTestActive();
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

// Reads the touch button while the board is awake. On battery a touch opens
// the web page; with the charger plugged in it does nothing (everything is
// already on) but it is logged, so the wiring can be tested over USB.
void ecoPollTouch() {
  static unsigned long lastCheck = 0;
  static bool touched = false;
  if (touchThreshold == 0 || millis() - lastCheck < 100) return;
  lastCheck = millis();
  touch_value_t value = touchRead(TOUCH_PIN);
  if (value < lowestReading) lowestReading = value;
  if (value >= touchThreshold) {
    touched = false;
    return;
  }
  if (touched || !touchConfirmed()) return;
  touched = true;
  touchesConfirmed++;
  if (ecoActive()) {
    Serial.printf("[ECO] Touch (value %u): web page active for 10 minutes\n", (unsigned)value);
    ecoOpenWebWindow();
    lastDisplayUpdate = 0;  // Redraw at once: WiFi icon and address in the footer
  } else {
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
    touchWakeups++;
    if (touchConfirmed()) {
      touchesConfirmed++;
      falseTouches = 0;
      Serial.println("[ECO] Touch: web page active for 10 minutes");
      ecoOpenWebWindow();
      lastDisplayUpdate = 0;  // Redraw at once: WiFi icon and address in the footer
    } else {
      touchesFalse++;
      if (++falseTouches >= MAX_FALSE_TOUCHES) {
        touchWakeEnabled = false;
        Serial.println("[ECO] Too many false touches: touch wake-up disabled until the next power change");
      } else {
        Serial.println("[ECO] False touch ignored");
      }
    }
  }
}
