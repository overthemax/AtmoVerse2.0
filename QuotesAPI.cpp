#include "QuotesAPI.h"
#include "Hardware.h"
#include <SD.h>
#include <ArduinoJson.h>

// Percorso del file JSON contenente le citazioni sulla SD
extern const char* QUOTES_FILE; // Definito in QuotesManager.cpp

// Funzione per ottenere tutte le citazioni dal file quotes.json
// Restituisce un JSON formattato per l'API web
String getAllQuotesAsJson() {
  Serial.println("[QuotesAPI] ========== getAllQuotesAsJson START ==========");
  
  // Inizializza la SD se necessario
  Serial.println("[QuotesAPI] Tentativo inizializzazione SD...");
  if (!SD.begin(SD_CS, sdSPI, SD_SPI_FREQ)) {
    Serial.println("[QuotesAPI] ERRORE: SD non inizializzata");
    return "{\"error\":\"SD non inizializzata\",\"quotes\":[]}";
  }
  Serial.println("[QuotesAPI] SD inizializzata con successo");
  
  // Verifica che il file esista
  Serial.print("[QuotesAPI] Verifica esistenza file: ");
  Serial.println(QUOTES_FILE);
  if (!SD.exists(QUOTES_FILE)) {
    Serial.println("[QuotesAPI] ERRORE: File quotes.json NON TROVATO");
    Serial.println("[QuotesAPI] Provo percorso alternativo: quotes.json (senza /)");
    if (!SD.exists("quotes.json")) {
      Serial.println("[QuotesAPI] ERRORE: Nemmeno quotes.json senza / trovato");
      return "{\"error\":\"File quotes.json non trovato\",\"quotes\":[]}";
    }
    Serial.println("[QuotesAPI] Trovato con percorso alternativo");
  } else {
    Serial.println("[QuotesAPI] File quotes.json TROVATO");
  }
  
  // Apri il file
  Serial.println("[QuotesAPI] Apertura file...");
  File file = SD.open(QUOTES_FILE, FILE_READ);
  if (!file) {
    Serial.println("[QuotesAPI] ERRORE: Impossibile aprire quotes.json");
    return "{\"error\":\"Impossibile aprire quotes.json\",\"quotes\":[]}";
  }
  Serial.print("[QuotesAPI] File aperto, dimensione: ");
  Serial.print(file.size());
  Serial.println(" bytes");
  
  // Leggi il file JSON
  Serial.println("[QuotesAPI] Parsing JSON...");
  DynamicJsonDocument doc(32768); // Buffer più grande per tutte le citazioni
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  
  if (error) {
    Serial.print("[QuotesAPI] ERRORE parsing JSON: ");
    Serial.println(error.c_str());
    return "{\"error\":\"Errore parsing JSON\",\"quotes\":[]}";
  }
  Serial.println("[QuotesAPI] JSON parsed con successo");
  
  // Costruisci l'array di citazioni per l'API
  Serial.println("[QuotesAPI] Costruzione array citazioni...");
  DynamicJsonDocument responseDoc(32768);
  JsonArray quotesArray = responseDoc.createNestedArray("quotes");
  
  int totalQuotes = 0;
  
  // Itera su tutte le categorie nel file
  for (JsonPair kv : doc.as<JsonObject>()) {
    String category = kv.key().c_str();
    Serial.print("[QuotesAPI] Categoria: ");
    Serial.print(category);
    
    JsonArray categoryQuotes = kv.value().as<JsonArray>();
    Serial.print(" - ");
    Serial.print(categoryQuotes.size());
    Serial.println(" citazioni");
    
    for (JsonVariant v : categoryQuotes) {
      JsonObject quoteObj = v.as<JsonObject>();
      JsonObject newQuote = quotesArray.createNestedObject();
      
      newQuote["text"] = quoteObj["text"].as<String>();
      newQuote["author"] = quoteObj["author"] | "";
      newQuote["category"] = category;
      
      // Aggiungi campo time se presente
      if (quoteObj.containsKey("time")) {
        newQuote["time"] = quoteObj["time"].as<String>();
      }
      
      totalQuotes++;
    }
  }
  
  Serial.print("[QuotesAPI] Totale citazioni processate: ");
  Serial.println(totalQuotes);
  
  // Serializza il documento in JSON
  Serial.println("[QuotesAPI] Serializzazione risposta...");
  String response;
  serializeJson(responseDoc, response);
  
  Serial.print("[QuotesAPI] Dimensione risposta: ");
  Serial.print(response.length());
  Serial.println(" bytes");
  Serial.println("[QuotesAPI] ========== getAllQuotesAsJson END ==========");
  
  return response;
}

// Funzione per aggiungere una citazione al file quotes.json
bool addQuoteToFile(const String& category, const String& text, const String& author, const String& time) {
  // Inizializza la SD
  if (!SD.begin(SD_CS, sdSPI, SD_SPI_FREQ)) {
    return false;
  }
  
  // Leggi il file esistente
  DynamicJsonDocument doc(32768);
  
  if (SD.exists(QUOTES_FILE)) {
    File file = SD.open(QUOTES_FILE, FILE_READ);
    if (file) {
      deserializeJson(doc, file);
      file.close();
    }
  }
  
  // Crea la categoria se non esiste
  if (!doc.containsKey(category)) {
    doc.createNestedArray(category);
  }
  
  // Aggiungi la nuova citazione
  JsonArray categoryArray = doc[category].as<JsonArray>();
  JsonObject newQuote = categoryArray.createNestedObject();
  newQuote["text"] = text;
  if (author.length() > 0) {
    newQuote["author"] = author;
  }
  if (time.length() > 0) {
    newQuote["time"] = time;
  }
  
  // Scrivi il file aggiornato
  File file = SD.open(QUOTES_FILE, FILE_WRITE);
  if (!file) {
    return false;
  }
  
  serializeJson(doc, file);
  file.close();
  
  return true;
}

// Funzione per eliminare una citazione dal file
bool deleteQuoteFromFile(const String& category, int index) {
  // Inizializza la SD
  if (!SD.begin(SD_CS, sdSPI, SD_SPI_FREQ)) {
    return false;
  }
  
  if (!SD.exists(QUOTES_FILE)) {
    return false;
  }
  
  // Leggi il file
  File file = SD.open(QUOTES_FILE, FILE_READ);
  if (!file) {
    return false;
  }
  
  DynamicJsonDocument doc(32768);
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  
  if (error || !doc.containsKey(category)) {
    return false;
  }
  
  // Rimuovi l'elemento all'indice specificato
  JsonArray categoryArray = doc[category].as<JsonArray>();
  if (index < 0 || index >= categoryArray.size()) {
    return false;
  }
  
  categoryArray.remove(index);
  
  // Scrivi il file aggiornato
  file = SD.open(QUOTES_FILE, FILE_WRITE);
  if (!file) {
    return false;
  }
  
  serializeJson(doc, file);
  file.close();
  
  return true;
}
