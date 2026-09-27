#ifndef WEBMINIMAL_H
#define WEBMINIMAL_H

#include <Arduino.h>

// Minimal setup page built into the firmware (HTML, CSS and JS in a single
// file, no need for the SD card). Used when the full pages are not on the
// SD card: new or empty card, first installation. After the setup the
// device downloads the full pages from GitHub.
#include <WiFi.h>
void sendFallbackSetupPage(WiFiClient& client);

#endif // WEBMINIMAL_H
