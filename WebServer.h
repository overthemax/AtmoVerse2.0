#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiServer.h>
#include "Config.h"
#include "WeatherUtils.h"

// Web server instance
extern WiFiServer server;

// Web server functions
void setupServer();
void handleClientRequests();
void serveWeatherIcon(WiFiClient& client, const String& path);
void serveQRCode(WiFiClient& client);
void sendResponse(WiFiClient& client, const String& contentType, const String& content, int statusCode = 200);
void sendJsonResponse(WiFiClient& client, const String& jsonContent, int statusCode = 200);
void performWiFiScan(WiFiClient& client);

// Asks the loop to check for updates now (defined in the .ino)
void requestUpdateCheck();

#endif // WEBSERVER_H
