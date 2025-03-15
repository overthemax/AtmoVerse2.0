/*
 * AtmoVerse 2.0 - Gestione delle citazioni
 * 
 * Ottimizzato per ESP32 con gestione della memoria efficiente
 */

#ifndef QUOTES_H
#define QUOTES_H

#include "config.h"

// Costante per il numero massimo di citazioni per categoria
#define MAX_QUOTES_PER_CATEGORY 5

// Struttura ottimizzata per ridurre l'uso di memoria
struct CustomQuotes {
  char clearSky[MAX_QUOTES_PER_CATEGORY][QUOTE_BUFFER_SIZE];
  char clouds[MAX_QUOTES_PER_CATEGORY][QUOTE_BUFFER_SIZE];
  char rain[MAX_QUOTES_PER_CATEGORY][QUOTE_BUFFER_SIZE];
  char snow[MAX_QUOTES_PER_CATEGORY][QUOTE_BUFFER_SIZE];
  char thunderstorm[MAX_QUOTES_PER_CATEGORY][QUOTE_BUFFER_SIZE];
  char mist[MAX_QUOTES_PER_CATEGORY][QUOTE_BUFFER_SIZE];
  char brokenClouds[MAX_QUOTES_PER_CATEGORY][QUOTE_BUFFER_SIZE];
  uint8_t clearSkyCount;
  uint8_t cloudsCount;
  uint8_t rainCount;
  uint8_t snowCount;
  uint8_t thunderstormCount;
  uint8_t mistCount;
  uint8_t brokenCloudsCount;
};

// Dichiarazioni funzioni
bool loadQuotes();
bool saveQuotes();
String getQuoteForWeather(String weatherCategory);

// Variabili esterne
extern CustomQuotes customQuotes;

#endif
