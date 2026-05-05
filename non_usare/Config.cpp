#include "Config.h"
#include "Hardware.h"
#include "Debug.h"
#include <SD.h>
#include <SPI.h>
#include <ArduinoJson.h>

// Definizione variabili globali
Config config;

// Constanti in PROGMEM per risparmiare DRAM
// Supporta entrambi i nomi file per compatibilità
const char CONFIG_FILE[] PROGMEM = "/conf.json";
const char CONFIG_FILE_ALT[] PROGMEM = "/config.json";

// Carica la configurazione dalla SD
bool loadConfig() {
  DEBUG_TRACE();
  Serial.println(F("\n====================="));
  Serial.println(F("CARICAMENTO CONFIGURAZIONE"));
  Serial.println(F("====================="));
  
  // STEP 1: Inizializzazione SD card con più tentativi
  Serial.println(F("\nPasso 1: Inizializzazione SD card"));
  
  bool sdInitialized = false;
  const int MAX_SD_ATTEMPTS = 3;
  
  for (int attempt = 1; attempt <= MAX_SD_ATTEMPTS; attempt++) {
    Serial.print(F("Tentativo "));
    Serial.print(attempt);
    Serial.print(F("/"));
    Serial.print(MAX_SD_ATTEMPTS);
    Serial.println(F("..."));
    
    if (SD.begin(SD_CS, sdSPI)) {
      Serial.println(F("✓ SD card inizializzata con successo!"));
      sdInitialized = true;
      break;
    } else {
      if (attempt < MAX_SD_ATTEMPTS) {
        Serial.println(F("✗ Inizializzazione fallita, riprovo..."));
        SD.end();
        delay(500);
      } else {
        Serial.println(F("✗ Tutti i tentativi falliti. Verificare l'hardware."));
      }
    }
  }
  
  if (!sdInitialized) {
    Serial.println(F("### ERRORE CRITICO: SD card non accessibile ###"));
    Serial.println(F("Possibili cause: SD non inserita, connessioni difettose, formato non corretto"));
    return false;
  }
  
  // STEP 2: Elenca files su SD per debug
  Serial.println(F("\nPasso 2: Verifica contenuto SD card"));
  Serial.println(F("File e cartelle presenti:"));
  
  File root = SD.open("/");
  if (!root) {
    Serial.println(F("✗ Impossibile accedere alla directory principale"));
    return false;
  }
  
  if (!root.isDirectory()) {
    Serial.println(F("✗ La root non è una directory"));
    root.close();
    return false;
  }
  
  // Elenca tutti i file in formato chiaro
  int fileCount = 0;
  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;
    
    fileCount++;
    if (entry.isDirectory()) {
      Serial.print("  ");
      Serial.print(entry.name());
      Serial.println("/");
    } else {
      Serial.print("  ");
      Serial.print(entry.name());
      Serial.print(" - ");
      Serial.print(entry.size());
      Serial.println(" bytes");
    }
    entry.close();
  }
  
  root.close();
  
  if (fileCount == 0) {
    Serial.println(F("✗ SD card vuota. Nessun file presente."));
    return false;
  } else {
    Serial.print(F("✓ Trovati "));
    Serial.print(fileCount);
    Serial.println(F(" file/directory"));
  }
  
  // STEP 3: Apertura file configurazione con supporto per nomi multipli
  Serial.println(F("\nPasso 3: Ricerca file configurazione"));
  
  const char* configFileNames[] = {CONFIG_FILE, CONFIG_FILE_ALT, PSTR("/settings.json")};
  const int numConfigFiles = 3;
  File configFile;
  String usedFileName = "";
  
  for (int i = 0; i < numConfigFiles; i++) {
    String fileName = configFileNames[i];
    Serial.print(F("Provo file: "));
    Serial.print(fileName);
    
    configFile = SD.open(fileName, FILE_READ);
    if (configFile) {
      Serial.println(F(" ✓ TROVATO!"));
      usedFileName = fileName;
      break;
    } else {
      Serial.println(F(" ✗ non trovato"));
    }
  }
  
  if (!configFile) {
    Serial.println(F("### ERRORE: Nessun file di configurazione trovato! ###"));
    Serial.println(F("Verificare che almeno uno dei file di configurazione esista sulla SD."));
    return false;
  }
  
  Serial.print(F("✓ Configurazione caricata da: "));
  Serial.println(usedFileName);
  
  // STEP 4: Lettura e parsing del contenuto JSON
  Serial.println(F("\nPasso 4: Lettura e parsing JSON"));
  
  // Verifica la dimensione del file
  size_t size = configFile.size();
  Serial.print(F("Dimensione file: "));
  Serial.print(size);
  Serial.println(F(" bytes"));
  
  if (size > 1024) {
    Serial.println(F("✗ ERRORE: File configurazione troppo grande (>1024 bytes)"));
    configFile.close();
    return false;
  }
  
  // Verifica che il file non sia vuoto
  if (size == 0) {
    Serial.println(F("✗ ERRORE: File configurazione vuoto"));
    configFile.close();
    return false;
  }
  
  // Stampa i primi byte del file per debug dettagliato
  Serial.println(F("Dump esadecimale dei primi 32 byte del file:"));
  Serial.print(F("  "));
  configFile.seek(0); // Riposiziona all'inizio del file
  for (int i = 0; i < min(32, (int)size); i++) {
    byte b = configFile.read();
    if (b < 16) Serial.print("0"); // Padding per numeri < 16
    Serial.print(b, HEX);
    Serial.print(" ");
    if ((i+1) % 8 == 0) Serial.print(" "); // Spazio extra ogni 8 byte
  }
  Serial.println();
  
  // Riposiziona all'inizio
  configFile.seek(0);
  
  // Crea il buffer per il JSON con la giusta dimensione
  // Aggiungiamo un po' di margine per sicurezza
  char jsonBuffer[size + 10];
  memset(jsonBuffer, 0, size + 10); // Inizializza a zero il buffer
  
  // Leggi il file nel buffer
  size_t bytesRead = configFile.readBytes(jsonBuffer, size);
  if (bytesRead != size) {
    Serial.print("✗ ERRORE: Lettura file incompleta. Letti ");
    Serial.print(bytesRead);
    Serial.print(" byte su ");
    Serial.println(size);
    configFile.close();
    return false;
  }
  
  // Aggiungi il terminatore di stringa
  jsonBuffer[bytesRead] = '\0';
  
  // Chiudi il file dopo averlo letto completamente
  configFile.close();
  
  // Verifica la validità dei caratteri nel JSON (cerca caratteri non stampabili)
  bool hasInvalidChars = false;
  for (size_t i = 0; i < bytesRead; i++) {
    char c = jsonBuffer[i];
    // Verifica se ci sono caratteri non stampabili (ad eccezione di spazi, tab, nuova linea)
    if (c != '\r' && c != '\n' && c != '\t' && (c < 32 || c > 126)) {
      Serial.print(F("✗ ATTENZIONE: Carattere non valido trovato alla posizione "));
      Serial.print(i);
      Serial.print(F(" (hex: "));
      Serial.print((uint8_t)c, HEX);
      Serial.println(F("))"));
      hasInvalidChars = true;
      // Sostituiamo il carattere non valido con uno spazio
      jsonBuffer[i] = ' ';
    }
  }
  
  if (hasInvalidChars) {
    Serial.println(F("File di configurazione contiene caratteri non validi che sono stati sostituiti"));
  }
  
  // Stampa il JSON per debug (solo parziale se grande)
  Serial.println(F("Contenuto JSON:"));
  if (size < 200) {
    Serial.println(jsonBuffer);
  } else {
    // Stampa solo i primi 100 caratteri
    char previewBuffer[101];
    strncpy(previewBuffer, jsonBuffer, 100);
    previewBuffer[100] = '\0';
    Serial.print(previewBuffer);
    Serial.println(F("... (troncato)"));
  }
  
  // Deserializza il JSON
  Serial.println(F("Deserializzazione JSON..."));
  DynamicJsonDocument doc(1024); // Ridotto ulteriormente da 1536 a 1024 per risparmiare DRAM
  DeserializationError error = deserializeJson(doc, jsonBuffer);
  Serial.print(F("Dimensione JSON stimata: "));
  Serial.print(doc.memoryUsage());
  Serial.println(F(" bytes"));
  
  if (error) {
    Serial.print(F("✗ ERRORE JSON: "));
    Serial.println(error.c_str());
    Serial.println(F("Verificare la sintassi del file JSON"));
    return false;
  }
  
  Serial.println(F("✓ JSON deserializzato con successo"));
  
  // Copia i valori dal documento JSON alla struttura di configurazione con debug esteso
  Serial.println(F("\nValori letti dal file di configurazione:"));
  
  // Campo SSID
  const char* ssidValue = doc["ssid"] | static_cast<const char*>("");
  strlcpy(config.ssid, ssidValue, sizeof(config.ssid));
  Serial.print(F("SSID: '"));
  Serial.print(ssidValue);
  Serial.println(F("'"));
  
  // Campo password (non mostriamo il valore reale per sicurezza)
  strlcpy(config.password, doc["password"] | "", sizeof(config.password));
  Serial.print(F("Password presente: "));
  Serial.println(strlen(config.password) > 0 ? F("Sì") : F("No"));
  
  // Campo city
  const char* cityValue = doc["city"] | PSTR("Rome");
  strlcpy(config.city, cityValue, sizeof(config.city));
  Serial.print(F("Città: '"));
  Serial.print(cityValue);
  Serial.println(F("'"));
  
  // Coordinate geografiche
  config.lat = doc["lat"] | 41.9028;
  config.lon = doc["lon"] | 12.4964;
  Serial.print(F("Coordinate: "));
  Serial.print(config.lat);
  Serial.print(F(", "));
  Serial.println(config.lon);
  
  // API key di OpenWeatherMap
  const char* apiKeyValue = doc["api_key"] | "";
  strlcpy(config.api_key, apiKeyValue, sizeof(config.api_key));
  Serial.print(F("API Key presente: "));
  Serial.println(strlen(config.api_key) > 0 ? F("Sì") : F("No"));
  
  // Impostazioni di fuso orario
  config.gmtOffset_sec = doc["gmtOffset_sec"] | 3600;
  config.daylightOffset_sec = doc["daylightOffset_sec"] | 3600;
  Serial.print(F("Fuso orario (secondi): "));
  Serial.println(config.gmtOffset_sec);
  
  // Server NTP
  const char* ntpValue = doc["ntpServer"] | PSTR("pool.ntp.org");
  strlcpy(config.ntpServer, ntpValue, sizeof(config.ntpServer));
  Serial.print(F("NTP Server: '"));
  Serial.print(ntpValue);
  Serial.println(F("'"));
  
  // Nuove impostazioni risparmio energetico
  config.powerSavingEnabled = doc["powerSavingEnabled"] | true;
  config.powerSavingStartHour = doc["powerSavingStartHour"] | 22;
  config.powerSavingEndHour = doc["powerSavingEndHour"] | 7;
  config.normalUpdateInterval = doc["normalUpdateInterval"] | 30;
  config.powerSavingUpdateInterval = doc["powerSavingUpdateInterval"] | 120;
  
  // Impostazioni gestione errori
  config.maxNetworkRetries = doc["maxNetworkRetries"] | 3;
  config.showLastDataOnError = doc["showLastDataOnError"] | true;
  
  Serial.println(F("Configurazione caricata con successo"));
  return true;
}

// Salva la configurazione sulla SD
bool saveConfig() {
  DEBUG_TRACE();
  
  Serial.println(F("Salvataggio configurazione..."));
  
  // Verifica che la SD sia inizializzata
  if (!SD.begin(SD_CS, sdSPI)) {
    Serial.println(F("Impossibile inizializzare la SD card per il salvataggio"));
    return false;
  }
  
  // Rimuovi entrambi i file di configurazione se esistono
  if (SD.exists(CONFIG_FILE)) {
    SD.remove(CONFIG_FILE);
  }
  if (SD.exists(CONFIG_FILE_ALT)) {
    SD.remove(CONFIG_FILE_ALT);
  }
  
  // Crea il file di configurazione con il nuovo nome
  File file = SD.open(CONFIG_FILE, FILE_WRITE);
  if (!file) {
    Serial.println(F("Impossibile aprire il file di configurazione per il salvataggio"));
    return false;
  }
  
  // Crea un documento JSON
  StaticJsonDocument<384> doc; // Ridotto ulteriormente da 512 a 384 per minimizzare l'uso della DRAM
  
  // Copia i valori dalla struttura di configurazione al documento JSON
  doc["ssid"] = config.ssid;
  doc["password"] = config.password;
  doc["city"] = config.city;
  doc["lat"] = config.lat;
  doc["lon"] = config.lon;
  doc["api_key"] = config.api_key;
  doc["gmtOffset_sec"] = config.gmtOffset_sec;
  doc["daylightOffset_sec"] = config.daylightOffset_sec;
  doc["ntpServer"] = config.ntpServer;
  
  // Nuove impostazioni risparmio energetico
  doc["powerSavingEnabled"] = config.powerSavingEnabled;
  doc["powerSavingStartHour"] = config.powerSavingStartHour;
  doc["powerSavingEndHour"] = config.powerSavingEndHour;
  doc["normalUpdateInterval"] = config.normalUpdateInterval;
  doc["powerSavingUpdateInterval"] = config.powerSavingUpdateInterval;
  
  // Impostazioni gestione errori
  doc["maxNetworkRetries"] = config.maxNetworkRetries;
  doc["showLastDataOnError"] = config.showLastDataOnError;
  
  // Serializza il JSON nel file
  if (serializeJson(doc, file) == 0) {
    Serial.println(F("Errore durante la serializzazione JSON"));
    file.close();
    return false;
  }
  
  file.close();
  
  // Verifica che il file sia stato scritto correttamente
  if (!SD.exists(CONFIG_FILE)) {
    Serial.println(F("Errore: file non trovato dopo il salvataggio"));
    return false;
  }
  
  Serial.println(F("Configurazione salvata con successo"));
  return true;
}

// Ripristina la configurazione predefinita
bool resetConfig() {
  DEBUG_TRACE();
  
  Serial.println(F("Ripristino configurazione predefinita..."));
  
  // Reimposta tutti i valori ai default
  strcpy(config.ssid, "");
  strcpy(config.password, "");
  strcpy(config.city, "Rome");
  config.lat = 41.9028;
  config.lon = 12.4964;
  strcpy(config.api_key, "");
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

// Verifica che la configurazione sia valida
bool checkConfigValidity() {
  Serial.println(F("\nValidazione configurazione:"));
  bool isValid = true;
  
  // Verifica che i campi essenziali siano valorizzati
  // Validazione SSID (lungezza minima 2 caratteri)
  if (strlen(config.ssid) < 2) {
    Serial.println(F("ERRORE: SSID troppo corto o vuoto"));
    isValid = false;
  } else {
    Serial.print(F("SSID valido: '"));
    Serial.print(config.ssid);
    Serial.println(F("'"));
  }
  
  // Validazione password Wi-Fi (lungezza minima 8 caratteri per sicurezza)
  if (strlen(config.password) < 8) {
    Serial.println(F("AVVISO: Password Wi-Fi troppo corta o vuota"));
    // Non consideriamo questo un errore critico, potrebbe trattarsi di una rete aperta
  }
  
  // Validazione API key OpenWeatherMap (opzionale)
  if (strlen(config.api_key) < 20) {
    Serial.println(F("AVVISO: API key OpenWeatherMap mancante o non valida"));
    Serial.println(F("  Le funzionalità meteo potrebbero non essere disponibili"));
    // Non consideriamo questo un errore critico, permettiamo l'avvio senza API key
  } else {
    Serial.println(F("API key presente e valida"));
  }
  
  // Validazione città (almeno 2 caratteri)
  if (strlen(config.city) < 2) {
    Serial.println(F("ERRORE: Nome città mancante o non valido"));
    isValid = false;
  } else {
    Serial.print(F("Città valida: '"));
    Serial.print(config.city);
    Serial.println(F("'"));
  }
  
  // Verifica delle impostazioni di risparmio energetico
  if (config.powerSavingStartHour < 0 || config.powerSavingStartHour > 23) {
    Serial.println(F("Corretto orario inizio risparmio energetico non valido"));
    config.powerSavingStartHour = 22; // Default
    isValid = false;
  }
  
  if (config.powerSavingEndHour < 0 || config.powerSavingEndHour > 23) {
    Serial.println(F("Corretto orario fine risparmio energetico non valido"));
    config.powerSavingEndHour = 7; // Default
    isValid = false;
  }
  
  if (config.normalUpdateInterval < 5) {
    Serial.println(F("Corretto intervallo aggiornamento normale non valido"));
    config.normalUpdateInterval = 30; // Default
    isValid = false;
  }
  
  if (config.powerSavingUpdateInterval < 10) {
    Serial.println(F("Corretto intervallo aggiornamento risparmio energetico non valido"));
    config.powerSavingUpdateInterval = 120; // Default
    isValid = false;
  }
  
  // Riepilogo della validazione
  Serial.print(F("Risultato validazione: "));
  Serial.println(isValid ? F("Configurazione VALIDA") : F("Configurazione NON VALIDA"));
  
  return isValid;
}
