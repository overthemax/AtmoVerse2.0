#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Struttura per la configurazione dell'applicazione
struct Config {
  char ssid[32];          // SSID WiFi
  char password[64];      // Password WiFi
  char city[32];          // Città per le previsioni meteo
  float lat;              // Latitudine della posizione
  float lon;              // Longitudine della posizione
  char api_key[64];       // API key OpenWeatherMap
  char units[8];          // Unità meteo: "metric" o "imperial"
  char language[8];       // Lingua meteo: es. "it", "en"
  long gmtOffset_sec;     // Offset GMT in secondi
  int daylightOffset_sec; // Offset per ora legale in secondi
  char ntpServer[64];     // Server NTP
  bool use24hFormat;      // Formato orario: true=24h, false=12h
  
  // NOTA: Il tema è stato rimosso - usa /layout.json sulla SD per personalizzare il display
  
  // Impostazioni risparmio energetico
  bool powerSavingEnabled;      // Modalità risparmio energetico attiva
  int powerSavingStartHour;     // Ora di inizio risparmio energetico (es. 22 per le 22:00)
  int powerSavingEndHour;       // Ora di fine risparmio energetico (es. 7 per le 7:00)
  int normalUpdateInterval;     // Intervallo di aggiornamento normale in minuti
  int powerSavingUpdateInterval; // Intervallo di aggiornamento in risparmio energetico in minuti
  
  // Intervalli aggiornamenti meteo (minuti)
  int weatherUpdateInterval;            // Aggiornamento meteo in uso normale (min)
  int weatherPowerSavingUpdateInterval; // Aggiornamento meteo in risparmio (min)
  
  // Impostazioni gestione errori di rete
  int maxNetworkRetries;        // Numero massimo di tentativi in caso di errore di rete
  bool showLastDataOnError;     // Mostra l'ultimo dato disponibile in caso di errore

  // Impostazioni refresh display (secondi)
  int displayRefreshIntervalSec;            // Intervallo refresh display in modalità normale (sec)
  int displayRefreshIntervalSecPowerSaving; // Intervallo refresh display in modalità risparmio (sec)
  int apTimeRefreshIntervalSec;             // Intervallo refresh solo-orario in modalità AP (sec)

  // DEPRECATO: quotePosX e quotePosY rimossi - usa layout.json per posizionare gli elementi
  // Mantieni questi campi per compatibilità ma non sono più utilizzati
  int quotePosX;  // DEPRECATO - non più utilizzato (usa layout.json)
  int quotePosY;  // DEPRECATO - non più utilizzato (usa layout.json)
  
  // Nuove impostazioni avanzate
  
  // Night Mode (rimosso cambio tema automatico - usa layout.json per personalizzare)
  // DEPRECATO: nightModeEnabled, nightModeStartHour, nightModeEndHour non più utilizzati
  // Mantieni questi campi per compatibilità con vecchie configurazioni
  bool nightModeEnabled;          // DEPRECATO - non più utilizzato
  int nightModeStartHour;         // DEPRECATO - non più utilizzato  
  int nightModeEndHour;           // DEPRECATO - non più utilizzato
  
  // Battery Management
  bool batteryMonitorEnabled;     // Abilita monitoraggio batteria
  int batteryADCPin;              // Pin ADC per lettura tensione (es: 34)
  float batteryVoltageDivider;    // Rapporto voltage divider (es: 2.0)
  bool batteryShowOnDisplay;      // Mostra stato batteria su display
  
  // Weather Alerts
  bool alertsEnabled;             // Abilita sistema alert
  float alertTempHigh;            // Soglia temperatura alta (°C)
  float alertTempLow;             // Soglia temperatura bassa (°C)
  float alertWindHigh;            // Soglia vento forte (km/h)
  bool alertRain;                 // Alert per pioggia
  bool alertSnow;                 // Alert per neve
  bool alertStorm;                // Alert per temporale
  bool alertShowOnDisplay;        // Mostra alert su display
  char alertWebhookUrl[128];      // URL webhook per notifiche
  
  // History / Statistics
  bool historyEnabled;            // Abilita raccolta statistiche
  int historyKeepDays;            // Giorni di storia da mantenere
  bool historyShowOnDisplay;      // Mostra stats su display
};

// Dichiarazione funzioni di gestione configurazione
bool loadConfig();
bool saveConfig();
bool createDefaultConfig();
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
