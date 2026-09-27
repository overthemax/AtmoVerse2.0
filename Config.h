#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Struttura per la configurazione dell'applicazione
struct Config {
  char ssid[32];          // SSID WiFi
  char password[64];      // Password WiFi
  char city[32];          // Città per le previsioni meteo
  char api_key[64];       // API key OpenWeatherMap
  char units[8];          // Unità meteo: "metric" o "imperial"
  char language[8];       // Lingua meteo: es. "it", "en"
  long gmtOffset_sec;     // Offset GMT in secondi
  int daylightOffset_sec; // Offset per ora legale in secondi
  char ntpServer[64];     // Server NTP
  bool use24hFormat;      // Formato orario: true=24h, false=12h

  // Impostazioni risparmio energetico
  bool powerSavingEnabled;       // Modalità risparmio energetico attiva
  int powerSavingStartHour;      // Ora di inizio risparmio energetico (es. 22 per le 22:00)
  int powerSavingEndHour;        // Ora di fine risparmio energetico (es. 7 per le 7:00)
  int normalUpdateInterval;      // Intervallo di aggiornamento normale in minuti
  int powerSavingUpdateInterval; // Intervallo di aggiornamento in risparmio energetico in minuti

  // Impostazioni gestione errori di rete
  int maxNetworkRetries;         // Numero massimo di tentativi in caso di errore di rete

  // Impostazioni refresh display (secondi)
  int displayRefreshIntervalSec;            // Intervallo refresh display in modalità normale (sec)
  int displayRefreshIntervalSecPowerSaving; // Intervallo refresh display in modalità risparmio (sec)

  // Batteria
  bool batteryShowOnDisplay;     // Mostra stato batteria su display
};

// Dichiarazione funzioni di gestione configurazione
bool loadConfig();
bool saveConfig();
bool checkConfigValidity();

// Dichiarazione variabili esterne
extern Config config;

#endif // CONFIG_H
