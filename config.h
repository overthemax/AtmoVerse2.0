/*
 * AtmoVerse 2.0 - Configurazioni
 * 
 * Contiene definizioni, pin e variabili globali
 * Ottimizzato per ridurre l'utilizzo di memoria sull'ESP32
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Buffer sizes
#define SSID_BUFFER_SIZE 32
#define PASSWORD_BUFFER_SIZE 64
#define API_KEY_BUFFER_SIZE 40
#define CITY_BUFFER_SIZE 32
#define COUNTRY_BUFFER_SIZE 3
#define TIMEZONE_BUFFER_SIZE 64
#define QUOTE_BUFFER_SIZE 256

// Pin definitions
const int SD_CLK_PIN = 27;
const int SD_MISO_PIN = 25;
const int SD_MOSI_PIN = 26;
#define SD_CS 15
#define CONFIG_BUTTON_PIN 0
const int LED_BLUE = 2;

// E-ink display pins
#define EPD_BUSY    4
#define EPD_RST     16
#define EPD_DC      17
#define EPD_CS      5
#define EPD_SCK     18
#define EPD_MOSI    23

// Timing intervals
#define UPDATE_INTERVAL_MS     600000
#define CHECK_WIFI_INTERVAL_MS 30000
#define WEATHER_UPDATE_INTERVAL (30 * 60 * 1000)
#define DISPLAY_UPDATE_INTERVAL (10 * 60 * 1000)
#define WIFI_CONNECT_TIMEOUT    (5 * 60 * 1000)
#define CONFIG_MODE_TIMEOUT    300000
#define DNS_PORT               53

// OpenWeatherMap configuration
#define OWM_API_KEY_LEN 32
#define OWM_CITY_LEN 50
#define OWM_COUNTRY_LEN 2

extern char owm_api_key[OWM_API_KEY_LEN + 1];
extern char owm_city[OWM_CITY_LEN + 1];
extern char owm_country[OWM_COUNTRY_LEN + 1];

// Wi-Fi credentials
extern char ssid[SSID_BUFFER_SIZE + 1];
extern char password[PASSWORD_BUFFER_SIZE + 1];

// Timezone settings
extern char timezone[TIMEZONE_BUFFER_SIZE + 1];
extern bool dst_enabled;

// External variables
extern bool sdCardAvailable;

#endif // CONFIG_H
