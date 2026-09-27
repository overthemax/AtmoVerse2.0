/**
 * @file WeatherIconMap.h
 * @brief Weather condition -> BMP icon file in the /icons folder of the SD card
 */
#ifndef WEATHER_ICON_MAP_H
#define WEATHER_ICON_MAP_H

#include <Arduino.h>

// Icon file for an OpenWeatherMap condition code. windMs is the wind speed
// in m/s: strong wind picks the "windy" variants of the cloud icons.
const char* weatherIconPath(int weatherId, bool isNight, float windMs);

#endif // WEATHER_ICON_MAP_H
