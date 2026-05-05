#include "Display.h"
#include "Weather.h"
#ifndef DEBUG_PRINTLN
#define DEBUG_PRINTLN(x) Serial.println(x)
#endif
#include "Config.h"
#include "Debug.h"
#include "WeatherUtils.h"
#include "Calendar.h"
#include "Debug.h" 
#include "AtmoVerseConstants.h"  // Per utilizzare ATMOVERSE_AP_PASSWORD
#include "QuotesManager.h"  // Per la gestione delle citazioni
#include "Hardware.h"  // Per l'oggetto display e PanelType
#include "WeatherIconData.h" // Added for weather icon bitmap data
// Utilizziamo un solo font per risparmiare memoria DRAM
#include <Fonts/FreeSansBold12pt7b.h>    // Unico font utilizzato
#include <WiFi.h>
#include <math.h>
#include "SVGHelper.h"  // Per il supporto ai file SVG
#include "WeatherIcons.h"  // Per le icone OpenWeatherMap
#include "SystemMonitor.h"  // Per la lettura dello stato batteria
#include "DisplayBuffers.h"  // Buffer condivisi per risparmiare memoria DRAM

// Flag di funzionalità per ridurre lo spazio flash: disabilita SVG/PNG
#ifndef USE_SVG
#define USE_SVG 0
#endif
#ifndef USE_OWM_ICONS
#define USE_OWM_ICONS 0
#endif

// Stringhe costanti per display in PROGMEM
static const char ATMOVERSE_TITLE[] PROGMEM = "AtmoVerse 2.0";
static const char WEATHER_CALENDAR_SYSTEM[] PROGMEM = "Sistema meteo con calendario";
static const char LOADING_TEXT[] PROGMEM = "Caricamento...";
static const char VERSION_TEXT[] PROGMEM = "v2.0 - 2025";
static const char CONFIG_TITLE[] PROGMEM = "Prima Configurazione";
static const char WIFI_NETWORK_TEXT[] PROGMEM = "Rete: ";
static const char FIND_WIFI_NETWORK[] PROGMEM = "Cerca questa rete WiFi sul tuo dispositivo";
static const char PASSWORD_TEXT[] PROGMEM = "Password: ";
static const char INSERT_PASSWORD[] PROGMEM = "Inserisci questa password quando richiesto";
static const char OPEN_BROWSER[] PROGMEM = "Apri nel browser:";
static const char CONFIG_SAVE[] PROGMEM = "Configura il dispositivo e salva";
static const char FOOTER_TEXT[] PROGMEM = "Calendario meteo integrato - AtmoVerse 2.0";

// L'oggetto display è dichiarato in Hardware.h/Hardware.cpp

// Forward declarations
void execDisplayUpdate();

/**
 * @brief Returns the current time
 * @return Current time_t value
 */
time_t getNow() {
  return time(nullptr);
}

// Visualizza un semplice box di testo centrale con il messaggio passato
void showStatusOnDisplay(const char* msg) {
  DEBUG_TRACE("showStatusOnDisplay"); // added
  
  // Esecuzione immediata per i messaggi di stato importanti
  // Aggiorna immediatamente il display senza passare per il sistema asincrono
  Serial.print(F("[DISPLAY] Mostra messaggio di stato: ")); Serial.print(msg);
  
  // Forziamo l'aggiornamento per i messaggi di stato importanti
  lastDisplayPhysicalUpdate = 0; // Reset del timer per forzare l'aggiornamento
  displayUpdateRequested = true;
  displayRefreshInProgress = true;
  
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    // bordo
    display.drawRect(5, 5, display.width()-10, display.height()-10, GxEPD_BLACK);
    // testo centrato
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeSansBold12pt7b);
    int16_t tbx, tby; uint16_t tbw, tbh;
    display.getTextBounds(msg, 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setCursor((display.width() - tbw) / 2, (display.height() + tbh) / 2);
    display.print(msg);
  } while (display.nextPage());
  
  // Aggiorna i flag di stato
  displayUpdateRequested = false;
  displayRefreshInProgress = false;
  lastDisplayPhysicalUpdate = millis();
  
  Serial.print(F("[DISPLAY] Messaggio di stato visualizzato: ")); Serial.print(msg);
}

// Definizione pin per display e-ink gi├á dichiarati nel file principale
// Usiamo solo la referenza all'oggetto display tramite extern

// Inizializzazione del display e-ink
void initDisplay() {
  DEBUG_TRACE("initDisplay"); // added
  Serial.println(F("Inizializzazione display..."));
  // Diagnostica stato BUSY prima della init (molti pannelli hanno BUSY open-drain -> usare pull-up)
  pinMode(EPD_BUSY, INPUT_PULLUP);
  int busyBefore = digitalRead(EPD_BUSY);
  Serial.print(F("EPD_BUSY (prima della init) = ")); Serial.println(busyBefore);

  // Reset manuale del pannello (fallback prima della init)
  pinMode(EPD_RST, OUTPUT);
  digitalWrite(EPD_RST, LOW);
  delay(500);
  digitalWrite(EPD_RST, HIGH);
  delay(500);

  // Sonda rapida: leggi BUSY per alcuni cicli per verificare cambiamenti
  Serial.print(F("Probe BUSY: "));
  for (int i = 0; i < 10; ++i) {
    int b = digitalRead(EPD_BUSY);
    Serial.print(b);
    if (i < 9) Serial.print(',');
    delay(50);
  }
  Serial.println();

  // Init con reset più lungo (50ms) e senza setBusyCallback
  display.init(115200, true, 50, false);

  // Lascia assestare l'hardware prima di procedere con i primi draw
  delay(200);

  // Diagnostica stato BUSY dopo init
  int busyAfter = digitalRead(EPD_BUSY);
  Serial.print(F("EPD_BUSY (dopo la init) = ")); Serial.println(busyAfter);
  display.setRotation(0);
  display.setTextColor(GxEPD_BLACK);
  display.setFullWindow();
  Serial.println(F("Display inizializzato"));
}

// Funzione per visualizzare la schermata di avvio
void displayStartupScreen() {
  DEBUG_TRACE("displayStartupScreen");
  
  display.setFullWindow();
  display.firstPage();
  
  do {
    display.fillScreen(GxEPD_WHITE);
    
    // Bordo decorativo
    display.drawRect(5, 5, display.width()-10, display.height()-10, GxEPD_BLACK);
    
    // Logo AtmoVerse 2.0
    display.setFont(&FreeSansBold12pt7b);
    int16_t tbx, tby; 
    uint16_t tbw, tbh;
    
    display.getTextBounds(FPSTR(ATMOVERSE_TITLE), 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setCursor((display.width() - tbw) / 2, display.height() / 3);
    display.print(FPSTR(ATMOVERSE_TITLE));
    
    // Sottotitolo
    display.setFont(&FreeSansBold12pt7b); // Sostituito FreeSerif9pt7b con FreeSansBold12pt7b
    display.getTextBounds(FPSTR(WEATHER_CALENDAR_SYSTEM), 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setCursor((display.width() - tbw) / 2, display.height() / 2);
    display.print(FPSTR(WEATHER_CALENDAR_SYSTEM));
    
    // Messaggio di caricamento invece della data
    display.getTextBounds(FPSTR(LOADING_TEXT), 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setCursor((display.width() - tbw) / 2, display.height() * 2 / 3);
    display.print(FPSTR(LOADING_TEXT));
    
    // Versione
    display.setFont(NULL);
    display.setCursor(display.width() / 2 - 30, display.height() - 20);
    display.print(FPSTR(VERSION_TEXT));
    
  } while (display.nextPage());
  
  DEBUG_PRINTLN(F("Schermata di avvio visualizzata"));
}

// Funzione per visualizzare la schermata di configurazione
void displaySetupScreen(String apName, String ipAddress) {
  DEBUG_TRACE("displaySetupScreen"); // added
  
  // La schermata di configurazione è importante e viene visualizzata immediatamente
  // bypassando il sistema asincrono
  Serial.println(F("[DISPLAY] Visualizzazione schermata di configurazione..."));
  
  // Forziamo l'aggiornamento per la schermata di configurazione
  lastDisplayPhysicalUpdate = 0; // Reset del timer per forzare l'aggiornamento
  displayUpdateRequested = true;
  displayRefreshInProgress = true;
  
  // Genera QR code con URL del portale captive
  String url = String("http://") + ipAddress;
  
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    
    // Bordo esterno
    display.drawRect(5, 5, display.width()-10, display.height()-10, GxEPD_BLACK);
    
    // Disegna il sole - spostato a sinistra del titolo
    int sunX = 50;
    int sunY = 35;
    display.fillCircle(sunX, sunY, 8, GxEPD_BLACK);
    for(int i = 0; i < 8; i++) {
        float angle = i * PI / 4;
        int x1 = sunX + cos(angle) * 12;
        int y1 = sunY + sin(angle) * 12;
        int x2 = sunX + cos(angle) * 16;
        int y2 = sunY + sin(angle) * 16;
        display.drawLine(x1, y1, x2, y2, GxEPD_BLACK);
    }
    
    // Disegna la nuvola - spostata a destra del titolo
    int cloudX = display.width() - 50;
    int cloudY = 35;
    display.fillCircle(cloudX, cloudY, 6, GxEPD_BLACK);
    display.fillCircle(cloudX + 8, cloudY, 8, GxEPD_BLACK);
    display.fillCircle(cloudX - 6, cloudY + 4, 5, GxEPD_BLACK);
    
    // Titolo AtmoVerse - ora al centro, tra sole e nuvola
    display.setFont(&FreeSansBold12pt7b);
    int16_t tbx, tby; uint16_t tbw, tbh;
    display.getTextBounds("AtmoVerse", 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setCursor((display.width() - tbw) / 2, 45);
    display.print("AtmoVerse");
    
    // Linea separatrice
    display.drawLine(20, 65, display.width() - 20, 65, GxEPD_BLACK);
    
    // Cambio da "Modalità Configurazione" a "Prima Configurazione"
    display.setFont(&FreeSansBold12pt7b);
    display.getTextBounds(FPSTR(CONFIG_TITLE), 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setCursor((display.width() - tbw) / 2, 95);
    display.print(FPSTR(CONFIG_TITLE));
    
    // Istruzioni di connessione con font pi├╣ grande
    // Aumento lo spazio tra le fasi
    int textY = 135;
    display.setFont(&FreeSansBold12pt7b); // Sostituito FreeSerif9pt7b con FreeSansBold12pt7b
    
    // Passo 1 - Connessione alla rete AtmoVerse
    display.fillCircle(30, textY, 12, GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    display.setCursor(26, textY+4);
    display.print("1");
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(50, textY+4);
    // Uso font più grande per il titolo del passo
    display.setFont(&FreeSansBold12pt7b);
    display.print(FPSTR(WIFI_NETWORK_TEXT));
    display.print(apName);
    
    // Spiegazione pi├╣ concisa del passo 1
    display.setFont(&FreeSansBold12pt7b); // Sostituito FreeSerif9pt7b con FreeSansBold12pt7b
    display.setCursor(50, textY+25);
    display.print(FPSTR(FIND_WIFI_NETWORK));
    
    // Passo 2 - Password per connettersi
    textY += 70; // Aumentata spaziatura tra le fasi
    display.fillCircle(30, textY, 12, GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    display.setCursor(26, textY+4);
    display.print("2");
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(50, textY+4);
    // Uso font più grande per il titolo del passo
    display.setFont(&FreeSansBold12pt7b);
    display.print(FPSTR(PASSWORD_TEXT));
    display.print(ATMOVERSE_AP_PASSWORD);
    
    // Spiegazione pi├╣ concisa del passo 2
    display.setFont(&FreeSansBold12pt7b); // Sostituito FreeSerif9pt7b con FreeSansBold12pt7b
    display.setCursor(50, textY+25);
    display.print(FPSTR(INSERT_PASSWORD));
    
    // Passo 3 - Apertura browser
    textY += 70; // Aumentata spaziatura tra le fasi
    display.fillCircle(30, textY, 12, GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    display.setCursor(26, textY+4);
    display.print("3");
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(50, textY+4);
    // Uso font pi├╣ grande per il titolo del passo
    display.setFont(&FreeSansBold12pt7b);
    display.print(FPSTR(OPEN_BROWSER));
    
    // Indirizzo IP con font ancora pi├╣ grande
    display.setFont(&FreeSansBold12pt7b);
    display.setCursor(50, textY+35);
    display.print(url);
    
    // Spiegazione pi├╣ concisa del passo 3
    display.setFont(&FreeSansBold12pt7b); // Sostituito FreeSerif9pt7b con FreeSansBold12pt7b
    display.setCursor(50, textY+60);
    display.print(FPSTR(CONFIG_SAVE));
    
    // Versione in basso
    display.setFont(&FreeSansBold12pt7b); // Sostituito FreeSerif9pt7b con FreeSansBold12pt7b
    display.setCursor(20, display.height() - 20);
    display.print(FPSTR(FOOTER_TEXT));
    
  } while (display.nextPage());
  
  // Aggiorna i flag di stato
  displayUpdateRequested = false;
  displayRefreshInProgress = false;
  lastDisplayPhysicalUpdate = millis();
  
  Serial.println(F("[DISPLAY] Schermata di configurazione visualizzata"));
}

// Versione aggiornata della funzione showAPModeInfo che utilizza displaySetupScreen
void showAPModeInfo() {
  // Utilizza la funzione displaySetupScreen per mostrare le informazioni in modalit├á AP
  // Prende le informazioni di rete da WiFi
  String macAddr = WiFi.macAddress();
  macAddr.replace(":", "");
  macAddr = macAddr.substring(6);
  
  // Usa il SSID effettivo dell'access point
  String apName = WiFi.softAPSSID();
  displaySetupScreen(apName, WiFi.softAPIP().toString());
}

// Funzione che aggiorna solo l'ora (minimale)
void updateTimeOnly() {
  DEBUG_TRACE("updateTimeOnly"); // added
  
  // Registra solo la richiesta di aggiornamento dell'orario
  requestDisplayUpdate(false); // false = non è un aggiornamento completo
  // Rimuoviamo flag non dichiarato
  
  Serial.println(F("[DISPLAY] Richiesto aggiornamento orario (in attesa intervallo minimo)"));
}

// Funzione che aggiorna orario e citazioni
void updateTimeAndQuotes() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) return;
  
  strftime(sharedDisplayBuffer, sizeof(sharedDisplayBuffer), "%H:%M", &timeinfo);
  
  display.setFullWindow();
  display.firstPage();
  do {
    int timeX = 20;
    int timeY = 70;
    int16_t tbx, tby; uint16_t tbw, tbh;
    display.setFont(&FreeSansBold12pt7b);
    display.getTextBounds(sharedDisplayBuffer, 0, 0, &tbx, &tby, &tbw, &tbh);
    display.fillRect(timeX - 2, timeY - tbh - 2, tbw + 4, tbh + 4, GxEPD_WHITE);
    display.fillRect(10, display.height() - 25, 150, 20, GxEPD_WHITE);
    display.setCursor(timeX, timeY);
    display.print(sharedDisplayBuffer);
    drawLastUpdate(10, display.height() - 10, currentWeather.last_update);
  } while (display.nextPage());
}

// Flag per indicare se ├¿ il primo avvio
static bool isFirstBoot = true;

// Variabile esterna per lo stato dell'ultimo aggiornamento
extern bool lastWeatherUpdateSuccess; // Definita in AtmoVerse_2.0.ino

// Sistema globale di controllo della frequenza di aggiornamento
static unsigned long lastDisplayRefreshTime = 0;
static const unsigned long DISPLAY_MIN_REFRESH_INTERVAL = 5 * 60 * 1000; // 5 minuti
bool displayRefreshInProgress = false;

// Sistema di aggiornamento asincrono per display e-ink
// Questo permette di desincronizzare gli aggiornamenti del display dal ciclo principale

// Flag che indica se è stata richiesta una modifica dei contenuti
bool displayUpdateRequested = false;

// Timestamp dell'ultimo aggiornamento reale del display
unsigned long lastDisplayPhysicalUpdate = 0;

// Intervallo minimo tra aggiornamenti fisici del display (5 minuti)
const unsigned long DISPLAY_MIN_PHYSICAL_INTERVAL = 300000; // 5 minuti

// Buffer di stato per informazioni sul meteo e altri dati
static bool weatherUpdatePending = false;
static bool timeUpdatePending = false;
static bool fullUpdatePending = false;
static bool forceDisplayUpdate = false;

// Accesso alla variabile mutex esterna
extern SemaphoreHandle_t displayMutex;

// Richiede un aggiornamento del display con controllo antirimbalzo
void requestDisplayUpdate(bool isFullUpdate) {
  displayUpdateRequested = true;
  if (isFullUpdate) {
    fullUpdatePending = true;
  }
  
  // Debug logging della richiesta
  DEBUG_DISP_F("Richiesto aggiornamento display (Full: %s)", isFullUpdate ? "Si" : "No");
}

// Questa funzione è stata spostata più avanti nel file con una implementazione più completa
// La versione duplicata è stata rimossa per evitare errori di compilazione

// La funzione execDisplayUpdate è definita più avanti nel file
// Questa definizione è stata rimossa per evitare duplicazioni

// Funzione per controllare se è necessario aggiornare il display
// Questa viene chiamata periodicamente dal task display
void checkAndUpdateDisplay() {
  unsigned long currentMillis = millis();
  
  // Se non c'è una richiesta di aggiornamento e non è forzato, non fare nulla
  if (!displayUpdateRequested && !forceDisplayUpdate) {
    return;
  }
  
  // Verifica se è trascorso l'intervallo minimo dall'ultimo aggiornamento fisico
  if ((currentMillis - lastDisplayPhysicalUpdate >= DISPLAY_MIN_PHYSICAL_INTERVAL) || forceDisplayUpdate) {
    // È possibile aggiornare il display
    execDisplayUpdate();
  } else {
    // Non è ancora trascorso l'intervallo minimo
    static unsigned long lastWarningTime = 0;
    // Log ogni 30 secondi al massimo per evitare spam
    if (currentMillis - lastWarningTime >= 30000) {
      lastWarningTime = currentMillis;
      DEBUG_DISP_F("Aggiornamento posticipato: intervallo minimo non trascorso (%d sec rimanenti)", 
                 (DISPLAY_MIN_PHYSICAL_INTERVAL - (currentMillis - lastDisplayPhysicalUpdate)) / 1000);
    }
    
    // Se il display non viene aggiornato per troppo tempo, forziamo l'aggiornamento
    if (currentMillis - lastDisplayPhysicalUpdate >= 15 * 60 * 1000) { // 15 minuti
      DEBUG_DISP("Forzo aggiornamento display dopo 15 minuti di inattività");
      forceDisplayUpdate = true;
    }
  }
}

// Verifica se è possibile eseguire un aggiornamento fisico del display
bool canUpdateDisplayPhysically() {
  unsigned long currentMillis = millis();
  
  // Se non è stata richiesta nessuna modifica, non aggiornare
  if (!displayUpdateRequested) {
    return false;
  }
  
  // Se è passato troppo poco tempo dall'ultimo aggiornamento fisico
  if (currentMillis - lastDisplayPhysicalUpdate < DISPLAY_MIN_PHYSICAL_INTERVAL) {
    static unsigned long lastLogTime = 0;
    if (currentMillis - lastLogTime > 30000) { // Log ogni 30 secondi max
      lastLogTime = currentMillis;
      Serial.println(F("[DISPLAY] Aggiornamento fisico rinviato: intervallo minimo non rispettato"));
      Serial.print(F("[DISPLAY] Prossimo aggiornamento possibile tra: "));
      Serial.print((DISPLAY_MIN_PHYSICAL_INTERVAL - (currentMillis - lastDisplayPhysicalUpdate)) / 1000);
      Serial.println(" secondi");
    }
    return false;
  }
  
  // Se è passato abbastanza tempo, consenti l'aggiornamento fisico
  static const char DEBUG_SEPARATOR[] PROGMEM = "[DISPLAY] =================================================================\n";
  static const char DEBUG_UPDATE_AUTHORIZED[] PROGMEM = "[DISPLAY] AGGIORNAMENTO FISICO AUTORIZZATO - INTERVALLO MINIMO RISPETTATO\n";
  static const char DEBUG_INTERVAL[] PROGMEM = "[DISPLAY] Intervallo trascorso: ";
  static const char DEBUG_SECONDS[] PROGMEM = " secondi";
  
  Serial.print(FPSTR(DEBUG_SEPARATOR));
  Serial.print(FPSTR(DEBUG_UPDATE_AUTHORIZED));
  Serial.print(FPSTR(DEBUG_INTERVAL)); 
  Serial.print((currentMillis - lastDisplayPhysicalUpdate) / 1000);
  Serial.println(FPSTR(DEBUG_SECONDS));
  Serial.print(FPSTR(DEBUG_SEPARATOR));
  return true;
}

// Funzione per registrare l'avvenuto aggiornamento del display
void markDisplayRefreshed() {
  lastDisplayRefreshTime = millis();
  displayRefreshInProgress = false;
  Serial.println(F("[DISPLAY] Aggiornamento completato e registrato"));
}

// Funzione per verificare se siamo in modalità risparmio energetico
bool isPowerSavingMode() { return false; }

// Implementazione del sistema di aggiornamento asincrono del display
// Questa funzione ora registra solo la richiesta di aggiornamento invece di eseguirlo immediatamente
void updateDisplay() {
  // Registra solo la richiesta senza aggiornare immediatamente il display
  requestDisplayUpdate(true);
}

/**
 * @brief Versione completa della funzione execDisplayUpdate che gestisce l'aggiornamento del display
 */
// La funzione drawDisplayContent è implementata più avanti nel file

/**
 * @brief Esegue l'aggiornamento fisico del display
 */
void execDisplayUpdate() {
  DEBUG_TRACE("execDisplayUpdate");
  
  // Verifiche iniziali
  if (displayRefreshInProgress) {
    Serial.println(F("[DISPLAY] Aggiornamento già in corso - salta"));
    return;
  }
  
  // Imposta flag di aggiornamento in corso
  displayRefreshInProgress = true;
  Serial.println(F("[DISPLAY] Avvio aggiornamento display"));
  
  // Disegna il contenuto del display
  drawDisplayContent();
  
  // Aggiorna i timestamp di aggiornamento
  lastDisplayPhysicalUpdate = millis();
  
  // Resetta i flag di richiesta
  displayUpdateRequested = false;
  weatherUpdatePending = false;
  timeUpdatePending = false;
  fullUpdatePending = false;
  forceDisplayUpdate = false;
  
  // Reset flag di aggiornamento in corso
  displayRefreshInProgress = false;
  
  Serial.println(F("[DISPLAY] Aggiornamento display completato"));
}

// Funzione per disegnare le diverse fasi lunari usando i file SVG
void drawMoonPhase(int centerX, int centerY, int phase) {
  // Dimensioni della luna
  int size = 40;
  
  // Utilizziamo l'helper SVG per caricare e disegnare l'icona appropriata dalla SD
  #if USE_SVG
  if (SVGHelper::begin()) {
    // L'helper selezionerebbe automaticamente il file SVG appropriato
    SVGHelper::drawMoonPhase(display, centerX - size/2, centerY - size/2, size, phase);
    return;
  }
  #endif
  
  // Fallback nel caso in cui la SD non sia disponibile o il file non sia trovato
  // Raggio ottimizzato per un aspetto elegante
  int r = 20;
  
  // Disegniamo il contorno della luna perfettamente circolare
  display.drawCircle(centerX, centerY, r, GxEPD_BLACK);
  
  // Gestiamo la fase lunare
  if (phase == 0 || phase == 12) {
    // Luna nuova - solo il contorno
  } 
  else if (phase == 6) {
    // Luna piena - cerchio completamente nero
    display.fillCircle(centerX, centerY, r-1, GxEPD_BLACK);
  }
  else {
    // Per le fasi intermedie, calcoliamo la porzione illuminata
    bool isWaxing = (phase < 6); // Crescente o calante
    
    if (isWaxing) {
      // Luna crescente (illuminata a destra)
      for (int y = centerY - r + 1; y < centerY + r; y++) {
        int width = sqrt(r*r - (y-centerY)*(y-centerY));
        display.fillRect(centerX, y, width, 1, GxEPD_BLACK);
      }
    } else {
      // Luna calante (illuminata a sinistra)
      for (int y = centerY - r + 1; y < centerY + r; y++) {
        int width = sqrt(r*r - (y-centerY)*(y-centerY));
        display.fillRect(centerX - width, y, width, 1, GxEPD_BLACK);
      }
    }
  }
}

// Funzione per disegnare una nuvola usando i file SVG
void drawCloud(int centerX, int centerY) {
  // Dimensioni della nuvola
  int size = 40;
  
  // Utilizziamo l'helper SVG per caricare e disegnare l'icona dalla SD
  #if USE_SVG
  if (SVGHelper::begin()) {
    // Carichiamo il file SVG della nuvola
    if (SVGHelper::loadSVG(display, "/icons/cloud.svg", centerX - size/2, centerY - size/2, size, size)) {
      return;
    }
  }
  #endif
  
  // Fallback nel caso in cui la SD non sia disponibile o il file non sia trovato
  // Dimensioni precise per replicare l'immagine condivisa
  int baseWidth = 30;  // Larghezza rettangolo base
  int baseHeight = 18; // Altezza rettangolo base
  
  // Posizione del rettangolo base
  int baseY = centerY;
  int baseX = centerX;
  
  // Disegniamo il rettangolo base
  display.drawRect(baseX - baseWidth/2, baseY - baseHeight, baseWidth, baseHeight, GxEPD_BLACK);
  
  // Dimensioni del semicerchio superiore (pi├╣ stretto e proporzionato)
  int ovalWidth = 18;
  int ovalHeight = 12;
  int ovalY = baseY - baseHeight - 1;  // Collegato precisamente al rettangolo
  
  // Punto centrale dell'ovale
  int ovalCenterY = ovalY - ovalHeight/2;
  
  // Disegniamo l'ovale superiore
  for (int i = 0; i <= 180; i += 5) { // Step pi├╣ piccoli per un contorno pi├╣ pulito
    float angle = i * PI / 180.0;
    int x1 = baseX + (ovalWidth/2) * cos(angle);
    int y1 = ovalCenterY + (ovalHeight/2) * sin(angle);
    
    int x2 = baseX + (ovalWidth/2) * cos((i+5) * PI / 180.0);
    int y2 = ovalCenterY + (ovalHeight/2) * sin((i+5) * PI / 180.0);
    
    display.drawLine(x1, y1, x2, y2, GxEPD_BLACK);
  }
  
  // Linee di connessione tra ovale e rettangolo
  display.drawLine(baseX - ovalWidth/2, ovalCenterY, baseX - ovalWidth/2, ovalY, GxEPD_BLACK);
  display.drawLine(baseX + ovalWidth/2, ovalCenterY, baseX + ovalWidth/2, ovalY, GxEPD_BLACK);
}

// Disegna l'icona meteo in base all'ID utilizzando le icone OpenWeatherMap
void drawWeatherIcon(int x, int y, int weatherId, bool isNight) {
  int iconSize = 140; // Dimensione dell'icona meteo (ingrandita come da preferenze utente)
  int iconCenterX = x + iconSize/2;
  int iconCenterY = y + iconSize/2;
  
  // Inizializza il gestore delle icone se necessario
  #if USE_OWM_ICONS
  if (!WeatherIcons::begin()) {
    Serial.println(F("Impossibile inizializzare il gestore delle icone meteo"));
  }
  
  // Converti l'ID meteo nel codice icona OpenWeatherMap
  String iconCode = getOpenWeatherIconCode(weatherId, isNight);
  
  // Prova a disegnare l'icona OpenWeatherMap
  if (WeatherIcons::drawWeatherIcon(display, iconCode, x, y, iconSize)) {
    // Se abbiamo condizioni serene di notte con luna, aggiungiamo la fase lunare
    // separata dall'icona principale (65px più in alto, come da preferenze utente)
    if (isNight && (weatherId == 800 || weatherId == 801)) {
      // Utilizziamo la fase lunare SVG (se disponibile)
      #if USE_SVG
      if (SVGHelper::begin()) {
        SVGHelper::drawMoonPhase(display, iconCenterX - 30, iconCenterY - 65, 40, currentWeather.moon_phase);
      } else
      #endif
      {
        // Fallback per la luna se SVG non è disponibile
        drawMoonPhase(iconCenterX, iconCenterY - 65, currentWeather.moon_phase);
      }
    }
    return;
  }
  #endif // USE_OWM_ICONS
  
  // Se non è stato possibile utilizzare le icone OpenWeatherMap, utilizziamo il fallback SVG
  #if USE_SVG
  if (SVGHelper::begin()) {
    // Converti l'ID meteo nell'icona SVG appropriata
    WeatherIcon icon = SVGHelper::getIconFromWeatherID(weatherId, isNight);
    
    // Disegna l'icona dal file SVG
    if (SVGHelper::drawWeatherIcon(display, icon, x, y, iconSize)) {
      // Se abbiamo condizioni serene di notte, aggiungiamo anche la luna
      if (isNight && (weatherId == 800 || weatherId == 801)) {
        // Disegna la fase lunare (65px più in alto, come da preferenze utente)
        SVGHelper::drawMoonPhase(display, iconCenterX - 30, iconCenterY - 65, 40, currentWeather.moon_phase);
      }
      return;
    }
  }
  #endif
  
  // Fallback se nè OpenWeatherMap nè SVG sono disponibili
  // Determina il tipo di icona meteo da disegnare
  if (weatherId >= 200 && weatherId < 300) {
    // Temporale
    // Posizione della nuvola 40px più in basso (come da preferenze utente)
    // Posizione della nuvola 40px pi├╣ in basso (come da preferenze utente)
    drawCloud(iconCenterX, iconCenterY + 30);
    // Fulmine
    display.drawLine(iconCenterX, iconCenterY + 60, iconCenterX - 10, iconCenterY + 80, GxEPD_BLACK);
    display.drawLine(iconCenterX - 10, iconCenterY + 80, iconCenterX + 5, iconCenterY + 80, GxEPD_BLACK);
    display.drawLine(iconCenterX + 5, iconCenterY + 80, iconCenterX - 5, iconCenterY + 100, GxEPD_BLACK);
  } 
  else if (weatherId >= 300 && weatherId < 500) {
    // Pioviggine
    // Posizione della nuvola 40px pi├╣ in basso (come da preferenze utente)
    drawCloud(iconCenterX, iconCenterY + 30);
    display.drawLine(iconCenterX - 15, iconCenterY + 60, iconCenterX - 15, iconCenterY + 70, GxEPD_BLACK);
    display.drawLine(iconCenterX, iconCenterY + 65, iconCenterX, iconCenterY + 75, GxEPD_BLACK);
    display.drawLine(iconCenterX + 15, iconCenterY + 60, iconCenterX + 15, iconCenterY + 70, GxEPD_BLACK);
  } 
  else if (weatherId >= 500 && weatherId < 600) {
    // Pioggia
    // Posizione della nuvola 40px pi├╣ in basso (come da preferenze utente)
    drawCloud(iconCenterX, iconCenterY + 30);
    display.drawLine(iconCenterX - 15, iconCenterY + 60, iconCenterX - 15, iconCenterY + 80, GxEPD_BLACK);
    display.drawLine(iconCenterX, iconCenterY + 65, iconCenterX, iconCenterY + 85, GxEPD_BLACK);
    display.drawLine(iconCenterX + 15, iconCenterY + 60, iconCenterX + 15, iconCenterY + 80, GxEPD_BLACK);
  } 
  else if (weatherId >= 600 && weatherId < 700) {
    // Neve
    // Posizione della nuvola 40px pi├╣ in basso (come da preferenze utente)
    drawCloud(iconCenterX, iconCenterY + 30);
    display.drawLine(iconCenterX - 15, iconCenterY + 65, iconCenterX - 15, iconCenterY + 75, GxEPD_BLACK);
    display.drawLine(iconCenterX - 20, iconCenterY + 70, iconCenterX - 10, iconCenterY + 70, GxEPD_BLACK);  
    display.drawLine(iconCenterX, iconCenterY + 65, iconCenterX, iconCenterY + 75, GxEPD_BLACK);
    display.drawLine(iconCenterX - 5, iconCenterY + 70, iconCenterX + 5, iconCenterY + 70, GxEPD_BLACK);
    display.drawLine(iconCenterX + 15, iconCenterY + 65, iconCenterX + 15, iconCenterY + 75, GxEPD_BLACK);
    display.drawLine(iconCenterX + 10, iconCenterY + 70, iconCenterX + 20, iconCenterY + 70, GxEPD_BLACK);
  } 
  else if (weatherId >= 700 && weatherId < 800) {
    // Nebbia/atmosfera - design minimalista
    display.drawLine(iconCenterX - 30, iconCenterY - 20, iconCenterX + 30, iconCenterY - 20, GxEPD_BLACK);
    display.drawLine(iconCenterX - 20, iconCenterY, iconCenterX + 20, iconCenterY, GxEPD_BLACK);
    display.drawLine(iconCenterX - 30, iconCenterY + 20, iconCenterX + 30, iconCenterY + 20, GxEPD_BLACK);
    display.drawLine(iconCenterX - 25, iconCenterY + 40, iconCenterX + 25, iconCenterY + 40, GxEPD_BLACK);
  } 
  else if (weatherId == 800) {
    // Sereno
    if (isNight) {
      // Luna (65px pi├╣ in alto, come da preferenze utente)
      drawMoonPhase(iconCenterX, iconCenterY - 65, currentWeather.moon_phase);
    } else {
      // Sole - design minimalista senza bordo nero circolare
      display.drawCircle(iconCenterX, iconCenterY, 30, GxEPD_BLACK);
      // Raggi del sole
      display.drawLine(iconCenterX, iconCenterY - 45, iconCenterX, iconCenterY - 35, GxEPD_BLACK);
      display.drawLine(iconCenterX, iconCenterY + 35, iconCenterX, iconCenterY + 45, GxEPD_BLACK);
      display.drawLine(iconCenterX - 45, iconCenterY, iconCenterX - 35, iconCenterY, GxEPD_BLACK);
      display.drawLine(iconCenterX + 35, iconCenterY, iconCenterX + 45, iconCenterY, GxEPD_BLACK);
      // Diagonali
      display.drawLine(iconCenterX - 32, iconCenterY - 32, iconCenterX - 25, iconCenterY - 25, GxEPD_BLACK);
      display.drawLine(iconCenterX + 25, iconCenterY - 25, iconCenterX + 32, iconCenterY - 32, GxEPD_BLACK);
      display.drawLine(iconCenterX - 32, iconCenterY + 32, iconCenterX - 25, iconCenterY + 25, GxEPD_BLACK);
      display.drawLine(iconCenterX + 25, iconCenterY + 25, iconCenterX + 32, iconCenterY + 32, GxEPD_BLACK);
    }
  } 
  else if (weatherId > 800 && weatherId < 900) {
    // Nuvoloso
    if (weatherId == 801) { // Poco nuvoloso
      if (isNight) {
        // Luna (65px pi├╣ in alto, come da preferenze utente)
        drawMoonPhase(iconCenterX, iconCenterY - 65, currentWeather.moon_phase);
      } else {
        // Sole - design minimalista
        display.drawCircle(iconCenterX, iconCenterY, 30, GxEPD_BLACK);
      }
      // Nuvola (40px pi├╣ in basso, come da preferenze utente)
      drawCloud(iconCenterX + 20, iconCenterY + 80);
    } else if (weatherId == 802) { // Nubi sparse
      if (isNight) {
        // Luna (65px pi├╣ in alto, come da preferenze utente)
        drawMoonPhase(iconCenterX, iconCenterY - 65, currentWeather.moon_phase);
      } else {
        // Sole - design minimalista
        display.drawCircle(iconCenterX - 20, iconCenterY - 10, 20, GxEPD_BLACK);
      }
      // Nuvola (40px pi├╣ in basso, come da preferenze utente)
      drawCloud(iconCenterX + 10, iconCenterY + 60);
    } else { // Molto nuvoloso
      // Nuvole (40px pi├╣ in basso, come da preferenze utente)
      drawCloud(iconCenterX - 25, iconCenterY + 30);
      drawCloud(iconCenterX + 15, iconCenterY + 50);
    }
  } else {
    // Icona predefinita per ID sconosciuti - design minimalista
    // Disegna solo il contorno del riquadro senza riempimento
    display.drawRect(iconCenterX - 30, iconCenterY - 30, 60, 60, GxEPD_BLACK);
    display.drawLine(iconCenterX - 30, iconCenterY - 30, iconCenterX + 30, iconCenterY + 30, GxEPD_BLACK);
    display.drawLine(iconCenterX - 30, iconCenterY + 30, iconCenterX + 30, iconCenterY - 30, GxEPD_BLACK);
  }
}

#if 0  // Legacy bitmap-based drawWeatherIcon DISABLED
/**
 * @brief Disegna l'icona meteo in base all'ID del tempo e se è notte
 * @param x Coordinata X
 * @param y Coordinata Y
 * @param weatherId ID del meteo da OpenWeatherMap
 * @param isNight Flag che indica se è notte
 */
void drawWeatherIcon(int x, int y, int weatherId, bool isNight) {
  DEBUG_TRACE("drawWeatherIcon");
  
  // Seleziona l'icona appropriata in base al weatherId e se è notte
  // Ad esempio, per la pioggia potremmo avere una diversa icona di notte
  const uint8_t* iconData = nullptr;
  int iconWidth = 64;
  int iconHeight = 64;
  
  // Determina quale icona utilizzare in base al weatherId
  // IDs di OpenWeatherMap: https://openweathermap.org/weather-conditions
  if (weatherId >= 200 && weatherId < 300) {  // Temporale
    iconData = isNight ? thunder_night : thunder;
  } else if (weatherId >= 300 && weatherId < 400) {  // Pioviggine
    iconData = isNight ? drizzle_night : drizzle;
  } else if (weatherId >= 500 && weatherId < 600) {  // Pioggia
    iconData = isNight ? rain_night : rain;
  } else if (weatherId >= 600 && weatherId < 700) {  // Neve
    iconData = isNight ? snow_night : snow;
  } else if (weatherId >= 700 && weatherId < 800) {  // Atmosfera (nebbia, smog, ecc)
    iconData = isNight ? fog_night : fog;
  } else if (weatherId == 800) {  // Sereno
    iconData = isNight ? clear_night : clear;
  } else if (weatherId > 800 && weatherId < 900) {  // Nuvoloso
    if (weatherId == 801) {  // Poche nuvole
      iconData = isNight ? few_clouds_night : few_clouds;
    } else if (weatherId == 802) {  // Nuvole sparse
      iconData = isNight ? scattered_clouds_night : scattered_clouds;
    } else {  // Molto nuvoloso
      iconData = isNight ? broken_clouds_night : broken_clouds;
    }
  } else {  // Default: icona generica
    iconData = isNight ? clear_night : clear;
  }
  
  // Disegna l'icona se è stata selezionata
  if (iconData) {
    display.drawBitmap(x - iconWidth/2, y - iconHeight/2, iconData, iconWidth, iconHeight, GxEPD_BLACK);
  }
}
#endif // legacy drawWeatherIcon

// Funzione per ottenere il codice icona OpenWeatherMap in base all'ID meteo
String getOpenWeatherIconCode(int weatherId, bool isNight) {
  String suffix = isNight ? "n" : "d";
  
  if (weatherId >= 200 && weatherId < 300) {
    // Temporali
    if (weatherId == 210 || weatherId == 211) {
      return "11" + suffix; // Temporale
    } else if (weatherId >= 212) {
      return "11" + suffix; // Temporale forte
    } else {
      return "11" + suffix; // Temporale con pioggia
    }
  }
  else if (weatherId >= 300 && weatherId < 400) {
    // Pioviggine
    return "09" + suffix;
  }
  else if (weatherId >= 500 && weatherId < 600) {
    // Pioggia
    if (weatherId == 500) {
      return "10" + suffix; // Pioggia leggera
    } else if (weatherId == 501) {
      return "10" + suffix; // Pioggia moderata
    } else if (weatherId >= 502) {
      return "10" + suffix; // Pioggia intensa
    } else if (weatherId >= 520) {
      return "09" + suffix; // Pioggia a rovesci
    }
    return "10" + suffix;
  }
  else if (weatherId >= 600 && weatherId < 700) {
    // Neve
    if (weatherId == 600 || weatherId == 601) {
      return "13" + suffix; // Neve leggera/moderata
    } else if (weatherId > 601) {
      return "13" + suffix; // Neve intensa
    } else if (weatherId == 611 || weatherId == 612 || weatherId == 613) {
      return "13" + suffix; // Nevischio
    } else if (weatherId == 615 || weatherId == 616) {
      return "13" + suffix; // Pioggia e neve
    } else {
      return "13" + suffix; // Altri fenomeni nevosi
    }
  }
  else if (weatherId >= 700 && weatherId < 800) {
    // Atmosfera
    if (weatherId == 701 || weatherId == 741) {
      return "50" + suffix; // Nebbia
    } else {
      return "50" + suffix; // Altri fenomeni atmosferici
    }
  }
  else if (weatherId == 800) {
    // Sereno
    return "01" + suffix;
  }
  else if (weatherId > 800 && weatherId < 900) {
    // Nuvoloso
    if (weatherId == 801) {
      return "02" + suffix; // Poco nuvoloso
    } else if (weatherId == 802) {
      return "03" + suffix; // Nubi sparse
    } else if (weatherId == 803) {
      return "04" + suffix; // Nuvoloso
    } else {
      return "04" + suffix; // Coperto
    }
  }
  
  // Codice di fallback per ID sconosciuti
  return "01" + suffix;
}

// Mostra informazioni sul dispositivo
//void drawSwapInfo(int x, int y) {
//  display.setFont(NULL);
//  display.setCursor(x, y);
//  display.print("SWAP: ");
//}

// Funzione per visualizzare messaggi di errore
void displayError(const char* message) {
  DEBUG_TRACE("displayError"); // added
  
  // I messaggi di errore sono critici e vengono visualizzati immediatamente
  // bypassando il sistema asincrono
  Serial.print(F("[DISPLAY] Visualizzazione errore: ")); Serial.print(message);
  
  // Forziamo l'aggiornamento per i messaggi di errore
  lastDisplayPhysicalUpdate = 0; // Reset del timer per forzare l'aggiornamento
  displayUpdateRequested = true;
  displayRefreshInProgress = true;
  
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.drawRect(10, 10, display.width()-20, display.height()-20, GxEPD_BLACK);
    display.setFont(&FreeSansBold12pt7b);
    display.setCursor(30, 60);
    display.print(F("Errore:"));
    display.setCursor(30, 100);
    display.print(message);
  } while (display.nextPage());
  
  // Aggiorna i flag di stato
  displayUpdateRequested = false;
  displayRefreshInProgress = false;
  lastDisplayPhysicalUpdate = millis();
  
  Serial.println(F("[DISPLAY] Messaggio di errore visualizzato"));
}

// Funzione per disegnare la temperatura - rimossa definizione duplicata
// Vedere implementazione completa più in basso

// Funzione per disegnare l'umidit├á

// Funzione per disegnare la pressione

// Funzione per disegnare il vento

// Stringhe di formato in PROGMEM per drawDateTime
static const char TIME_FORMAT[] PROGMEM = "%02d:%02d:%02d";
static const char DATE_FORMAT[] PROGMEM = "%02d/%02d/%04d";

// Funzione per disegnare data e ora
void drawDateTime(int x, int y) {
  time_t now = getNow();
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  
  display.setFont(&FreeSansBold12pt7b);
  display.setCursor(x, y);
  
  // Format: 15:30 - Utilizziamo buffer condiviso
  char formatTimeStr[20];
  strcpy_P(formatTimeStr, TIME_FORMAT);
  sprintf(sharedDisplayBuffer, formatTimeStr, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  display.print(sharedDisplayBuffer);
  
  display.setFont(&FreeSansBold12pt7b); // Sostituito FreeSerif9pt7b con FreeSansBold12pt7b
  display.setCursor(x, y + 25);
  
  // Format: 15/05/2025 - Utilizziamo buffer condiviso secondario
  char formatDateStr[20];
  strcpy_P(formatDateStr, DATE_FORMAT);
  sprintf(secondaryDisplayBuffer, formatDateStr, timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
  display.print(secondaryDisplayBuffer);
}

// Stringa per ultimo aggiornamento in PROGMEM
static const char LAST_UPDATE_FORMAT[] PROGMEM = "Aggiornato: %02d:%02d";

// Funzione per disegnare l'ultimo aggiornamento
void drawLastUpdate(int x, int y, time_t lastUpdate) {
  display.setFont(&FreeSansBold12pt7b); // Sostituito FreeSerif9pt7b con FreeSansBold12pt7b
  display.setCursor(x, y);
  
  struct tm timeinfo;
  localtime_r(&lastUpdate, &timeinfo);
  
  // Utilizziamo buffer condiviso
  char formatLastUpdateStr[30];
  strcpy_P(formatLastUpdateStr, LAST_UPDATE_FORMAT);
  sprintf(sharedDisplayBuffer, formatLastUpdateStr, timeinfo.tm_hour, timeinfo.tm_min);
  display.print(sharedDisplayBuffer);
}

// Funzione per disegnare l'umidit├á

// Funzione per disegnare la pressione

// Funzione per disegnare il vento

// Funzione per disegnare una citazione nella parte inferiore del display
void drawQuote(int x, int y, int maxWidth) {
  display.setFont(&FreeSansBold12pt7b); // Sostituito FreeSerif9pt7b con FreeSansBold12pt7b
  
  // Array di citazioni ispiranti in PROGMEM per risparmiare memoria DRAM
  const char quote1[] PROGMEM = "Non misurare la vita in respiri, ma in momenti che tolgono il respiro.";
  const char quote2[] PROGMEM = "La bellezza è negli occhi di chi guarda.";
  const char quote3[] PROGMEM = "Non c'è nulla di permanente tranne il cambiamento.";
  const char quote4[] PROGMEM = "La vita è ciò che ti accade mentre sei impegnato a fare altri progetti.";
  const char quote5[] PROGMEM = "La felicità non è qualcosa di pronto all'uso. Viene dalle tue azioni.";
  
  // Array di puntatori alle citazioni in PROGMEM
  const char* const quotes[] PROGMEM = {
    quote1,
    quote2,
    quote3,
    quote4,
    quote5
  };
  
  // Scegliamo una citazione casuale
  int quoteIndex = random(0, sizeof(quotes) / sizeof(quotes[0]));
  // Utilizziamo buffer condiviso per copiare la citazione da PROGMEM
  // Non serve allocare un nuovo buffer
  // Copiamo la citazione dalla memoria PROGMEM alla DRAM
  const char* progmemQuotePtr;
  memcpy_P(&progmemQuotePtr, &quotes[quoteIndex], sizeof(progmemQuotePtr));
  strcpy_P(sharedDisplayBuffer, progmemQuotePtr);
  const char* quote = sharedDisplayBuffer;
  
  // Disegniamo la citazione con wrapping del testo
  int16_t cursorX = x;
  int16_t cursorY = y;
  String word = "";
  String line = "";
  int lineWidth = 0;
  
  for (int i = 0; i < strlen(quote); i++) {
    if (quote[i] == ' ' || i == strlen(quote) - 1) {
      // Aggiungi l'ultimo carattere se siamo alla fine della citazione
      if (i == strlen(quote) - 1 && quote[i] != ' ') {
        word += quote[i];
      }
      
      // Calcola la larghezza della parola
      int16_t tbx, tby;
      uint16_t tbw, tbh;
      display.getTextBounds(word.c_str(), 0, 0, &tbx, &tby, &tbw, &tbh);
      
      // Se aggiungere questa parola supera la larghezza massima, vai a capo
      if (lineWidth + tbw > maxWidth) {
        display.setCursor(cursorX, cursorY);
        display.print(line);
        line = word + " ";
        lineWidth = tbw + 6; // 6 ├¿ la larghezza approssimativa di uno spazio
        cursorY += tbh + 5;  // 5 ├¿ lo spazio tra le righe
      } else {
        line += word + " ";
        lineWidth += tbw + 6;
      }
      
      word = "";
    } else {
      word += quote[i];
    }
  }
  
  // Stampa l'ultima riga
  if (line.length() > 0) {
    display.setCursor(cursorX, cursorY);
    display.print(line);
  }
}

// Funzione per disegnare lo stato della batteria
void drawBattery(int x, int y, int percentage) {
  int width = 25;
  int height = 12;
  
  // Disegniamo il contorno della batteria
  display.drawRect(x, y, width, height, GxEPD_BLACK);
  display.drawRect(x + width, y + 3, 2, height - 6, GxEPD_BLACK);
  
  // Disegniamo il livello della batteria
  int fillWidth = map(percentage, 0, 100, 0, width - 4);
  if (fillWidth > 0) {
    display.fillRect(x + 2, y + 2, fillWidth, height - 4, GxEPD_BLACK);
  }
  
  // Disegniamo la percentuale
  display.setFont(NULL);
  display.setCursor(x + width + 5, y + height - 3);
  display.print(String(percentage) + "%");
}
// Seconda definizione di drawProgress rimossa per evitare errori di ridefinizione

/**
 * @brief Funzione modificata per non visualizzare più le informazioni di connessione sul display
 * @param ipAddress Indirizzo IP assegnato al dispositivo
 */
void displayConnectionInfo(const char* ipAddress) {
  DEBUG_TRACE("displayConnectionInfo");
  
  // Logga le informazioni ma non visualizzarle sul display
  Serial.print(F("[INFO] Connesso alla rete WiFi: ")); Serial.println(config.ssid);
  Serial.print(F("[INFO] Indirizzo IP: ")); Serial.println(ipAddress);
  Serial.print(F("[INFO] Interfaccia web disponibile su: http://")); Serial.println(ipAddress);
  
  // Non aggiornare il display - questo rimuove la schermata di connessione
  Serial.println(F("[DISPLAY] Schermata di connessione disabilitata su richiesta dell'utente"));
}

/**
 * @brief Visualizza le informazioni della modalità Access Point sul display
 */
void displayAPModeInfo() {
  DEBUG_TRACE("displayAPModeInfo");
  const char apName[] PROGMEM = "AtmoVerse-Setup";
  strcpy_P(sharedDisplayBuffer, apName);
  const char ipAddress[] PROGMEM = "192.168.4.1";
  strcpy_P(secondaryDisplayBuffer, ipAddress);
  
  // Utilizziamo la funzione esistente displaySetupScreen per mostare le info AP
  displaySetupScreen(sharedDisplayBuffer, secondaryDisplayBuffer);
  
  Serial.println(F("Display aggiornato in modalita AP"));
}

// Seconda definizione di drawProgress rimossa per evitare errori di ridefinizione
// Funzioni aggiuntive per Display.cpp
// Questo file contiene le implementazioni delle funzioni mancanti
// Copiare queste funzioni alla fine del file Display.cpp

// Messaggio di errore in PROGMEM
static const char ERROR_GET_TIME[] PROGMEM = "[DISPLAY] Errore: Impossibile ottenere l'ora corrente";

/**
 * @brief Disegna il contenuto principale sul display
 */
void drawDisplayContent() {
  DEBUG_TRACE("drawDisplayContent");
  
  // Ottieni l'ora corrente
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println(FPSTR(ERROR_GET_TIME));
    return;
  }
  
  // Imposta i parametri di visualizzazione
  display.setRotation(1); // Landscape
  display.setTextColor(GxEPD_BLACK);
  
  // --- SEZIONE ORA E DATA ---
  // Disegna l'ora in grande
  display.setFont(&FreeSansBold12pt7b);
  sprintf(sharedDisplayBuffer, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  
  display.setCursor(20, 40);
  display.println(sharedDisplayBuffer);
  
  // Disegna la data sotto l'ora
  display.setFont(&FreeSansBold12pt7b); // Sostituito FreeSerif9pt7b con FreeSansBold12pt7b
  sprintf(secondaryDisplayBuffer, "%02d/%02d/%04d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
  
  display.setCursor(20, 70);
  display.println(secondaryDisplayBuffer);
  
  // --- SEZIONE BATTERIA (header in alto a destra) ---
  {
    float vbat = SystemMonitor::getBatteryVoltage();
    if (vbat > 0.0f) {
      int pct = SystemMonitor::getBatteryPercent(vbat);
      // Posiziona l'icona batteria nell'angolo in alto a destra
      int bx = display.width() - 70;
      int by = 10;
      drawBattery(bx, by, pct);
    }
  }
  
  // Messaggio per dati meteo non disponibili in PROGMEM
  static const char NO_WEATHER_DATA[] PROGMEM = "Dati meteo non disponibili";

  // --- SEZIONE METEO ---
  // Verifica se abbiamo dati meteo validi
  if (isWeatherDataValid()) {
    extern WeatherData currentWeather;
    
    // Disegna l'icona meteo
    drawWeatherIcon(400, 30, currentWeather.weather_id, isNightTime());
    
    // Disegna la città
    drawCityInfo(400, 110, config.city);
    
    // Disegna la temperatura
    drawTemperature(20, 120, currentWeather.temp, currentWeather.feels_like);
    
    // Disegna l'umidità
    drawHumidity(20, 160, currentWeather.humidity);
    
    // Disegna la pressione
    drawPressure(20, 200, currentWeather.pressure);
    
    // Disegna descrizione del tempo
    display.setFont(&FreeSansBold12pt7b); // Sostituito FreeSerif9pt7b con FreeSansBold12pt7b
    display.setCursor(200, 160);
    display.println(currentWeather.description);
  } else {
    // Se non abbiamo dati meteo validi, mostra un messaggio
    display.setFont(&FreeSansBold12pt7b); // Sostituito FreeSerif9pt7b con FreeSansBold12pt7b
    display.setCursor(200, 120);
    display.println(FPSTR(NO_WEATHER_DATA));
  }
  
  // --- SEZIONE CITAZIONE ---
  // Ottieni una citazione appropriata per il display
  Quote quoteObj = QuotesManager::getQuoteForDisplay();
  
  display.setFont(&FreeSansBold12pt7b); // Sostituito FreeSerif9pt7b con FreeSansBold12pt7b
  
  // Disegna la citazione nella parte inferiore del display
  int quoteY = display.height() - 60;
  display.setCursor(20, quoteY);
  display.println(quoteObj.text);
  
  // Disegna l'autore sotto la citazione
  display.setFont(&FreeSansBold12pt7b); // Sostituito FreeSerif9pt7b con FreeSansBold12pt7b
  display.setCursor(display.width() - 150, quoteY + 30);
  display.println(quoteObj.author);
}

#if 0  // Seconda definizione di drawWeatherIcon disabilitata - duplicata
/**
 * @brief Disegna un'icona meteo in base all'ID del tempo e se è notte
 * @param x Coordinata X
 * @param y Coordinata Y
 * @param weatherId ID del meteo da OpenWeatherMap
 * @param isNight Flag che indica se è notte
 */
void drawWeatherIcon(int x, int y, int weatherId, bool isNight) {
  DEBUG_TRACE("drawWeatherIcon");
  
  // Seleziona l'icona appropriata in base al weatherId e se è notte
  // Ad esempio, per la pioggia potremmo avere una diversa icona di notte
  const uint8_t* iconData = nullptr;
  int iconWidth = 64;
  int iconHeight = 64;
  
  // Determina quale icona utilizzare in base al weatherId
  // IDs di OpenWeatherMap: https://openweathermap.org/weather-conditions
  if (weatherId >= 200 && weatherId < 300) {  // Temporale
    iconData = isNight ? thunder_night : thunder;
  } else if (weatherId >= 300 && weatherId < 400) {  // Pioviggine
    iconData = isNight ? drizzle_night : drizzle;
  } else if (weatherId >= 500 && weatherId < 600) {  // Pioggia
    iconData = isNight ? rain_night : rain;
  } else if (weatherId >= 600 && weatherId < 700) {  // Neve
    iconData = isNight ? snow_night : snow;
  } else if (weatherId >= 700 && weatherId < 800) {  // Atmosfera (nebbia, smog, ecc)
    iconData = isNight ? fog_night : fog;
  } else if (weatherId == 800) {  // Sereno
    iconData = isNight ? clear_night : clear;
  } else if (weatherId > 800 && weatherId < 900) {  // Nuvoloso
    if (weatherId == 801) {  // Poche nuvole
      iconData = isNight ? few_clouds_night : few_clouds;
    } else if (weatherId == 802) {  // Nuvole sparse
      iconData = isNight ? scattered_clouds_night : scattered_clouds;
    } else {  // Molto nuvoloso
      iconData = isNight ? broken_clouds_night : broken_clouds;
    }
  } else {  // Default: icona generica
    iconData = isNight ? clear_night : clear;
  }
  
  // Disegna l'icona se è stata selezionata
  if (iconData) {
    display.drawBitmap(x - iconWidth/2, y - iconHeight/2, iconData, iconWidth, iconHeight, GxEPD_BLACK);
  }
}
#endif // Fine seconda definizione di drawWeatherIcon

/**
 * @brief Disegna le informazioni sulla città
 * @param x Coordinata X
 * @param y Coordinata Y
 * @param cityName Nome della città
 */
void drawCityInfo(int x, int y, const char* cityName) {
  DEBUG_TRACE("drawCityInfo");
  
  display.setFont(&FreeSansBold12pt7b); // Sostituito FreeSerif9pt7b con FreeSansBold12pt7b
  display.setTextColor(GxEPD_BLACK);
  
  // Calcola la larghezza del testo per centrarlo
  int16_t tbx, tby;
  uint16_t tbw, tbh;
  display.getTextBounds(cityName, 0, 0, &tbx, &tby, &tbw, &tbh);
  
  // Disegna il nome della città centrato orizzontalmente
  display.setCursor(x - tbw/2, y);
  display.println(cityName);
}

// Stringhe di formato per temperatura in PROGMEM
static const char TEMP_FORMAT[] PROGMEM = "%.1f°C";
static const char FEELS_LIKE_FORMAT[] PROGMEM = "Percepita: %.1f°C";

/**
 * @brief Disegna la temperatura e temperatura percepita
 * @param x Coordinata X
 * @param y Coordinata Y
 * @param temp Temperatura in gradi Celsius
 * @param feelsLike Temperatura percepita in gradi Celsius
 */
void drawTemperature(int x, int y, float temp, float feelsLike) {
  DEBUG_TRACE("drawTemperature");
  
  // Utilizziamo buffer condiviso invece di allocare un nuovo buffer
  
  // Imposta il font per la temperatura principale
  display.setFont(&FreeSansBold12pt7b);
  display.setTextColor(GxEPD_BLACK);
  
  // Formatta e disegna la temperatura principale
  char formatTempStr[15];
  strcpy_P(formatTempStr, TEMP_FORMAT);
  sprintf(sharedDisplayBuffer, formatTempStr, temp);
  display.setCursor(x, y);
  display.println(sharedDisplayBuffer);
  
  // Imposta un font più piccolo per la temperatura percepita
  display.setFont(&FreeSansBold12pt7b); // Sostituito FreeSerif9pt7b con FreeSansBold12pt7b
  
  // Formatta e disegna la temperatura percepita
  char formatFeelsLikeStr[30];
  strcpy_P(formatFeelsLikeStr, FEELS_LIKE_FORMAT);
  sprintf(secondaryDisplayBuffer, formatFeelsLikeStr, feelsLike);
  display.setCursor(x, y + 25);
  display.println(secondaryDisplayBuffer);
}

// Stringa di formato per umidità in PROGMEM
static const char HUMIDITY_FORMAT[] PROGMEM = "Umidità: %.0f%%";

/**
 * @brief Disegna l'umidità
 * @param x Coordinata X
 * @param y Coordinata Y
 * @param humidity Percentuale di umidità
 */
void drawHumidity(int x, int y, float humidity) {
  DEBUG_TRACE("drawHumidity");
  
  // Utilizziamo buffer condiviso invece di allocare un nuovo buffer
  
  // Imposta il font per l'umidità
  display.setFont(&FreeSansBold12pt7b); // Sostituito FreeSerif9pt7b con FreeSansBold12pt7b
  display.setTextColor(GxEPD_BLACK);
  
  // Formatta e disegna l'umidità
  char formatHumidityStr[20];
  strcpy_P(formatHumidityStr, HUMIDITY_FORMAT);
  sprintf(sharedDisplayBuffer, formatHumidityStr, humidity);
  display.setCursor(x, y);
  display.println(sharedDisplayBuffer);
}

// Stringa di formato per pressione in PROGMEM
static const char PRESSURE_FORMAT[] PROGMEM = "Pressione: %.0f hPa";

/**
 * @brief Disegna la pressione atmosferica
 * @param x Coordinata X
 * @param y Coordinata Y
 * @param pressure Pressione in hPa
 */
void drawPressure(int x, int y, float pressure) {
  DEBUG_TRACE("drawPressure");
  
  // Utilizziamo buffer condiviso invece di allocare un nuovo buffer
  
  // Imposta il font per la pressione
  display.setFont(&FreeSansBold12pt7b); // Sostituito FreeSerif9pt7b con FreeSansBold12pt7b
  display.setTextColor(GxEPD_BLACK);
  
  // Formatta e disegna la pressione
  char formatPressureStr[25];
  strcpy_P(formatPressureStr, PRESSURE_FORMAT);
  sprintf(sharedDisplayBuffer, formatPressureStr, pressure);
  display.setCursor(x, y);
  display.println(sharedDisplayBuffer);
}
