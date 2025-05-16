#include "QuotesUtils.h"

// Ottiene una citazione casuale dalla categoria specificata
String getRandomQuote(String category) {
  // Assicurati che la SD card sia montata
  if (!SD.begin(5)) {
    // Ritorna una citazione di default in caso di errore
    return "\"La vita non è aspettare che passi la tempesta, ma imparare a ballare sotto la pioggia.\" - Seneca";
  }

  // Controlla se il file delle citazioni esiste
  if (!SD.exists("/quotes.json")) {
    // Ritorna una citazione di default se il file non esiste
    return "\"La semplicità è la più grande sofisticazione.\" - Leonardo da Vinci";
  }

  // Apri il file delle citazioni
  File file = SD.open("/quotes.json", FILE_READ);
  if (!file) {
    // Ritorna una citazione di default in caso di errore di apertura
    return "\"Non c'è nulla di nobile nell'essere superiori al prossimo. La vera nobiltà sta nell'essere superiori alla persona che eravamo ieri.\" - H. Wells";
  }

  // Carica il file JSON
  DynamicJsonDocument doc(4096);
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    // Ritorna una citazione di default in caso di errore JSON
    return "\"Il vero viaggio di scoperta non consiste nel cercare nuove terre, ma nell'avere nuovi occhi.\" - Proust";
  }

  // Controlla se la categoria specificata esiste e se contiene citazioni
  if (!doc.containsKey(category) || doc[category].size() == 0) {
    // Prova con la categoria "any" se quella specificata non esiste
    if (category != "any" && doc.containsKey("any") && doc["any"].size() > 0) {
      category = "any";
    } else {
      // Ritorna una citazione di default se non ci sono citazioni disponibili
      return "\"Fai di ogni giorno il tuo capolavoro.\" - John Wooden";
    }
  }

  // Scegli una citazione casuale dalla categoria
  JsonArray quotes = doc[category].as<JsonArray>();
  int numQuotes = quotes.size();
  
  if (numQuotes == 0) {
    // Ritorna una citazione di default se non ci sono citazioni nella categoria
    return "\"La saggezza inizia con la meraviglia.\" - Socrate";
  }

  // Genera un indice casuale
  int randomIndex = random(numQuotes);
  
  // Ritorna la citazione selezionata
  return quotes[randomIndex].as<String>();
}
