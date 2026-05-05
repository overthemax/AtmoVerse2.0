#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Struttura per la configurazione dell'applicazione
struct Config {
  char ssid[32];          // SSID WiFi
  char password[64];      // Password WiFi
  char city[24];          // Città per le previsioni meteo - ridotto da 32 a 24 byte per risparmiare DRAM
  float lat;              // Latitudine della posizione
  float lon;              // Longitudine della posizione
  char api_key[40];       // API key OpenWeatherMap - ridotto da 64 a 40 byte per risparmiare DRAM
  long gmtOffset_sec;     // Offset GMT in secondi
  int daylightOffset_sec; // Offset per ora legale in secondi
  char ntpServer[40];     // Server NTP - ridotto da 64 a 40 byte per risparmiare DRAM

  // Impostazioni risparmio energetico
  bool powerSavingEnabled;      // Modalità risparmio energetico attiva
  int powerSavingStartHour;     // Ora di inizio risparmio energetico (es. 22 per le 22:00)
  int powerSavingEndHour;       // Ora di fine risparmio energetico (es. 7 per le 7:00)
  int normalUpdateInterval;     // Intervallo di aggiornamento normale in minuti
  int powerSavingUpdateInterval; // Intervallo di aggiornamento in risparmio energetico in minuti

  // Impostazioni gestione errori di rete
  int maxNetworkRetries;        // Numero massimo di tentativi in caso di errore di rete
  bool showLastDataOnError;     // Mostra l'ultimo dato disponibile in caso di errore
};

// Dichiarazione funzioni di gestione configurazione
bool loadConfig();
bool saveConfig();
bool resetConfig();
bool checkConfigValidity();
void logToSD(const char* msg);

// Dichiarazione variabili esterne
extern Config config;

// Costanti per i server NTP predefiniti
const char* const NTP_SERVERS[] = {
    "pool.ntp.org",
    "europe.pool.ntp.org",
    "it.pool.ntp.org",
    "time.google.com",
    "time.windows.com"
};
const int NTP_SERVERS_COUNT = 5;

#endif // CONFIG_H
