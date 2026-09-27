#include "Config.h"
#include "AtmoVerseConstants.h"
#include "Hardware.h"
#include "Debug.h"
#include <SD.h>
#include <SPI.h>

#include <ArduinoJson.h>
#include <stdio.h>

// Definizione variabili globali
Config config;
SPIClass sdSPI(HSPI);

// Funzione helper per popolare la struttura config da un JsonDocument
static void populateConfigFromJson(const JsonDocument& doc) {
  strlcpy(config.ssid, doc["ssid"] | "", sizeof(config.ssid));
  strlcpy(config.password, doc["password"] | "", sizeof(config.password));
  strlcpy(config.api_key, doc["api_key"] | "", sizeof(config.api_key));
  strlcpy(config.units, doc["units"] | "metric", sizeof(config.units));
  strlcpy(config.language, doc["language"] | "it", sizeof(config.language));
  strlcpy(config.city, doc["city"] | ATMOVERSE_DEFAULT_CITY, sizeof(config.city));
  config.gmtOffset_sec = doc["gmt_offset"] | ATMOVERSE_DEFAULT_GMT_OFFSET;
  config.daylightOffset_sec = doc["dst_offset"] | ATMOVERSE_DEFAULT_DST_OFFSET;
  strlcpy(config.ntpServer, doc["ntp_server"] | ATMOVERSE_DEFAULT_NTP, sizeof(config.ntpServer));
  config.use24hFormat = doc["use24hFormat"] | true;
  config.displayRefreshIntervalSec = doc["displayRefreshIntervalSec"] | 60;
  config.displayRefreshIntervalSecPowerSaving = doc["displayRefreshIntervalSecPowerSaving"] | 300;

  // Risparmio energetico notturno
  config.powerSavingEnabled = doc["powerSavingEnabled"] | false;
  config.powerSavingStartHour = doc["powerSavingStartHour"] | 22;
  config.powerSavingEndHour = doc["powerSavingEndHour"] | 7;
  
  // Intervalli critici con fallback garantiti
  int normalInterval = doc["normalUpdateInterval"] | 0;
  if (normalInterval <= 0) {
    // Configurazioni vecchie: intervallo nel campo weatherUpdateInterval
    int legacyInterval = doc["weatherUpdateInterval"] | 0;
    config.normalUpdateInterval = legacyInterval > 0 ? legacyInterval : 30;
  } else {
    config.normalUpdateInterval = normalInterval;
  }

  int powerSavingInterval = doc["powerSavingUpdateInterval"] | 0;
  if (powerSavingInterval <= 0) {
    config.powerSavingUpdateInterval = 120;
  } else {
    config.powerSavingUpdateInterval = powerSavingInterval;
  }
  
  int maxRetries = doc["maxNetworkRetries"] | 0;
  if (maxRetries <= 0) {
    config.maxNetworkRetries = 3;
  } else {
    config.maxNetworkRetries = maxRetries;
  }
  
  // Battery Management
  config.batteryShowOnDisplay = doc["batteryShowOnDisplay"] | true;
}

// Percorso del file di configurazione sulla SD (percorso assoluto per ESP32)
const char* CONFIG_FILE = "/conf.json";

// Crea un file di configurazione di default
bool createDefaultConfig() {
  
  // Inizializza SD se non già fatto
  if (!initSD()) {
    return false;
  }
  
  // Crea documento JSON con configurazione di default
  DynamicJsonDocument doc(2048);  // Buffer adeguato per configurazione completa
  
  // Parametri base (vuoti per setup)
  doc["ssid"] = "";
  doc["password"] = "";
  doc["city"] = "Rome";
  doc["api_key"] = "";
  
  // Parametri con default sensati
  doc["units"] = "metric";
  doc["language"] = "it";
  doc["gmt_offset"] = 3600;
  doc["dst_offset"] = 3600;
  doc["ntp_server"] = "pool.ntp.org";
  doc["use24hFormat"] = true;
  
  // Intervalli aggiornamento
  doc["displayRefreshIntervalSec"] = 60;
  doc["displayRefreshIntervalSecPowerSaving"] = 300;
  
  // Power Saving
  doc["powerSavingEnabled"] = false;
  doc["powerSavingStartHour"] = 22;
  doc["powerSavingEndHour"] = 7;
  doc["normalUpdateInterval"] = 30;
  doc["powerSavingUpdateInterval"] = 120;
  
  // Rete
  doc["maxNetworkRetries"] = 3;
  
  // Batteria (misurata dall'INA219, se presente)
  doc["batteryShowOnDisplay"] = true;
  
  // Scrivi il file
  File file = SD.open("/conf.json", FILE_WRITE);
  if (!file) {
    return false;
  }
  
  if (serializeJson(doc, file) == 0) {
    file.close();
    return false;
  }
  
  file.close();
  return true;
}

// Carica la configurazione
bool loadConfig() {
  DEBUG_TRACE();
  
  bool success = false;
  
  // Lista di possibili percorsi da provare
  const char* possiblePaths[] = {
    CONFIG_FILE,         // "conf.json" 
    "/conf.json",        // "/conf.json"
    "/config.json",      // "/config.json"
    "config.json"        // "config.json"
  };
  
  const int numPaths = sizeof(possiblePaths) / sizeof(possiblePaths[0]);
  
  // Inizializzazione SD centralizzata
  
  bool sdInitialized = initSD();
  
  if (!sdInitialized) {
    return false;
  }
  
  // Lista i file nella root per diagnostica
  File root = SD.open("/");
  if (root) {
    File entry = root.openNextFile();
    int fileCount = 0;
    while (entry) {
      fileCount++;
      entry = root.openNextFile();
    }
    if (fileCount == 0) {
    }
    root.close();
  } else {
  }
  File file;
  const char* usedPath = nullptr;
  
  // Prova tutti i possibili percorsi
  for (int i = 0; i < numPaths; i++) {
    if (SD.exists(possiblePaths[i])) {
      file = SD.open(possiblePaths[i], FILE_READ);
      if (file) {
        usedPath = possiblePaths[i];
        break;
      } else {
      }
    } else {
    }
  }
  
  // Se nessun file trovato, crea conf.json di default
  if (!file) {
    if (createDefaultConfig()) {
      // Prova a caricare il file appena creato
      file = SD.open("/conf.json", FILE_READ);
      if (!file) {
        return false;
      }
    } else {
      return false;
    }
  }
  
  if (file) {
    // Utilizza un buffer JSON più grande per configurazione completa
    DynamicJsonDocument doc(2048);  // Sufficiente per conf.json con tutti i parametri
    
    // Ottieni la dimensione del file
    size_t fileSize = file.size();
    
    // Leggi il contenuto del file in un buffer
    String fileContent = file.readString();
    file.close();  // Chiudi subito il file dopo averlo letto
    
    // Deserializza direttamente dalla stringa (più affidabile)
    DeserializationError error = deserializeJson(doc, fileContent);
    
    if (!error) {
      
      // Popola la configurazione usando la funzione helper
      populateConfigFromJson(doc);
      
      success = true;
    } else {
    }
  } else {
  }

  // Se il caricamento è fallito, usa configurazione predefinita
  if (!success) {
    strcpy(config.ssid, "");
    strcpy(config.password, "");
    strcpy(config.api_key, "");
    strcpy(config.units, "metric");
    strcpy(config.language, "it");
    strcpy(config.city, ATMOVERSE_DEFAULT_CITY);
    config.gmtOffset_sec = ATMOVERSE_DEFAULT_GMT_OFFSET;
    config.daylightOffset_sec = ATMOVERSE_DEFAULT_DST_OFFSET;
    strcpy(config.ntpServer, ATMOVERSE_DEFAULT_NTP);
    config.use24hFormat = true;
    // Default intervalli meteo
    // Default refresh display (secondi)
    config.displayRefreshIntervalSec = 60;
    config.displayRefreshIntervalSecPowerSaving = 300;
  }
  
  if (!checkConfigValidity()) {
    return false;
  }
  return success;
}

// Controlla che i campi fondamentali della config siano validi
bool checkConfigValidity() {
  DEBUG_TRACE();
  
  // Verifico la presenza di SSID
  if (strlen(config.ssid) == 0) {
    return false;
  }
  
  // Verifico la presenza della password (la maggior parte delle reti WiFi richiede una password)
  // NOTA: reti aperte hanno password vuota, ma sono rare - per sicurezza richiediamo password
  if (strlen(config.password) == 0) {
    return false;
  }
  
  // Avviso se API_KEY è vuota ma non blocco la validazione
  if (strlen(config.api_key) == 0) {
  }
  
  return true;
}

// Salva la configurazione
bool saveConfig() {
  DEBUG_TRACE();
  
  bool success = false;
  
  // Test validità SSID prima del salvataggio
  
  // Prova a inizializzare la SD card tramite funzione centralizzata
  bool sdInitialized = initSD();
  
  // Se fallisce, facciamo un piccolo retry locale (potrebbe non servire con initSD statico, ma per sicurezza)
  if (!sdInitialized) {
    delay(500);
    sdInitialized = initSD();
  }
  
  if (sdInitialized) {
    
    // Verifica se esiste già il file e lo cancella prima di scrivere
    if (SD.exists(CONFIG_FILE)) {
      SD.remove(CONFIG_FILE);
      delay(100); // Attendi che il sistema di file si aggiorni
    }
    
    delay(100); // Attendi per sicurezza
    
    // Apri il file in scrittura (percorso già assoluto)
    File file = SD.open(CONFIG_FILE, FILE_WRITE);
    
    if (file) {
      // Crea un buffer JSON per tutti i campi (aumentato per i nuovi campi)
      // Usa heap per evitare stack overflow
      DynamicJsonDocument* doc = new DynamicJsonDocument(JSON_BUFFER_LARGE);
      
      // Copia i valori dalla struttura config
      (*doc)["ssid"] = config.ssid;
      (*doc)["password"] = config.password;
      (*doc)["api_key"] = config.api_key;
      (*doc)["units"] = config.units;
      (*doc)["language"] = config.language;
      (*doc)["city"] = config.city;
      (*doc)["gmt_offset"] = config.gmtOffset_sec;
      (*doc)["dst_offset"] = config.daylightOffset_sec;
      (*doc)["ntp_server"] = config.ntpServer;
      (*doc)["use24hFormat"] = config.use24hFormat;
      // Intervalli refresh display (secondi)
      (*doc)["displayRefreshIntervalSec"] = config.displayRefreshIntervalSec;
      (*doc)["displayRefreshIntervalSecPowerSaving"] = config.displayRefreshIntervalSecPowerSaving;
      
      (*doc)["powerSavingEnabled"] = config.powerSavingEnabled;
      (*doc)["powerSavingStartHour"] = config.powerSavingStartHour;
      (*doc)["powerSavingEndHour"] = config.powerSavingEndHour;
      (*doc)["normalUpdateInterval"] = config.normalUpdateInterval;
      (*doc)["powerSavingUpdateInterval"] = config.powerSavingUpdateInterval;
      (*doc)["maxNetworkRetries"] = config.maxNetworkRetries;
      
      // Battery Management
      (*doc)["batteryShowOnDisplay"] = config.batteryShowOnDisplay;
      
      // Serializza il JSON nel file
      
      // Prova a serializzare prima in una stringa per debug
      String jsonString;
      serializeJson(*doc, jsonString);
      
      // Ora serializza nel file
      size_t bytesWritten = serializeJson(*doc, file);
      
      delete doc; // Libera memoria
      
      if (bytesWritten > 0) {
        success = true;
        
        // Flush e chiusura del file per assicurare che i dati siano scritti
        file.flush();
        delay(100); // Breve attesa dopo il flush
        file.close();
        delay(100); // Breve attesa dopo la chiusura
        
        // Attesa per sincronizzazione filesystem
        delay(500);
        
        // Verifica l'esistenza del file
        if (SD.exists(CONFIG_FILE)) {
          success = true;
        } else {
          // Se abbiamo scritto i byte, consideriamo l'operazione un successo
          // (potrebbe essere un problema di cache del filesystem)
          if (bytesWritten > 0) {
            success = true;
          } else {
            success = false;
          }
        }
      } else {
        file.close();
      }
    } else {
    }
  } else {
  }
  
  return success;
}
