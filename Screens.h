/**
 * @file Screens.h
 * @brief E-ink display screens with a single typographic system
 *
 * U8g2 fonts (FreeUniversal and Lucida Sans) with accented letters, shared
 * margins and grid, 100 px weather icons scaled exactly x2.
 * The draw* functions are called only by the display task (DisplayTask.cpp),
 * inside a firstPage()/nextPage() cycle, and use only the model.
 */
#ifndef SCREENS_H
#define SCREENS_H

#include <Arduino.h>
#include "DisplayTask.h"

void drawMainScreen(const ScreenModel& m);
void drawSetupScreen(const ScreenModel& m);
void drawUpdateScreen(const ScreenModel& m);
void drawMessageScreen(const ScreenModel& m);
void drawBatteryScreen(const ScreenModel& m);

// true if the quote fits the box in full, even with the smallest font.
// Called by the loop: uses a measuring object separate from the task's.
bool quoteFitsDisplay(const String& text, const String& author);

#endif // SCREENS_H
