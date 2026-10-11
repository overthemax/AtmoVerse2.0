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
#include "EcoPower.h"

extern bool apMode;                    // NetworkUtils.cpp
extern bool lastWeatherUpdateSuccess;  // AtmoVerse_2.0.ino

static ScreenModel screenModel;  // Used by the loop only

// A touch or the charger redraws the display at once, before the WiFi has
// reconnected. The footer then shows the last address the board had: DHCP
// almost always gives the same one back, so no second refresh is needed
// (see displayIpOutdated).
static String lastKnownIp;
static String shownIp;
static bool shownWifi = false;

// Is the WiFi on, or about to be? On the charger it is always on; on battery
// while the web page window is open.
static bool wifiExpected() {
  if (WiFi.status() == WL_CONNECTED) return true;
  if (apMode || strlen(config.ssid) == 0) return false;
  return ecoActive() ? ecoWebWindowOpen() : true;
}

bool displayIpOutdated() {
  return shownWifi && WiFi.status() == WL_CONNECTED && shownIp != WiFi.localIP().toString();
}

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

void showTouchHold(int stage) {
  if (stage >= 3) {
    showMessage(TR("Annullato: rilascia la vite", "Cancelled: release the screw"),
                TR("Non succede nulla, AtmoVerse continua come prima.",
                   "Nothing happens, AtmoVerse carries on as before."));
  } else if (stage == 2) {
    showMessage(TR("Rilascia per cancellare le impostazioni", "Release to erase the settings"),
                TR("AtmoVerse ripartirà in modalità configurazione. Citazioni e file della SD restano. "
                   "Per annullare, tieni premuto fino a 45 secondi.",
                   "AtmoVerse will restart in setup mode. Quotes and SD files are kept. "
                   "To cancel, keep holding up to 45 seconds."));
  } else {
    showMessage(TR("Rilascia per riavviare", "Release to restart"),
                TR("Tieni premuto fino a 30 secondi per cancellare le impostazioni, fino a 45 per annullare.",
                   "Keep holding up to 30 seconds to erase the settings, up to 45 to cancel."));
  }
}

void showSettingsErased() {
  showMessage(TR("Impostazioni cancellate", "Settings erased"),
              TR("AtmoVerse si riavvia in modalità configurazione.", "AtmoVerse restarts in setup mode."));
  waitDisplayIdle(10000);  // The message must reach the panel before the restart
}

void showRestarting() {
  showMessage(TR("Riavvio in corso", "Restarting"), TR("AtmoVerse torna tra pochi istanti.", "AtmoVerse is back in a moment."));
  waitDisplayIdle(10000);  // The message must reach the panel before the restart
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
  m.use24h = config.use24hFormat;
  if (WiFi.status() == WL_CONNECTED) lastKnownIp = WiFi.localIP().toString();
  m.wifiOn = wifiExpected();
  if (m.wifiOn) m.ip = lastKnownIp;
  shownWifi = m.wifiOn;
  shownIp = m.ip;

  // Quote and icon are read from the SD card here, in the loop: the display
  // task never accesses the SD card
  Quote q = getQuoteForDisplay();
  m.quoteText = q.text;
  m.quoteAuthor = q.author;
  if (currentWeather.valid && initSD()) {
    const char* iconPath = weatherIconPath(currentWeather.weather_id, isNightTime(), windSpeedMs());
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
