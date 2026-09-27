#ifndef NETWORK_UTILS_H
#define NETWORK_UTILS_H

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include "Config.h"

// Server instances
extern DNSServer dnsServer;
extern bool apMode;

// Network setup and management
bool setupWiFi();
void setupTimeServer();
void applyTimezone();  // Time zone only, without NTP
bool isWiFiConnected();
bool connectToWiFi(const char* ssid, const char* password);
void startAccessPoint(bool forceStart = false);

#endif // NETWORK_UTILS_H
