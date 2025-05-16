#ifndef PROGMEM_ICONS_H
#define PROGMEM_ICONS_H

#include <Arduino.h>

// Le icone salvate in PROGMEM occuperanno meno spazio nella memoria flash
// PROGMEM dice al compilatore di mantenere queste stringhe nella memoria Flash invece che in RAM

// Icone di navigazione minimaliste
extern const char SVG_EDIT_ICON[] PROGMEM;
extern const char SVG_INFO_ICON[] PROGMEM;
extern const char SVG_SETTINGS_ICON[] PROGMEM;

// Icona meteo principale
extern const char SVG_CLOUD_ICON[] PROGMEM;

// Icone per le condizioni meteo
extern const char SVG_THUNDER_ICON[] PROGMEM;
extern const char SVG_RAIN_ICON[] PROGMEM;
extern const char SVG_SNOW_ICON[] PROGMEM;
extern const char SVG_MIST_ICON[] PROGMEM;
extern const char SVG_SUN_ICON[] PROGMEM;
extern const char SVG_UNKNOWN_ICON[] PROGMEM;

// Icone per la funzionalità citazioni
extern const char SVG_QUOTE_ICON[] PROGMEM;

// Icone per la tabella meteo
extern const char SVG_TEMP_ICON[] PROGMEM;
extern const char SVG_HUMIDITY_ICON[] PROGMEM;
extern const char SVG_PRESSURE_ICON[] PROGMEM;
extern const char SVG_LOCATION_ICON[] PROGMEM;
extern const char SVG_TIMEZONE_ICON[] PROGMEM;
extern const char SVG_UPDATE_ICON[] PROGMEM;
extern const char SVG_REFRESH_ICON[] PROGMEM;

// Funzione helper per usare le icone dalla flash memory
String getProgmemString(const char* progmemString);

#endif // PROGMEM_ICONS_H
