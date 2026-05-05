#ifndef WEBUIPAGES_H
#define WEBUIPAGES_H

#include <Arduino.h>
#include "WeatherUtils.h"

// Stringhe HTML per le pagine web in PROGMEM per ridurre l'uso della DRAM
extern const char HTML_CSS[] PROGMEM;
extern const char HTML_HEADER_START[] PROGMEM;
extern const char HTML_HEADER_END[] PROGMEM;
extern const char HTML_CONTAINER_START[] PROGMEM;
extern const char HTML_WIFI_SECTION_START[] PROGMEM;
extern const char HTML_NTP_SECTION[] PROGMEM;
extern const char HTML_BUTTON_GROUP[] PROGMEM;
extern const char HTML_RESET_SECTION[] PROGMEM;
extern const char HTML_CONTAINER_END[] PROGMEM;
extern const char HTML_WIFI_SCAN_SCRIPT[] PROGMEM;

// Funzioni essenziali per la generazione delle pagine web - versione ridotta
String generateSettingsPage(String ssid = "");
String generateWiFiScanPage();
String generateStyleCSS();
String formatDateTime(unsigned long timestamp);
String getLocalIPString();
void displayAPInfo(const char* ssid, const char* password);

#endif // WEBUIPAGES_H
