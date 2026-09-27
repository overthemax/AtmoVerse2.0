/*
 * AtmoVerse 2.0
 * Weather and quotes on an ESP32 with an e-ink display and a web interface
 */

// Core libraries
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <time.h>

// Project modules
#include "Hardware.h"
#include "Config.h"
#include "NetworkUtils.h"
#include "WeatherUtils.h"
#include "WebServer.h"
#include "Display.h"
#include "AtmoVerseConstants.h"
#include "QuotesManager.h"
#include "BatteryManager.h"
#include "RTCManager.h"
#include "Updater.h"
#include "Version.h"
#include "DisplayTask.h"
#include "EcoPower.h"

// The loop also runs the HTTPS connections (weather, updates): the
// default 8 KB stack is at its limit during the TLS handshake
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

// Automatic updates from GitHub (see Updater.h)
const unsigned long UPDATE_CHECK_MS     = 6UL * 60 * 60 * 1000;  // Check every 6 hours
const unsigned long UPDATE_RETRY_MS     = 10UL * 60 * 1000;      // New attempt after an error
const unsigned long FIRMWARE_HEALTHY_MS = 60UL * 1000;           // After 60 s the firmware is confirmed
const unsigned long AP_RETRY_MS         = 5UL * 60 * 1000;       // In AP mode: new attempt on the configured network

// ---------------------------------------------------------------------------
// Battery empty: deep sleep
// ---------------------------------------------------------------------------
// At the critical level the display shows the tired face and the board
// sleeps, waking every 30 minutes only to measure the battery. The variable
// in RTC memory survives deep sleep.
const uint64_t BATTERY_SLEEP_US = 30ULL * 60 * 1000000;
RTC_DATA_ATTR bool sleepingForBattery = false;

void enterBatterySleep() {
  Serial.printf("[BATTERY] Battery at %d%%: deep sleep, next check in 30 minutes\n",
                battery.getPercentage());
  sleepingForBattery = true;
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  esp_sleep_enable_timer_wakeup(BATTERY_SLEEP_US);
  Serial.flush();
  esp_deep_sleep_start();
}

// Wake-up from the battery sleep: measure and, if it is still empty and not
// charging, sleep again without touching display and WiFi (the face stays)
void checkBatteryAfterSleep() {
  if (!sleepingForBattery || esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_TIMER) {
    sleepingForBattery = false;
    return;
  }
  battery.begin();
  if (battery.isAvailable() && !battery.charging() &&
      battery.getPercentage() < BATTERY_CRITICAL_EXIT_PERCENT) {
    enterBatterySleep();
  }
  Serial.println("[BATTERY] Battery recharged: normal start");
  sleepingForBattery = false;
}

// Update check requested from the web page (see WebServer.cpp)
volatile bool updateCheckRequested = false;
void requestUpdateCheck() {
  updateCheckRequested = true;
}

// Time of the last weather update
unsigned long lastWeatherUpdate = 0;

// Time of the last display update
unsigned long lastDisplayUpdate = 0;

// Weather connection attempts
int networkRetryCounter = 0;

// Result of the last weather update
bool lastWeatherUpdateSuccess = true;

// Setup
void setup() {
  // CPU at 80 MHz to save power (WiFi works down to 80 MHz)
  setCpuFrequencyMhz(80);

  // Serial for debugging
  Serial.begin(115200);
  delay(1000);

  // After a sleep for an empty battery: if it is still empty, back to sleep here
  checkBatteryAfterSleep();
  
  // Let the system settle before the SD card
  delay(500);
  
  // --- SD CARD on HSPI, BEFORE THE DISPLAY ---
  // The SD card is mounted first to avoid SPI conflicts
  bool sdAvailable = initSD();
  if (sdAvailable) migrateClockFolder();  // /orari -> /clock (2.1.17)
  
  // --- DISPLAY: start and boot screen AFTER the SD card ---
  delay(BOOT_DELAY_MS);

  SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS); // VSPI for the display
  initDisplay();          // Also starts the display task on core 0
  displayStartupScreen();

  delay(BOOT_SPLASH_DURATION_MS);
  
  // Is there a settings file? (only with an SD card)
  bool configFileExists = false;
  if (sdAvailable) {
    configFileExists = SD.exists("/conf.json") || SD.exists("conf.json");
  }

  // Load the settings
  loadConfig();
  
  // Is a network name set?
  bool hasSSID = strlen(config.ssid) > 0;

  
  // The file exists and was read, but the settings are not valid:
  // maybe a read error. Read it again up to 3 times.
  if (configFileExists && !hasSSID && sdAvailable) {
    // Read again up to CONFIG_READ_MAX_RETRIES times before giving up on the settings
    for (int i = 0; i < CONFIG_READ_MAX_RETRIES; i++) {
      delay(CONFIG_RETRY_DELAY_MS);
      
      // Reload the settings
      loadConfig();
      hasSSID = strlen(config.ssid) > 0;
      
      if (hasSSID) {
        break;
      }
    }
  }

  // Detects a firmware rollback and completes an interrupted
  // update of the SD files
  initUpdater();
  Serial.println("[SETUP] AtmoVerse " ATMOVERSE_VERSION);

  // Time zone at once: the RTC time (UTC) is shown correctly
  // even without internet
  applyTimezone();

  // Random generator seeded with ADC noise and the hardware RNG
  randomSeed(analogRead(0) ^ (esp_random() & 0xFFFF));

  // Hardware started before AP mode: during the setup too the display
  // needs the battery and the RTC time
  initHardware();

  // Battery: the INA219 is always looked for; without it the battery is not shown
  battery.begin();
  ecoBegin();  // Touch button for the web page on battery

  // DS3231 RTC: sets the internal clock at once if present
  rtcBegin();

  // Final check of the settings
  if (!checkConfigValidity()) {
    // Settings not valid: start the access point
    startAccessPoint(true);
    showAPModeInfo();
    return;
  }


  // --- Normal start ---
  // Network configured but unreachable: AP mode for the setup.
  // In AP mode the loop tries the network again every 5 minutes (if nobody is connected to the AP).
  if (!setupWiFi()) {
    startAccessPoint(true);
    showAPModeInfo();
  }
  // Wait for the NTP sync (max 5 s), then update the DS3231
  // (NTP server and time zone are set by connectToWiFi)
  if (!apMode && isWiFiConnected()) {
    struct tm ntpTime;
    if (getLocalTime(&ntpTime, 5000)) {
      syncToRTC();
    }
  }
  // Without NTP, the RTC time is used if present
  if (rtcAvailable() && !rtcLostPower()) {
    syncFromRTC();
  }
  setupServer();
  getWeatherData();
  updateDisplay();
}

// Are we in the power saving hours?
bool isPowerSavingMode() {
  if (!config.powerSavingEnabled) {
    return false; // Power saving hours disabled
  }

  // Current time
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 0)) {
    return false; // Time not available: normal mode
  }
  
  int currentHour = timeinfo.tm_hour;
  
  // Start hour later than the end hour
  // (e.g. from 22:00 to 7:00 of the next day)
  if (config.powerSavingStartHour > config.powerSavingEndHour) {
    return (currentHour >= config.powerSavingStartHour || currentHour < config.powerSavingEndHour);
  } else {
    return (currentHour >= config.powerSavingStartHour && currentHour < config.powerSavingEndHour);
  }
}

// Weather update interval for the current mode
unsigned long getUpdateInterval() {
  // Low battery: weather updated less often
  if (isPowerSavingMode() || (battery.isAvailable() && battery.getLevel() != BATTERY_LEVEL_OK)) {
    return config.powerSavingUpdateInterval * 60 * 1000; // Minutes to milliseconds
  } else {
    return config.normalUpdateInterval * 60 * 1000; // Minutes to milliseconds
  }
}

// Display refresh interval in ms
unsigned long getDisplayRefreshIntervalMs() {
  if (isPowerSavingMode()) {
    return (unsigned long)config.displayRefreshIntervalSecPowerSaving * 1000UL;
  } else {
    return (unsigned long)config.displayRefreshIntervalSec * 1000UL;
  }
}

// Main loop
void loop() {
  unsigned long currentMillis = millis();
  battery.update();  // Also without a sensor: look for it again every minute
  if (battery.isAvailable()) {

    // Charger plugged in/out: display updated at once (not at the next minute)
    static unsigned long lastChargePoll = 0;
    if (currentMillis - lastChargePoll >= 2000) {
      lastChargePoll = currentMillis;
      if (battery.pollCharging()) {
        ecoPowerChanged();
        lastDisplayUpdate = 0;
      }
    }

    // Critical level steady for at least a minute (two readings): face and sleep
    static unsigned long criticalSince = 0;
    if (battery.getLevel() == BATTERY_LEVEL_CRITICAL) {
      if (criticalSince == 0) {
        criticalSince = currentMillis;
      } else if (currentMillis - criticalSince >= 60000UL) {
        showBatteryEmpty();
        waitDisplayIdle(15000);  // The screen must be on the panel before sleeping
        enterBatterySleep();
      }
    } else {
      criticalSince = 0;
    }
  }
  
  // NTP sync every 30 minutes to keep the DS3231 accurate
  static unsigned long lastNTPSync = 0;
  const unsigned long NTP_SYNC_INTERVAL_MS = 30UL * 60UL * 1000UL; // 30 minutes
  // The first DS3231 update happens as soon as the NTP time is available
  // (at boot it may arrive after the 5 s wait in setup), then every 30 minutes.
  // No waiting: SNTP already keeps the system time in sync in the background.
  static bool rtcSynced = false;
  if (!apMode && WiFi.status() == WL_CONNECTED && time(nullptr) > 1700000000 &&
      (!rtcSynced || (unsigned long)(currentMillis - lastNTPSync) >= NTP_SYNC_INTERVAL_MS)) {
    lastNTPSync = currentMillis;
    if (syncToRTC()) {
      rtcSynced = true;
      Serial.println("[RTC] RTC updated with the NTP time");
    }
  }
  
  // After 60 seconds without a crash the firmware is confirmed: if a freshly
  // installed version crashes earlier, the bootloader goes back to the previous one
  static bool firmwareConfirmed = false;
  if (!firmwareConfirmed && currentMillis >= FIRMWARE_HEALTHY_MS) {
    markFirmwareHealthy();
    firmwareConfirmed = true;
  }

  static bool updateDue = true;  // First update check as soon as possible
  static unsigned long lastUpdateCheck = 0;
  static unsigned long updateWaitMs = UPDATE_CHECK_MS;

  // In AP mode with a configured network: new attempt every 5 minutes, only if
  // no phone is connected to the AP (not to interrupt the setup)
  static unsigned long apSince = 0;
  if (apMode) {
    if (apSince == 0) apSince = currentMillis;
    if (strlen(config.ssid) > 0 && WiFi.softAPgetStationNum() == 0 &&
        currentMillis - apSince >= AP_RETRY_MS) {
      Serial.println("[WIFI] AP mode: trying the configured network again");
      if (connectToWiFi(config.ssid, config.password)) {
        setupServer();
        getWeatherData();
        updateDisplay();
        updateDue = true;
      } else {
        startAccessPoint(true);
      }
      apSince = millis();
    }
  } else {
    apSince = 0;
  }

  // Battery saving: WiFi is switched on only when needed (weather and time,
  // updates, web page after a touch) and switched off right after
  bool eco = ecoActive();
  if (eco) {
    bool weatherDue = (unsigned long)(currentMillis - lastWeatherUpdate) >= getUpdateInterval();
    bool updatesDue = updateDue || updateCheckRequested || currentMillis - lastUpdateCheck >= updateWaitMs;
    if (weatherDue || updatesDue || ecoWebWindowOpen()) ecoEnsureWiFi();
  } else if (!apMode && strlen(config.ssid) > 0 && WiFi.getMode() == WIFI_OFF) {
    // Charger plugged in again after power saving: WiFi always on again
    ecoEnsureWiFi();
  }

  // Updates from GitHub: at boot (so also right after the first setup),
  // then every 6 hours; after an error, new attempt in 10 minutes.
  // The HTTPS certificates need the right time (NTP or RTC).
  // If a new firmware is installed, checkForUpdates() restarts the device.
  if (!apMode && WiFi.status() == WL_CONNECTED && time(nullptr) > 1700000000 &&
      (updateDue || updateCheckRequested || currentMillis - lastUpdateCheck >= updateWaitMs)) {
    bool manualCheck = updateCheckRequested;  // From the web page: full check of the files
    updateDue = false;
    updateCheckRequested = false;
    lastUpdateCheck = currentMillis;
    updateWaitMs = checkForUpdates(manualCheck) ? UPDATE_CHECK_MS : UPDATE_RETRY_MS;
  }

  // Weather updates only outside AP mode
  if (!apMode) {
    // Update interval for the current power mode
    unsigned long updateInterval = getUpdateInterval();
    
    // Weather update at the computed interval
    if ((unsigned long)(currentMillis - lastWeatherUpdate) >= updateInterval) {
      lastWeatherUpdate = currentMillis;
      
      // Try to update the weather
      if (getWeatherData()) {
        // Success: reset the attempt counter
        networkRetryCounter = 0;
        lastWeatherUpdateSuccess = true;
        // The display is not updated here, only by the fixed-interval refresh
      } else {
        // Failure: count the attempts
        networkRetryCounter++;
        
        if (networkRetryCounter <= config.maxNetworkRetries) {
          // Try again in a minute
          lastWeatherUpdate = currentMillis - updateInterval + (60 * 1000);
        } else {
          // Maximum attempts reached
          lastWeatherUpdateSuccess = false;
          // The display is not updated here, only by the fixed-interval refresh
          // Reset the counter and try again at the next interval
          networkRetryCounter = 0;
        }
      }
    }
    
    // Display updated when the minute changes (not 60 s after boot, otherwise
    // the time shown could lag behind by almost a minute)
    static int lastShownMinute = -1;
    unsigned long refreshMs = getDisplayRefreshIntervalMs();
    bool refreshDue;
    struct tm nowTm;
    if (getLocalTime(&nowTm, 0)) {
      int minuteOfDay = nowTm.tm_hour * 60 + nowTm.tm_min;
      int stepMin = max(1, (int)(refreshMs / 60000UL));
      refreshDue = lastDisplayUpdate == 0 ||
                   (minuteOfDay != lastShownMinute &&
                    (minuteOfDay % stepMin == 0 ||
                     (unsigned long)(currentMillis - lastDisplayUpdate) >= refreshMs + 60000UL));
      if (refreshDue) lastShownMinute = minuteOfDay;
    } else {
      refreshDue = (unsigned long)(currentMillis - lastDisplayUpdate) >= refreshMs;
    }
    if (refreshDue) {
      lastDisplayUpdate = currentMillis ? currentMillis : 1;
      updateDisplay();
    }
  } else {
    // In AP mode the display shows the static setup screen.
    // The time is not updated: without NTP it is not reliable, and setPartialWindow crashed
  }
  
  
  // Web requests and captive portal DNS at every loop pass (about every 10 ms):
  // a page loads its HTML, CSS and scripts with separate requests, and serving
  // one every 800 ms made each page take about 3 seconds to open
  handleClientRequests();
  
  // On battery with WiFi off: sleep until the next minute
  if (eco) {
    ecoWiFiOffIfIdle();
    if (WiFi.getMode() == WIFI_OFF && !updateCheckRequested) {
      ecoSleep(60000);
      return;
    }
  }

  // Short delay to keep the web server responsive
  // (the former 500 ms made the interface slow)
  delay(10); 
}
