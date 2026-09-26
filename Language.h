/**
 * @file Language.h
 * @brief Lingua dell'interfaccia (display e messaggi)
 *
 * Con config.language = "it" tutto è in italiano; con qualsiasi altra lingua
 * l'interfaccia è in inglese (le descrizioni del meteo arrivano da
 * OpenWeatherMap nella lingua scelta). Le pagine web usano /www/i18n.js.
 */
#ifndef LANGUAGE_H
#define LANGUAGE_H

#include <Arduino.h>

bool uiItalian();

// Testo nella lingua dell'interfaccia: TR("Batteria scarica", "Battery empty")
inline const char* TR(const char* it, const char* en) { return uiItalian() ? it : en; }

#endif // LANGUAGE_H
