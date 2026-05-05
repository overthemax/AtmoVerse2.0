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
#define ATMOVERSE_LOG_FILE "/atmoverse.log"

// Costanti per buffer JSON
#define JSON_BUFFER_SMALL 512      // Per configurazioni minime
#define JSON_BUFFER_MEDIUM 2048    // Per conf.json completo e dati meteo
#define JSON_BUFFER_LARGE 32768    // Per layout e citazioni complesse

// Costanti per timing e retry
#define CONFIG_READ_MAX_RETRIES 3
#define CONFIG_RETRY_DELAY_MS 500
#define HTTP_TIMEOUT_MS 10000      // 10 secondi per chiamate API
#define WIFI_CONNECT_MAX_ATTEMPTS 60  // 30 secondi (60 * 500ms)
#define WIFI_CONNECT_RETRY_DELAY_MS 500

// Costanti per intervalli di controllo nel loop
#define BUTTON_CHECK_INTERVAL_MS 300
#define WEB_SERVER_CHECK_INTERVAL_MS 800
#define MAIN_LOOP_DELAY_MS 500

// Costanti per display e boot (ottimizzate per avvio veloce)
#define BOOT_SPLASH_DURATION_MS 500  // Ridotto da 800ms
#define BOOT_DELAY_MS 200           // Ridotto da 400ms
#define AP_INFO_DISPLAY_DURATION_MS 3000

// Costanti per WiFi check
#define WIFI_CHECK_INTERVAL 60000  // 60 secondi

#endif // ATMOVERSE_CONSTANTS_H
