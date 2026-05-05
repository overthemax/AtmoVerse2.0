#include "QuotesUtils.h"

// Definizione delle stringhe costanti in PROGMEM per ridurre l'utilizzo della DRAM
const char DEFAULT_QUOTE1[] PROGMEM = "\"La vita non è aspettare che passi la tempesta, ma imparare a ballare sotto la pioggia.\" - Seneca";
const char DEFAULT_QUOTE2[] PROGMEM = "\"La semplicità è la più grande sofisticazione.\" - Leonardo da Vinci";
const char DEFAULT_QUOTE3[] PROGMEM = "\"Non c'è nulla di nobile nell'essere superiori al prossimo. La vera nobiltà sta nell'essere superiori alla persona che eravamo ieri.\" - H. Wells";
const char DEFAULT_QUOTE4[] PROGMEM = "\"Il vero viaggio di scoperta non consiste nel cercare nuove terre, ma nell'avere nuovi occhi.\" - Proust";
const char DEFAULT_QUOTE5[] PROGMEM = "\"Fai di ogni giorno il tuo capolavoro.\" - John Wooden";
const char DEFAULT_QUOTE6[] PROGMEM = "\"La saggezza inizia con la meraviglia.\" - Socrate";

// Ottiene una citazione casuale dalla categoria specificata
String getRandomQuote(String category) {
#if defined(MINIMIZE_BUFFERS)
  // In modalità ottimizzata, utilizziamo citazioni predefinite per risparmiare codice di parsing JSON
  int index = random(6); // Abbiamo 6 citazioni predefinite
  switch (index) {
    case 0: return FPSTR(DEFAULT_QUOTE1);
    case 1: return FPSTR(DEFAULT_QUOTE2);
    case 2: return FPSTR(DEFAULT_QUOTE3);
    case 3: return FPSTR(DEFAULT_QUOTE4);
    case 4: return FPSTR(DEFAULT_QUOTE5);
    default: return FPSTR(DEFAULT_QUOTE6);
  }
#else
  // Assicurati che la SD card sia montata
  if (!SD.begin(5)) {
    return FPSTR(DEFAULT_QUOTE1);
  }
  
  // Controlla se il file delle citazioni esiste
  if (!SD.exists("/quotes.json")) {
    return FPSTR(DEFAULT_QUOTE2);
  }

  // Apri il file delle citazioni
  File file = SD.open("/quotes.json", FILE_READ);
  if (!file) {
    return FPSTR(DEFAULT_QUOTE3);
  }

  // Carica il file JSON con ottimizzazioni per la memoria
#if defined(MINIMIZE_BUFFERS)
  StaticJsonDocument<384> doc; // Usa un buffer statico più piccolo quando MINIMIZE_BUFFERS è attivo
#else
  DynamicJsonDocument doc(512); // Buffer dinamico standard
#endif
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    return FPSTR(DEFAULT_QUOTE4);
  }

  // Versione semplificata per risparmiare codice
  JsonArray quotes = doc["quotes"].as<JsonArray>();
  if (quotes.isNull() || quotes.size() == 0) {
    return FPSTR(DEFAULT_QUOTE5);
  }

  // Seleziona una citazione casuale ma con meno uso di DRAM
  int index = random(quotes.size());
  const char* quote = quotes[index].as<const char*>();
  return String(quote); // Converti alla fine per minimizzare l'uso della DRAM
#endif
}
