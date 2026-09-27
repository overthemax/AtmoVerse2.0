#include "Config.h"
#include "AtmoVerseConstants.h"
#include "Hardware.h"
#include "Debug.h"
#include <SD.h>
#include <SPI.h>
#include <ArduinoJson.h>

Config config;
SPIClass sdSPI(HSPI);

static const char* CONFIG_FILE = "/conf.json";
static const char* CONFIG_TMP = "/conf.tmp";
static const char* LEGACY_CONFIG_FILE = "/config.json";

// Fills the configuration from a JSON document. Every missing key gets its
// default value, so an empty document gives the complete default configuration.
static void populateConfigFromJson(const JsonDocument& doc) {
  strlcpy(config.ssid, doc["ssid"] | "", sizeof(config.ssid));
  strlcpy(config.password, doc["password"] | "", sizeof(config.password));
  strlcpy(config.api_key, doc["api_key"] | "", sizeof(config.api_key));
  strlcpy(config.units, doc["units"] | "metric", sizeof(config.units));
  strlcpy(config.language, doc["language"] | "it", sizeof(config.language));
  strlcpy(config.city, doc["city"] | ATMOVERSE_DEFAULT_CITY, sizeof(config.city));
  config.gmtOffset_sec = doc["gmt_offset"] | ATMOVERSE_DEFAULT_GMT_OFFSET;
  config.daylightOffset_sec = doc["dst_offset"] | ATMOVERSE_DEFAULT_DST_OFFSET;
  strlcpy(config.ntpServer, doc["ntp_server"] | ATMOVERSE_DEFAULT_NTP, sizeof(config.ntpServer));
  config.use24hFormat = doc["use24hFormat"] | true;
  config.displayRefreshIntervalSec = doc["displayRefreshIntervalSec"] | 60;
  config.displayRefreshIntervalSecPowerSaving = doc["displayRefreshIntervalSecPowerSaving"] | 300;

  config.powerSavingEnabled = doc["powerSavingEnabled"] | false;
  config.powerSavingStartHour = doc["powerSavingStartHour"] | 22;
  config.powerSavingEndHour = doc["powerSavingEndHour"] | 7;

  // Old configurations stored the weather interval in "weatherUpdateInterval"
  int normalInterval = doc["normalUpdateInterval"] | 0;
  if (normalInterval <= 0) normalInterval = doc["weatherUpdateInterval"] | 0;
  config.normalUpdateInterval = normalInterval > 0 ? normalInterval : 30;
  config.powerSavingUpdateInterval = doc["powerSavingUpdateInterval"] | 120;
  config.maxNetworkRetries = doc["maxNetworkRetries"] | 3;

  config.batteryShowOnDisplay = doc["batteryShowOnDisplay"] | true;
}

static void applyDefaults() {
  JsonDocument empty;
  populateConfigFromJson(empty);
}

// Brings every value back into a valid range. A value out of range (a damaged
// file, a wrong value from the web page) must never stop the device: for
// example a weather interval of 0 would make it query the weather non-stop.
static void sanitizeConfig() {
  config.normalUpdateInterval = constrain(config.normalUpdateInterval, 5, 360);
  config.powerSavingUpdateInterval = constrain(config.powerSavingUpdateInterval, 5, 720);
  config.powerSavingStartHour = constrain(config.powerSavingStartHour, 0, 23);
  config.powerSavingEndHour = constrain(config.powerSavingEndHour, 0, 23);
  config.displayRefreshIntervalSec = constrain(config.displayRefreshIntervalSec, 60, 3600);
  config.displayRefreshIntervalSecPowerSaving = constrain(config.displayRefreshIntervalSecPowerSaving, 60, 7200);
  config.maxNetworkRetries = constrain(config.maxNetworkRetries, 1, 10);
  config.gmtOffset_sec = constrain(config.gmtOffset_sec, -12L * 3600, 14L * 3600);
  if (config.daylightOffset_sec != 0) config.daylightOffset_sec = 3600;
  if (strcmp(config.units, "metric") != 0 && strcmp(config.units, "imperial") != 0) {
    strlcpy(config.units, "metric", sizeof(config.units));
  }
}

// Loads the configuration from the SD card. Returns true only if the file was
// read and the configuration is usable (a network name is set). In every other
// case (no SD card, no file, damaged file) the complete default configuration
// is in memory, so the rest of the firmware always finds valid values.
bool loadConfig() {
  DEBUG_TRACE();
  applyDefaults();

  if (!initSD()) {
    sanitizeConfig();
    return false;
  }

  const char* path = SD.exists(CONFIG_FILE) ? CONFIG_FILE
                    : SD.exists(LEGACY_CONFIG_FILE) ? LEGACY_CONFIG_FILE : nullptr;
  if (!path) {
    // First start: the default configuration is written to the card
    sanitizeConfig();
    saveConfig();
    return false;
  }

  bool success = false;
  File file = SD.open(path, FILE_READ);
  if (file) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (!error) {
      populateConfigFromJson(doc);
      success = true;
    } else {
      Serial.printf("[CONFIG] %s cannot be read (%s): default settings in use\n", path, error.c_str());
    }
  }
  sanitizeConfig();
  return success && checkConfigValidity();
}

// A network name is enough: an empty password means an open network
bool checkConfigValidity() {
  DEBUG_TRACE();
  return strlen(config.ssid) > 0;
}

// Saves the configuration. It is written to a temporary file first and then
// replaces conf.json: if the power fails while writing, the previous
// configuration (with the WiFi password) is still there.
bool saveConfig() {
  DEBUG_TRACE();
  if (!initSD()) return false;
  sanitizeConfig();

  JsonDocument doc;
  doc["ssid"] = config.ssid;
  doc["password"] = config.password;
  doc["api_key"] = config.api_key;
  doc["units"] = config.units;
  doc["language"] = config.language;
  doc["city"] = config.city;
  doc["gmt_offset"] = config.gmtOffset_sec;
  doc["dst_offset"] = config.daylightOffset_sec;
  doc["ntp_server"] = config.ntpServer;
  doc["use24hFormat"] = config.use24hFormat;
  doc["displayRefreshIntervalSec"] = config.displayRefreshIntervalSec;
  doc["displayRefreshIntervalSecPowerSaving"] = config.displayRefreshIntervalSecPowerSaving;
  doc["powerSavingEnabled"] = config.powerSavingEnabled;
  doc["powerSavingStartHour"] = config.powerSavingStartHour;
  doc["powerSavingEndHour"] = config.powerSavingEndHour;
  doc["normalUpdateInterval"] = config.normalUpdateInterval;
  doc["powerSavingUpdateInterval"] = config.powerSavingUpdateInterval;
  doc["maxNetworkRetries"] = config.maxNetworkRetries;
  doc["batteryShowOnDisplay"] = config.batteryShowOnDisplay;

  SD.remove(CONFIG_TMP);
  File file = SD.open(CONFIG_TMP, FILE_WRITE);
  if (!file) return false;
  size_t written = serializeJson(doc, file);
  file.close();

  File check = SD.open(CONFIG_TMP, FILE_READ);
  bool complete = check && written > 0 && check.size() == written;
  if (check) check.close();
  if (!complete) {
    SD.remove(CONFIG_TMP);
    Serial.println("[CONFIG] Saving failed: the previous configuration is kept");
    return false;
  }

  SD.remove(CONFIG_FILE);
  if (!SD.rename(CONFIG_TMP, CONFIG_FILE)) {
    Serial.println("[CONFIG] Cannot replace conf.json");
    return false;
  }
  SD.remove(LEGACY_CONFIG_FILE);  // Only one configuration file from now on
  return true;
}
