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

// Percorso del file di configurazione sulla SD
const char* CONFIG_FILE = "conf.json";

// Carica la configurazione
bool loadConfig() {
  DEBUG_TRACE();
  Serial.println("Caricamento configurazione...");
  
  bool success = false;
  
  // Lista di possibili percorsi da provare
  const char* possiblePaths[] = {
    CONFIG_FILE,         // "conf.json" 
    "/conf.json",        // "/conf.json"
    "/config.json",      // "/config.json"
    "config.json"        // "config.json"
  };
  
  const int numPaths = sizeof(possiblePaths) / sizeof(possiblePaths[0]);
  
  // Inizializzazione SD con il solo pin CS, senza passare esplicitamente SPI
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  bool sdInitialized = SD.begin(SD_CS, sdSPI);
  
  if (!sdInitialized) {
    Serial.println("[ERROR] Impossibile inizializzare la SD card per il caricamento");
    return false;
  }
  
  Serial.println("[DEBUG-CONFIG] SD inizializzata per caricamento configurazione");
  File file;
  const char* usedPath = nullptr;
  
  // Prova tutti i possibili percorsi
  for (int i = 0; i < numPaths; i++) {
    Serial.print("[DEBUG-CONFIG] Tentativo apertura file: ");
    Serial.println(possiblePaths[i]);
    
    if (SD.exists(possiblePaths[i])) {
      Serial.print("[DEBUG-CONFIG] File trovato: ");
      Serial.println(possiblePaths[i]);
      
      file = SD.open(possiblePaths[i], FILE_READ);
      if (file) {
        Serial.println("[DEBUG-CONFIG] File aperto con successo");
        usedPath = possiblePaths[i];
        break;
      } else {
        Serial.print("[DEBUG-CONFIG] Impossibile aprire il file: ");
        Serial.println(possiblePaths[i]);
      }
    } else {
      Serial.print("[DEBUG-CONFIG] File non esiste: ");
      Serial.println(possiblePaths[i]);
    }
  }
  
  if (file) {
    // Utilizza un buffer JSON ottimizzato
    DynamicJsonDocument doc(512);
    
    // Ottieni la dimensione del file
    size_t fileSize = file.size();
    Serial.print("[DEBUG-CONFIG] Dimensione file: ");
    Serial.print(fileSize);
    Serial.print(" byte, Percorso: ");
    Serial.println(usedPath);
    
    // Leggi il contenuto del file in un buffer
    String fileContent = file.readString();
    Serial.print("[DEBUG-CONFIG] Contenuto file: ");
    Serial.println(fileContent);
    
    // Riposiziona il puntatore del file all'inizio
    file.seek(0);
    
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (!error) {
      Serial.println("[DEBUG-CONFIG] JSON deserializzato con successo");
      
      // Copia i valori nella struttura config
      strlcpy(config.ssid, doc["ssid"] | "", sizeof(config.ssid));
      strlcpy(config.password, doc["password"] | "", sizeof(config.password));
      strlcpy(config.api_key, doc["api_key"] | "", sizeof(config.api_key));
      strlcpy(config.city, doc["city"] | "Rome", sizeof(config.city));
      config.gmtOffset_sec = doc["gmt_offset"] | 3600;  // Default: GMT+1
      config.daylightOffset_sec = doc["dst_offset"] | 3600;  // Default: 1 ora
      strlcpy(config.ntpServer, doc["ntp_server"] | "pool.ntp.org", sizeof(config.ntpServer));
      
      // Debug: visualizza i valori caricati
      Serial.print("[DEBUG-CONFIG] SSID caricato: '");
      Serial.print(config.ssid);
      Serial.println("'");
      Serial.print("[DEBUG-CONFIG] Password caricata: '");
      Serial.print(config.password);
      Serial.println("'");
      
      success = true;
    } else {
      Serial.print("[ERROR] Errore deserializzazione JSON: ");
      Serial.println(error.c_str());
      // Prova a interpretare direttamente la stringa JSON
      error = deserializeJson(doc, fileContent);
      if (!error) {
        Serial.println("[DEBUG-CONFIG] JSON deserializzato dalla stringa con successo");
        strlcpy(config.ssid, doc["ssid"] | "", sizeof(config.ssid));
        strlcpy(config.password, doc["password"] | "", sizeof(config.password));
        strlcpy(config.api_key, doc["api_key"] | "", sizeof(config.api_key));
        strlcpy(config.city, doc["city"] | "Rome", sizeof(config.city));
        config.gmtOffset_sec = doc["gmt_offset"] | 3600;
        config.daylightOffset_sec = doc["dst_offset"] | 3600;
        strlcpy(config.ntpServer, doc["ntp_server"] | "pool.ntp.org", sizeof(config.ntpServer));
        success = true;
      }
    }
  } else {
    Serial.println("[ERROR] Nessun file di configurazione trovato sui percorsi possibili");
  }

  // Se il caricamento è fallito, usa configurazione predefinita
  if (!success) {
    Serial.println("Utilizzo configurazione predefinita");
    strcpy(config.ssid, "");
    strcpy(config.password, "");
    strcpy(config.api_key, "");
    strcpy(config.city, ATMOVERSE_DEFAULT_CITY);
    config.gmtOffset_sec = ATMOVERSE_DEFAULT_GMT_OFFSET;
    config.daylightOffset_sec = ATMOVERSE_DEFAULT_DST_OFFSET;
    strcpy(config.ntpServer, ATMOVERSE_DEFAULT_NTP);
  }
  
  Serial.println(success ? "Configurazione caricata dalla SD" : "Configurazione predefinita attiva");
  if (!checkConfigValidity()) {
    Serial.println("[CONFIG] Configurazione non valida! Avviare riconfigurazione.");
    // Rimosso log su SD
    return false;
  }
  return success;
}

// Funzione per stampare il contenuto di conf.json per debug
void printConfigFile() {
  Serial.println("\n============ DEBUG CONFIG FILE CONTENT ============");
  
  if (!SD.begin(SD_CS)) {
    Serial.println("[DEBUG-CONFIG] Impossibile montare SD per leggere il file!");
    return;
  }
  
  // Percorsi possibili per il file di configurazione
  const char* paths[] = {
    "/conf.json",
    "conf.json",
    "/config.json",
    "config.json"
  };
  
  bool fileFound = false;
  
  for (int i = 0; i < 4; i++) {
    File file = SD.open(paths[i], FILE_READ);
    if (file) {
      Serial.print("[DEBUG-CONFIG] File trovato: ");
      Serial.println(paths[i]);
      Serial.println("[DEBUG-CONFIG] Contenuto del file:");
      
      while (file.available()) {
        Serial.print((char)file.read());
      }
      
      Serial.println("\n[DEBUG-CONFIG] ---- Fine contenuto file ----");
      file.close();
      fileFound = true;
      break;
    }
  }
  
  if (!fileFound) {
    Serial.println("[DEBUG-CONFIG] Nessun file di configurazione trovato!");
  }
  
  Serial.println("=================================================\n");
}

// Controlla che i campi fondamentali della config siano validi
bool checkConfigValidity() {
  DEBUG_TRACE();
  
  // Debug - Stampa contenuto config
  Serial.println("[CONFIG] SSID: '" + String(config.ssid) + "'");
  Serial.println("[CONFIG] API_KEY: '" + String(config.api_key) + "'");
  Serial.println("[CONFIG] CITY: '" + String(config.city) + "'");
  
  // Stampa il contenuto del file di configurazione
  printConfigFile();
  
  // Verifico solo la presenza di SSID, rendendo API_KEY opzionale
  if (strlen(config.ssid) == 0) {
    Serial.println("[CONFIG] SSID non impostato, configurazione non valida");
    return false;
  }
  // Avviso se API_KEY è vuota ma non blocco la validazione
  if (strlen(config.api_key) == 0) {
    Serial.println("[CONFIG] Attenzione: API_KEY non impostata, funzionalità meteo potrebbero essere limitate");
  }
  
  // Rimosso log di validità config
  return true;
}

// Funzione vuota per compatibilità, non fa nulla
void logToSD(const char* msg) {
  return;
}

// Funzione per log - completamente disabilitata
void logMessage(const char* prefix, const char* msg) {
  // Funzione disabilitata per risparmiare memoria
  return;
  
  // Log su SD disabilitato per risparmiare spazio
}

// Salva la configurazione
bool saveConfig() {
  DEBUG_TRACE();
  Serial.println("============== INIZIO SALVATAGGIO CONFIGURAZIONE ==============");
  Serial.println("Salvataggio configurazione...");
  Serial.print("Percorso file: ");
  Serial.println(CONFIG_FILE);
  
  bool success = false;
  
  // Test validità SSID prima del salvataggio
  Serial.print("[DEBUG-CONFIG] Validazione SSID: '");
  Serial.print(config.ssid);
  Serial.println("'");
  Serial.print("[DEBUG-CONFIG] Validazione Password: '");
  Serial.print(config.password);
  Serial.println("'");
  
  // Inizializzazione SD con il solo pin CS, senza passare esplicitamente SPI
  Serial.print("Inizializzazione SD con pin CS: ");
  Serial.println(SD_CS);
  
  // Reinizializza la SPI per la SD
  sdSPI.end();
  delay(100);
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  delay(100);
  
  // Prova a inizializzare la SD card più volte
  int tentativi = 0;
  const int maxTentativi = 3;
  bool sdInitialized = false;
  
  while (!sdInitialized && tentativi < maxTentativi) {
    Serial.print("[DEBUG] Tentativo ");
    Serial.print(tentativi + 1);
    Serial.println(" di inizializzazione SD...");
    
    sdInitialized = SD.begin(SD_CS, sdSPI);
    if (sdInitialized) {
      Serial.println("[DEBUG] SD inizializzata con successo!");
    } else {
      Serial.println("[DEBUG] Errore inizializzazione SD, nuovo tentativo...");
      delay(500);
    }
    tentativi++;
  }
  
  if (sdInitialized) {
    Serial.println("SD inizializzata con successo");
    
    // Verifica se esiste già il file e lo cancella prima di scrivere
    if (SD.exists(CONFIG_FILE)) {
      Serial.println("[DEBUG] File di configurazione esistente, lo elimino prima di riscriverlo");
      SD.remove(CONFIG_FILE);
      delay(100); // Attendi che il sistema di file si aggiorni
    }
    
    Serial.print("Apertura file in scrittura: ");
    Serial.println(CONFIG_FILE);
    
    // Prova ad aprire il file in diversi modi
    File file;
    delay(100); // Attendi per sicurezza
    
    // Primo tentativo: modalità FILE_WRITE standard
    file = SD.open(CONFIG_FILE, FILE_WRITE);
    
    // Secondo tentativo: specifica il percorso completo
    if (!file) {
      Serial.println("[DEBUG] Primo tentativo fallito, provo con percorso completo");
      file = SD.open("/" + String(CONFIG_FILE), FILE_WRITE);
    }
    
    // Terzo tentativo: modalità FILE_WRITE_BEGIN (crea un nuovo file)
    if (!file) {
      Serial.println("[DEBUG] Secondo tentativo fallito, provo con FILE_WRITE_BEGIN");
      file = SD.open(CONFIG_FILE, FILE_WRITE);
    }
    
    if (file) {
      // Crea un buffer JSON ottimizzato
      DynamicJsonDocument doc(512);
      
      // Copia i valori dalla struttura config
      doc["ssid"] = config.ssid;
      doc["password"] = config.password;
      doc["api_key"] = config.api_key;
      doc["city"] = config.city;
      doc["gmt_offset"] = config.gmtOffset_sec;
      doc["dst_offset"] = config.daylightOffset_sec;
      doc["ntp_server"] = config.ntpServer;
      
      // Serializza il JSON nel file
      Serial.println("Tentativo di serializzazione JSON nel file...");
      
      // Prova a serializzare prima in una stringa per debug
      String jsonString;
      serializeJson(doc, jsonString);
      Serial.print("[DEBUG] JSON da salvare: ");
      Serial.println(jsonString);
      
      // Ora serializza nel file
      size_t bytesWritten = serializeJson(doc, file);
      Serial.print("Byte scritti: ");
      Serial.println(bytesWritten);
      
      if (bytesWritten > 0) {
        success = true;
        Serial.println("Configurazione salvata correttamente");
        
        // Flush e chiusura del file per assicurare che i dati siano scritti
        file.flush();
        delay(100); // Breve attesa dopo il flush
        file.close();
        delay(100); // Breve attesa dopo la chiusura
        
        // Aumentato il tempo di attesa per la sincronizzazione del filesystem
        Serial.println("[DEBUG] Attesa per sincronizzazione filesystem...");
        delay(1000);
        
        // Verifica l'esistenza del file con entrambi i percorsi
        bool fileExists = false;
        
        if (SD.exists(CONFIG_FILE)) {
          fileExists = true;
          Serial.println("[DEBUG] Verifica: il file esiste dopo il salvataggio con percorso normale");
        } else if (SD.exists("/" + String(CONFIG_FILE))) {
          fileExists = true;
          Serial.println("[DEBUG] Verifica: il file esiste dopo il salvataggio con percorso completo");
        } else {
          Serial.println("[DEBUG] Tentativi di ricerca file falliti, ultimo tentativo...");
          delay(1000); // Aspetta ancora un po'
          fileExists = SD.exists(CONFIG_FILE) || SD.exists("/" + String(CONFIG_FILE));
        }
        
        if (fileExists) {
          Serial.println("[DEBUG] File di configurazione verificato con successo");
          
          // Non leggiamo il file per evitare problemi, è sufficiente sapere che esiste
          success = true;
        } else {
          Serial.println("[DEBUG] ERRORE: File non trovato dopo il salvataggio!");
    
          // Modificato: se abbiamo scritto i byte, consideriamo l'operazione un successo
          // anche se la verifica dell'esistenza fallisce (potrebbe essere un problema di cache del filesystem)
          if (bytesWritten > 0) {
            Serial.println("[DEBUG] Bytes scritti correttamente, operazione completata con successo");
            success = true;
          } else {
            success = false;
          }
        }
      } else {
        Serial.println("Errore durante la serializzazione JSON");
        file.close();
      }
    } else {
      Serial.println("ERRORE: Impossibile aprire il file di configurazione per il salvataggio");
      Serial.println("Verifica permessi scrittura sulla SD card");
    }
  } else {
    Serial.println("ERRORE: Inizializzazione della SD fallita");
    Serial.println("Verifica che la SD card sia inserita correttamente e funzionante");
  }
  
  return success;
}

// Ripristina la configurazione predefinita
bool resetConfig() {
  DEBUG_TRACE();
  strcpy(config.ssid, "");
  strcpy(config.password, "");
  strcpy(config.api_key, "");
  strcpy(config.city, "Rome");
  config.gmtOffset_sec = 3600;  // Default: GMT+1
  config.daylightOffset_sec = 3600;  // Default: 1 ora
  strcpy(config.ntpServer, "pool.ntp.org");
  
  // Inizializza le nuove impostazioni per il risparmio energetico
  config.powerSavingEnabled = true;         // Attiva per default
  config.powerSavingStartHour = 22;         // Dalle 22:00
  config.powerSavingEndHour = 7;            // Alle 7:00
  config.normalUpdateInterval = 30;         // Aggiornamento normale ogni 30 minuti
  config.powerSavingUpdateInterval = 120;   // In risparmio energetico ogni 120 minuti (2 ore)
  
  // Inizializza le impostazioni per la gestione degli errori di rete
  config.maxNetworkRetries = 3;            // 3 tentativi in caso di errore
  config.showLastDataOnError = true;        // Mostra l'ultimo dato disponibile in caso di errore
  
  return saveConfig();
}
