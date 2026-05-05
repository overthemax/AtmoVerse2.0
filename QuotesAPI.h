#ifndef QUOTES_API_H
#define QUOTES_API_H

#include <Arduino.h>

// Ottiene tutte le citazioni come JSON per l'API web
String getAllQuotesAsJson();

// Aggiunge una citazione al file quotes.json
bool addQuoteToFile(const String& category, const String& text, const String& author, const String& time = "");

// Elimina una citazione dal file
bool deleteQuoteFromFile(const String& category, int index);

#endif // QUOTES_API_H
