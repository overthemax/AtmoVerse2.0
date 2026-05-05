#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include "WeatherUtils.h"
#include "Hardware.h"

// Sistema di aggiornamento asincrono del display e-ink
// Queste variabili sono definite in Display.cpp
extern bool displayUpdateRequested;
extern unsigned long lastDisplayPhysicalUpdate;
extern const unsigned long DISPLAY_MIN_PHYSICAL_INTERVAL;
extern bool displayRefreshInProgress;

// Funzioni per il sistema di aggiornamento asincrono
void requestDisplayUpdate(bool isFullUpdate = false);
bool canUpdateDisplayPhysically();
void checkAndUpdateDisplay();

// Dichiarazioni delle funzioni per il display
void initDisplay();
void displayStartupScreen();
void updateDisplay();
void updateTimeOnly();
void updateTimeAndQuotes();
void drawDisplayContent();
void displaySetupScreen(String apName, String ipAddress);
void displayError(const char* errorMessage);
void showAPModeInfo();
void drawWeatherIcon(int x, int y, int weatherId, bool isNight);
void drawBattery(int x, int y, int percentage);
void drawProgress(int x, int y, int width, int progress);
void drawCityInfo(int x, int y, const char* cityName);
void drawTemperature(int x, int y, float temp, float feelsLike);
void drawHumidity(int x, int y, float humidity);
void drawPressure(int x, int y, float pressure);
void drawWind(int x, int y, float windSpeed);
void drawDateTime(int x, int y);
void drawLastUpdate(int x, int y, time_t lastUpdate);
void drawQuote(int x, int y, int maxWidth);
void drawQRCode(int x, int y, int size);
void showStatusOnDisplay(const char* msg);
String getOpenWeatherIconCode(int weatherId, bool isNight);

// Nuove funzioni per la gestione delle informazioni di connessione
void displayConnectionInfo(const char* ipAddress);
void displayAPModeInfo();

#endif // DISPLAY_H
