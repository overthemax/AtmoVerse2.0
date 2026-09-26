#include "QuotesManager.h"
#include "Hardware.h"
#include <Arduino.h>
#include "AtmoVerseConstants.h" // Per JSON_BUFFER_LARGE
#include <SD.h> // Aggiunto per SD
#include "Screens.h"  // quoteFitsDisplay()

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
  if (!getLocalTime(&timeinfo, 0)) {
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
  if (!getLocalTime(&timeinfo, 0)) {
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

// ---------------------------------------------------------------------------
// Citazioni programmate
// ---------------------------------------------------------------------------
// Sezione "programmate" di quotes.json. Ogni voce:
//   "text", "author"
//   "ora":    "HH:MM"        inizio (facoltativo)
//   "durata": minuti         per quanto resta attiva dall'"ora" (default 60)
//   "giorni": "lun,mer,ven"  giorni della settimana (facoltativo)
//   "data":   "MM-DD" ogni anno, oppure "YYYY-MM-DD" una volta (facoltativo);
//             con la sola data la citazione vale tutto il giorno
// Serve almeno uno tra "ora" e "data". Se più voci sono attive vince la più
// specifica (data + ora, poi data, poi ora); a parità si sceglie a caso.

static const char* SCHEDULED_KEY = "programmate";

// "HH:MM" -> minuti dalla mezzanotte, -1 se non valido
static int parseClock(const char* s) {
  int h, m;
  if (!s || sscanf(s, "%d:%d", &h, &m) != 2 || h < 0 || h > 23 || m < 0 || m > 59) return -1;
  return h * 60 + m;
}

// "lun,mar,..." contiene il giorno tm_wday (0 = domenica)?
static bool dayMatches(const char* days, int wday) {
  static const char* names[] = {"dom", "lun", "mar", "mer", "gio", "ven", "sab"};
  return timeMatches(days, names[wday]);
}

// "MM-DD" o "YYYY-MM-DD" corrisponde alla data di oggi?
static bool dateMatches(const char* date, const struct tm& t) {
  int y, m, d;
  if (sscanf(date, "%d-%d-%d", &y, &m, &d) == 3) {
    return y == t.tm_year + 1900 && m == t.tm_mon + 1 && d == t.tm_mday;
  }
  if (sscanf(date, "%d-%d", &m, &d) == 2) {
    return m == t.tm_mon + 1 && d == t.tm_mday;
  }
  return false;
}

static bool loadScheduledQuote(Quote& quote) {
  struct tm t;
  if (!getLocalTime(&t, 0)) return false;  // Senza ora valida niente programmate
  if (!initSD() || !SD.exists(QUOTES_FILE)) return false;

  File file = SD.open(QUOTES_FILE, FILE_READ);
  if (!file) return false;
  DynamicJsonDocument doc(JSON_BUFFER_LARGE);
  DynamicJsonDocument filter(JSON_BUFFER_SMALL);
  filter[SCHEDULED_KEY] = true;
  DeserializationError error = deserializeJson(doc, file, DeserializationOption::Filter(filter));
  file.close();
  if (error) return false;

  JsonArray list = doc[SCHEDULED_KEY].as<JsonArray>();
  if (list.isNull() || list.size() == 0) return false;

  int nowMin = t.tm_hour * 60 + t.tm_min;
  int bestScore = -1;
  int bestCount = 0;
  int chosen = -1;

  for (int i = 0; i < (int)list.size(); i++) {
    JsonObject q = list[i].as<JsonObject>();
    const char* ora = q["ora"] | "";
    const char* data = q["data"] | "";
    const char* giorni = q["giorni"] | "";
    bool hasTime = ora[0] != '\0';
    bool hasDate = data[0] != '\0';
    if (!hasTime && !hasDate) continue;

    if (hasDate && !dateMatches(data, t)) continue;
    if (giorni[0] != '\0' && !dayMatches(giorni, t.tm_wday)) continue;

    if (hasTime) {
      int start = parseClock(ora);
      if (start < 0) continue;
      int duration = q["durata"] | 60;
      if (duration < 1) duration = 1;
      // Minuti trascorsi dall'inizio, anche a cavallo della mezzanotte
      int elapsed = (nowMin - start + 1440) % 1440;
      if (elapsed >= duration) continue;
    }

    int score = (hasDate ? 2 : 0) + (hasTime ? 1 : 0);
    if (score > bestScore) {
      bestScore = score;
      bestCount = 1;
      chosen = i;
    } else if (score == bestScore) {
      // Scelta casuale uniforme tra le voci ugualmente specifiche
      bestCount++;
      if (random(bestCount) == 0) chosen = i;
    }
  }

  if (chosen < 0) return false;
  JsonObject selected = list[chosen].as<JsonObject>();
  quote.text = String(selected["text"] | "");
  quote.author = String(selected["author"] | "");
  return quote.text.length() > 0;
}

// ---------------------------------------------------------------------------
// Citazioni "orologio letterario": /orari/HH.txt sulla SD
// ---------------------------------------------------------------------------
// Un file per ora, una riga per citazione: "MM|testo|Autore, Opera"
// (generati da tools/prepara_citazioni_orarie.py e copiati a mano sulla SD:
// non fanno parte degli aggiornamenti automatici e non vengono mai toccati).
// Si legge solo il file dell'ora corrente, riga per riga, senza caricarlo in RAM.

// Scarto rapido prima della misura vera: oltre questa lunghezza la citazione
// non entra nel riquadro nemmeno col carattere più piccolo
static const size_t CLOCK_QUOTE_MAX_CHARS = 700;

static bool loadClockQuote(Quote& quote) {
  struct tm t;
  if (!getLocalTime(&t, 0)) return false;
  if (!initSD()) return false;

  char path[16];
  snprintf(path, sizeof(path), "/orari/%02d.txt", t.tm_hour);
  File f = SD.open(path, FILE_READ);
  if (!f) return false;

  char minute[3];
  snprintf(minute, sizeof(minute), "%02d", t.tm_min);

  // Scelta casuale uniforme tra le righe del minuto (reservoir sampling)
  int matches = 0;
  String chosen;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    if (line.length() < 4 || line[0] != minute[0] || line[1] != minute[1] || line[2] != '|') continue;
    int sep = line.indexOf('|', 3);
    if (sep < 0 || (size_t)(sep - 3) > CLOCK_QUOTE_MAX_CHARS) continue;
    // Solo citazioni che il display mostra per intero (font adattivo compreso)
    String author = line.substring(sep + 1);
    author.trim();
    if (!quoteFitsDisplay(line.substring(3, sep), author)) continue;
    matches++;
    if (random(matches) == 0) chosen = line;
  }
  f.close();
  if (matches == 0) return false;

  int sep = chosen.indexOf('|', 3);
  quote.text = chosen.substring(3, sep);
  quote.author = chosen.substring(sep + 1);
  quote.author.trim();
  return true;
}

// Funzione principale per ottenere una citazione da visualizzare.
// Priorità: 1) programmate (quotes.json)  2) orologio letterario (/orari)
//           3) categoria meteo corrente
Quote getQuoteForDisplay() {
  Quote quote;
  Quote prev = getCurrentQuote();

  // 1) Citazione programmata attiva in questo momento
  bool loaded = loadScheduledQuote(quote);
  String category = loaded ? String(SCHEDULED_KEY) : "";

  // 2) Citazione che cita l'orario attuale
  if (!loaded) {
    loaded = loadClockQuote(quote);
    if (loaded) category = "orari";
  }

  // 3) Citazione per la categoria meteo corrente
  if (!loaded) {
    category = getWeatherCategory();
    loaded = (category.length() > 0) && loadRandomQuote(category, quote);
  }

  if (!loaded) {
    // Nessuna citazione per questa categoria: si lascia quella precedente
    // (prima veniva mostrato sul display il testo di debug "cat: <categoria>")
    Serial.println("[QUOTES] Nessuna citazione per la categoria: " + category);
    quote = prev;
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
