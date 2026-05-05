/**
 * @file Weather.cpp
 * @brief Implementazione delle funzioni per la gestione dei dati meteorologici
 * 
 * Questo file contiene l'implementazione delle funzioni per ottenere
 * e gestire i dati meteorologici da OpenWeatherMap per AtmoVerse 2.0
 */

#include "Weather.h"
#include "Config.h"
#include "RetryUtils.h"
#include "DebugUtils.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "WeatherOpt.h" // Include file di ottimizzazione

// Forward declaration della funzione urlEncode
String urlEncode(const char* msg);

// Definizione della variabile globale per i dati meteo
WeatherData currentWeather;

// Stringhe costanti per URL API OpenWeatherMap (OWM) in PROGMEM
const char WEATHER_API_URL[] PROGMEM = "http://api.openweathermap.org/data/2.5/weather?q=%s&appid=%s&lang=it&units=metric";

// Gestione dei tentativi per il recupero dei dati meteo
RetryManager weatherRetryManager(5, 30000, 300000, 1.5); // 5 tentativi, partendo da 30s fino a 5min

// Monitoraggio stabilità del servizio meteo
StabilityMonitor weatherServiceMonitor(3, 2); // 3 errori per considerare instabile, 2 successi per recupero

/**
 * @brief Ottiene i dati meteo correnti dall'API OpenWeatherMap
 * 
 * @return true se i dati sono stati aggiornati con successo, false altrimenti
 */
bool getWeatherData() {
#if defined(OPTIMIZE_SIZE)
  // In modalità ottimizzata, utilizziamo la versione semplificata
  return getWeatherDataOpt();
#else
  // Verifica che il WiFi sia connesso
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("[WEATHER] Impossibile ottenere dati meteo: WiFi non connesso"));
    weatherRetryManager.recordFailure(F("WiFi non connesso"));
    return false;
  }

  // Verifica che la chiave API e la città siano configurate
  if (strlen(config.api_key) == 0 || strlen(config.city) == 0) {
    Serial.println(F("[WEATHER] Impossibile ottenere dati meteo: configurazione incompleta"));
    weatherRetryManager.recordFailure(F("Configurazione incompleta"));
    return false;
  }

  Serial.print(F("[WEATHER] Richiesta dati meteo per: "));
  Serial.println(config.city);

  // Costruisci l'URL per la richiesta API - Usando PROGMEM per ridurre DRAM
  char url[256];  // Buffer URL
  // Copiamo prima la stringa PROGMEM in un buffer temporaneo
  char urlTemplate[150];
  strcpy_P(urlTemplate, WEATHER_API_URL);
  sprintf(url, urlTemplate, config.city, config.api_key);
  
  // Log del tentativo
  DEBUG_LOG(DEBUG_CATEGORY_WEATHER, DEBUG_LEVEL_INFO, 
            "Tentativo di aggiornamento meteo %d/%d", 
            weatherRetryManager.getAttemptCount() + 1, 5);
            
  HTTPClient http;
  http.begin(url);
  // Impostiamo timeout per migliorare la resilienza
  http.setTimeout(10000); // 10 secondi di timeout
  
  // Esegui la richiesta GET con gestione errori migliorata
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    // Lettura della risposta JSON
    String payload = http.getString();
    Serial.println(F("[WEATHER] Risposta API ricevuta"));
    
    // Parsing del JSON con allocazione in PSRAM se disponibile
    #ifdef CONFIG_SPIRAM_SUPPORT
    JsonDocument* docPtr = nullptr;
    if (psramInit()) {
      // Controlla se è disponibile la PSRAM e in tal caso usala per allocare memoria
      if(psramFound()) {
        DynamicJsonDocument doc(10240); // Ridotto da 16384 per risparmiare memoria
        return parseAPIResponse(httpResponse, doc);
      } else {
        // Ridotta capacità per restare nei limiti della DRAM
        DynamicJsonDocument doc(6144); // Ridotto da 8192 per risparmiare DRAM
        return parseAPIResponse(httpResponse, doc);
      }
    } else {
      docPtr = new DynamicJsonDocument(512);
      Serial.println(F("[WEATHER] ArduinoJSON allocato in DRAM"));
    }
    JsonDocument& doc = *docPtr;
    #else
    DynamicJsonDocument doc(400); // Ridotto ulteriormente da 512 a 400 per minimizzare l'uso della DRAM
    #endif
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
      Serial.print(F("[WEATHER] Errore nel parsing JSON: "));
      Serial.println(error.c_str());
      http.end();
      weatherServiceMonitor.recordError();
      weatherRetryManager.recordFailure(F("Errore parsing JSON"));
      #ifdef CONFIG_SPIRAM_SUPPORT
      if (docPtr) delete docPtr;
      #endif
      return false;
    }
    
    // Estrazione dei dati meteo
    currentWeather.weather_id = doc["weather"][0]["id"];
    // Salva la descrizione in un buffer statico
    static char descriptionBuffer[32];
    const char* descTemp = doc["weather"][0]["description"] | "N/A";
    strncpy(descriptionBuffer, descTemp, sizeof(descriptionBuffer) - 1);
    descriptionBuffer[sizeof(descriptionBuffer) - 1] = '\0';
    currentWeather.description = descriptionBuffer; // Assegna il puntatore al buffer statico
    
    // Copia l'icona
    const char* iconCode = doc["weather"][0]["icon"] | "01d";
    strncpy(currentWeather.icon, iconCode, sizeof(currentWeather.icon) - 1);
    currentWeather.icon[sizeof(currentWeather.icon) - 1] = '\0';
    
    currentWeather.temp = doc["main"]["temp"];
    currentWeather.feels_like = doc["main"]["feels_like"];
    currentWeather.humidity = doc["main"]["humidity"];
    currentWeather.pressure = doc["main"]["pressure"];
    
    currentWeather.wind_speed = doc["wind"]["speed"];
    currentWeather.wind_deg = doc["wind"]["deg"] | 0;
    
    // Calcola la fase lunare (semplificato) - variabili statiche
    static time_t now;
    time(&now);
    static struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    static const int daysInMonth = 30; // Approssimazione
    float dayOfMonth = timeinfo.tm_mday;
    currentWeather.moon_phase = (dayOfMonth / daysInMonth); // Valore tra 0 e 1
    
    // Aggiorna il timestamp dell'ultimo aggiornamento
    currentWeather.last_update = time(NULL);
    
    // Segna i dati come validi
    currentWeather.valid = true;
    
    Serial.println(F("[WEATHER] Dati meteo aggiornati con successo"));
    Serial.print(F("[WEATHER] Temperatura: "));
    Serial.print(currentWeather.temp);
    Serial.print(F("°C, Umidità: "));
    Serial.print(currentWeather.humidity);
    Serial.print(F("%, Condizioni: "));
    Serial.println(currentWeather.description);
    
    http.end();
    
    // Registra successo nei sistemi di resilienza
    weatherServiceMonitor.recordSuccess();
    weatherRetryManager.recordSuccess();
    
    return true;
  } else {
    // Gestione errori HTTP specifica per codice
    String errorMsg;
    
    switch(httpCode) {
      case -1: 
        errorMsg = F("Timeout connessione"); 
        break;
      case 401: 
        errorMsg = F("API key non valida"); 
        break;
      case 404: 
        errorMsg = F("Città non trovata"); 
        break;
      case 429: 
        errorMsg = F("Limite richieste API superato"); 
        break;
      default: 
        errorMsg = String(F("Errore HTTP: ")) + String(httpCode);
    }
    
    Serial.println("[WEATHER] " + errorMsg);
    http.end();
    
    // Registra errore nei sistemi di resilienza
    weatherServiceMonitor.recordError();
    weatherRetryManager.recordFailure(errorMsg);
    
    // Se il componente è instabile, incrementa ulteriormente il tempo di ritardo
    if (!weatherServiceMonitor.isStable()) {
      DEBUG_LOG(DEBUG_CATEGORY_WEATHER, DEBUG_LEVEL_WARNING, 
                "Servizio meteo instabile. %d errori consecutivi.", 
                weatherServiceMonitor.getConsecutiveErrors());
    }
    
    return false;
  }
#endif
}

/**
 * @brief Verifica se i dati meteo sono validi e aggiornati
 * 
 * @return true se i dati meteo sono validi, false altrimenti
 */
bool isWeatherDataValid() {
  // Verifica se i dati sono stati inizializzati
  if (!currentWeather.valid) {
    return false;
  }
  
  // Verifica se i dati sono troppo vecchi (più di 3 ore)
  time_t now = time(NULL);
  if (now - currentWeather.last_update > 3 * 60 * 60) {
    return false;
  }
  
  return true;
}

/**
 * @brief Determina se è giorno o notte in base all'ora attuale
 * 
 * @return true se è notte, false se è giorno
 */
bool isNightTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    // In caso di errore, assume che sia giorno
    return false;
  }
  
  // Considera notte dalle 19:00 alle 6:59
  return (timeinfo.tm_hour >= 19 || timeinfo.tm_hour < 7);
}

/**
 * @brief Codifica una stringa per l'uso in un URL
 * 
 * @param input Stringa da codificare
 * @return String Stringa codificata
 */
String urlEncode(const char* input) {
  String encoded = "";
  char c;
  char code0;
  char code1;
  
  for (int i = 0; input[i]; i++) {
    c = input[i];
    if (c == ' ') {
      encoded += '+';
    } else if (isalnum(c)) {
      encoded += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) {
        code1 = (c & 0xf) - 10 + 'A';
      }
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) {
        code0 = c - 10 + 'A';
      }
      encoded += '%';
      encoded += code0;
      encoded += code1;
    }
  }
  
  return encoded;
}

/**
 * @brief Ottiene l'icona corrispondente alle condizioni meteorologiche
 * 
 * @param weatherId ID della condizione meteo (da OpenWeatherMap)
 * @param isNight true se è notte, false se è giorno
 * @return String nome del file dell'icona
 */
String getWeatherIconName(int weatherId, bool isNight) {
#if defined(OPTIMIZE_SIZE)
  // Versione ottimizzata che restituisce solo icone di base
  return getWeatherIconNameOpt(weatherId, isNight);
#else
  // Questa è una versione semplificata, da espandere in base alle icone disponibili
  
  // Thunderstorm
  if (weatherId >= 200 && weatherId < 300) {
    return "11d"; // Thunderstorm
  }
  
  // Drizzle and Rain
  if ((weatherId >= 300 && weatherId < 400) || (weatherId >= 500 && weatherId < 600)) {
    return isNight ? "09n" : "09d"; // Rain
  }
  
  // Snow
  if (weatherId >= 600 && weatherId < 700) {
    return "13d"; // Snow
  }
  
  // Atmosphere (fog, mist, etc.)
  if (weatherId >= 700 && weatherId < 800) {
    return "50d"; // Mist
  }
  
  // Clear
  if (weatherId == 800) {
    return isNight ? "01n" : "01d"; // Clear
  }
  
  // Clouds
  if (weatherId > 800 && weatherId < 900) {
    if (weatherId == 801) {
      return isNight ? "02n" : "02d"; // Few clouds
    } else if (weatherId == 802) {
      return isNight ? "03n" : "03d"; // Scattered clouds
    } else {
      return isNight ? "04n" : "04d"; // Broken or overcast clouds
    }
  }
  
  // Default
  return isNight ? "01n" : "01d";
#endif
}

/**
 * @brief Verifica se i dati meteo sono validi e aggiornati
 * 
 * @return true se i dati meteo sono validi, false altrimenti
 */
/**
 * @brief Verifica se è il momento di ritentare un aggiornamento meteo fallito
 * 
 * @return true se è il momento di ritentare, false altrimenti
 */
bool isTimeToRetryWeather() {
  return weatherRetryManager.isTimeToRetry();
}

/**
 * @brief Ottiene informazioni sullo stato dei tentativi di aggiornamento meteo
 * 
 * @return String con informazioni sui tentativi
 */
String getWeatherRetryStatus() {
  if (weatherRetryManager.getAttemptCount() == 0) {
    return "";
  }
  
  return String("Tentativi: ") + 
         String(weatherRetryManager.getAttemptCount()) + 
         String("/5 - ") + 
         weatherRetryManager.getLastError();
}
