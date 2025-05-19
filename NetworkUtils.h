#ifndef NETWORK_UTILS_H
#define NETWORK_UTILS_H

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include "Config.h"

// Istanze del server
extern DNSServer dnsServer;
extern bool apMode;

// Intervallo di verifica WiFi (20 secondi)
const unsigned long WIFI_CHECK_INTERVAL = 20 * 1000;

// Struttura dati per le reti WiFi
struct WiFiNetwork {
  String ssid;
  int32_t rssi;
  bool secure;
  int channel;
  String encryption;
};

// Funzioni di configurazione e gestione rete
bool setupWiFi();
void setupTimeServer();
bool isWiFiConnected();
bool connectToWiFi(const char* ssid, const char* password, uint32_t timeoutMs = 30000);
bool reconnectIfNeeded();
bool reconnectToWiFi();
void startAccessPoint(bool forceStart = false);
IPAddress getLocalIP();
String scanWiFiNetworks();
void checkWiFiConnection();
int performWiFiScanForPage();

#endif // NETWORK_UTILS_H
