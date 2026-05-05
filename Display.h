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
void drawWeatherIcon(int x, int y, int weatherId, bool isNight, int iconSize = 160);
void drawBattery(int x, int y, int percentage);
void drawProgress(int x, int y, int width, int progress);
void displayError(const char* message);
void showSDCardMissing();
void showConfigSaved();  // Mostra conferma salvataggio configurazione
void drawCityInfo(int x, int y, const char* cityName);
void drawTemperature(int x, int y, float temp, float feelsLike);
void drawHumidity(int x, int y, float humidity);
void drawPressure(int x, int y, float pressure);
void drawWind(int x, int y, float windSpeed);
void drawDateTime(int x, int y);
void drawLastUpdate(int x, int y, time_t lastUpdate);
void drawQuote(int x, int y, int maxWidth, int fontSize = 12);
void showStatusOnDisplay(const char* msg);

// Funzioni per layout personalizzabili
void drawDefaultLayout();  // Layout di default quando /layout.json non esiste

// Funzioni per grafica migliorata
void drawThermometerIcon(int x, int y, int size);
void drawDropletIcon(int x, int y, int size);
void drawWindIcon(int x, int y, int size);
void drawHorizontalDivider(int x, int y, int width, bool decorative = false);
void drawVerticalDivider(int x, int y, int height);
void drawRoundedBox(int x, int y, int width, int height, int radius, bool filled = false);
void drawDecorativeFrame(int margin = 5);
void drawWeatherDataWithIcons(int x, int y, int iconSize);
void drawSDCardError();  // Schermata errore SD semplificata
void drawBigDigit(int x, int y, int digit, int height);  // Disegna una singola cifra grande
void drawBigNumber(int x, int y, float number, int height, bool showDecimal = true);  // Disegna numero grande (es. temperatura)

#endif // DISPLAY_H
