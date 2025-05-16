#include "NetworkUtils.h"
#include "Config.h"
#include "WebServer.h"
#include "WebUIPages.h"
#include "Debug.h"
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
  String macAddress = WiFi.macAddress();
  String lastFourMac = macAddress.substring(macAddress.length() - 5);
  lastFourMac.replace(":", "");
  
  String apSSID = String(ATMOVERSE_AP_SSID) + "_" + lastFourMac;
  // Rimozione stampe debug
  
  // Configura la rete dell'AP (IP 192.168.4.1)
  IPAddress localIP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);

  // Imposta modalità AP esplicita per evitare stati incoerenti
  WiFi.mode(WIFI_AP);
  delay(100);
  WiFi.softAPConfig(localIP, gateway, subnet);
  
  // Avvia l'AP
  bool success = WiFi.softAP(apSSID.c_str(), ATMOVERSE_AP_PASSWORD);
  delay(500);  // Attesa per la stabilizzazione
  
  if (!success) {
    // Rimozione stampe debug
    return;
  }
  
  // Rimozione stampe debug
  
  // Avvia il server DNS captive portal
  IPAddress apIP = WiFi.softAPIP();
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", apIP);
  
  apMode = true;
  
  // Mostra informazioni AP sul display
  displayAPInfo(apSSID.c_str(), ATMOVERSE_AP_PASSWORD);

  // AVVIA IL SERVER WEB anche in AP
  setupServer();
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

// Connessione a una rete WiFi specifica - versione robusta con protezione anti-crash
bool connectToWiFi(const char* ssid, const char* password) {
  DEBUG_TRACE();
  if (strlen(ssid) == 0) {
    // Rimozione stampe debug
    return false;
  }
  
  // Rimozione logging e debug seriale per risparmiare memoria
  // Rimozione riferimenti al watchdog
  
  // Se siamo in modalità AP, cambia modalità con protezioni
  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    // Rimozione stampe debug
    
    // Chiudi il server DNS se attivo
    if (apMode) {
      dnsServer.stop();
      apMode = false;
    }
    
    // Passa da AP a STA con maggiori delay per evitare conflitti
    WiFi.softAPdisconnect(true);
    delay(200);
    WiFi.mode(WIFI_OFF);
    delay(1000);  // Attesa più lunga per assicurare il cambio di modalità
    WiFi.mode(WIFI_STA);
    delay(1000);  // Attesa più lunga per stabilizzare
  }
  
  // Assicura di essere in modalità STA
  if (WiFi.getMode() != WIFI_STA) {
    // Rimozione stampe debug
    WiFi.mode(WIFI_STA);
    delay(1000);  // Attesa più lunga per stabilizzare
  }
  
  // Configura il WiFi per connessione ottimizzata
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);      // Disabilita power saving per connessione più stabile
  WiFi.persistent(false);   // Evita scritture flash che possono causare problemi
  
  // Versione robusta senza messaggi di debug
  WiFi.begin(ssid, password);
  
  // Attendi fino a 30 secondi per la connessione
  int attemptCount = 0;
  const int maxAttempts = 60;  // 30 secondi (60 * 500ms)
  
  while (WiFi.status() != WL_CONNECTED && attemptCount < maxAttempts) {
    delay(500);
    attemptCount++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    // Imposta il server NTP per ottenere l'ora
    setupTimeServer();
    return true;
  } else {
    return false;
  }
}

// Setup WiFi iniziale
bool setupWiFi() {
  DEBUG_TRACE();
  bool configLoaded = loadConfig();
  
  // Se la configurazione non è valida, avvia l'Access Point
  if (!configLoaded || strlen(config.ssid) == 0) {
    Serial.println("[WIFI] Configurazione non valida o assente");
    startAccessPoint();
    return false;
  }
  
  // Tenta di connettersi alla rete configurata
  return connectToWiFi(config.ssid, config.password);
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

// Funzione per il controllo periodico della connessione WiFi
void checkWiFiConnection() {
  DEBUG_TRACE();
  static unsigned long lastCheck = 0;
  unsigned long currentMillis = millis();
  
  // Controlla ogni WIFI_CHECK_INTERVAL millisecondi
  if (currentMillis - lastCheck >= WIFI_CHECK_INTERVAL) {
    lastCheck = currentMillis;
    
    // Non controlliamo in modalità AP
    if (apMode) {
      return;
    }
    
    // Se non connesso, tenta la riconnessione
    if (WiFi.status() != WL_CONNECTED) {
      // Rimozione stampe debug
      
      // Tenta di riconnettersi utilizzando le credenziali salvate
      reconnectToWiFi();
    }
  }
}

// Funzione originale lasciata per compatibilità ma non utilizzata dalla nuova UI
int performWiFiScanForPage() {
  DEBUG_TRACE();
  Serial.println("[WIFI] Avvio scansione reti...");
  int numNetworks = WiFi.scanNetworks();
  Serial.print("[WIFI] Scansione completata. Trovate ");
  Serial.print(numNetworks);
  Serial.println(" reti.");
  return numNetworks;
}
