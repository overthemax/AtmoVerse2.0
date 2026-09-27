#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Application settings
struct Config {
  char ssid[32];          // SSID WiFi
  char password[64];      // Password WiFi
  char city[32];          // City for the weather
  char api_key[64];       // API key OpenWeatherMap
  char units[8];          // Weather units: "metric" or "imperial"
  char language[8];       // Language: e.g. "it", "en"
  long gmtOffset_sec;     // GMT offset in seconds
  int daylightOffset_sec; // Daylight saving offset in seconds
  char ntpServer[64];     // Server NTP
  bool use24hFormat;      // Clock format: true = 24 h, false = 12 h

  // Impostazioni risparmio energetico
  bool powerSavingEnabled;       // Power saving hours enabled
  int powerSavingStartHour;      // Power saving start hour (e.g. 22 for 22:00)
  int powerSavingEndHour;        // Power saving end hour (e.g. 7 for 7:00)
  int normalUpdateInterval;      // Normal update interval in minutes
  int powerSavingUpdateInterval; // Update interval during power saving, in minutes

  // Network error handling
  int maxNetworkRetries;         // Maximum attempts on a network error

  // Display refresh (seconds)
  int displayRefreshIntervalSec;            // Display refresh interval, normal mode (s)
  int displayRefreshIntervalSecPowerSaving; // Display refresh interval, power saving (s)

  // Battery
  bool batteryShowOnDisplay;     // Show the battery on the display
};

// Settings functions
bool loadConfig();
bool saveConfig();
bool checkConfigValidity();

// Dichiarazione variabili esterne
extern Config config;

#endif // CONFIG_H
