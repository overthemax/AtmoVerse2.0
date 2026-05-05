#ifndef WEBUIPAGES_H
#define WEBUIPAGES_H

#include <Arduino.h>
#include "WeatherUtils.h"

// Funzioni essenziali per la generazione delle pagine web - versione ridotta
String generateSettingsPage(String ssid = "");
String generateWiFiScanPage();
String generateStyleCSS();
String formatDateTime(unsigned long timestamp);
String getLocalIPString();
void displayAPInfo(const char* ssid, const char* password);

#endif // WEBUIPAGES_H
