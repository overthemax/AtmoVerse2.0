#include "QuotesManager.h"
#include "Hardware.h"

// Citazioni di default in PROGMEM
const char DEFAULT_QUOTE_TEXT1[] PROGMEM = "La conoscenza è potere.";
const char DEFAULT_QUOTE_AUTHOR1[] PROGMEM = "Francis Bacon";
const char DEFAULT_QUOTE_TEXT2[] PROGMEM = "Il tempo è l'unica risorsa che non puoi recuperare.";
const char DEFAULT_QUOTE_AUTHOR2[] PROGMEM = "Anonimo";
const char DEFAULT_QUOTE_TEXT3[] PROGMEM = "Ogni giorno è una nuova opportunità.";
const char DEFAULT_QUOTE_AUTHOR3[] PROGMEM = "Anonimo";
const char DEFAULT_QUOTE_TEXT4[] PROGMEM = "Una citazione motivazionale";
const char DEFAULT_QUOTE_AUTHOR4[] PROGMEM = "Anonimo";
const char DEFAULT_QUOTE_TEXT5[] PROGMEM = "Una citazione interessante";
const char DEFAULT_QUOTE_AUTHOR5[] PROGMEM = "Anonimo";

// Percorso del file JSON contenente le citazioni sulla SD
const char QUOTES_FILE[] PROGMEM = "/quotes.json";

// Inizializzazione dei membri statici
bool QuotesManager::initialized = false;

// Inizializza il gestore citazioni
bool QuotesManager::begin() {
  if (initialized) {
    return true;
  }
  
  // Verifica che il file delle citazioni esista sulla SD
  if (!SD.exists(FPSTR(QUOTES_FILE))) {
    Serial.print(F("[ERROR] File delle citazioni non trovato: "));
    Serial.println(FPSTR(QUOTES_FILE));
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
      case MORNING: return F("mattina");
      case AFTERNOON: return F("pomeriggio");
      case EVENING: return F("sera");
      default: return F("motivazione");
    }
  }
  
  // Determina la categoria in base al codice meteo di OpenWeatherMap
  int weatherId = currentWeather.weather_id;
  
  // Pioggia: 200-531 (temporali, pioggia, nevischio)
  if ((weatherId >= 200 && weatherId < 600)) {
    return F("pioggia");
  } 
  // Neve: 600-622
  else if (weatherId >= 600 && weatherId < 700) {
    return F("neve");
  } 
  // Nebbia/atmosfera: 700-781
  else if (weatherId >= 700 && weatherId < 800) {
    return F("nuvole");
  } 
  // Sereno: 800
  else if (weatherId == 800) {
    return F("sole");
  } 
  // Nuvoloso: 801-804
  else if (weatherId > 800 && weatherId <= 804) {
    return F("nuvole");
  }
  
  // Se il vento è significativo (oltre 20 km/h)
  if (currentWeather.wind_speed > 20) {
    return F("vento");
  }
  
  // In caso di assenza di condizioni specifiche, usa la categoria basata sul momento della giornata
  TimeCategory timeCategory = getCurrentTimeCategory();
  switch (timeCategory) {
    case MORNING: return F("mattina");
    case AFTERNOON: return F("pomeriggio");
    case EVENING: return F("sera");
    default: return F("motivazione");
  }
}

// Carica una citazione casuale per la categoria specificata
bool loadRandomQuote(const String& category, Quote& quote) {
  // Inizializza la SD se non è già inizializzata
  if (!SD.begin(SD_CS, sdSPI)) {
    return false;
  }
  
  // Verifica che il file quotes.json esista
  if (!SD.exists(FPSTR(QUOTES_FILE))) {
    strcpy_P(quote.text, DEFAULT_QUOTE_TEXT1);
    strcpy_P(quote.author, DEFAULT_QUOTE_AUTHOR1);
    return true; // Restituisci una citazione di default se il file non esiste
  }
  
  File file = SD.open(FPSTR(QUOTES_FILE), FILE_READ);
  if (!file) {
    return false;
  }
  
  // Usa una DynamicJsonDocument per analizzare il file JSON
#if defined(MINIMIZE_JSON_BUFFER_SIZE)
  DynamicJsonDocument doc(384); // Ridotto ulteriormente a 384 bytes per minimizzare l'uso della DRAM
#else
  DynamicJsonDocument doc(1024); // Ridotto da 4096 a 1024 per minimizzare l'uso della DRAM
#endif
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
      // Usando direttamente la stringa da PROGMEM quando necessario
      if (motivationalQuotes[randomIndex].containsKey("text") && !motivationalQuotes[randomIndex]["text"].isNull()) {
        const char* quoteText = motivationalQuotes[randomIndex]["text"];
        strlcpy(quote.text, quoteText, sizeof(quote.text));
      } else {
        strcpy_P(quote.text, DEFAULT_QUOTE_TEXT4);
      }
      
      if (motivationalQuotes[randomIndex].containsKey("author") && !motivationalQuotes[randomIndex]["author"].isNull()) {
        const char* quoteAuthor = motivationalQuotes[randomIndex]["author"];
        strlcpy(quote.author, quoteAuthor, sizeof(quote.author));
      } else {
        strcpy_P(quote.author, DEFAULT_QUOTE_AUTHOR4);
      }
      return true;
    } else {
      // Se neanche "motivazione" esiste, usa una citazione di default
      strcpy_P(quote.text, DEFAULT_QUOTE_TEXT2);
      strcpy_P(quote.author, DEFAULT_QUOTE_AUTHOR2);
      return true;
    }
  }
  
  // Recupera citazioni per la categoria richiesta
  JsonArray categoryQuotes = doc[category].as<JsonArray>();
  int randomIndex = random(categoryQuotes.size());
  
  // Usando direttamente la stringa da PROGMEM quando necessario
  if (categoryQuotes[randomIndex].containsKey("text") && !categoryQuotes[randomIndex]["text"].isNull()) {
    const char* categoryQuoteText = categoryQuotes[randomIndex]["text"];
    strlcpy(quote.text, categoryQuoteText, sizeof(quote.text));
  } else {
    strcpy_P(quote.text, DEFAULT_QUOTE_TEXT5);
  }
  
  if (categoryQuotes[randomIndex].containsKey("author") && !categoryQuotes[randomIndex]["author"].isNull()) {
    const char* categoryQuoteAuthor = categoryQuotes[randomIndex]["author"];
    strlcpy(quote.author, categoryQuoteAuthor, sizeof(quote.author));
  } else {
    strcpy_P(quote.author, DEFAULT_QUOTE_AUTHOR5);
  }
  
  return true;
}

// Funzione principale per ottenere una citazione da visualizzare
Quote QuotesManager::getQuoteForDisplay() {
  Quote quote;
  
  // Determina la categoria appropriata in base al tempo e alle condizioni meteo
#if defined(REDUCE_STRING_LITERALS)
  char category[16]; // Buffer statico più piccolo invece di String
  strlcpy(category, getWeatherCategory().c_str(), sizeof(category));
#else
  String category = getWeatherCategory();
#endif
  
  // Tenta di caricare una citazione per la categoria
  if (!loadRandomQuote(category, quote)) {
    // In caso di errore, fornisci una citazione di fallback
    strcpy_P(quote.text, DEFAULT_QUOTE_TEXT3);
    strcpy_P(quote.author, DEFAULT_QUOTE_AUTHOR3);
  }
  
  return quote;
}
