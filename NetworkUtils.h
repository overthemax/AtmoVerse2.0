#ifndef NETWORK_UTILS_H
#define NETWORK_UTILS_H

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include "Config.h"

// Istanze del server
extern DNSServer dnsServer;
extern bool apMode;

// Funzioni di configurazione e gestione rete
bool setupWiFi();
void setupTimeServer();
void applyTimezone();  // Solo fuso orario, senza NTP
bool isWiFiConnected();
bool connectToWiFi(const char* ssid, const char* password);
void startAccessPoint(bool forceStart = false);

#endif // NETWORK_UTILS_H
