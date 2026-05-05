#include "QuotesManager.h"
#include "Hardware.h"
#include <Arduino.h>
#include "AtmoVerseConstants.h" // Per JSON_BUFFER_LARGE
#include <SD.h> // Aggiunto per SD

// Stato in memoria della citazione attualmente mostrata sul display
static Quote g_currentQuote = {"", ""};

void setCurrentQuote(const Quote& q) {
  g_currentQuote = q;
}

Quote getCurrentQuote() {
  return g_currentQuote;
}

// Percorso del file JSON contenente le citazioni sulla SD
const char* QUOTES_FILE = "/quotes.json";

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

// Verifica se la stringa timeValue (eventualmente contenente più valori separati da virgola)
// contiene il valore target (case-insensitive), ad esempio "mattina,giorno" contiene "mattina".
static bool timeMatches(const char* timeValue, const String& target) {
  if (!timeValue) return false;
  if (target.length() == 0) return false;

  String timeStr = String(timeValue);
  timeStr.trim();
  if (timeStr.length() == 0) return false;

  int start = 0;
  int len = timeStr.length();
  while (start <= len) {
    int commaIndex = timeStr.indexOf(',', start);
    String token;
    if (commaIndex == -1) {
      token = timeStr.substring(start);
      start = len + 1; // termina il ciclo
    } else {
      token = timeStr.substring(start, commaIndex);
      start = commaIndex + 1;
    }

    token.trim();
    if (token.length() == 0) {
      continue;
    }

    if (token.equalsIgnoreCase(target)) {
      return true;
    }
  }

  return false;
}

static bool isCertainAuthor(const char* authorValue) {
  if (!authorValue) return false;
  String a = String(authorValue);
  a.trim();
  if (a.length() == 0) return false;
  return a.indexOf(',') >= 0;
}

static String getCurrentSeasonTag() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "";
  }
  int month = timeinfo.tm_mon + 1;
  if (month == 12 || month == 1 || month == 2) {
    return "inverno";
  } else if (month >= 3 && month <= 5) {
    return "primavera";
  } else if (month >= 6 && month <= 8) {
    return "estate";
  } else if (month >= 9 && month <= 11) {
    return "autunno";
  }
  return "";
}

// Determina la categoria meteo corrente in base al codice meteo
String getWeatherCategory() {
  extern WeatherData currentWeather;
  
  // Se i dati meteo non sono validi o assenti, usa la categoria temporale
  if (!currentWeather.valid) {
    return "";
  }
  
  // Determina la categoria in base al codice meteo di OpenWeatherMap
  int weatherId = currentWeather.weather_id;
  float windSpeed = currentWeather.wind_speed;

  // 2xx: Temporali / Tempeste
  if (weatherId >= 200 && weatherId <= 232) {
    // Codici pif intensi oppure vento molto forte f Tempesta
    if (weatherId == 202 || weatherId == 212 || weatherId == 221 || weatherId == 232 || windSpeed >= 40.0f) {
      return "tempesta";
    } else {
      return "temporale";
    }
  }

  // 3xx: Pioggerella / Pioggia leggera
  if (weatherId >= 300 && weatherId <= 321) {
    return "pioggia_leggera";
  }

  // 5xx: Pioggia (distinguiamo leggera da intensa)
  // 500=light rain, 520=light shower rain -> pioggia_leggera
  // 501=moderate rain, 502-531=heavy/shower rain -> pioggia
  if (weatherId == 500 || weatherId == 520) {
    return "pioggia_leggera";
  }
  if (weatherId == 501 || (weatherId >= 502 && weatherId <= 531)) {
    return "pioggia";
  }

  // Neve: 600-622
  if (weatherId >= 600 && weatherId < 700) {
    return "neve";
  }

  // Nebbia/atmosfera: 700-781
  if (weatherId >= 700 && weatherId < 800) {
    return "nebbia";
  }

  // Sereno e nuvole: 800-804
  if (weatherId == 800) {
    return "cielo_sereno";
  }
  if (weatherId == 801) {
    return "poche_nuvole";
  }
  if (weatherId == 802) {
    return "nuvole_sparse";
  }
  if (weatherId == 803 || weatherId == 804) {
    return "nuvole_abbondanti";
  }

  // Condizioni di vento forte senza altre indicazioni specifiche
  if (windSpeed > 20.0f) {
    return "vento";
  }
  
  // In caso di assenza di condizioni specifiche, usa la categoria basata sul momento della giornata
  return "";
}

// Carica una citazione casuale per la categoria specificata
bool loadRandomQuote(const String& category, Quote& quote) {
  // Inizializza la SD in modo centralizzato
  if (!initSD()) {
    Serial.println("[QUOTES] loadRandomQuote - ERRORE: initSD() fallita");
    return false;
  }
  
  // Verifica che il file quotes.json esista
  if (!SD.exists(QUOTES_FILE)) {
    Serial.println("[QUOTES] loadRandomQuote - ERRORE: /quotes.json non trovato su SD");
    return false;
  }
  
  File file = SD.open(QUOTES_FILE, FILE_READ);
  if (!file) {
    Serial.println("[QUOTES] loadRandomQuote - ERRORE: impossibile aprire /quotes.json");
    return false;
  }
  
  // Usa una DynamicJsonDocument filtrata per analizzare solo la categoria richiesta
#if defined(ESP32)
  Serial.print("[QUOTES] loadRandomQuote - heap prima JSON: ");
  Serial.println(ESP.getFreeHeap());
#endif
  DynamicJsonDocument doc(JSON_BUFFER_LARGE);
  DynamicJsonDocument filter(JSON_BUFFER_SMALL);
  filter[category] = true;
  DeserializationError error = deserializeJson(doc, file, DeserializationOption::Filter(filter));
  file.close();
  
  if (error) {
    Serial.print("[QUOTES] loadRandomQuote - ERRORE deserializeJson: ");
    Serial.println(error.c_str());
    return false;
  }
  
#if defined(ESP32)
  Serial.print("[QUOTES] loadRandomQuote - heap dopo JSON: ");
  Serial.println(ESP.getFreeHeap());
#endif
  
  // Verifica che la categoria richiesta esista
  if (!doc.containsKey(category) || doc[category].size() == 0) {
    Serial.print("[QUOTES] loadRandomQuote - categoria non trovata nel JSON: ");
    Serial.println(category);
    return false;
  }
  
  // Recupera citazioni per la categoria richiesta
  JsonArray categoryQuotes = doc[category].as<JsonArray>();
  int total = categoryQuotes.size();
  if (total == 0) {
    Serial.print("[QUOTES] loadRandomQuote - array vuoto per categoria: ");
    Serial.println(category);
    return false;
  }

  // Determina fascia oraria corrente (specifica e ampia)
  TimeCategory timeCategory = getCurrentTimeCategory();
  String specificTime; // mattina / pomeriggio / sera
  String broadTime;    // giorno / notte
  String seasonTag = getCurrentSeasonTag();

  switch (timeCategory) {
    case MORNING:
      specificTime = "mattina";
      broadTime = "giorno";
      break;
    case AFTERNOON:
      specificTime = "pomeriggio";
      broadTime = "giorno";
      break;
    case EVENING:
    default:
      specificTime = "sera";
      broadTime = "notte";
      break;
  }

  int selectedIndex = -1;

  // Selezione a pool cumulativo: aggiungi candidati da step progressivamente
  // più permissivi finché il pool raggiunge MIN_POOL o si esauriscono gli step.
  const int MAX_CANDIDATES = 50;
  const int MIN_POOL = 3;
  int candidates[MAX_CANDIDATES];
  int candidateCount = 0;

  auto alreadyIn = [&](int idx) -> bool {
    for (int j = 0; j < candidateCount; j++) if (candidates[j] == idx) return true;
    return false;
  };
  auto seasonOkFor = [&](const char* s) -> bool {
    if (seasonTag.length() == 0) return true;
    String sv = String(s); sv.trim();
    return (sv.length() == 0 || timeMatches(s, seasonTag));
  };

  // Step 1: time specifico (mattina/pomeriggio/sera) + stagione + autore certo
  if (candidateCount < MIN_POOL && specificTime.length() > 0) {
    for (int i = 0; i < total && candidateCount < MAX_CANDIDATES; i++) {
      if (alreadyIn(i)) continue;
      JsonObject obj = categoryQuotes[i].as<JsonObject>();
      if (!isCertainAuthor(obj["author"] | "")) continue;
      if (seasonOkFor(obj["season"] | "") && timeMatches(obj["time"] | "", specificTime))
        candidates[candidateCount++] = i;
    }
  }

  // Step 2: time ampio (giorno/notte) + stagione + autore certo
  if (candidateCount < MIN_POOL && broadTime.length() > 0) {
    for (int i = 0; i < total && candidateCount < MAX_CANDIDATES; i++) {
      if (alreadyIn(i)) continue;
      JsonObject obj = categoryQuotes[i].as<JsonObject>();
      if (!isCertainAuthor(obj["author"] | "")) continue;
      if (seasonOkFor(obj["season"] | "") && timeMatches(obj["time"] | "", broadTime))
        candidates[candidateCount++] = i;
    }
  }

  // Step 3: stagione only + autore certo (qualsiasi orario)
  if (candidateCount < MIN_POOL) {
    for (int i = 0; i < total && candidateCount < MAX_CANDIDATES; i++) {
      if (alreadyIn(i)) continue;
      JsonObject obj = categoryQuotes[i].as<JsonObject>();
      if (!isCertainAuthor(obj["author"] | "")) continue;
      if (seasonOkFor(obj["season"] | ""))
        candidates[candidateCount++] = i;
    }
  }

  // Step 4: nessun filtro, accetta tutti (anche senza autore certo)
  if (candidateCount == 0) {
    for (int i = 0; i < total && i < MAX_CANDIDATES; i++)
      candidates[i] = i;
    candidateCount = min(total, MAX_CANDIDATES);
  }

  selectedIndex = candidates[random(candidateCount)];

  JsonObject selected = categoryQuotes[selectedIndex].as<JsonObject>();

  const char* selText = selected["text"] | "";
  const char* selAuthor = selected["author"] | "";
  const char* selTime = selected["time"] | "";
  const char* selSeason = selected["season"] | "";

  // Verifica se la nuova citazione è diversa da quella attuale
  Quote prev = getCurrentQuote();
  bool changed = (prev.text != String(selText)) || (prev.author != String(selAuthor));

  if (changed) {
    Serial.print("[QUOTES] loadRandomQuote - categoria: ");
    Serial.print(category);
    Serial.print(", indice: ");
    Serial.println(selectedIndex);

    if (selTime && selTime[0] != '\0') {
      Serial.print("[QUOTES]   time: ");
      Serial.println(selTime);
    }
    if (selSeason && selSeason[0] != '\0') {
      Serial.print("[QUOTES]   season: ");
      Serial.println(selSeason);
    }

    Serial.print("[QUOTES]   autore: ");
    Serial.println(selAuthor);
    Serial.print("[QUOTES]   testo: ");
    Serial.println(selText);
  }

  quote.text = String(selText);
  quote.author = String(selAuthor);
  
  return true;
}

// Funzione principale per ottenere una citazione da visualizzare
Quote getQuoteForDisplay() {
  Quote quote;
  Quote prev = getCurrentQuote();
  
  // Determina la categoria appropriata in base al tempo e alle condizioni meteo
  String category = getWeatherCategory();
  
  // Tenta di caricare una citazione per la categoria meteo
  bool loaded = (category.length() > 0) && loadRandomQuote(category, quote);

  if (!loaded) {
    // Nessun fallback: mostra la categoria non trovata sul display
    quote.text = category.length() > 0 ? ("cat: " + category) : "meteo non disponibile";
    quote.author = "";
  }
  
  // Logga solo se la citazione è cambiata rispetto alla precedente
  bool changed = (quote.text != prev.text) || (quote.author != prev.author);
  if (changed) {
    Serial.print("[QUOTES] getQuoteForDisplay - categoria meteo: ");
    if (category.length() == 0) {
      Serial.println("(vuota, uso fallback)");
    } else {
      Serial.println(category);
    }
    if (quote.text == "citazione non trovata, controllare la sd ed il file quotes.json") {
      Serial.println("[QUOTES] getQuoteForDisplay - fallback: nessuna citazione valida trovata");
    }
    Serial.print("[QUOTES] getQuoteForDisplay - citazione finale: ");
    Serial.println(quote.text);
    Serial.print("[QUOTES] getQuoteForDisplay - autore finale: ");
    Serial.println(quote.author);
  }
  
  // Memorizza come citazione corrente per sincronizzazione Web
  setCurrentQuote(quote);
  return quote;
}
