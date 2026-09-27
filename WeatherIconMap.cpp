/**
 * @file WeatherIconMap.cpp
 * @brief Weather condition -> icon file (see WeatherIconMap.h)
 *
 * Icons: Weather Icons by Erik Flowers, converted to 1-bit BMP (sd_files/icons).
 * Condition codes: https://openweathermap.org/weather-conditions
 * Every path returned here exists in sd_files/icons: before, several
 * conditions (haze, smoke, dust, sand, ash, snow or sleet with thunder,
 * cloudy with wind) had no file and fell back to the "sunny" icon.
 */
#include "WeatherIconMap.h"
#include "WeatherUtils.h"

#define ICON(name) "/icons/wi-" name ".bmp"

// Day or night variant
static const char* dn(bool night, const char* day, const char* nightIcon) {
  return night ? nightIcon : day;
}

const char* weatherIconPath(int id, bool night, float windMs) {
  bool strongWind = windMs >= WIND_STRONG_MS;
  bool gale = windMs >= WIND_GALE_MS;

  // 2xx: thunderstorm (21x and 221 are the ones without heavy rain)
  if (id >= 200 && id < 300) {
    if (id >= 210 && id <= 221) return dn(night, ICON("day-lightning"), ICON("night-alt-lightning"));
    if (id == 202 || id == 232) return dn(night, ICON("day-storm-showers"), ICON("night-alt-storm-showers"));
    return dn(night, ICON("day-thunderstorm"), ICON("night-alt-thunderstorm"));
  }

  // 3xx: drizzle
  if (id >= 300 && id < 400) return dn(night, ICON("day-sprinkle"), ICON("night-alt-sprinkle"));

  // 5xx: rain
  if (id >= 500 && id < 600) {
    if (id == 511) return dn(night, ICON("day-sleet"), ICON("night-alt-sleet"));      // Freezing rain
    if (id == 500) return dn(night, ICON("day-sprinkle"), ICON("night-alt-sprinkle")); // Light rain
    if (id >= 520) return dn(night, ICON("day-showers"), ICON("night-alt-showers"));   // Shower rain
    if (strongWind) return dn(night, ICON("day-rain-wind"), ICON("night-alt-rain-wind"));
    return dn(night, ICON("day-rain"), ICON("night-alt-rain"));
  }

  // 6xx: snow
  if (id >= 600 && id < 700) {
    if (id == 602 || id == 622) return ICON("snowflake-cold");                                   // Heavy snow
    if (id >= 611 && id <= 613) return dn(night, ICON("day-sleet"), ICON("night-alt-sleet"));
    if (id == 615 || id == 616) return dn(night, ICON("day-rain-mix"), ICON("night-alt-rain-mix"));
    if (strongWind) return dn(night, ICON("day-snow-wind"), ICON("night-alt-snow-wind"));
    return dn(night, ICON("day-snow"), ICON("night-alt-snow"));
  }

  // 7xx: atmosphere
  if (id >= 700 && id < 800) {
    switch (id) {
      case 711: return ICON("smoke");
      case 721: return dn(night, ICON("day-haze"), ICON("night-fog"));
      case 731:
      case 761: return ICON("dust");
      case 751: return ICON("sandstorm");
      case 762: return ICON("volcano");
      case 771: return ICON("strong-wind");  // Squalls
      case 781: return ICON("tornado");
      default:  return dn(night, ICON("day-fog"), ICON("night-fog"));
    }
  }

  // 800: clear sky (with a gale the wind matters more than the sun)
  if (id == 800) {
    if (gale) return ICON("strong-wind");
    return dn(night, ICON("day-sunny"), ICON("night-clear"));
  }

  // 801-802: few or scattered clouds
  if (id == 801 || id == 802) {
    if (gale) return dn(night, ICON("day-cloudy-gusts"), ICON("night-alt-cloudy-gusts"));
    if (strongWind) return dn(night, ICON("day-cloudy-windy"), ICON("night-alt-cloudy-windy"));
    return dn(night, ICON("day-cloudy"), ICON("night-alt-cloudy"));
  }

  // 803-804: broken or overcast clouds
  if (id == 803 || id == 804) {
    if (gale) return ICON("cloudy-gusts");
    if (strongWind) return ICON("cloudy-windy");
    return ICON("cloudy");
  }

  return dn(night, ICON("day-cloudy"), ICON("night-alt-cloudy"));
}
