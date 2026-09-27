#ifndef ATMOVERSE_CONSTANTS_H
#define ATMOVERSE_CONSTANTS_H

// Costanti per SSID e password AP
#define ATMOVERSE_AP_SSID "AtmoVerse_AP"
#define ATMOVERSE_AP_PASSWORD "atmoverse"

// Costanti di configurazione predefinita
#define ATMOVERSE_DEFAULT_CITY "Rome"
#define ATMOVERSE_DEFAULT_NTP "pool.ntp.org"
#define ATMOVERSE_DEFAULT_GMT_OFFSET 3600
#define ATMOVERSE_DEFAULT_DST_OFFSET 3600

// Costanti per buffer JSON
#define JSON_BUFFER_SMALL 512      // Per configurazioni minime
#define JSON_BUFFER_MEDIUM 2048    // Per conf.json completo e dati meteo
#define JSON_BUFFER_LARGE 32768    // Per layout e citazioni complesse

// Costanti per timing e retry
#define CONFIG_READ_MAX_RETRIES 3
#define CONFIG_RETRY_DELAY_MS 500

// Costanti per display e boot (ottimizzate per avvio veloce)
#define BOOT_SPLASH_DURATION_MS 500  // Ridotto da 800ms
#define BOOT_DELAY_MS 200           // Ridotto da 400ms

#endif // ATMOVERSE_CONSTANTS_H
