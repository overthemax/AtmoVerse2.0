/**
 * @file Display.cpp
 * @brief Loop-side screen builders (see Display.h)
 *
 * These functions run in the main loop (core 1): they gather the data, copy it
 * into a ScreenModel and hand it to the display task (core 0), which draws it.
 * None of them touches the panel, so they return immediately.
 */

#include "Display.h"
#include <WiFi.h>
#include "Config.h"
#include "Language.h"
#include "QuotesManager.h"
#include "WeatherIconMap.h"
#include "BatteryManager.h"
#include "Screens.h"
#include "DisplayTask.h"
#include "Updater.h"

extern bool apMode;                    // NetworkUtils.cpp
extern bool lastWeatherUpdateSuccess;  // AtmoVerse_2.0.ino

static ScreenModel screenModel;  // Used by the loop only

static void fillBattery(ScreenModel& m) {
  m.showBattery = config.batteryShowOnDisplay && battery.isAvailable();
  m.batteryPercent = battery.getPercentage();
  m.batteryCharging = battery.charging();
  m.batteryFull = battery.chargeComplete();
}

static void showMessage(const char* title, const char* text) {
  screenModel = ScreenModel();
  screenModel.kind = SCREEN_MESSAGE;
  screenModel.title = title;
  screenModel.text = text;
  fillBattery(screenModel);
  showScreen(screenModel);
}

void initDisplay() {
  display.init(115200);
  display.setRotation(0);
  display.setTextColor(GxEPD_BLACK);
  display.setFullWindow();
  // From here on only the display task uses the panel
  startDisplayTask();
}

void displayStartupScreen() {
  showMessage("AtmoVerse", TR("Avvio in corso...", "Starting..."));
}

void showUpdateProgress(const char* phase, int filesDone, int filesTotal, int percent, int etaSec) {
  screenModel = ScreenModel();
  screenModel.kind = SCREEN_UPDATE;
  screenModel.updPhase = phase;
  screenModel.updFilesDone = filesDone;
  screenModel.updFilesTotal = filesTotal;
  screenModel.updPercent = constrain(percent, 0, 100);
  screenModel.updEtaSec = etaSec;
  fillBattery(screenModel);
  showScreen(screenModel);
}

void showUpdateError(const char* title, const char* text) {
  showMessage(title, text);
}

void showBatteryEmpty() {
  screenModel = ScreenModel();
  screenModel.kind = SCREEN_BATTERY;
  fillBattery(screenModel);
  showScreen(screenModel);
}

void displaySetupScreen(String apName, String ipAddress) {
  screenModel = ScreenModel();
  screenModel.kind = SCREEN_SETUP;
  screenModel.apName = apName;
  screenModel.apIp = ipAddress;
  fillBattery(screenModel);
  showScreen(screenModel);
}

void showAPModeInfo() {
  displaySetupScreen(WiFi.softAPSSID(), WiFi.softAPIP().toString());
}

void showConfigSaved() {
  showMessage(TR("Impostazioni salvate", "Settings saved"),
              TR("AtmoVerse si riavvia tra pochi istanti.", "AtmoVerse restarts in a moment."));
  // The caller restarts right after: the message must reach the panel first
  waitDisplayIdle(10000);
}

void updateDisplay() {
  // The setup screen only while the access point is really active: if the
  // WiFi drops for a moment the weather screen stays (with the last data)
  if (apMode) {
    showAPModeInfo();
    return;
  }

  ScreenModel& m = screenModel;
  m = ScreenModel();
  m.kind = SCREEN_MAIN;
  m.weather = currentWeather;
  m.weatherUpdateOk = lastWeatherUpdateSuccess;
  m.city = config.city;
  m.metric = strlen(config.units) == 0 || strcmp(config.units, "metric") == 0;
  m.wifiOn = WiFi.status() == WL_CONNECTED;
  if (m.wifiOn) m.ip = WiFi.localIP().toString();

  // Quote and icon are read from the SD card here, in the loop: the display
  // task never accesses the SD card
  Quote q = getQuoteForDisplay();
  m.quoteText = q.text;
  m.quoteAuthor = q.author;
  if (currentWeather.valid && initSD()) {
    const char* iconPath = weatherIconPath(weatherIconFor(currentWeather.weather_id, isNightTime(), currentWeather.wind_speed));
    if (!loadIconBitmap(iconPath, m)) {
      Serial.printf("[DISPLAY] Cannot read icon: %s\n", iconPath);
    }
  }

  m.notice = getUpdateNotice();
  if (m.notice.length() == 0 && battery.getLevel() != BATTERY_LEVEL_OK) {
    m.notice = TR("Batteria scarica: collega il caricatore", "Battery low: plug in the charger");
  }
  fillBattery(m);
  showScreen(m);
}
