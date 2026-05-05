/**
 * @file Weather.h
 * @brief Funzioni per la gestione dei dati meteorologici
 * 
 * Questo file contiene le dichiarazioni di funzioni per la gestione
 * dei dati meteorologici in AtmoVerse 2.0, utilizzando la struttura
 * WeatherData definita in WeatherUtils.h
 */

#ifndef WEATHER_H
#define WEATHER_H

#include <Arduino.h>
#include <time.h>
#include "WeatherUtils.h"  // Include la definizione di WeatherData

// Variabile globale per i dati meteo (definita in Weather.cpp)
extern WeatherData currentWeather;

/**
 * @brief Ottiene i dati meteo correnti dall'API OpenWeatherMap
 * 
 * Questa funzione si connette a OpenWeatherMap, ottiene i dati meteo
 * per la città configurata e aggiorna la struttura currentWeather.
 * 
 * @return true se i dati sono stati aggiornati con successo, false altrimenti
 */
bool getWeatherData();

/**
 * @brief Verifica se i dati meteo sono validi e aggiornati
 * 
 * @return true se i dati meteo sono validi, false altrimenti
 */
bool isWeatherDataValid();

/**
 * @brief Determina se è giorno o notte in base all'ora attuale
 * 
 * @return true se è notte, false se è giorno
 */
bool isNightTime();

/**
 * @brief Ottiene l'icona corrispondente alle condizioni meteorologiche
 * 
 * @param weatherId ID della condizione meteo (da OpenWeatherMap)
 * @param isNight true se è notte, false se è giorno
 * @return String nome del file dell'icona
 */
String getWeatherIconName(int weatherId, bool isNight);

/**
 * @brief Verifica se è il momento di ritentare un aggiornamento meteo fallito
 * 
 * @return true se è il momento di ritentare, false altrimenti
 */
bool isTimeToRetryWeather();

/**
 * @brief Ottiene informazioni sullo stato dei tentativi di aggiornamento meteo
 * 
 * @return String con informazioni sui tentativi
 */
String getWeatherRetryStatus();

#endif // WEATHER_H
