#ifndef QUOTES_MANAGER_H
#define QUOTES_MANAGER_H

#include <Arduino.h>
#include <time.h>
#include <SD.h>
#include <ArduinoJson.h>
#include "WeatherUtils.h"

// Struttura per rappresentare una citazione
struct Quote {
  String text;
  String author;
};

// Categorie di tempo del giorno
enum TimeCategory {
  MORNING,   // 5:00 - 11:59
  AFTERNOON, // 12:00 - 17:59
  EVENING,   // 18:00 - 4:59
};

// Recupera una citazione basata sul momento del giorno e sulle condizioni meteo
Quote getQuoteForDisplay();

// Orologio letterario (/orari/HH.txt, righe "MM|testo|Autore, Opera"):
// true se la citazione viene mostrata per intero dal display
bool clockQuoteFits(const String& text, const String& author);

// Ottiene la categoria temporale corrente
TimeCategory getCurrentTimeCategory();

// Carica una citazione casuale per la categoria specificata
bool loadRandomQuote(const String& category, Quote& quote);

// Determina la categoria meteo corrente
String getWeatherCategory();

// Stato: ultima citazione mostrata sul display
void setCurrentQuote(const Quote& q);
Quote getCurrentQuote();

#endif // QUOTES_MANAGER_H
