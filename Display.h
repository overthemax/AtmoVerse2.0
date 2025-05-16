#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include "WeatherUtils.h"
#include "Hardware.h"

// Dichiarazioni delle funzioni per il display
void initDisplay();
void displayStartupScreen();
void updateDisplay();
void updateTimeOnly();
void updateTimeAndQuotes();
void drawDisplayContent();
void displaySetupScreen(String apName, String ipAddress);
void showAPModeInfo();
void drawWeatherIcon(int x, int y, int weatherId, bool isNight);
void drawBattery(int x, int y, int percentage);
void drawProgress(int x, int y, int width, int progress);
void displayError(const char* message);
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

#endif // DISPLAY_H
