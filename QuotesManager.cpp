#include "QuotesManager.h"
#include "Hardware.h"

// Percorso del file JSON contenente le citazioni sulla SD
const char* QUOTES_FILE = "/quotes.json";

// Inizializzazione dei membri statici
bool QuotesManager::initialized = false;

// Inizializza il gestore citazioni
bool QuotesManager::begin() {
  if (initialized) {
    return true;
  }
  
  // Verifica che il file delle citazioni esista sulla SD
  if (!SD.exists(QUOTES_FILE)) {
    Serial.print(F("[ERROR] File delle citazioni non trovato: "));
    Serial.println(QUOTES_FILE);
    return false;
  }
  
  initialized = true;
  return true;
}

// Ottiene la categoria temporale corrente
TimeCategory getCurrentTimeCategory() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return AFTERNOON; // Default in caso di errore
  }
  
  int hour = timeinfo.tm_hour;
  
  if (hour >= 5 && hour < 12) {
    return MORNING;
  } else if (hour >= 12 && hour < 18) {
    return AFTERNOON;
  } else {
    return EVENING;
  }
}

// Determina la categoria meteo corrente in base al codice meteo
String getWeatherCategory() {
  extern WeatherData currentWeather;
  
  // Se i dati meteo non sono validi o assenti, usa la categoria temporale
  if (!currentWeather.valid) {
    TimeCategory timeCategory = getCurrentTimeCategory();
    switch (timeCategory) {
      case MORNING: return "mattina";
      case AFTERNOON: return "pomeriggio";
      case EVENING: return "sera";
      default: return "motivazione";
    }
  }
  
  // Determina la categoria in base al codice meteo di OpenWeatherMap
  int weatherId = currentWeather.weather_id;
  
  // Pioggia: 200-531 (temporali, pioggia, nevischio)
  if ((weatherId >= 200 && weatherId < 600)) {
    return "pioggia";
  } 
  // Neve: 600-622
  else if (weatherId >= 600 && weatherId < 700) {
    return "neve";
  } 
  // Nebbia/atmosfera: 700-781
  else if (weatherId >= 700 && weatherId < 800) {
    return "nuvole";
  } 
  // Sereno: 800
  else if (weatherId == 800) {
    return "sole";
  } 
  // Nuvoloso: 801-804
  else if (weatherId > 800 && weatherId <= 804) {
    return "nuvole";
  }
  
  // Se il vento è significativo (oltre 20 km/h)
  if (currentWeather.wind_speed > 20) {
    return "vento";
  }
  
  // In caso di assenza di condizioni specifiche, usa la categoria basata sul momento della giornata
  TimeCategory timeCategory = getCurrentTimeCategory();
  switch (timeCategory) {
    case MORNING: return "mattina";
    case AFTERNOON: return "pomeriggio";
    case EVENING: return "sera";
    default: return "motivazione";
  }
}

// Carica una citazione casuale per la categoria specificata
bool loadRandomQuote(const String& category, Quote& quote) {
  // Inizializza la SD se non è già inizializzata
  if (!SD.begin(SD_CS, sdSPI)) {
    return false;
  }
  
  // Verifica che il file quotes.json esista
  if (!SD.exists(QUOTES_FILE)) {
    quote.text = "La conoscenza è potere.";
    quote.author = "Francis Bacon";
    return true; // Restituisci una citazione di default se il file non esiste
  }
  
  File file = SD.open(QUOTES_FILE, FILE_READ);
  if (!file) {
    return false;
  }
  
  // Usa una DynamicJsonDocument per analizzare il file JSON
  DynamicJsonDocument doc(8192); // Dimensione adeguata per contenere il file
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  
  if (error) {
    return false;
  }
  
  // Verifica che la categoria richiesta esista
  if (!doc.containsKey(category) || doc[category].size() == 0) {
    // Se la categoria non esiste, usa la categoria "motivazione"
    if (doc.containsKey("motivazione") && doc["motivazione"].size() > 0) {
      JsonArray motivationalQuotes = doc["motivazione"].as<JsonArray>();
      int randomIndex = random(motivationalQuotes.size());
      quote.text = motivationalQuotes[randomIndex]["text"].as<String>();
      quote.author = motivationalQuotes[randomIndex]["author"].as<String>();
      return true;
    } else {
      // Se neanche "motivazione" esiste, usa una citazione di default
      quote.text = "Il tempo è l'unica risorsa che non puoi recuperare.";
      quote.author = "Anonimo";
      return true;
    }
  }
  
  // Recupera citazioni per la categoria richiesta
  JsonArray categoryQuotes = doc[category].as<JsonArray>();
  int randomIndex = random(categoryQuotes.size());
  
  quote.text = categoryQuotes[randomIndex]["text"].as<String>();
  quote.author = categoryQuotes[randomIndex]["author"].as<String>();
  
  return true;
}

// Funzione principale per ottenere una citazione da visualizzare
Quote QuotesManager::getQuoteForDisplay() {
  Quote quote;
  
  // Determina la categoria appropriata in base al tempo e alle condizioni meteo
  String category = getWeatherCategory();
  
  // Tenta di caricare una citazione per la categoria
  if (!loadRandomQuote(category, quote)) {
    // In caso di errore, fornisci una citazione di fallback
    quote.text = "Ogni giorno è una nuova opportunità.";
    quote.author = "Anonimo";
  }
  
  return quote;
}
