/**
 * @file Language.h
 * @brief Interface language (display and messages)
 *
 * With config.language = "it" everything is in Italian; with any other
 * language the interface is in English (the weather descriptions come from
 * OpenWeatherMap in the chosen language). The web pages use /www/i18n.js.
 */
#ifndef LANGUAGE_H
#define LANGUAGE_H

#include <Arduino.h>

bool uiItalian();

// Text in the interface language: TR("Batteria scarica", "Battery empty")
inline const char* TR(const char* it, const char* en) { return uiItalian() ? it : en; }

#endif // LANGUAGE_H
