#ifndef QUOTES_MANAGER_H
#define QUOTES_MANAGER_H

#include <Arduino.h>
#include <time.h>
#include <SD.h>
#include <ArduinoJson.h>
#include "WeatherUtils.h"

// A quote shown on the display
struct Quote {
  String text;
  String author;
};

// Parts of the day
enum TimeCategory {
  MORNING,   // 5:00 - 11:59
  AFTERNOON, // 12:00 - 17:59
  EVENING,   // 18:00 - 4:59
};

// Literary clock folder on the SD card: one file per hour, CLOCK_DIR "/08.txt",
// lines "MM|text|Author, Work". Before 2.1.17 it was LEGACY_CLOCK_DIR.
#define CLOCK_DIR "/clock"
#define LEGACY_CLOCK_DIR "/orari"

// Renames the pre-2.1.17 literary clock folder, once, at boot
void migrateClockFolder();

// Quote for the display: scheduled, then literary clock, then weather
Quote getQuoteForDisplay();

// Literary clock: true if the display shows the whole quote
bool clockQuoteFits(const String& text, const String& author);

// Current part of the day
TimeCategory getCurrentTimeCategory();

// Random quote from a quotes.json section
bool loadRandomQuote(const String& category, Quote& quote);

// quotes.json section for the current weather ("" if there is none)
String getWeatherCategory();

// Last quote shown on the display
void setCurrentQuote(const Quote& q);
Quote getCurrentQuote();

#endif // QUOTES_MANAGER_H
