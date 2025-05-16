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
void serveWeatherIcon(WiFiClient& client, const String& path);
void serveQRCode(WiFiClient& client);
void sendResponse(WiFiClient& client, const String& contentType, const String& content, int statusCode = 200);
void sendJsonResponse(WiFiClient& client, const String& jsonContent, int statusCode = 200);
void sendRedirect(WiFiClient& client, const String& location);
void performWiFiScan(WiFiClient& client);

#endif // WEBSERVER_H
