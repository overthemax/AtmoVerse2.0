/**
 * @file Display.h
 * @brief What the rest of the firmware asks the display to show
 *
 * Each function builds a ScreenModel in the main loop and hands it to the
 * display task (DisplayTask.h), which draws it on core 0 (Screens.h).
 */
#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include "WeatherUtils.h"
#include "Hardware.h"

void initDisplay();           // Initialises the panel and starts the display task
void displayStartupScreen();  // "Starting..." screen
void updateDisplay();         // Main screen: time, weather, quote, battery

// Setup screen with the QR code of the configuration network
void displaySetupScreen(String apName, String ipAddress);
void showAPModeInfo();

void showConfigSaved();                     // "Settings saved", waits until it is on the panel
void showRestarting();                      // "Restarting", waits until it is on the panel

// Update download: phase, files, percentage, estimated seconds (-1 = unknown)
void showUpdateProgress(const char* phase, int filesDone, int filesTotal, int percent, int etaSec);
// Update error that needs user action (e.g. SD card full)
void showUpdateError(const char* title, const char* text);
// Battery empty: tired face (stays visible during deep sleep)
void showBatteryEmpty();

#endif // DISPLAY_H
