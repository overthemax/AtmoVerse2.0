#include "weather.h"
#include "config.h"

void setupTime() {
  // Configura il server NTP e imposta il fuso orario
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  setenv("TZ", timezone, 1);
  tzset();
  
  // Attendi che il tempo sia impostato
  Serial.println(F("Attesa per la sincronizzazione NTP..."));
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println();
  
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print(F("Timestamp attuale: "));
  Serial.println(asctime(&timeinfo));
}

void updateWeather() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("WiFi non connesso. Impossibile aggiornare i dati meteo."));
    return;
  }

  // Add your weather update logic here
}


  if (strlen(owm_api_key) == 0 || strlen(owm_city) == 0) {
    Serial.println(F("API key o città non configurate."));
    return;
  }

  HTTPClient http;
  String url = "http://api.openweathermap.org/data/2.5/weather?q=";
  url += owm_city;
  
  if (strlen(owm_country) > 0) {
    url += "," + String(owm_country);
  }
  
  url += "&units=metric&appid=" + String(owm_api_key);

  Serial.print(F("Richiesta meteo: "));
  Serial.println(url);
  
  http.begin(url);
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    // Ottimizza l'elaborazione dei dati meteo
    processWeatherData(http.getString());
    
    // Determina la categoria principale
    if (currentWeather.weatherId >= 200 && currentWeather.weatherId < 300) {
      currentWeather.weatherCategory = "Thunderstorm";
    } else if (currentWeather.weatherId >= 300 && currentWeather.weatherId < 400) {
      currentWeather.weatherCategory = "Drizzle";
    } else if (currentWeather.weatherId >= 500 && currentWeather.weatherId < 600) {
      currentWeather.weatherCategory = "Rain";
    } else if (currentWeather.weatherId >= 600 && currentWeather.weatherId < 700) {
      currentWeather.weatherCategory = "Snow";
    } else if (currentWeather.weatherId >= 700 && currentWeather.weatherId < 800) {
      currentWeather.weatherCategory = "Mist";
    } else if (currentWeather.weatherId == 800) {
      currentWeather.weatherCategory = "Clear";
    } else if (currentWeather.weatherId > 800 && currentWeather.weatherId < 900) {
      currentWeather.weatherCategory = "Clouds";
    }
    
    Serial.println(F("Dati meteo aggiornati con successo"));
  } else {
    Serial.print(F("Errore nella richiesta HTTP: "));
    Serial.println(httpCode);
  }
  
  http.end();
}

// Ottimizza l'elaborazione dei dati meteo
void processWeatherData(const String& json) {
    StaticJsonDocument<384> doc;
    DeserializationError error = deserializeJson(doc, json);

    if (error) {
        Serial.println(F("Errore nel parsing dei dati meteo"));
        return;
    }

    // Assegna i valori direttamente senza variabili temporanee
    currentWeather.temperature = doc["main"]["temp"];
    currentWeather.humidity = doc["main"]["humidity"];
    currentWeather.pressure = doc["main"]["pressure"];
    currentWeather.windSpeed = doc["wind"]["speed"];
    currentWeather.weatherId = doc["weather"][0]["id"];
    currentWeather.weatherCondition = doc["weather"][0]["main"].as<String>();
    currentWeather.cityName = doc["name"].as<String>();
    currentWeather.countryCode = doc["sys"]["country"].as<String>();

    // Aggiorna l'ora dell'ultimo aggiornamento
    unsigned long now = millis();
    currentWeather.lastUpdate = String(now / 3600000) + "h " + String((now % 3600000) / 60000) + "m";
}
