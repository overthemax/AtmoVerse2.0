/**
 * @file EcoPower.h
 * @brief Power saving on battery
 *
 * With the charger plugged in everything stays on as usual. On battery:
 * - WiFi is switched on only when needed (weather and time, update check)
 *   and switched off right after;
 * - between two display updates the board sleeps (light sleep: RAM and
 *   state kept) until the next minute;
 * - touching the top-right screw (back) wakes the board, which keeps WiFi
 *   and the web page on for 10 minutes, extended at every request.
 */
#ifndef ECO_POWER_H
#define ECO_POWER_H

#include <Arduino.h>

// Call in setup() after battery.begin(): calibrates the touch button
void ecoBegin();

// Call when the charger is plugged in or out: recalibrates the touch
void ecoPowerChanged();

// true on battery (not charging), with a network configured and outside AP mode
bool ecoActive();

// Web page window opened by a touch
bool ecoWebWindowOpen();
void ecoOpenWebWindow();

// Called by the web server at every request: extends the window
void ecoNoteWebActivity();

// Switches WiFi on and connects if needed (waiting between failed attempts)
bool ecoEnsureWiFi();

// Switches WiFi off if nobody is using it
void ecoWiFiOffIfIdle();

// Sleeps until the next minute (at most maxMs), only with WiFi off
// and the display idle. A touch wakes the board and opens the web page.
void ecoSleep(unsigned long maxMs);

#endif // ECO_POWER_H
