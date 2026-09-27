/**
 * @file DisplayTask.h
 * @brief E-ink drawing in a dedicated task on core 0
 *
 * The loop (core 1) prepares a ScreenModel with everything the screen needs
 * (weather, quote, icon already read from the SD card, battery, IP) and hands
 * it over with showScreen(), which returns at once. The display task draws it
 * while the loop keeps serving the web server, DNS and network: a full e-ink
 * refresh takes about 4 seconds.
 *
 * The task never touches the SD card, the network or the program's global
 * variables: it uses only its own copy of the model. The only shared data is
 * the pending request, protected by a mutex. If several requests arrive
 * during a refresh, only the last one is drawn.
 */
#ifndef DISPLAY_TASK_H
#define DISPLAY_TASK_H

#include <Arduino.h>
#include "WeatherUtils.h"

enum ScreenKind : uint8_t {
  SCREEN_MAIN,     // Weather, time, quote
  SCREEN_SETUP,    // AP mode with QR code
  SCREEN_UPDATE,   // Update download
  SCREEN_MESSAGE,  // Titolo + testo
  SCREEN_BATTERY,  // Battery empty (tired face)
};

// Weather icon in RAM: 1 bit per pixel, rows from the top, bit 1 = black
static const int ICON_MAX_SIZE = 128;
static const int ICON_MAX_BYTES = ICON_MAX_SIZE * ICON_MAX_SIZE / 8;

struct ScreenModel {
  ScreenKind kind = SCREEN_MESSAGE;

  // Main screen
  WeatherData weather = {};
  bool weatherUpdateOk = true;
  String quoteText;
  String quoteAuthor;
  String city;
  String ip;
  bool wifiOn = false;  // WiFi icon in the header
  bool metric = true;
  bool use24h = true;   // false = 12-hour clock with AM/PM
  uint16_t iconWidth = 0;
  uint16_t iconHeight = 0;
  uint8_t icon[ICON_MAX_BYTES] = {};
  String notice;  // Persistent footer warning (e.g. SD card full)

  // Battery (shown on every screen)
  bool showBattery = false;
  int batteryPercent = 0;
  bool batteryCharging = false;
  bool batteryFull = false;     // Charger plugged in, charge finished

  // Update screen
  String updPhase;        // "SD files" / "New firmware"
  int updFilesDone = 0;
  int updFilesTotal = 0;  // 0 = step without files (firmware)
  int updPercent = 0;
  int updEtaSec = -1;     // -1 = no estimate yet

  // Setup screen
  String apName;
  String apIp;

  // Messaggio
  String title;
  String text;
};

// Starts the display task (once, after display.init)
void startDisplayTask();

// Hands over a screen to draw. Non-blocking.
void showScreen(const ScreenModel& model);

// Waits for the current and the pending drawing to finish (e.g. before a restart)
bool waitDisplayIdle(uint32_t timeoutMs);

// Reads a monochrome BMP icon from the SD card into the model (from the loop, not the task)
bool loadIconBitmap(const char* path, ScreenModel& model);

#endif // DISPLAY_TASK_H
