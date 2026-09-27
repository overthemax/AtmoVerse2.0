#include "QuotesManager.h"
#include "Hardware.h"
#include <Arduino.h>
#include "AtmoVerseConstants.h" // JSON_BUFFER_LARGE
#include <SD.h>
#include "Screens.h"  // quoteFitsDisplay()

// Quote currently shown on the display
static Quote g_currentQuote = {"", ""};

void setCurrentQuote(const Quote& q) {
  g_currentQuote = q;
}

Quote getCurrentQuote() {
  return g_currentQuote;
}

// Quotes file on the SD card
const char* QUOTES_FILE = "/quotes.json";

// ---------------------------------------------------------------------------
// English names and their pre-2.1.17 Italian equivalents
// ---------------------------------------------------------------------------
// quotes.json files written before 2.1.17 use Italian section names, field
// names and values. They are still read; the quote editor converts the file
// to the English names when it saves it.

struct NamePair {
  const char* en;
  const char* legacy;
};

static const NamePair SECTION_NAMES[] = {
  {"rain", "pioggia"},
  {"light_rain", "pioggia_leggera"},
  {"thunderstorm", "temporale"},
  {"storm", "tempesta"},
  {"clear_sky", "cielo_sereno"},
  {"few_clouds", "poche_nuvole"},
  {"scattered_clouds", "nuvole_sparse"},
  {"cloudy", "nuvole_abbondanti"},
  {"fog", "nebbia"},
  {"snow", "neve"},
  {"wind", "vento"},
  {"scheduled", "programmate"},
};

// Values of the "period", "season" and "days" fields
static const NamePair TAG_NAMES[] = {
  {"morning", "mattina"}, {"afternoon", "pomeriggio"}, {"evening", "sera"},
  {"day", "giorno"}, {"night", "notte"},
  {"winter", "inverno"}, {"spring", "primavera"}, {"summer", "estate"}, {"autumn", "autunno"},
  {"sun", "dom"}, {"mon", "lun"}, {"tue", "mar"}, {"wed", "mer"}, {"thu", "gio"}, {"fri", "ven"}, {"sat", "sab"},
};

template <size_t N>
static const char* legacyOf(const NamePair (&table)[N], const char* en) {
  for (const NamePair& p : table) {
    if (strcmp(p.en, en) == 0) return p.legacy;
  }
  return nullptr;
}

// Value of a text field, or of its pre-2.1.17 name when missing
static const char* fieldOf(JsonObject obj, const char* name, const char* legacy) {
  const char* v = obj[name] | "";
  return v[0] != '\0' ? v : (obj[legacy] | "");
}

// Current part of the day
TimeCategory getCurrentTimeCategory() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 0)) {
    return AFTERNOON; // No valid time yet
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

// True if the comma-separated list (e.g. "morning,day") contains target,
// ignoring case
static bool listContains(const char* list, const String& target) {
  if (!list) return false;
  if (target.length() == 0) return false;

  String listStr = String(list);
  listStr.trim();
  if (listStr.length() == 0) return false;

  int start = 0;
  int len = listStr.length();
  while (start <= len) {
    int commaIndex = listStr.indexOf(',', start);
    String token;
    if (commaIndex == -1) {
      token = listStr.substring(start);
      start = len + 1; // last token
    } else {
      token = listStr.substring(start, commaIndex);
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

// Like listContains, also accepting the Italian name of the tag
static bool tagMatches(const char* list, const char* tag) {
  if (listContains(list, tag)) return true;
  const char* legacy = legacyOf(TAG_NAMES, tag);
  return legacy && listContains(list, legacy);
}

static bool isCertainAuthor(const char* authorValue) {
  if (!authorValue) return false;
  String a = String(authorValue);
  a.trim();
  if (a.length() == 0) return false;
  return a.indexOf(',') >= 0;
}

static const char* getCurrentSeasonTag() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 0)) {
    return "";
  }
  int month = timeinfo.tm_mon + 1;
  if (month == 12 || month == 1 || month == 2) {
    return "winter";
  } else if (month >= 3 && month <= 5) {
    return "spring";
  } else if (month >= 6 && month <= 8) {
    return "summer";
  }
  return "autumn";
}

// quotes.json section for the current weather
String getWeatherCategory() {
  extern WeatherData currentWeather;

  if (!currentWeather.valid) {
    return "";
  }

  // Category from the OpenWeatherMap condition code. Precipitation and fog
  // come first; strong wind comes before the clear/cloudy codes, which cover
  // every remaining case.
  int weatherId = currentWeather.weather_id;
  float wind = windSpeedMs();

  // 2xx: thunderstorms; the most violent ones or gale-force wind -> storm
  if (weatherId >= 200 && weatherId <= 232) {
    bool violent = weatherId == 202 || weatherId == 212 || weatherId == 221 || weatherId == 232;
    return (violent || wind >= WIND_GALE_MS) ? "storm" : "thunderstorm";
  }

  // 3xx drizzle, 500 light rain, 520 light shower -> light rain
  if ((weatherId >= 300 && weatherId <= 321) || weatherId == 500 || weatherId == 520) {
    return "light_rain";
  }
  if (weatherId >= 501 && weatherId <= 531) {
    return "rain";
  }
  if (weatherId >= 600 && weatherId < 700) {
    return "snow";
  }
  if (weatherId >= 700 && weatherId < 800) {
    return "fog";
  }

  // Dry weather with strong wind
  if (wind >= WIND_GALE_MS) return "storm";
  if (wind >= WIND_STRONG_MS) return "wind";

  if (weatherId == 800) return "clear_sky";
  if (weatherId == 801) return "few_clouds";
  if (weatherId == 802) return "scattered_clouds";
  if (weatherId == 803 || weatherId == 804) return "cloudy";
  return "";
}

// Reads one section of quotes.json (or its pre-2.1.17 name) into doc.
// Only that section is parsed, so the whole file never sits in RAM.
static JsonArray loadSection(const char* section, JsonDocument& doc) {
  if (!initSD() || !SD.exists(QUOTES_FILE)) {
    Serial.println("[QUOTES] /quotes.json not found on the SD card");
    return JsonArray();
  }
  File file = SD.open(QUOTES_FILE, FILE_READ);
  if (!file) {
    Serial.println("[QUOTES] Cannot open /quotes.json");
    return JsonArray();
  }

  const char* legacy = legacyOf(SECTION_NAMES, section);
  JsonDocument filter;
  filter[section] = true;
  if (legacy) filter[legacy] = true;
  DeserializationError error = deserializeJson(doc, file, DeserializationOption::Filter(filter));
  file.close();
  if (error) {
    Serial.printf("[QUOTES] quotes.json is not valid JSON: %s\n", error.c_str());
    return JsonArray();
  }

  JsonArray list = doc[section].as<JsonArray>();
  if ((list.isNull() || list.size() == 0) && legacy) list = doc[legacy].as<JsonArray>();
  return list;
}

// Random quote from a quotes.json section, preferring quotes tagged for the
// current part of the day and season and with a known work
bool loadRandomQuote(const String& category, Quote& quote) {
  JsonDocument doc;
  JsonArray categoryQuotes = loadSection(category.c_str(), doc);
  int total = categoryQuotes.isNull() ? 0 : categoryQuotes.size();
  if (total == 0) {
    Serial.println("[QUOTES] No quotes in the section: " + category);
    return false;
  }

  // Current part of the day, specific and broad
  const char* specificTime; // morning / afternoon / evening
  const char* broadTime;    // day / night
  const char* seasonTag = getCurrentSeasonTag();

  switch (getCurrentTimeCategory()) {
    case MORNING:
      specificTime = "morning";
      broadTime = "day";
      break;
    case AFTERNOON:
      specificTime = "afternoon";
      broadTime = "day";
      break;
    case EVENING:
    default:
      specificTime = "evening";
      broadTime = "night";
      break;
  }

  // Cumulative pool: candidates are added from progressively looser steps
  // until the pool reaches MIN_POOL or the steps run out
  const int MAX_CANDIDATES = 50;
  const int MIN_POOL = 3;
  int candidates[MAX_CANDIDATES];
  int candidateCount = 0;

  auto alreadyIn = [&](int idx) -> bool {
    for (int j = 0; j < candidateCount; j++) if (candidates[j] == idx) return true;
    return false;
  };
  // A quote without a season fits every season
  auto seasonOk = [&](JsonObject obj) -> bool {
    if (seasonTag[0] == '\0') return true;
    String sv = String(obj["season"] | ""); sv.trim();
    return sv.length() == 0 || tagMatches(sv.c_str(), seasonTag);
  };
  // "period" was called "time" before 2.1.17
  auto periodOf = [](JsonObject obj) -> const char* {
    return fieldOf(obj, "period", "time");
  };
  // Adds the quotes with a known work that pass the test
  auto addCandidates = [&](auto test) {
    for (int i = 0; i < total && candidateCount < MAX_CANDIDATES; i++) {
      if (alreadyIn(i)) continue;
      JsonObject obj = categoryQuotes[i].as<JsonObject>();
      if (isCertainAuthor(obj["author"] | "") && test(obj)) candidates[candidateCount++] = i;
    }
  };

  // Step 1: specific part of the day + season
  addCandidates([&](JsonObject o) { return seasonOk(o) && tagMatches(periodOf(o), specificTime); });
  // Step 2: day/night + season
  if (candidateCount < MIN_POOL) {
    addCandidates([&](JsonObject o) { return seasonOk(o) && tagMatches(periodOf(o), broadTime); });
  }
  // Step 3: season only, any time
  if (candidateCount < MIN_POOL) {
    addCandidates([&](JsonObject o) { return seasonOk(o); });
  }
  // Step 4: no filter at all, unknown works included
  if (candidateCount == 0) {
    for (int i = 0; i < total && i < MAX_CANDIDATES; i++)
      candidates[i] = i;
    candidateCount = min(total, MAX_CANDIDATES);
  }

  int selectedIndex = candidates[random(candidateCount)];
  JsonObject selected = categoryQuotes[selectedIndex].as<JsonObject>();

  const char* selText = selected["text"] | "";
  const char* selAuthor = selected["author"] | "";

  Quote prev = getCurrentQuote();
  if (prev.text != String(selText) || prev.author != String(selAuthor)) {
    Serial.printf("[QUOTES] Section %s, quote %d of %d\n", category.c_str(), selectedIndex + 1, total);
    const char* selPeriod = periodOf(selected);
    const char* selSeason = selected["season"] | "";
    if (selPeriod[0] != '\0') Serial.printf("[QUOTES]   period: %s\n", selPeriod);
    if (selSeason[0] != '\0') Serial.printf("[QUOTES]   season: %s\n", selSeason);
  }

  quote.text = String(selText);
  quote.author = String(selAuthor);

  return true;
}

// ---------------------------------------------------------------------------
// Scheduled quotes
// ---------------------------------------------------------------------------
// "scheduled" section of quotes.json. Each entry:
//   "text", "author"
//   "at":       "HH:MM"          start (optional)
//   "duration": minutes          how long it stays active after "at" (default 60)
//   "days":     "mon,wed,fri"    days of the week (optional)
//   "date":     "MM-DD" every year, or "YYYY-MM-DD" once (optional);
//               with a date only, the quote lasts the whole day
// At least one of "at" and "date" is required. When several entries are
// active the most specific wins (date + time, then date, then time); ties are
// broken at random. Before 2.1.17 the fields were "ora", "durata", "giorni"
// and "data", with Italian day names.

static const char* SCHEDULED_KEY = "scheduled";

// "HH:MM" -> minutes since midnight, -1 if not valid
static int parseClock(const char* s) {
  int h, m;
  if (!s || sscanf(s, "%d:%d", &h, &m) != 2 || h < 0 || h > 23 || m < 0 || m > 59) return -1;
  return h * 60 + m;
}

// Does "mon,tue,..." contain the day tm_wday (0 = Sunday)?
static bool dayMatches(const char* days, int wday) {
  static const char* names[] = {"sun", "mon", "tue", "wed", "thu", "fri", "sat"};
  return tagMatches(days, names[wday]);
}

// Is "MM-DD" or "YYYY-MM-DD" today?
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
  if (!getLocalTime(&t, 0)) return false;  // No valid time, no scheduled quotes

  JsonDocument doc;
  JsonArray list = loadSection(SCHEDULED_KEY, doc);
  if (list.isNull() || list.size() == 0) return false;

  int nowMin = t.tm_hour * 60 + t.tm_min;
  int bestScore = -1;
  int bestCount = 0;
  int chosen = -1;

  for (int i = 0; i < (int)list.size(); i++) {
    JsonObject q = list[i].as<JsonObject>();
    const char* at = fieldOf(q, "at", "ora");
    const char* date = fieldOf(q, "date", "data");
    const char* days = fieldOf(q, "days", "giorni");
    bool hasTime = at[0] != '\0';
    bool hasDate = date[0] != '\0';
    if (!hasTime && !hasDate) continue;

    if (hasDate && !dateMatches(date, t)) continue;
    if (days[0] != '\0' && !dayMatches(days, t.tm_wday)) continue;

    if (hasTime) {
      int start = parseClock(at);
      if (start < 0) continue;
      int duration = q["duration"].is<int>() ? q["duration"].as<int>() : (q["durata"] | 60);
      if (duration < 1) duration = 1;
      // Minutes since the start, also across midnight
      int elapsed = (nowMin - start + 1440) % 1440;
      if (elapsed >= duration) continue;
    }

    int score = (hasDate ? 2 : 0) + (hasTime ? 1 : 0);
    if (score > bestScore) {
      bestScore = score;
      bestCount = 1;
      chosen = i;
    } else if (score == bestScore) {
      // Uniform random choice among equally specific entries
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
// Literary clock quotes: CLOCK_DIR/HH.txt on the SD card
// ---------------------------------------------------------------------------
// One file per hour, one line per quote: "MM|text|Author, Work"
// (made by tools/make_clock_quotes.py or the quote editor; never part of the
// automatic updates). Only the current hour's file is read, line by line,
// without loading it into RAM.

void migrateClockFolder() {
  if (!initSD()) return;
  if (SD.exists(LEGACY_CLOCK_DIR) && !SD.exists(CLOCK_DIR)) {
    bool ok = SD.rename(LEGACY_CLOCK_DIR, CLOCK_DIR);
    Serial.printf("[QUOTES] Literary clock folder %s renamed to %s: %s\n",
                  LEGACY_CLOCK_DIR, CLOCK_DIR, ok ? "ok" : "FAILED");
  }
}

// Quick check before the real measurement: longer quotes do not fit the box
// even with the smallest font
static const size_t CLOCK_QUOTE_MAX_CHARS = 700;

bool clockQuoteFits(const String& text, const String& author) {
  // Length first: measuring huge texts with the font takes time
  return text.length() <= CLOCK_QUOTE_MAX_CHARS && quoteFitsDisplay(text, author);
}

static bool loadClockQuote(Quote& quote) {
  struct tm t;
  if (!getLocalTime(&t, 0)) return false;
  if (!initSD()) return false;

  char path[20];
  snprintf(path, sizeof(path), CLOCK_DIR "/%02d.txt", t.tm_hour);
  File f = SD.open(path, FILE_READ);
  if (!f) return false;

  char minute[3];
  snprintf(minute, sizeof(minute), "%02d", t.tm_min);

  // Uniform random choice among the lines of this minute (reservoir sampling)
  int matches = 0;
  String chosen;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    if (line.length() < 4 || line[0] != minute[0] || line[1] != minute[1] || line[2] != '|') continue;
    int sep = line.indexOf('|', 3);
    if (sep < 0 || (size_t)(sep - 3) > CLOCK_QUOTE_MAX_CHARS) continue;
    // Only quotes the display shows in full (adaptive font included)
    String author = line.substring(sep + 1);
    author.trim();
    if (!clockQuoteFits(line.substring(3, sep), author)) continue;
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

// Quote for the display.
// Priority: 1) scheduled (quotes.json)  2) literary clock (CLOCK_DIR)
//           3) current weather section
Quote getQuoteForDisplay() {
  Quote quote;
  Quote prev = getCurrentQuote();

  // 1) Scheduled quote active now
  bool loaded = loadScheduledQuote(quote);
  String category = loaded ? String(SCHEDULED_KEY) : "";

  // 2) Quote that mentions the current time
  if (!loaded) {
    loaded = loadClockQuote(quote);
    if (loaded) category = "clock";
  }

  // 3) Quote for the current weather
  if (!loaded) {
    category = getWeatherCategory();
    loaded = (category.length() > 0) && loadRandomQuote(category, quote);
  }

  if (!loaded) {
    // Nothing found: the previous quote stays on the display
    Serial.println("[QUOTES] No quote for the section: " + (category.length() ? category : String("(no weather)")));
    quote = prev;
  }

  if (quote.text != prev.text || quote.author != prev.author) {
    Serial.println("[QUOTES] New quote (" + category + "): " + quote.text + " — " + quote.author);
  }

  // Kept for the web page
  setCurrentQuote(quote);
  return quote;
}
