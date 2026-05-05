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
  char ssid[33];    // SSID è limitato a 32 caratteri + null terminator
  int32_t rssi;
  bool secure;
  int channel;
  char encryption[16]; // Buffer per il tipo di crittografia
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
void scanWiFiNetworks(char* result, size_t resultSize);
void checkWiFiConnection();
int performWiFiScanForPage();

// Gestione riconnessione WiFi avanzata
bool checkAndReconnectWiFi();
void initWiFiReconnect();

// Funzione per convertire il tipo di crittografia in stringa
void getEncryptionTypeString(wifi_auth_mode_t encryptionType, char* output, size_t outputSize);

#endif // NETWORK_UTILS_H
