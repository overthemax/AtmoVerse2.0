/*
 * AtmoVerse 2.0 - Dichiarazioni Server Web
 * 
 * Questo file contiene le dichiarazioni delle funzioni e delle variabili necessarie
 * per il funzionamento del server web. Il server web gestisce le richieste HTTP,
 * inclusa la configurazione delle impostazioni e la visualizzazione dei dati meteo.
 * 
 * Ottimizzato per l'ESP32 con particolare attenzione alla gestione della memoria.
 */

#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <DNSServer.h>
#include "config.h"
#include "weather.h"

// Removed WeatherData definition to avoid redefinition

// Prototipi delle funzioni
void setupWebServer();  // Configura il server web
void startWebServer();  // Avvia il server web
void startAPMode();  // Avvia la modalità Access Point
String getContentType(String filename);  // Restituisce il tipo di contenuto di un file
bool handleFileRead(String path);  // Gestisce la lettura di un file

// Gestori di richieste HTTP
void handleRoot(AsyncWebServerRequest *request);  // Gestisce la richiesta della pagina principale
void handleSettings(AsyncWebServerRequest *request);  // Gestisce la richiesta della pagina delle impostazioni
void handleQuotes(AsyncWebServerRequest *request);  // Gestisce la richiesta della pagina delle citazioni
void handleSaveSettings(AsyncWebServerRequest *request);  // Gestisce il salvataggio delle impostazioni
void handleSaveQuotes(AsyncWebServerRequest *request);  // Gestisce il salvataggio delle citazioni

// Funzioni di configurazione
void saveConfig();  // Salva la configurazione
void loadConfig();  // Carica la configurazione

// Dichiarazione di variabili esterne
extern bool inAPMode;  // Indica se il dispositivo è in modalità Access Point
extern AsyncWebServer server;  // Server web asincrono
extern DNSServer dnsServer;  // Server DNS
extern WeatherData currentWeather;  // Dati meteo attuali

#endif // WEBSERVER_H
