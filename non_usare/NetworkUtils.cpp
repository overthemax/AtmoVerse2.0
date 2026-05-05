#include "NetworkUtils.h"
#include "Config.h"
#include "WebServer.h"
#include "WebUIPages.h"
#include "Debug.h"
#include "DebugUtils.h"
#include "TimerUtils.h"
#include "SystemMonitor.h"
#include <WiFi.h>
#include <SD.h>
#include <time.h>
#include <DNSServer.h>
// Rimosso esp_task_wdt.h per risparmiare memoria

// Istanze globali
DNSServer dnsServer;
bool apMode = false;

// Costanti
#include "AtmoVerseConstants.h"
// const char* AP_SSID = "AtmoVerse_Setup"; // ora definito in AtmoVerseConstants.h
// const char* AP_PASSWORD = "atmoverse"; // ora definito in AtmoVerseConstants.h
const byte DNS_PORT = 53;

// Configurazione del server NTP
void setupTimeServer() {
  DEBUG_TRACE();
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
}

// Avvia il punto di accesso
void startAccessPoint(bool forceStart) {
  DEBUG_TRACE();
  // Se siamo già in modalità AP, non fare nulla
  if (apMode && !forceStart) {
    // Rimozione stampe debug
    return;
  }

  // Rimozione stampe debug

  // Tenta di caricare la configurazione
  if (!loadConfig()) {
    // Rimozione stampe debug
  }

  // Genera SSID con ultimi 4 caratteri del MAC
  char macAddress[18]; // MAC address formato XX:XX:XX:XX:XX:XX + null terminator
  char lastFourMac[5]; // Ultimi 4 caratteri del MAC + null terminator
  char apSSID[32]; // Buffer per SSID dell'AP
  
  // Ottieni il MAC address come stringa
  snprintf(macAddress, sizeof(macAddress), "%s", WiFi.macAddress().c_str());
  
  // Estrai gli ultimi 4 caratteri (ignorando i :)
  int macLen = strlen(macAddress);
  int j = 0;
  for (int i = macLen - 5; i < macLen; i++) {
    if (macAddress[i] != ':') {
      lastFourMac[j++] = macAddress[i];
    }
  }
  lastFourMac[j] = '\0';
  
  // Genera l'SSID dell'AP
  snprintf(apSSID, sizeof(apSSID), "%s_%s", ATMOVERSE_AP_SSID, lastFourMac);
  // Rimozione stampe debug

  // Configura la rete dell'AP (IP 192.168.4.1)
  IPAddress localIP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);

  // Imposta modalità AP esplicita per evitare stati incoerenti
  WiFi.mode(WIFI_AP);
  
  // Utilizzo Timer per attesa non bloccante invece di delay
  static Timer apSetupTimer(100, false); // 100ms
  apSetupTimer.reset();
  apSetupTimer.enable();
  
  // Nota: in una implementazione completa, questo dovrebbe essere gestito in maniera asincrona
  // con una macchina a stati. Per ora mantengo l'approccio bloccante ma con la nuova API
  while (!apSetupTimer.isReady()) { yield(); } // Equivalente a delay(100) ma consente yield
  
  WiFi.softAPConfig(localIP, gateway, subnet);

  // Avvia l'AP
  bool success = WiFi.softAP(apSSID, ATMOVERSE_AP_PASSWORD);
  
  // Attesa stabilizzazione AP
  static Timer apStabilizeTimer(500, false); // 500ms
  apStabilizeTimer.reset();
  apStabilizeTimer.enable();
  while (!apStabilizeTimer.isReady()) { yield(); } // Consente a ESP32 di gestire altri task

  if (!success) {
    DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_ERROR, "Impossibile avviare l'AP");
    SystemMonitor::recordError(COMPONENT_WIFI, "Fallito avvio Access Point");
    return;
  }

  // Avvia il server DNS captive portal
  IPAddress apIP = WiFi.softAPIP();
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  bool dnsStarted = dnsServer.start(DNS_PORT, "*", apIP);
  
  if (!dnsStarted) {
    DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_WARNING, "Avvio DNS captive portal fallito");
  } else {
    DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_INFO, "DNS captive portal avviato su IP: %s", apIP.toString().c_str());
  }

  apMode = true;
  SystemMonitor::recordSuccess(COMPONENT_WIFI); // AP avviato con successo
  SystemMonitor::recordSuccess(COMPONENT_WEB_SERVER); // Server AP avviato

  // Mostra informazioni AP sul display
  displayAPInfo(apSSID, ATMOVERSE_AP_PASSWORD);

  // AVVIA IL SERVER WEB anche in AP
  setupServer();
  
  // Log completo
  DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_INFO, "Access Point avviato - SSID: %s - IP: %s", 
           apSSID, apIP.toString().c_str());
}

// Verifica se è connesso a WiFi
bool isWiFiConnected() {
  DEBUG_TRACE();
  return WiFi.status() == WL_CONNECTED;
}

// Verifica se è necessario riconnettersi al WiFi
bool reconnectIfNeeded() {
  DEBUG_TRACE();
  if (WiFi.status() != WL_CONNECTED) {
    // Rimozione stampe debug
    return reconnectToWiFi();
  }
  return true;  // Già connesso
}

// Tenta di riconnettersi alla rete WiFi salvata
bool reconnectToWiFi() {
  DEBUG_TRACE();
  if (!loadConfig()) {
    // Rimozione stampe debug
    return false;
  }

  // Utilizzo della variabile globale config definita in Config.cpp
  return connectToWiFi(config.ssid, config.password);
}

// Connessione a una rete WiFi specifica - versione robusta con protezione anti-crash e risoluzione problemi
bool connectToWiFi(const char* ssid, const char* password, uint32_t timeoutMs) {
  DEBUG_TRACE();
  if (strlen(ssid) == 0) {
    Serial.println("ERRORE: SSID vuoto, impossibile connettersi");
    return false;
  }

  // Reset completo del WiFi per evitare stati incoerenti
  Serial.println("Eseguo reset completo del WiFi...");
  WiFi.disconnect(true);   // Disconnetti da eventuali reti
  WiFi.persistent(false);  // Evita scritture flash che possono causare problemi
  
  // Se siamo in modalità AP, chiudi completamente l'AP
  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    Serial.println("- Chiusura della modalità AP");

    // Chiudi il server DNS se attivo
    if (apMode) {
      dnsServer.stop();
      apMode = false;
    }

    // Passa da AP a STA con procedura robusta
    WiFi.softAPdisconnect(true);
    
    // Utilizzo Timer invece di delay(500)
    static Timer apDisconnectTimer(500, false);
    apDisconnectTimer.reset();
    apDisconnectTimer.enable();
    while (!apDisconnectTimer.isReady()) { yield(); }
  }
  
  // Spegni completamente il WiFi e aspetta
  WiFi.mode(WIFI_OFF);
  
  // Utilizzo Timer invece di delay(1000)
  static Timer wifiOffTimer(1000, false);
  wifiOffTimer.reset();
  wifiOffTimer.enable();
  while (!wifiOffTimer.isReady()) { yield(); } // Attesa per assicurare lo spegnimento completo
  
  // Imposta la modalità station e configura
  WiFi.mode(WIFI_STA);
  
  // Utilizzo Timer invece di delay(500)
  static Timer staSetupTimer(500, false);
  staSetupTimer.reset();
  staSetupTimer.enable();
  while (!staSetupTimer.isReady()) { yield(); }
  
  // Configurazioni ottimizzate per connessione stabile
  WiFi.setAutoReconnect(false);  // Gestiamo noi la riconnessione
  WiFi.setSleep(false);          // Disabilita power saving per connessione più stabile
  
  // Imposta l'hostname specifico per facilitare l'identificazione nel router
  WiFi.setHostname("AtmoVerse-ESP32");
  
  // Numero massimo di tentativi e configurazioni per la riconnessione
  const int MAX_CONNECTION_ATTEMPTS = 5;  // Aumentato a 5 per dare più possibilità
  const int DELAY_BETWEEN_ATTEMPTS = 5000; // Aumentato a 5 secondi tra tentativi
  
  // Aspetta un po' prima di iniziare (per stabilizzazione hardware)
  DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_INFO, "Inizializzazione connessione WiFi...");
  DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_DEBUG, "Attesa 2 secondi per stabilizzazione hardware...");
  
  // Utilizzo Timer invece di delay(2000)
  static Timer hardwareStabilizeTimer(2000, false);
  hardwareStabilizeTimer.reset();
  hardwareStabilizeTimer.enable();
  while (!hardwareStabilizeTimer.isReady()) { yield(); }
  
  // Esegui una scansione per verificare se la rete è visibile
  DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_INFO, "Esecuzione scansione reti WiFi per verificare visibilità...");
  int numNetworks = WiFi.scanNetworks();
  bool networkFound = false;
  int networkRSSI = -100;
  
  if (numNetworks == 0) {
    DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_ERROR, "Nessuna rete WiFi trovata! Verifica il router.");
  } else {
    DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_INFO, "Trovate %d reti:", numNetworks);
    
    // Stampa tutte le reti trovate per debug
    for (int i = 0; i < numNetworks; i++) {
      char securityType[16];
      getEncryptionTypeString((wifi_auth_mode_t)WiFi.encryptionType(i), securityType, sizeof(securityType));
      DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_DEBUG, "%d: '%s' (Segnale: %d dBm, Sicurezza: %s)", 
               i + 1, 
               WiFi.SSID(i).c_str(), 
               WiFi.RSSI(i), 
               securityType);
    }
    
    // Cerca la rete desiderata
    networkFound = false;
    for (int i = 0; i < numNetworks; i++) {
      if (strcmp(WiFi.SSID(i).c_str(), ssid) == 0) {
        networkFound = true;
        networkRSSI = WiFi.RSSI(i);
        DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_INFO, "Rete di destinazione '%s' trovata! Potenza segnale: %d dBm", ssid, networkRSSI);
        
        // Verifica qualità segnale
        if (networkRSSI < -70) {
          DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_WARNING, "Segnale debole, potrebbe causare problemi di connessione!");
        }
        break;
      }
    }
    
    if (!networkFound) {
      DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_ERROR, "ERRORE CRITICO: La rete '%s' NON è stata trovata nella scansione!", ssid);
      DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_ERROR, "Verificare che la rete sia attiva e il nome sia corretto.");
    }
  }
  
  // Pulizia scansione
  WiFi.scanDelete();
  
  DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_INFO, "Preparazione tentativi di connessione...");
  
  // Utilizzo Timer invece di delay(1000)
  static Timer prepConnectTimer(1000, false);
  prepConnectTimer.reset();
  prepConnectTimer.enable();
  while (!prepConnectTimer.isReady()) { yield(); }
  
  // Loop di tentativi multipli
  for (int attempt = 1; attempt <= MAX_CONNECTION_ATTEMPTS; attempt++) {
    DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_INFO, "----------------------------------------");
    DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_INFO, "TENTATIVO %d/%d: Connessione a '%s'", 
             attempt, MAX_CONNECTION_ATTEMPTS, ssid);
    DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_INFO, "----------------------------------------");
    
    // Avvia connessione
    DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_DEBUG, "Chiamata a WiFi.begin() con SSID e password...");
    WiFi.begin(ssid, password);
    DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_DEBUG, "WiFi.begin() completata, attesa connessione...");
    
    // Timer per questo tentativo (max 10 secondi per tentativo)
    unsigned long startAttemptTime = millis();
    unsigned long attemptTimeoutMs = (timeoutMs / MAX_CONNECTION_ATTEMPTS < 10000) ? 10000 : timeoutMs / MAX_CONNECTION_ATTEMPTS;
    
    // Mostra progresso
    int dots = 0;
    
    // Attendi fino al timeout o alla connessione
    while (WiFi.status() != WL_CONNECTED) {
      // Mostra progresso con stato attuale
      if (dots % 5 == 0) {
        Serial.print("[");
        Serial.print(millis() - startAttemptTime);
        Serial.print("ms] Status: ");
        switch(WiFi.status()) {
          case WL_IDLE_STATUS: Serial.print("IDLE"); break;
          case WL_NO_SSID_AVAIL: Serial.print("NO_SSID"); break;
          case WL_SCAN_COMPLETED: Serial.print("SCAN_DONE"); break;
          case WL_CONNECTED: Serial.print("CONNECTED"); break;
          case WL_CONNECT_FAILED: Serial.print("FAILED"); break;
          case WL_CONNECTION_LOST: Serial.print("LOST"); break;
          case WL_DISCONNECTED: Serial.print("DISCONNECTED"); break;
          default: Serial.print(WiFi.status()); break;
        }
      }
      Serial.print(".");
      
      // Utilizzo Timer invece di delay(500)
      static Timer dotTimer(500, false);
      dotTimer.reset();
      dotTimer.enable();
      while (!dotTimer.isReady()) { yield(); }
      
      dots++;
      
      // Stampa una nuova riga ogni 10 punti
      if (dots >= 10) {
        Serial.println();
        dots = 0;
      }
      
      // Controlla se abbiamo superato il timeout per questo tentativo
      if (millis() - startAttemptTime > attemptTimeoutMs) {
        Serial.println("\n[WIFI DEBUG] TIMEOUT: Superato il tempo massimo per questo tentativo");
        Serial.print("[WIFI DEBUG] Stato finale: ");
        switch(WiFi.status()) {
          case WL_IDLE_STATUS: Serial.println("IDLE - In attesa"); break;
          case WL_NO_SSID_AVAIL: Serial.println("NO_SSID - SSID non trovato"); break;
          case WL_SCAN_COMPLETED: Serial.println("SCAN_DONE - Scansione completata"); break;
          case WL_CONNECTED: Serial.println("CONNECTED - Connesso"); break;
          case WL_CONNECT_FAILED: Serial.println("FAILED - Connessione fallita (password errata?)"); break;
          case WL_CONNECTION_LOST: Serial.println("LOST - Connessione persa"); break;
          case WL_DISCONNECTED: Serial.println("DISCONNECTED - Disconnesso"); break;
          default: Serial.println(WiFi.status()); break;
        }
        break;
      }
      
      // Se connesso, esci dal loop
      if (WiFi.status() == WL_CONNECTED) {
        break;
      }
    }
    
    // Controlliamo se la connessione è riuscita
    if (WiFi.status() == WL_CONNECTED) {
      // Registra il successo nel monitoraggio di sistema
      SystemMonitor::recordSuccess(COMPONENT_WIFI);
      
      DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_INFO, "WiFi connesso con successo!");
      DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_INFO, "Indirizzo IP: %s", WiFi.localIP().toString().c_str());
      DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_INFO, "Potenza segnale: %d dBm", WiFi.RSSI());
      
      // Controlla e mostra la qualità della connessione
      int signalStrength = WiFi.RSSI();
      if (signalStrength > -50) {
        DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_INFO, "Qualità segnale: Eccellente");
      } else if (signalStrength > -60) {
        DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_INFO, "Qualità segnale: Buona");
      } else if (signalStrength > -70) {
        DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_INFO, "Qualità segnale: Discreta");
      } else {
        DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_WARNING, "Qualità segnale: Debole - possibili disconnessioni");
        // Non registriamo un errore ma un avviso nel monitoraggio
        SystemMonitor::recordError(COMPONENT_WIFI, "Segnale WiFi debole");
      }
      
      // Imposta il server NTP per ottenere l'ora
      setupTimeServer();
      
      // Stampa lo stato del sistema dopo una connessione WiFi riuscita
      SystemMonitor::printStatusReport(true);
      
      return true;
    } else {
      // Stampa il codice di errore WiFi per debug
      DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_WARNING, "Connessione fallita. Stato WiFi: %d", WiFi.status());
      
      // Registro errore specifico nel sistema di monitoraggio
      String errorMsg;
      switch(WiFi.status()) {
        case WL_IDLE_STATUS: 
          errorMsg = "WiFi in stato IDLE";
          DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_WARNING, "WiFi in stato IDLE");
          break;
        case WL_NO_SSID_AVAIL: 
          errorMsg = "SSID non trovato";
          DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_ERROR, "SSID non trovato!"); 
          break;
        case WL_SCAN_COMPLETED: 
          errorMsg = "Scansione completata senza connessione";
          DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_WARNING, "Scansione completata senza connessione"); 
          break;
        case WL_CONNECT_FAILED: 
          errorMsg = "Connessione fallita - Possibile password errata";
          DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_ERROR, "Connessione fallita - Possibile password errata"); 
          break;
        case WL_CONNECTION_LOST: 
          errorMsg = "Connessione persa";
          DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_WARNING, "Connessione persa"); 
          break;
        case WL_DISCONNECTED: 
          errorMsg = "Disconnesso";
          DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_WARNING, "Disconnesso"); 
          break;
        default: 
          errorMsg = "Errore sconosciuto: " + String(WiFi.status());
          DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_ERROR, "Errore sconosciuto: %d", WiFi.status()); 
          break;
      }
      
      // Registra l'errore nel sistema di monitoraggio
      SystemMonitor::recordError(COMPONENT_WIFI, errorMsg.c_str());
      
      // Reset WiFi prima del prossimo tentativo
      DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_INFO, "Disconnessione e reset completo WiFi prima del prossimo tentativo...");
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      
      // Utilizzo Timer invece di delay(1000)
      static Timer resetWifiTimer(1000, false);
      resetWifiTimer.reset();
      resetWifiTimer.enable();
      while (!resetWifiTimer.isReady()) { yield(); }
      
      WiFi.mode(WIFI_STA);
      
      // Attesa più lunga tra tentativi
      DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_INFO, "Attesa %d secondi prima del prossimo tentativo...", 
               DELAY_BETWEEN_ATTEMPTS / 1000);
      
      // Mostra un countdown
      for (int i = DELAY_BETWEEN_ATTEMPTS / 1000; i > 0; i--) {
        DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_DEBUG, "Attesa... %d", i);
        
        // Utilizzo Timer invece di delay(1000)
        static Timer countdownTimer(1000, false);
        countdownTimer.reset();
        countdownTimer.enable();
        while (!countdownTimer.isReady()) { yield(); }
      }
      
      DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_INFO, "Preparazione prossimo tentativo...");
    }
  }
  
  DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_ERROR, "====================================================");
  DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_ERROR, "TUTTI I TENTATIVI DI CONNESSIONE FALLITI!");
  DEBUG_LOG(DEBUG_CATEGORY_WIFI, DEBUG_LEVEL_ERROR, "====================================================");
  
  Serial.println("Suggerimenti per risolvere:");
  Serial.println(" 1. Verifica che la password WiFi sia corretta");
  Serial.println("    - Prova a modificare la password temporaneamente a una più semplice");
  Serial.println("    - Controlla caratteri speciali o maiuscole/minuscole");
  Serial.println(" 2. Controlla il router:");
  Serial.println("    - Assicurati che sia acceso e funzionante");
  Serial.println("    - Verifica che il dispositivo sia abbastanza vicino al router");
  Serial.println("    - Assicurati che la rete funzioni a 2.4GHz (non solo 5GHz)");
  Serial.println(" 3. Altri controlli:");
  Serial.println("    - Verifica che il nome della rete (SSID) non sia cambiato");
  Serial.println("    - Controlla che non ci siano filtri MAC nel router");
  Serial.println("    - Prova a riavviare sia il dispositivo che il router");
  Serial.println("    - Verifica che la rete non abbia raggiunto il limite di dispositivi");
  
  return false; // Tutti i tentativi falliti
}

// Funzione di debug per il contenuto della configurazione
void debugConfigContent() {
  Serial.println("\n==== CONTENUTO CONFIGURAZIONE ====");
  Serial.print("SSID: '");
  Serial.print(config.ssid);
  Serial.println("'");
  
  Serial.print("Password presente: ");
  Serial.println(strlen(config.password) > 0 ? "Si" : "No");
  
  Serial.print("Città: '");
  Serial.print(config.city);
  Serial.println("'");
  
  Serial.print("Coordinate: ");
  Serial.print(config.lat, 4);
  Serial.print(", ");
  Serial.println(config.lon, 4);
  
  Serial.print("API Key presente: ");
  Serial.println(strlen(config.api_key) > 0 ? "Si" : "No");
  
  Serial.print("Fuso orario (secondi): ");
  Serial.println(config.gmtOffset_sec);
  
  Serial.print("NTP Server: '");
  Serial.print(config.ntpServer);
  Serial.println("'");
  Serial.println("=================================\n");
}

// Setup WiFi iniziale
bool setupWiFi() {
  DEBUG_TRACE();
  Serial.println("\n[WIFI] Inizializzazione connessione WiFi");
  
  bool configLoaded = loadConfig();

  // Verifica se la configurazione è stata caricata
  if (!configLoaded) {
    Serial.println("[WIFI] ERRORE: Impossibile caricare la configurazione dalla SD");
    Serial.println("[WIFI] Avvio in modalità Access Point per permettere la configurazione");
    startAccessPoint();
    return false;
  }
  
  // Stampa le informazioni di configurazione per debug
  debugConfigContent();
  
  // Verifica se l'SSID è vuoto
  if (strlen(config.ssid) == 0) {
    Serial.println("[WIFI] ERRORE: SSID nella configurazione è vuoto");
    Serial.println("[WIFI] Avvio in modalità Access Point per permettere la configurazione");
    startAccessPoint();
    return false;
  }
  
  Serial.print("[WIFI] Tentativo di connessione alla rete: ");
  Serial.println(config.ssid);

  // Tenta di connettersi alla rete configurata
  bool connected = connectToWiFi(config.ssid, config.password);
  
  if (!connected) {
    Serial.println("[WIFI] ERRORE: Impossibile connettersi alla rete WiFi configurata");
    Serial.println("[WIFI] Avvio in modalità Access Point per permettere la configurazione");
    startAccessPoint();
    return false;
  }
  
  Serial.println("[WIFI] Connessione WiFi stabilita con successo");
  return true;
}

// Restituisce l'IP locale corrente
IPAddress getLocalIP() {
  DEBUG_TRACE();
  if (apMode) {
    return WiFi.softAPIP();
  } else {
    return WiFi.localIP();
  }
}

// Esegue la scansione delle reti WiFi
void scanWiFiNetworks(char* result, size_t resultSize) {
  DEBUG_TRACE();
  if (result == NULL || resultSize == 0) {
    return;
  }

  // Inizializza il buffer di risultato
  result[0] = '\0';
  
  Serial.println("[WIFI] Avvio scansione reti...");
  int numNetworks = WiFi.scanNetworks();
  Serial.print("[WIFI] Scansione completata. Trovate ");
  Serial.print(numNetworks);
  Serial.println(" reti.");
  
  char networkBuffer[128];
  int totalLength = 0;
  
  for (int i = 0; i < numNetworks && totalLength < (int)resultSize - 1; i++) {
    char securityType[16];
    getEncryptionTypeString((wifi_auth_mode_t)WiFi.encryptionType(i), securityType, sizeof(securityType));
    
    int written = snprintf(networkBuffer, sizeof(networkBuffer), 
                          "%d: '%s' (Segnale: %d dBm, Sicurezza: %s)\n", 
                          i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), securityType);
    
    if (written > 0 && totalLength + written < (int)resultSize) {
      strlcat(result, networkBuffer, resultSize);
      totalLength += written;
    } else {
      // Buffer is full
      break;
    }
  }
  
  // Cleanup
  WiFi.scanDelete();
}

// Variabili per la gestione della riconnessione WiFi
static unsigned long lastReconnectAttempt = 0;
static int reconnectAttempts = 0;
const int MAX_RECONNECT_ATTEMPTS = 5;
const unsigned long RECONNECT_INTERVAL = 30000; // 30 secondi tra i tentativi

/**
 * Inizializza il sistema di riconnessione WiFi
 */
void initWiFiReconnect() {
    lastReconnectAttempt = 0;
    reconnectAttempts = 0;
}

/**
 * Verifica se è necessario riconnettersi al WiFi e gestisce la riconnessione
 * @return true se connesso o riconnessione in corso, false se non è possibile riconnettersi
 */
bool checkAndReconnectWiFi() {
    // Se siamo già connessi, non fare nulla
    if (WiFi.status() == WL_CONNECTED) {
        if (reconnectAttempts > 0) {
            DEBUG_PRINTLN("[WIFI] Connessione WiFi ripristinata con successo");
            reconnectAttempts = 0; // Resetta il contatore di tentativi
        }
        return true;
    }

    // Se siamo in modalità AP, non fare nulla
    if (apMode) {
        return false;
    }

    // Controlla se è il momento di tentare una riconnessione
    unsigned long currentTime = millis();
    if (currentTime - lastReconnectAttempt < RECONNECT_INTERVAL) {
        return false; // Non è ancora il momento di riprovare
    }

    // Incrementa il contatore di tentativi
    reconnectAttempts++;
    lastReconnectAttempt = currentTime;

    // Se abbiamo superato il numero massimo di tentativi, avvia la modalità AP
    if (reconnectAttempts > MAX_RECONNECT_ATTEMPTS) {
        DEBUG_PRINTLN("[WIFI] Superato il numero massimo di tentativi di riconnessione");
        DEBUG_PRINTLN("[WIFI] Avvio modalità AP per la configurazione");
        startAccessPoint(true); // Forza l'avvio della modalità AP
        return false;
    }

    // Tenta la riconnessione
    DEBUG_PRINTF("[WIFI] Tentativo di riconnessione %d/%d\n", reconnectAttempts, MAX_RECONNECT_ATTEMPTS);
    
    // Disconnessione pulita prima di riconnettersi
    WiFi.disconnect(true);
    
    // Utilizzo Timer invece di delay(100)
    static Timer disconnectTimer(100, false);
    disconnectTimer.reset();
    disconnectTimer.enable();
    while (!disconnectTimer.isReady()) { yield(); }
    
    // Prova a riconnettersi
    bool success = reconnectToWiFi();
    
    if (success) {
        DEBUG_PRINTLN("[WIFI] Riconnessione riuscita!");
        reconnectAttempts = 0; // Resetta il contatore di tentativi
        return true;
    }
    
    DEBUG_PRINTLN("[WIFI] Riconnessione fallita");
    return false;
}

// Funzione per il controllo periodico della connessione WiFi
void checkWiFiConnection() {
    static unsigned long lastCheck = 0;
    const unsigned long CHECK_INTERVAL = 30000; // 30 secondi tra i controlli
    
    // Se non è ancora passato abbastanza tempo, esci
    if (millis() - lastCheck < CHECK_INTERVAL) {
        return;
    }
    
    lastCheck = millis();
    
    // Usa la nuova funzione di riconnessione
    if (WiFi.status() != WL_CONNECTED) {
        DEBUG_PRINTLN("[WIFI] Connessione WiFi persa, avvio procedura di riconnessione...");
        checkAndReconnectWiFi();
    }
}

/**
 * Converte il tipo di crittografia WiFi in una stringa leggibile
 * @param encryptionType Tipo di crittografia da convertire
 * @param output Buffer di output dove salvare la stringa
 * @param outputSize Dimensione del buffer di output
 */
void getEncryptionTypeString(wifi_auth_mode_t encryptionType, char* output, size_t outputSize) {
    if (output == NULL || outputSize == 0) {
        return;
    }
    
    switch (encryptionType) {
        case WIFI_AUTH_OPEN:
            strncpy(output, "Aperta", outputSize);
            break;
        case WIFI_AUTH_WEP:
            strncpy(output, "WEP", outputSize);
            break;
        case WIFI_AUTH_WPA_PSK:
            strncpy(output, "WPA-PSK", outputSize);
            break;
        case WIFI_AUTH_WPA2_PSK:
            strncpy(output, "WPA2-PSK", outputSize);
            break;
        case WIFI_AUTH_WPA_WPA2_PSK:
            strncpy(output, "WPA/WPA2", outputSize);
            break;
        case WIFI_AUTH_WPA2_ENTERPRISE:
            strncpy(output, "WPA2-ENT", outputSize);
            break;
        case WIFI_AUTH_WPA3_PSK:
            strncpy(output, "WPA3-PSK", outputSize);
            break;
        case WIFI_AUTH_WPA2_WPA3_PSK:
            strncpy(output, "WPA2/WPA3", outputSize);
            break;
        case WIFI_AUTH_WAPI_PSK:
            strncpy(output, "WAPI-PSK", outputSize);
            break;
        default:
            strncpy(output, "Sconosciuto", outputSize);
            break;
    }
    
    // Assicurati che la stringa sia terminata correttamente
    output[outputSize - 1] = '\0';
}
