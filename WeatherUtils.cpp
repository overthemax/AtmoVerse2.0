#include "WeatherUtils.h"
#include "Config.h"
#include "Updater.h"  // httpsGet()
#include <Arduino.h>
#include <ArduinoJson.h>
#include <time.h>

WeatherData currentWeather;

// Current weather from OpenWeatherMap (free API 2.5)
bool getWeatherData() {
  Serial.println("[WEATHER] Requesting the weather...");

  // The settings are already in memory (loaded at boot and updated by the
  // web pages): the SD card is not read again at every update
  if (strlen(config.api_key) == 0 || strlen(config.city) == 0) {
    Serial.println("[WEATHER] API key or city not set");
    return false;
  }

  // Free OpenWeatherMap API 2.5, search by city name
  String url = "https://api.openweathermap.org/data/2.5/weather?q=";
  url += urlEncodeParam(config.city);
  url += "&units=";
  url += (strlen(config.units) ? config.units : "metric");
  url += "&lang=";
  url += (strlen(config.language) ? config.language : "it");
  url += "&appid=";
  url += config.api_key;

  // HTTPS with a verified certificate: the API key never travels in clear
  String payload;
  int httpCode = httpsGet(url, payload, 8192);

  if (httpCode == 200) {
    bool success = parseWeatherData(payload);
    if (success) {
      Serial.printf("[WEATHER] OK - id %d, %.1f°, %s\n", currentWeather.weather_id, currentWeather.temp, config.city);
    } else {
      Serial.println("[WEATHER] Cannot read the weather data");
    }
    return success;
  }

  if (httpCode == 401) {
    Serial.println("[WEATHER] API key not valid or not active yet (HTTP 401)");
  } else if (httpCode == 404) {
    Serial.println("[WEATHER] City not found (HTTP 404)");
  } else if (httpCode > 0) {
    Serial.printf("[WEATHER] HTTP error %d\n", httpCode);
  } else {
    Serial.println("[WEATHER] Connection error");
  }
  return false;
}

// Encodes a URL parameter (e.g. "San Donà di Piave" -> "San%20Don%C3%A0%20di%20Piave")
String urlEncodeParam(const char* text) {
  static const char hex[] = "0123456789ABCDEF";
  String out;
  for (const unsigned char* p = (const unsigned char*)text; *p; p++) {
    if (isalnum(*p) || *p == '-' || *p == '_' || *p == '.' || *p == '~') {
      out += (char)*p;
    } else {
      out += '%';
      out += hex[*p >> 4];
      out += hex[*p & 0x0F];
    }
  }
  return out;
}

// Parses the API 2.5 response into currentWeather
bool parseWeatherData(String& json) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    Serial.printf("[WEATHER] Invalid JSON: %s\n", error.c_str());
    return false;
  }

  if (!doc["main"].is<JsonObject>() || !doc["weather"].is<JsonArray>()) {
    return false;
  }

  currentWeather.temp = doc["main"]["temp"];
  currentWeather.feels_like = doc["main"]["feels_like"];
  currentWeather.humidity = doc["main"]["humidity"];
  currentWeather.pressure = doc["main"]["pressure"];
  currentWeather.wind_speed = doc["wind"]["speed"];
  currentWeather.wind_deg = doc["wind"]["deg"];
  currentWeather.sunrise = doc["sys"]["sunrise"] | 0;
  currentWeather.sunset = doc["sys"]["sunset"] | 0;

  JsonObject weather = doc["weather"][0];
  currentWeather.weather_id = weather["id"];
  strlcpy(currentWeather.icon, weather["icon"] | "", sizeof(currentWeather.icon));
  strlcpy(currentWeather.description, weather["description"] | "", sizeof(currentWeather.description));

  // Moon phase from the days since a known new moon (6 January 2000,
  // 18:14 UTC = 947182440); the lunar cycle lasts about 29.53059 days
  time_t now = time(NULL);
  const time_t referenceNewMoon = 947182440;
  const float lunarCycle = 29.53059;
  float daysSinceRef = (float)(now - referenceNewMoon) / 86400.0f;

  // 0.0 = new moon, 0.5 = full moon, 1.0 = new moon
  float phase = fmod(daysSinceRef / lunarCycle, 1.0f);
  if (phase < 0) phase += 1.0f;
  currentWeather.moon_phase = phase;

  currentWeather.last_update = time(NULL);
  currentWeather.valid = true;

  return true;
}

// True if the weather is recent (less than 3 hours old) and not empty
bool isWeatherDataValid() {
  if (!currentWeather.valid) {
    return false;
  }
  time_t now = time(NULL);
  if (now - currentWeather.last_update > 3 * 3600) {
    return false;
  }
  if (currentWeather.temp == 0 && currentWeather.humidity == 0 && currentWeather.pressure == 0) {
    return false;
  }
  return true;
}

// Night between sunset and sunrise. The times come from OpenWeatherMap and
// change by a few minutes a day, so the last ones received are compared as
// times of day (seconds since midnight UTC). Without weather data the night
// is from 19:00 to 7:00.
bool isNightTime() {
  time_t now = time(nullptr);
  const long DAY = 86400;
  if (currentWeather.sunrise > 0 && currentWeather.sunset > currentWeather.sunrise) {
    long t = now % DAY;
    long rise = currentWeather.sunrise % DAY;
    long set = currentWeather.sunset % DAY;
    // Where sunset falls after midnight UTC the interval wraps around
    return rise < set ? (t < rise || t >= set) : (t >= set && t < rise);
  }
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  return timeinfo.tm_hour >= 19 || timeinfo.tm_hour < 7;
}

float windSpeedMs() {
  bool imperial = strcmp(config.units, "imperial") == 0;
  return imperial ? currentWeather.wind_speed * 0.44704f : currentWeather.wind_speed;
}
