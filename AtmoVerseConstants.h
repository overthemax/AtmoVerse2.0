#ifndef ATMOVERSE_CONSTANTS_H
#define ATMOVERSE_CONSTANTS_H

// Access point SSID and password
#define ATMOVERSE_AP_SSID "AtmoVerse_AP"
#define ATMOVERSE_AP_PASSWORD "atmoverse"

// Default settings
#define ATMOVERSE_DEFAULT_CITY "Rome"
#define ATMOVERSE_DEFAULT_NTP "pool.ntp.org"
#define ATMOVERSE_DEFAULT_GMT_OFFSET 3600
#define ATMOVERSE_DEFAULT_DST_OFFSET 3600

// JSON buffer sizes
#define JSON_BUFFER_SMALL 512      // Small settings
#define JSON_BUFFER_MEDIUM 2048    // Full conf.json and weather data
#define JSON_BUFFER_LARGE 32768    // Large documents

// Timing and retries
#define CONFIG_READ_MAX_RETRIES 3
#define CONFIG_RETRY_DELAY_MS 500

// Display and boot (tuned for a fast start)
#define BOOT_SPLASH_DURATION_MS 500  // Ridotto da 800ms
#define BOOT_DELAY_MS 200           // Ridotto da 400ms

#endif // ATMOVERSE_CONSTANTS_H
