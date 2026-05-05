#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiServer.h>
#include "Config.h"
#include "WeatherUtils.h"

// Istanza del server web
extern WiFiServer server;

// Funzioni di gestione del server web
void setupServer();
void handleClientRequests();
void handleWebServerOnly();
void processWebRequests();
void serveWeatherIcon(WiFiClient& client, const char* path);
void serveQRCode(WiFiClient& client);
void sendResponse(WiFiClient& client, const char* contentType, const char* content, int statusCode = 200);
void sendJsonResponse(WiFiClient& client, const char* jsonContent, int statusCode = 200);
void sendJsonResponse(WiFiClient& client, const __FlashStringHelper* jsonContent, int statusCode = 200);
void sendRedirect(WiFiClient& client, const char* location);
void performWiFiScan(WiFiClient& client);

// Funzioni di utilità
void urldecode(const char* str, char* result, size_t resultSize);
void getContentType(const char* filename, char* result, size_t resultSize);
bool serveFileFromSD(WiFiClient& client, const char* path);

#endif // WEBSERVER_H
