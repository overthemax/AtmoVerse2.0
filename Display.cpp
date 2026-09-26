#include "Display.h"
#include "Hardware.h"
#include <SD.h>
#include <ArduinoJson.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include <Fonts/FreeSerif9pt7b.h>
#include <Fonts/FreeSerif12pt7b.h>
#include <Fonts/FreeSerifBoldItalic12pt7b.h>  // Font serif grassetto corsivo
#include "KAUFMANN20pt7b.h"                      // Font calligrafico Kaufmann per citazioni
#include <Fonts/FreeSansBold12pt7b.h>    // Font sans-serif più moderno e leggibile
#include <Fonts/FreeSansBold18pt7b.h>    // Font sans-serif grande per citazioni
#include <Fonts/FreeSansBold24pt7b.h>    // Font sans-serif 24pt per temperatura
#include <Fonts/FreeMonoBoldOblique9pt7b.h> // Font corsivo per l'autore
#include <WiFi.h>
#include <math.h>
#include <time.h>
#include "Config.h"
#include "WebServer.h" // per g_displayMode / g_displayTheme
#include "WeatherUtils.h"
#include "Calendar.h"
#include "Debug.h" // added
#include "AtmoVerseConstants.h"  // Per utilizzare ATMOVERSE_AP_PASSWORD
#include "QuotesManager.h"  // Per la gestione delle citazioni
#include "BMPHelper.h"  // Per il supporto alle immagini BMP
#include "SVGHelper.h"  // Per il supporto ai file SVG (fallback)
#include "WeatherIcons.h"  // Per le icone OpenWeatherMap (fallback)
#include "BatteryManager.h"  // Per accesso stato batteria
extern bool apMode;           // Definita in NetworkUtils.cpp (header non incluso: conflitto su WIFI_CHECK_INTERVAL)
#include <string.h>
#include "Screens.h"
#include "DisplayTask.h"

// Visualizza un semplice box di testo centrale con il messaggio passato
// ---------------------------------------------------------------------------
// Richieste al task del display (vedi DisplayTask.h)
// ---------------------------------------------------------------------------
// Queste funzioni girano nel loop (core 1): raccolgono i dati, li copiano in
// uno ScreenModel e lo consegnano al task del display (core 0), che disegna.
// Nessuna di loro tocca il pannello, quindi tornano subito.

static ScreenModel screenModel;  // Usato solo dal loop

static void fillBattery(ScreenModel& m) {
  m.showBattery = config.batteryShowOnDisplay && battery.isAvailable();
  m.batteryPercent = battery.getPercentage();
  m.batteryCharging = battery.charging();
}

static void showMessage(const char* title, const char* text) {
  screenModel = ScreenModel();
  screenModel.kind = SCREEN_MESSAGE;
  screenModel.title = title;
  screenModel.text = text;
  fillBattery(screenModel);
  showScreen(screenModel);
}

void showStatusOnDisplay(const char* msg) {
  String text = msg ? msg : "";
  text.replace("\n", " ");
  showMessage("", text.c_str());
}

// Schermata mostrata mentre si scarica un aggiornamento da GitHub
// (provvisoria: verrà ridisegnata insieme al nuovo aspetto del display)
void showUpdateScreen() {
  screenModel = ScreenModel();
  screenModel.kind = SCREEN_UPDATE;
  fillBattery(screenModel);
  showScreen(screenModel);
}

// Definizione pin per display e-ink già dichiarati nel file principale
// Usiamo solo la referenza all'oggetto display tramite extern

// Inizializzazione del display e-ink
void initDisplay() {
  DEBUG_TRACE("initDisplay");
  display.init(115200);
  display.setRotation(0);
  display.setTextColor(GxEPD_BLACK);
  display.setFullWindow();
  // Da qui in poi solo il task del display usa il pannello
  startDisplayTask();
}

// Funzione per visualizzare la schermata di avvio
void displayStartupScreen() {
  showMessage("AtmoVerse", "Avvio in corso...");
}

// Disegna la temperatura con font GFX grande (24pt) invece di BMP
// temp: temperatura in gradi Celsius
// glyphSize: dimensione massima del riquadro (ignorato, usa font 24pt)
void drawTemperatureBMP(int x, int y, float temp, int glyphSize) {
  // Usa font grande GFX per temperatura chiara e leggibile
  display.setFont(&FreeSansBold24pt7b);
  display.setTextColor(GxEPD_BLACK);
  
  // Formatta temperatura con 1 decimale
  String tempStr = String(temp, 1);
  
  // Calcola bounds per centratura verticale
  int16_t tbx, tby; uint16_t tbw, tbh;
  display.getTextBounds(tempStr.c_str(), 0, 0, &tbx, &tby, &tbw, &tbh);
  
  // Disegna temperatura
  display.setCursor(x, y + tbh);
  display.print(tempStr);
  
  // Disegna simbolo gradi come cerchietto (affidabile su qualsiasi font)
  int degreeX = x + tbw + 8;
  int degreeY = y + tbh - 33; // posizionato in alto accanto alla cifra
  int r = 4; // raggio cerchietto
  display.fillCircle(degreeX + r, degreeY + r, r, GxEPD_BLACK);
  display.fillCircle(degreeX + r, degreeY + r, r - 1, GxEPD_WHITE);
  
  // Reset font default
  display.setFont(NULL);
}

// Funzione per visualizzare la schermata di configurazione
void displaySetupScreen(String apName, String ipAddress) {
  screenModel = ScreenModel();
  screenModel.kind = SCREEN_SETUP;
  screenModel.apName = apName;
  screenModel.apIp = ipAddress;
  fillBattery(screenModel);
  showScreen(screenModel);
}

// Versione aggiornata della funzione showAPModeInfo che utilizza displaySetupScreen
void showAPModeInfo() {
  // Utilizza la funzione displaySetupScreen per mostrare le informazioni in modalità AP
  // Prende le informazioni di rete da WiFi
  String macAddr = WiFi.macAddress();
  macAddr.replace(":", "");
  macAddr = macAddr.substring(6);
  
  // Usa il SSID effettivo dell'access point
  String apName = WiFi.softAPSSID();
  displaySetupScreen(apName, WiFi.softAPIP().toString());
}



// Flag per indicare se è il primo avvio
static bool isFirstBoot = true;

// Variabile esterna per lo stato dell'ultimo aggiornamento
extern bool lastWeatherUpdateSuccess;

// Funzione esterna per verificare se siamo in modalità risparmio energetico
extern bool isPowerSavingMode();

// Implementazione completa della funzione di aggiornamento display
void updateDisplay() {
  // La schermata di configurazione solo quando l'AP è davvero attivo: se il
  // WiFi cade per un momento si continua a mostrare il meteo (ultimi dati)
  if (apMode) {
    showAPModeInfo();
    return;
  }

  ScreenModel& m = screenModel;
  m = ScreenModel();
  m.kind = SCREEN_MAIN;
  m.weather = currentWeather;
  m.weatherUpdateOk = lastWeatherUpdateSuccess;
  m.city = config.city;
  m.metric = strlen(config.units) == 0 || strcmp(config.units, "metric") == 0;
  if (WiFi.status() == WL_CONNECTED) m.ip = WiFi.localIP().toString();

  // Citazione e icona si leggono dalla SD qui, nel loop: il task del display
  // non accede mai alla SD
  Quote q = getQuoteForDisplay();
  m.quoteText = q.text;
  m.quoteAuthor = q.author;
  if (currentWeather.valid && BMPHelper::begin()) {
    WeatherIcon icon = SVGHelper::getIconFromWeatherID(currentWeather.weather_id, isNightTime(), currentWeather.wind_speed);
    if (!loadIconBitmap(BMPHelper::getIconPath(icon), m)) {
      Serial.printf("[DISPLAY] Icona non leggibile: %s\n", BMPHelper::getIconPath(icon));
    }
  }

  fillBattery(m);
  showScreen(m);
  if (isFirstBoot) isFirstBoot = false;
}



// ============================================================================
// LAYOUT DI DEFAULT - Usato quando /layout.json non esiste
// ============================================================================

void drawDefaultLayout() {
  // Serial.println("[DISPLAY] Disegno layout di default");
  
  // Layout fisso ispirato al mockup:
  // - in alto a sinistra: luogo, data, umidita', velocita' vento
  // - temperatura grande sulla sinistra
  // - icona meteo grande sulla destra
  // - citazione centrale
  // - footer con ultimo aggiornamento, IP e batteria

  int W = display.width();
  int H = display.height();

  // Colonna info in alto a sinistra
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  char dateBuffer[16];
  strftime(dateBuffer, sizeof(dateBuffer), "%d/%m/%Y", &timeinfo);

  int leftX = 10;
  int topY  = 10;

  // Luogo
  display.setFont(&FreeSerif9pt7b);
  display.setCursor(leftX, topY);
  display.print(config.city);

  // Data
  display.setCursor(leftX, topY + 18);
  display.print(dateBuffer);

  // Umidita'
  display.setCursor(leftX, topY + 36);
  display.print("Umidita': " + String((int)currentWeather.humidity) + "%");

  // Velocita' vento
  display.setCursor(leftX, topY + 54);
  display.print("Vento: " + String(currentWeather.wind_speed, 1) + " km/h");


  // Dimensione icona massima nel quadrante top-right
  int iconSize = min(W / 2, H / 2 + 80);
  if (iconSize < 80) iconSize = 80;

  // Temperatura grande sulla sinistra - usa cifre BMP da /fonts
  {
    int glyphSize = 80; // dimensione di riferimento per ogni cifra (leggermente piu' compatta)
    int tempX = leftX;

    // Temperatura posizionata sotto i dati info
    int tempY = topY + 70;

    drawTemperatureBMP(tempX, tempY, currentWeather.temp, glyphSize);
  }

  // Icona meteo: angolo top-right, alzata di 30px
  int iconX = W - iconSize;
  int iconY = -30;
  drawWeatherIcon(iconX, iconY, currentWeather.weather_id, isNightTime(), iconSize);

  // Ora grande sotto i gradi - font monospace doppia dimensione
  char timeBuffer[6];
  strftime(timeBuffer, sizeof(timeBuffer), "%H:%M", &timeinfo);
  display.setFont(&FreeMonoBold24pt7b);
  display.setTextSize(2);
  int16_t ttx, tty; uint16_t ttw, tth;
  display.getTextBounds(timeBuffer, 0, 0, &ttx, &tty, &ttw, &tth);
  int timeX = leftX;
  int timeY = topY + 70 + 80 + 15 + (int)tth - 40; // sotto la temperatura, salito di 40px
  display.setCursor(timeX, timeY);
  display.print(timeBuffer);
  display.setTextSize(1);

  // Citazione nella parte bassa
  int quoteTop = timeY + 30;
  int margin = 30;
  if (quoteTop < topY + 80) quoteTop = topY + 80;
  drawQuote(margin, quoteTop, W - 2 * margin);
  
  // Footer: ultimo aggiornamento, IP, batteria
  drawLastUpdate(10, display.height() - 30, currentWeather.last_update);
  
  display.setFont(NULL);
  String ipString = (WiFi.status() == WL_CONNECTED) ? 
                    ("IP: " + WiFi.localIP().toString()) : "IP: N/A";
  int16_t tbx, tby; uint16_t tbw, tbh;
  display.getTextBounds(ipString.c_str(), 0, 0, &tbx, &tby, &tbw, &tbh);
  int centerX = (display.width() - tbw) / 2;
  display.setCursor(centerX, display.height() - 10);
  display.print(ipString);
  
  // Disegna batteria con tensione reale per verifica calibrazione
  int batteryPercentage = battery.getPercentage();
  int battY = display.height() - 20;
  drawBattery(display.width() - 60, battY, batteryPercentage);
  // Mostra tensione reale allineata verticalmente all'icona batteria
  display.setFont(NULL);
  display.setTextSize(1);
  String voltStr = String(battery.getVoltage(), 2) + "V";
  int16_t vx, vy; uint16_t vw, vh;
  display.getTextBounds(voltStr.c_str(), 0, 0, &vx, &vy, &vw, &vh);
  display.setCursor(display.width() - 60 - vw - 4, battY + 2);
  display.print(voltStr);
}

// ============================================================================
// VECCHIE FUNZIONI DI LAYOUT - Mantenute per compatibilità ma non più usate
// ============================================================================

// Funzione focus - NON PIÙ USATA automaticamente, solo via custom layout
void drawFocusLayout() {
  if (true) {
    // Sfondo già pulito da updateDisplay
    // Città in alto
    display.setFont(&FreeSerif12pt7b);
    display.setCursor(20, 40);
    display.print(config.city);

    // Icona meteo grande centrata
    int iconSize = 150;
    int iconX = (display.width() - iconSize) / 2;
    int iconY = 30;
    drawWeatherIcon(iconX, iconY, currentWeather.weather_id, isNightTime());

    // Citazione centrata sotto
    int quoteTop = iconY + iconSize + 10;
    if (quoteTop < 170) quoteTop = 170;
    int margin = 20;
    drawQuote(margin, quoteTop, display.width() - 2 * margin);

    // Barra inferiore: ultimo aggiornamento a sinistra, IP centro, batteria a destra
    drawLastUpdate(10, display.height() - 30, currentWeather.last_update);
    display.setFont(NULL);
    String ipString = (WiFi.status() == WL_CONNECTED) ? (String("IP: ") + WiFi.localIP().toString()) : String("IP: N/A");
    int16_t tbx, tby; uint16_t tbw, tbh;
    display.getTextBounds(ipString.c_str(), 0, 0, &tbx, &tby, &tbw, &tbh);
    int centerX = (display.width() - tbw) / 2;
    int bottomY = display.height() - 15;
    display.setCursor(centerX, bottomY);
    display.setTextColor(GxEPD_BLACK);
    display.print(ipString);
    if (config.batteryMonitorEnabled && config.batteryShowOnDisplay) {
      int batteryPercentage = battery.getPercentage();
      drawBattery(display.width() - 60, display.height() - 20, batteryPercentage);
    }
  }
}

// ============================================================================
// Le seguenti funzioni sono mantenute ma non vengono più richiamate automaticamente
// Possono essere utilizzate tramite layout personalizzati in futuro
// ============================================================================

/*
// Funzione icon_quote - NON PIÙ USATA automaticamente
void drawIconQuoteLayout() {
    // =============================================
    // RIGA SUPERIORE: Icona grande al centro con dati meteo ai lati
    // =============================================
    
    int iconSize = 140;
    int iconX = (display.width() - iconSize) / 2; // Icona centrata
    int iconY = 20;
    
    // Disegna l'icona SVG grande al centro
    drawWeatherIcon(iconX, iconY, currentWeather.weather_id, isNightTime());
    
    // COLONNA SINISTRA: Città e ora
    display.setFont(&FreeSerif12pt7b);
    display.setCursor(10, 35);
    display.print(config.city);
    
    // Ora corrente sotto la città
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char timeBuffer[10];
    strftime(timeBuffer, sizeof(timeBuffer), "%H:%M", &timeinfo);
    
    display.setFont(&FreeSansBold12pt7b);
    display.setCursor(10, 65);
    display.print(timeBuffer);
    
    // Data sotto l'ora (formato breve)
    char dateBuffer[20];
    strftime(dateBuffer, sizeof(dateBuffer), "%d/%m", &timeinfo);
    display.setFont(&FreeSerif9pt7b);
    display.setCursor(10, 85);
    display.print(dateBuffer);
    
    // COLONNA DESTRA: Dati meteo principali
    int rightCol = display.width() - 90; // Colonna destra
    
    // Temperatura
    display.setFont(&FreeSansBold12pt7b);
    display.setCursor(rightCol, 35);
    String tempStr = String(currentWeather.temp, 1) + "°";
    display.print(tempStr);
    
    // Percepita (più piccola)
    display.setFont(&FreeSerif9pt7b);
    display.setCursor(rightCol, 55);
    display.print("Perc:");
    display.setCursor(rightCol, 70);
    display.print(String(currentWeather.feels_like, 1) + "°");
    
    // Umidità
    display.setCursor(rightCol, 90);
    display.print(String((int)currentWeather.humidity) + "%");
    
    // Vento
    display.setCursor(rightCol, 110);
    display.print(String(currentWeather.wind_speed, 0) + " km/h");
    
    // =============================================
    // RIGA INFERIORE: Citazione centrata (grande spazio)
    // =============================================
    
    int quoteTop = iconY + iconSize + 15; // Sotto l'icona
    int margin = 15;
    drawQuote(margin, quoteTop, display.width() - 2 * margin);
    
    // Footer minimale: ultimo aggiornamento e batteria
    int footerY = display.height() - 10;
    drawLastUpdate(10, footerY, currentWeather.last_update);
    int batteryPct = battery.getPercentage();
    drawBattery(display.width() - 60, display.height() - 20, batteryPct);
    
    return;
  }

  // Gestione tema: Enhanced - Layout professionale con icone e separatori
  if (strcmp(config.theme, "enhanced") == 0) {
    drawEnhancedLayout();
    return;
  }
  
  // Disegna il calendario ancora più grande in alto a destra
  drawCalendar(display.width() - 170, 5, 165, 170);
  
  // Disegna il nome della città a sinistra
  display.setFont(&FreeSerif12pt7b);
  display.setCursor(20, 40);
  display.print(config.city);
  
  // Disegna solo l'ora sotto la città (non la data)
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  char timeBuffer[10];
  strftime(timeBuffer, sizeof(timeBuffer), "%H:%M", &timeinfo);
  
  display.setFont(&FreeSerif12pt7b);
  display.setCursor(20, 70);
  display.print(timeBuffer);
  
  // Disegna temperatura, umidità e vento subito sotto l'ora
  display.setFont(&FreeSerif9pt7b);
  
  // Temperatura (con un decimale)
  String tempStr = String(currentWeather.temp, 1) + "°C";
  display.setCursor(20, 100);
  display.print(tempStr);
  
  // Umidità
  String humidityStr = String(int(currentWeather.humidity)) + "%";
  display.setCursor(20, 125);
  display.print(humidityStr);
  
  // Vento
  String windStr = "Vento: " + String(currentWeather.wind_speed, 1) + " km/h";
  display.setCursor(20, 150);
  display.print(windStr);
  
  // Disegna l'icona meteo molto più grande e centrata
  int iconSize = 140; // Dimensione significativamente maggiore dell'icona (considerando un raggio di 70)
  int iconX = (display.width() - iconSize) / 2 - 10; // Centrata e leggermente spostata a sinistra
  int iconY = 40; // Spostata ancora più in alto per dare spazio al box citazione ingrandito
  drawWeatherIcon(iconX, iconY, currentWeather.weather_id, isNightTime());
  
  // Disegna una citazione utilizzando la posizione configurabile
  int x = config.quotePosX;
  int y = config.quotePosY;
  // Clamping semplice nei limiti del display
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  if (x > display.width() - 10) x = display.width() - 10;
  if (y > display.height() - 10) y = display.height() - 10;
  int rightMargin = 20; // margine destro fisso per sicurezza
  int maxWidth = display.width() - x - rightMargin;
  if (maxWidth < 40) maxWidth = 40; // evita valori troppo piccoli
  drawQuote(x, y, maxWidth);
  
  // Rimossa la riga orizzontale separatrice per un aspetto più pulito
  
  // Disegna l'ultimo aggiornamento a sinistra, significativamente spostato più in basso 
  drawLastUpdate(10, display.height() - 30, currentWeather.last_update);
  
  // Mostra l'IP in basso al centro con font più piccolo (default)
  display.setFont(NULL);
  String ipString;
  if (WiFi.status() == WL_CONNECTED) {
      ipString = "IP: " + WiFi.localIP().toString();
  } else {
      ipString = "IP: N/A";
  }
  int16_t tbx, tby; uint16_t tbw, tbh;
  display.getTextBounds(ipString.c_str(), 0, 0, &tbx, &tby, &tbw, &tbh);
  int centerX = (display.width() - tbw) / 2;
  int bottomY = display.height() - 35; // Margine sicurezza dal bordo di 10px
  display.setCursor(centerX, bottomY);
  display.setTextColor(GxEPD_BLACK);
  display.print(ipString);
  
  // Disegna lo stato della batteria (se disponibile) - mantenuto a destra
  drawBattery(display.width() - 60, display.height() - 40, 80);
}
*/

// ============================================================================
// FUNZIONI UTILITY PER IL DISEGNO - Usate da tutti i layout
// ============================================================================

// Funzione per disegnare le diverse fasi lunari (solo disegno geometrico, niente SVG)
void drawMoonPhase(int centerX, int centerY, int phase) {
  // Dimensioni e raggio per disegno vettoriale semplice
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

// Funzione per disegnare una nuvola (solo disegno geometrico, niente SVG)
void drawCloud(int centerX, int centerY) {
  // Dimensioni della nuvola
  int size = 40;

  // Disegno procedurale della nuvola
  int baseWidth = 30;  // Larghezza rettangolo base
  int baseHeight = 18; // Altezza rettangolo base
  
  // Posizione del rettangolo base
  int baseY = centerY;
  int baseX = centerX;
  
  // Disegniamo il rettangolo base
  display.drawRect(baseX - baseWidth/2, baseY - baseHeight, baseWidth, baseHeight, GxEPD_BLACK);
  
  // Dimensioni del semicerchio superiore (più stretto e proporzionato)
  int ovalWidth = 18;
  int ovalHeight = 12;
  int ovalY = baseY - baseHeight - 1;  // Collegato precisamente al rettangolo
  
  // Punto centrale dell'ovale
  int ovalCenterY = ovalY - ovalHeight/2;
  
  // Disegniamo l'ovale superiore
  for (int i = 0; i <= 180; i += 5) { // Step più piccoli per un contorno più pulito
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

// Helper: converte l'ID meteo OpenWeatherMap in codice icona (es. "10d", "01n")
static String getOpenWeatherIconCode(int weatherId, bool isNight) {
  char suffix = isNight ? 'n' : 'd';
  String code;
  if (weatherId >= 200 && weatherId < 300)      code = "11";        // Temporale
  else if (weatherId >= 300 && weatherId < 400) code = "09";        // Pioggerella
  else if (weatherId >= 500 && weatherId < 600) code = (weatherId < 510 ? "10" : "09"); // Pioggia
  else if (weatherId >= 600 && weatherId < 700) code = "13";        // Neve
  else if (weatherId >= 700 && weatherId < 800) code = "50";        // Nebbia/atmosfera
  else if (weatherId == 800)                    code = "01";        // Sereno
  else if (weatherId == 801)                    code = "02";        // Poche nubi
  else if (weatherId == 802)                    code = "03";        // Nubi sparse
  else if (weatherId >= 803 && weatherId <= 804)code = "04";        // Nubi dense
  else                                          code = "01";        // Default
  code += suffix;
  return code;
}

// Disegna l'icona meteo in base all'ID utilizzando SOLO BMP / fallback vettoriale
void drawWeatherIcon(int x, int y, int weatherId, bool isNight, int iconSize) {
  int iconCenterX = x + iconSize/2;
  int iconCenterY = y + iconSize/2;

  // PRIORITÀ 1: icone BMP (mappate via WeatherIcon)
  if (BMPHelper::begin()) {
    // Serial.println("[DISPLAY] Tentativo caricamento icona BMP...");
    // Usiamo ancora la mappatura di SVGHelper solo per ottenere il tipo di icona
    WeatherIcon icon = SVGHelper::getIconFromWeatherID(weatherId, isNight, currentWeather.wind_speed);

    if (BMPHelper::drawWeatherIcon(display, icon, x, y, iconSize)) {
      // Serial.println("[DISPLAY] ✓ Icona BMP caricata");
      // Se abbiamo condizioni serene di notte, aggiungiamo anche la luna (disegno geometrico)
      if (isNight && (weatherId == 800 || weatherId == 801)) {
        drawMoonPhase(iconCenterX - 30, iconCenterY - 65, currentWeather.moon_phase);
      }
      return;
    }
  }

  // PRIORITÀ 2: Fallback con icone OpenWeatherMap (disegnate da WeatherIcons.cpp)
  if (WeatherIcons::begin()) {
    String iconCode = getOpenWeatherIconCode(weatherId, isNight);
    if (WeatherIcons::drawWeatherIcon(display, iconCode, x, y, iconSize)) {
      // Serial.println("[DISPLAY] ✓ Usata icona WeatherIcons come fallback");
      if (isNight && (weatherId == 800 || weatherId == 801)) {
        drawMoonPhase(iconCenterX - 30, iconCenterY - 65, currentWeather.moon_phase);
      }
      return;
    }
  } else {
    // Serial.println("[DISPLAY] Impossibile inizializzare WeatherIcons");
  }
  
  // PRIORITÀ 3: Se non si riesce a caricare nessuna icona, mostra testo semplice
  // Serial.println("[DISPLAY] ✗ Nessuna icona disponibile");
  // Rimuovo rettangolo "gettone" come richiesto
  // display.drawRect(x, y, iconSize, iconSize, GxEPD_BLACK);
  
  display.setFont(&FreeSerif9pt7b);
  
  // Calcola posizione centrata per il testo
  int16_t tbx, tby; uint16_t tbw, tbh;
  const char* errMsg = "Icona non trovata";
  display.getTextBounds(errMsg, 0, 0, &tbx, &tby, &tbw, &tbh);
  
  // Centra il testo nello spazio dell'icona
  int textX = x + (iconSize - tbw) / 2;
  int textY = y + iconSize / 2;
  
  display.setCursor(textX, textY);
  display.print(errMsg);
  return;
}

// [RIMOSSO] duplicato getOpenWeatherIconCode(non static)

// Mostra informazioni sul dispositivo
//void drawSwapInfo(int x, int y) {
//  display.setFont(NULL);
//  display.setCursor(x, y);
//  display.print("SWAP: ");
//}

// Funzione per visualizzare messaggi di errore
void displayError(const char* message) {
  showMessage("Errore", message);
}

// Funzione per mostrare schermata SD mancante
void showSDCardMissing() {
  showMessage("Scheda SD non trovata", "Inserisci una microSD formattata FAT32 e riavvia AtmoVerse.");
}

// Mostra conferma visiva di salvataggio configurazione
void showConfigSaved() {
  showMessage("Impostazioni salvate", "AtmoVerse si riavvia tra pochi istanti.");
  // Subito dopo il chiamante riavvia: il messaggio deve essere sul pannello
  waitDisplayIdle(10000);
}

// [RIMOSSO] duplicato drawCityInfo

// Funzione per disegnare la temperatura
void drawTemperature(int x, int y, float temp, float feelsLike) {
  display.setFont(&FreeSansBold12pt7b);
  display.setCursor(x, y);
  display.print(String(temp, 1));
  display.print("\xB0"); // Simbolo gradi
  display.print("C");
  
  display.setFont(&FreeSerif9pt7b);
  display.setCursor(x, y + 25);
  display.print("Percepita: ");
  display.print(String(feelsLike, 1));
  display.print("\xB0");
  display.print("C");
}
// Funzione per disegnare l'umidità
void drawHumidity(int x, int y, float humidity) {
  display.setFont(&FreeSerif9pt7b);
  display.setCursor(x, y);
  display.print("Umidità: ");
  display.print(String(humidity, 0));
  display.print("%");
}
// Funzione per disegnare la pressione
void drawPressure(int x, int y, float pressure) {
  display.setFont(&FreeSerif9pt7b);
  display.setCursor(x, y);
  display.print("Pressione: ");
  display.print(String(pressure, 0));
  display.print(" hPa");
}
// Funzione per disegnare il vento
void drawWind(int x, int y, float windSpeed) {
  display.setFont(&FreeSerif9pt7b);
  display.setCursor(x, y);
  display.print("Vento: ");
  display.print(String(windSpeed, 1));
  display.print(" m/s");
}
// Funzione per disegnare data e ora
void drawDateTime(int x, int y) {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  
  display.setFont(&FreeSansBold12pt7b);
  display.setCursor(x, y);
  
  // Format: 15:30
  char timeStr[6];
  sprintf(timeStr, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  display.print(timeStr);
  
  display.setFont(&FreeSerif9pt7b);
  display.setCursor(x, y + 25);
  
  // Format: 15/05/2025
  char dateStr[11];
  sprintf(dateStr, "%02d/%02d/%04d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
  display.print(dateStr);
}

// Funzione per disegnare le informazioni della città
void drawCityInfo(int x, int y, const char* cityName) {
  display.setFont(&FreeSerif9pt7b);
  display.setCursor(x, y);
  display.print(cityName);
}

// Funzione per disegnare l'ultimo aggiornamento
void drawLastUpdate(int x, int y, time_t lastUpdate) {
  // Font più piccolo per l'ultimo aggiornamento
  display.setFont(NULL);
  
  // Assicura margine minimo dai bordi (max 30px dal fondo, min 10px da sinistra)
  if (x < 10) x = 10;
  if (y > display.height() - 30) y = display.height() - 30;
  
  display.setCursor(x, y);

  struct tm timeinfo;
  localtime_r(&lastUpdate, &timeinfo);

  char timeStr[20];
  sprintf(timeStr, "Aggiornato: %02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  display.print(timeStr);
}

// [RIMOSSO] duplicati drawHumidity/drawPressure/drawWind

// Funzione per disegnare una citazione nella parte inferiore del display
void drawQuote(int x, int y, int maxWidth, int fontSize) {
  // Recupera una citazione dinamica in base a meteo/momento della giornata
  Quote q = getQuoteForDisplay();

  // Seleziona il font in base al fontSize richiesto
  if (fontSize >= 20) {
    display.setFont(&FreeSansBold18pt7b);
  } else if (fontSize >= 16) {
    display.setFont(&FreeSansBold12pt7b);
  } else if (fontSize >= 12) {
    display.setFont(&FreeSerif12pt7b);  // Serif leggibile
  } else if (fontSize >= 9) {
    display.setFont(&FreeSerif9pt7b);
  } else {
    display.setFont(NULL);  // Font di default piccolo
  }
  
  // Calcola l'altezza di una riga per calcolare lo spazio disponibile
  int16_t tbx_test, tby_test; uint16_t tbw_test, tbh_test;
  display.getTextBounds("Ag", 0, 0, &tbx_test, &tby_test, &tbw_test, &tbh_test);
  int lineHeight = tbh_test + 6; // Altezza riga + spaziatura
  int authorHeight = tbh_test + 8; // Spazio per l'autore sotto la citazione
  
  // Calcola il numero massimo di righe che possono stare nello spazio disponibile
  // Lasciamo sempre spazio per l'autore (minimo 30px dal fondo del display)
  int availableHeight = display.height() - y - 30 - authorHeight;
  int maxLines = max(1, availableHeight / lineHeight); // Minimo 1 riga
  if (maxLines > 4) maxLines = 4; // Limite massimo di 4 righe per leggibilità
  
  // Wrapping manuale del testo della citazione
  int16_t cursorX = x;
  int16_t cursorY = y;
  String word = "";
  String line = "";
  int lineWidth = 0;
  const char* quote = q.text.c_str();
  int lineCount = 1;
  bool truncatedQuote = false;
  
  for (int i = 0; i < (int)strlen(quote); i++) {
    if (quote[i] == ' ' || i == (int)strlen(quote) - 1) {
      // Aggiungi l'ultimo carattere se siamo alla fine della citazione
      if (i == (int)strlen(quote) - 1 && quote[i] != ' ') {
        word += quote[i];
      }
      
      // Calcola la larghezza della parola
      int16_t tbx, tby; uint16_t tbw, tbh;
      display.getTextBounds(word.c_str(), 0, 0, &tbx, &tby, &tbw, &tbh);
      
      // Se aggiungere questa parola supera la larghezza massima, vai a capo
      if (lineWidth + tbw > maxWidth) {
        display.setCursor(cursorX, cursorY);
        display.print(line);
        // vai a capo
        cursorY += tbh + 6;  // Spaziatura leggermente maggiore
        line = word + " ";
        lineWidth = tbw + 6; // 6: spazio approssimativo
        lineCount++;
        if (lineCount > maxLines) {
          // Occorre troncare la riga precedente con ellissi
          // Torna indietro alla riga corrente e stampa versione troncata
          const String ellipsis = "...";
          int16_t ebx, eby; uint16_t ebw, ebh;
          display.getTextBounds(ellipsis.c_str(), 0, 0, &ebx, &eby, &ebw, &ebh);
          // Ricomponi l'ultima riga valida (quella prima dell'a capo)
          String lastLine = line; // contiene già la parola corrente
          // Riduci lastLine finché sta nello spazio disponibile con ellissi
          while (lastLine.length() > 0) {
            int16_t lbx, lby; uint16_t lbw, lbh;
            display.getTextBounds(lastLine.c_str(), 0, 0, &lbx, &lby, &lbw, &lbh);
            if (lbw + ebw <= maxWidth) break;
            lastLine.remove(lastLine.length() - 1);
          }
          // Posizionati sulla riga precedente (cursorY è già avanzato di una riga): ripristina Y di una riga
          cursorY -= (tbh + 6);
          display.setCursor(cursorX, cursorY);
          display.print(lastLine + ellipsis);
          // Imposta flag di troncamento e salta il resto
          truncatedQuote = true;
          break;
        }
      } else {
        line += word + " ";
        lineWidth += tbw + 6;
      }
      word = "";
    } else {
      word += quote[i];
    }
  }
  
  // Stampa l'ultima riga della citazione
  if (!truncatedQuote && line.length() > 0) {
    display.setCursor(cursorX, cursorY);
    display.print(line);
  }
  
  // Spazio e autore su riga successiva, obliquo
  int16_t tbx, tby; uint16_t tbw, tbh;
  display.getTextBounds("Ag", 0, 0, &tbx, &tby, &tbw, &tbh); // misura altezza riga
  cursorY += tbh + 14; // ulteriore margine prima dell'autore (più distanziato dalla citazione)
  
  // NON forzare la posizione - se abbiamo calcolato bene maxLines, dovrebbe stare
  // Aggiungi solo un controllo di sicurezza per evitare di uscire dal display
  if (cursorY > display.height() - 20) {
    cursorY = display.height() - 20;
  }
  
  // Font autore: obliquo piccolo
  display.setFont(fontSize >= 12 ? &FreeMonoBoldOblique9pt7b : NULL);

  // Misura altezza riga autore
  int16_t abx, aby; uint16_t abw, abh;
  display.getTextBounds("Ag", 0, 0, &abx, &aby, &abw, &abh);
  int authorLineH = abh + 4;
  int maxAuthorWidth = maxWidth - 6;

  // Separa autore da opera alla prima virgola
  String authorStr = q.author.length() ? q.author : "Anonimo";
  int commaIdx = authorStr.indexOf(',');
  String authorLine1, authorLine2;
  if (commaIdx > 0) {
    authorLine1 = String("\u2014 ") + authorStr.substring(0, commaIdx) + ",";
    authorLine2 = authorStr.substring(commaIdx + 1);
    authorLine2.trim();
  } else {
    authorLine1 = String("\u2014 ") + authorStr;
    authorLine2 = "";
  }

  // Riga 1: autore, allineata a destra
  int16_t w1x, w1y; uint16_t w1w, w1h;
  display.getTextBounds(authorLine1.c_str(), 0, 0, &w1x, &w1y, &w1w, &w1h);
  while (authorLine1.length() > 2 && (int)w1w > maxAuthorWidth) {
    authorLine1.remove(authorLine1.length() - 1);
    display.getTextBounds(authorLine1.c_str(), 0, 0, &w1x, &w1y, &w1w, &w1h);
  }
  int authorX1 = x + maxWidth - (int)w1w - 3;
  if (authorX1 < x + 3) authorX1 = x + 3;
  display.setCursor(authorX1, cursorY);
  display.print(authorLine1);

  // Riga 2: opera, allineata a destra
  if (authorLine2.length() > 0 && cursorY + authorLineH <= display.height() - 8) {
    cursorY += authorLineH;
    int16_t w2x, w2y; uint16_t w2w, w2h;
    display.getTextBounds(authorLine2.c_str(), 0, 0, &w2x, &w2y, &w2w, &w2h);
    while (authorLine2.length() > 1 && (int)w2w > maxAuthorWidth) {
      authorLine2.remove(authorLine2.length() - 1);
      display.getTextBounds(authorLine2.c_str(), 0, 0, &w2x, &w2y, &w2w, &w2h);
    }
    int authorX2 = x + maxWidth - (int)w2w - 3;
    if (authorX2 < x + 3) authorX2 = x + 3;
    display.setCursor(authorX2, cursorY);
    display.print(authorLine2);
  }
}

// Funzione per disegnare lo stato della batteria
void drawBattery(int x, int y, int percentage) {
  // Dichiarazione esterna dell'istanza battery
  extern BatteryManager battery;
  
  // Dimensioni icona batteria
  int width = 25;
  int height = 12;

  // Assicura margini di sicurezza
  if (x > display.width() - width - 10) x = display.width() - width - 10;
  if (y > display.height() - height - 4) y = display.height() - height - 4;
  if (x < 0) x = 0;
  if (y < 0) y = 0;

  int safePercentage = percentage;
  if (!battery.isAvailable()) {
    safePercentage = 0;
  }

  // Contorno batteria + terminale
  display.drawRect(x, y, width, height, GxEPD_BLACK);
  display.drawRect(x + width, y + 3, 2, height - 6, GxEPD_BLACK);

  if (battery.charging()) {
    // IN CARICA: riempimento nero totale + simbolo USB in bianco
    display.fillRect(x + 1, y + 1, width - 2, height - 2, GxEPD_BLACK);

    // Testo "-" a sinistra e "+" a destra in bianco dentro la batteria nera
    display.setFont(NULL);
    display.setTextSize(1);
    display.setTextColor(GxEPD_WHITE);
    display.setCursor(x + 2, y + 2);
    display.print("-");
    display.setCursor(x + 16, y + 2);
    display.print("+");

  } else {
    // SCARICA: 4 segmenti verticali in base alla percentuale
    int segWidth = 4;  // larghezza singolo segmento
    int segGap = 1;    // spazio tra segmenti
    int segX = x + 2;  // inizio primo segmento
    int segY = y + 2;
    int segH = height - 4;
    
    // Attiva segmenti in base alla percentuale (0-25-50-75-100)
    int activeSegments = safePercentage / 25;  // 0-24=0, 25-49=1, 50-74=2, 75-99=3, 100=4
    if (safePercentage >= 100) activeSegments = 4;
    
    for (int i = 0; i < 4; i++) {
      if (i < activeSegments) {
        display.fillRect(segX + i * (segWidth + segGap), segY, segWidth, segH, GxEPD_BLACK);
      } else {
        display.drawRect(segX + i * (segWidth + segGap), segY, segWidth, segH, GxEPD_BLACK);
      }
    }
  }

  // Percentuale a destra, centrata verticalmente (font=8px, height=12 → y+2)
  display.setFont(NULL);
  display.setTextSize(1);
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(x + width + 5, y + 2);
  display.print(String(safePercentage) + "%");
}
// Funzione per disegnare una barra di progresso
void drawProgress(int x, int y, int width, int progress) {
  display.drawRect(x, y, width, 10, GxEPD_BLACK);
  display.fillRect(x + 1, y + 1, map(progress, 0, 100, 0, width - 2), 8, GxEPD_BLACK);
}
// ============================================================================
// FUNZIONI PER GRAFICA MIGLIORATA
// ============================================================================

// Disegna un singolo digit grande in stile 7-segment
void drawBigDigit(int x, int y, int digit, int height) {
  // height è l'altezza totale della cifra
  int w = height * 0.5;  // Larghezza proporzionale all'altezza
  int thick = height * 0.15;  // Spessore dei segmenti
  
  // Definizione segmenti 7-segment: [a,b,c,d,e,f,g]
  // a=top, b=top-right, c=bottom-right, d=bottom, e=bottom-left, f=top-left, g=middle
  bool segments[10][7] = {
    {1,1,1,1,1,1,0}, // 0
    {0,1,1,0,0,0,0}, // 1
    {1,1,0,1,1,0,1}, // 2
    {1,1,1,1,0,0,1}, // 3
    {0,1,1,0,0,1,1}, // 4
    {1,0,1,1,0,1,1}, // 5
    {1,0,1,1,1,1,1}, // 6
    {1,1,1,0,0,0,0}, // 7
    {1,1,1,1,1,1,1}, // 8
    {1,1,1,1,0,1,1}  // 9
  };
  
  if (digit < 0 || digit > 9) return;
  
  int halfH = height / 2;
  
  // Segmento a (top horizontal)
  if (segments[digit][0]) {
    display.fillRect(x + thick, y, w - 2*thick, thick, GxEPD_BLACK);
  }
  
  // Segmento b (top-right vertical)
  if (segments[digit][1]) {
    display.fillRect(x + w - thick, y + thick, thick, halfH - thick, GxEPD_BLACK);
  }
  
  // Segmento c (bottom-right vertical)
  if (segments[digit][2]) {
    display.fillRect(x + w - thick, y + halfH, thick, halfH - thick, GxEPD_BLACK);
  }
  
  // Segmento d (bottom horizontal)
  if (segments[digit][3]) {
    display.fillRect(x + thick, y + height - thick, w - 2*thick, thick, GxEPD_BLACK);
  }
  
  // Segmento e (bottom-left vertical)
  if (segments[digit][4]) {
    display.fillRect(x, y + halfH, thick, halfH - thick, GxEPD_BLACK);
  }
  
  // Segmento f (top-left vertical)
  if (segments[digit][5]) {
    display.fillRect(x, y + thick, thick, halfH - thick, GxEPD_BLACK);
  }
  
  // Segmento g (middle horizontal)
  if (segments[digit][6]) {
    display.fillRect(x + thick, y + halfH - thick/2, w - 2*thick, thick, GxEPD_BLACK);
  }
}

// Disegna un numero grande (temperatura) con dimensione personalizzata
void drawBigNumber(int x, int y, float number, int height, bool showDecimal) {
  int digitHeight = height;
  int digitWidth = digitHeight * 0.5;
  int spacing = digitHeight * 0.15;
  
  char buffer[10];
  if (showDecimal) {
    sprintf(buffer, "%.1f", number);
  } else {
    sprintf(buffer, "%d", (int)number);
  }
  
  int cursorX = x;
  
  for (int i = 0; i < strlen(buffer); i++) {
    if (buffer[i] >= '0' && buffer[i] <= '9') {
      drawBigDigit(cursorX, y, buffer[i] - '0', digitHeight);
      cursorX += digitWidth + spacing;
    } else if (buffer[i] == '.') {
      // Punto decimale (piccolo quadrato)
      int dotSize = digitHeight * 0.12;
      display.fillRect(cursorX, y + digitHeight - dotSize - spacing, dotSize, dotSize, GxEPD_BLACK);
      cursorX += dotSize + spacing * 2;
    } else if (buffer[i] == '-') {
      // Segno meno
      int minusW = digitWidth * 0.6;
      int minusH = digitHeight * 0.12;
      display.fillRect(cursorX, y + digitHeight/2 - minusH/2, minusW, minusH, GxEPD_BLACK);
      cursorX += minusW + spacing;
    }
  }
  
  // Simbolo di grado (°)
  int degreeSize = digitHeight * 0.25;
  display.drawCircle(cursorX + spacing, y + degreeSize/2, degreeSize/2, GxEPD_BLACK);
  display.drawCircle(cursorX + spacing, y + degreeSize/2, degreeSize/2 - 2, GxEPD_BLACK);
}

// Disegna un'icona minimalista di termometro
void drawThermometerIcon(int x, int y, int size) {
  int bulbRadius = size / 3;
  int tubeWidth = size / 5;
  int tubeHeight = size - bulbRadius;
  
  // Bulbo del termometro (cerchio in basso)
  display.fillCircle(x + size/2, y + size - bulbRadius, bulbRadius, GxEPD_BLACK);
  
  // Tubo del termometro (rettangolo sopra)
  int tubeX = x + (size - tubeWidth) / 2;
  int tubeY = y;
  display.fillRect(tubeX, tubeY, tubeWidth, tubeHeight, GxEPD_BLACK);
  
  // Interno bianco del tubo per effetto 3D
  display.fillRect(tubeX + 1, tubeY, tubeWidth - 2, tubeHeight - bulbRadius, GxEPD_WHITE);
  
  // Lineette di misurazione
  for (int i = 0; i < 3; i++) {
    int lineY = tubeY + (tubeHeight / 4) * i;
    display.drawLine(tubeX - 2, lineY, tubeX, lineY, GxEPD_BLACK);
  }
}

// Disegna un'icona minimalista di goccia d'acqua (umidità)
void drawDropletIcon(int x, int y, int size) {
  // Forma a goccia usando cerchi e triangolo
  int dropWidth = size;
  int dropHeight = size * 1.2;
  
  // Parte superiore (punta della goccia)
  int tipX = x + dropWidth / 2;
  int tipY = y;
  
  // Parte inferiore (corpo della goccia - cerchio)
  int bodyRadius = dropWidth / 2;
  int bodyX = tipX;
  int bodyY = y + dropHeight - bodyRadius;
  
  // Disegna il corpo (cerchio)
  display.drawCircle(bodyX, bodyY, bodyRadius, GxEPD_BLACK);
  
  // Disegna i lati della goccia (linee curve simulate)
  display.drawLine(tipX, tipY, tipX - bodyRadius, bodyY, GxEPD_BLACK);
  display.drawLine(tipX, tipY, tipX + bodyRadius, bodyY, GxEPD_BLACK);
  
  // Riempimento interno con pattern per effetto acqua
  for (int i = 0; i < bodyRadius - 2; i++) {
    int lineY = bodyY - bodyRadius + 2 + i;
    int lineWidth = sqrt(bodyRadius * bodyRadius - i * i) * 2 - 4;
    display.drawLine(bodyX - lineWidth/2, lineY, bodyX + lineWidth/2, lineY, GxEPD_BLACK);
  }
}

// Disegna un'icona minimalista di vento
void drawWindIcon(int x, int y, int size) {
  // Tre linee curve di diverse lunghezze per rappresentare il vento
  int lineSpacing = size / 4;
  
  // Prima linea (la più lunga)
  display.drawLine(x, y, x + size, y, GxEPD_BLACK);
  display.drawLine(x + size, y, x + size, y + lineSpacing/2, GxEPD_BLACK);
  
  // Seconda linea (media)
  display.drawLine(x + lineSpacing/2, y + lineSpacing, x + size - lineSpacing/2, y + lineSpacing, GxEPD_BLACK);
  display.drawLine(x + size - lineSpacing/2, y + lineSpacing, x + size - lineSpacing/2, y + lineSpacing * 1.5, GxEPD_BLACK);
  
  // Terza linea (la più corta)
  display.drawLine(x + lineSpacing, y + lineSpacing * 2, x + size - lineSpacing, y + lineSpacing * 2, GxEPD_BLACK);
}

// Disegna un separatore orizzontale elegante
void drawHorizontalDivider(int x, int y, int width, bool decorative) {
  if (decorative) {
    // Stile decorativo con rombo centrale
    int centerX = x + width / 2;
    
    // Linea sinistra
    display.drawLine(x, y, centerX - 10, y, GxEPD_BLACK);
    
    // Rombo centrale
    display.drawLine(centerX - 8, y, centerX, y - 4, GxEPD_BLACK);
    display.drawLine(centerX, y - 4, centerX + 8, y, GxEPD_BLACK);
    display.drawLine(centerX + 8, y, centerX, y + 4, GxEPD_BLACK);
    display.drawLine(centerX, y + 4, centerX - 8, y, GxEPD_BLACK);
    
    // Linea destra
    display.drawLine(centerX + 10, y, x + width, y, GxEPD_BLACK);
  } else {
    // Stile semplice
    display.drawLine(x, y, x + width, y, GxEPD_BLACK);
  }
}

// Disegna un separatore verticale elegante
void drawVerticalDivider(int x, int y, int height) {
  display.drawLine(x, y, x, y + height, GxEPD_BLACK);
}

// Disegna un box con bordi arrotondati
void drawRoundedBox(int x, int y, int width, int height, int radius, bool filled) {
  if (filled) {
    // Box riempito
    display.fillRect(x + radius, y, width - 2 * radius, height, GxEPD_BLACK);
    display.fillRect(x, y + radius, width, height - 2 * radius, GxEPD_BLACK);
    
    // Angoli arrotondati
    display.fillCircle(x + radius, y + radius, radius, GxEPD_BLACK);
    display.fillCircle(x + width - radius, y + radius, radius, GxEPD_BLACK);
    display.fillCircle(x + radius, y + height - radius, radius, GxEPD_BLACK);
    display.fillCircle(x + width - radius, y + height - radius, radius, GxEPD_BLACK);
  } else {
    // Solo bordo
    // Linee orizzontali
    display.drawLine(x + radius, y, x + width - radius, y, GxEPD_BLACK);
    display.drawLine(x + radius, y + height - 1, x + width - radius, y + height - 1, GxEPD_BLACK);
    
    // Linee verticali
    display.drawLine(x, y + radius, x, y + height - radius, GxEPD_BLACK);
    display.drawLine(x + width - 1, y + radius, x + width - 1, y + height - radius, GxEPD_BLACK);
    
    // Angoli arrotondati (quarti di cerchio)
    // In alto a sinistra
    for (int i = 0; i <= 90; i += 5) {
      float angle = i * PI / 180.0;
      int x1 = x + radius - radius * cos(angle);
      int y1 = y + radius - radius * sin(angle);
      int x2 = x + radius - radius * cos((i+5) * PI / 180.0);
      int y2 = y + radius - radius * sin((i+5) * PI / 180.0);
      display.drawLine(x1, y1, x2, y2, GxEPD_BLACK);
    }
    // In alto a destra
    for (int i = 0; i <= 90; i += 5) {
      float angle = i * PI / 180.0;
      int x1 = x + width - radius + radius * sin(angle);
      int y1 = y + radius - radius * cos(angle);
      int x2 = x + width - radius + radius * sin((i+5) * PI / 180.0);
      int y2 = y + radius - radius * cos((i+5) * PI / 180.0);
      display.drawLine(x1, y1, x2, y2, GxEPD_BLACK);
    }
    // In basso a sinistra
    for (int i = 0; i <= 90; i += 5) {
      float angle = i * PI / 180.0;
      int x1 = x + radius - radius * sin(angle);
      int y1 = y + height - radius + radius * cos(angle);
      int x2 = x + radius - radius * sin((i+5) * PI / 180.0);
      int y2 = y + height - radius + radius * cos((i+5) * PI / 180.0);
      display.drawLine(x1, y1, x2, y2, GxEPD_BLACK);
    }
    // In basso a destra
    for (int i = 0; i <= 90; i += 5) {
      float angle = i * PI / 180.0;
      int x1 = x + width - radius + radius * cos(angle);
      int y1 = y + height - radius + radius * sin(angle);
      int x2 = x + width - radius + radius * cos((i+5) * PI / 180.0);
      int y2 = y + height - radius + radius * sin((i+5) * PI / 180.0);
      display.drawLine(x1, y1, x2, y2, GxEPD_BLACK);
    }
  }
}

// Disegna una cornice decorativa attorno al display
void drawDecorativeFrame(int margin) {
  int width = display.width();
  int height = display.height();
  
  // Doppio bordo per effetto elegante
  display.drawRect(margin, margin, width - 2*margin, height - 2*margin, GxEPD_BLACK);
  display.drawRect(margin + 2, margin + 2, width - 2*margin - 4, height - 2*margin - 4, GxEPD_BLACK);
  
  // Angoli decorativi
  int cornerSize = 15;
  
  // Angolo in alto a sinistra
  display.drawLine(margin + cornerSize, margin, margin + cornerSize, margin + cornerSize, GxEPD_BLACK);
  display.drawLine(margin, margin + cornerSize, margin + cornerSize, margin + cornerSize, GxEPD_BLACK);
  
  // Angolo in alto a destra
  display.drawLine(width - margin - cornerSize, margin, width - margin - cornerSize, margin + cornerSize, GxEPD_BLACK);
  display.drawLine(width - margin - cornerSize, margin + cornerSize, width - margin, margin + cornerSize, GxEPD_BLACK);
  
  // Angolo in basso a sinistra
  display.drawLine(margin, height - margin - cornerSize, margin + cornerSize, height - margin - cornerSize, GxEPD_BLACK);
  display.drawLine(margin + cornerSize, height - margin - cornerSize, margin + cornerSize, height - margin, GxEPD_BLACK);
  
  // Angolo in basso a destra
  display.drawLine(width - margin - cornerSize, height - margin - cornerSize, width - margin, height - margin - cornerSize, GxEPD_BLACK);
  display.drawLine(width - margin - cornerSize, height - margin - cornerSize, width - margin - cornerSize, height - margin, GxEPD_BLACK);
}

// Disegna dati meteo con icone
void drawWeatherDataWithIcons(int x, int y, int iconSize) {
  int lineHeight = 30;
  int iconOffset = iconSize + 5;
  
  // Temperatura con icona termometro
  drawThermometerIcon(x, y, iconSize);
  display.setFont(&FreeSansBold12pt7b);
  display.setCursor(x + iconOffset, y + iconSize);
  display.print(String(currentWeather.temp, 1) + "\xB0" + "C");
  
  // Umidità con icona goccia
  drawDropletIcon(x, y + lineHeight, iconSize);
  display.setFont(&FreeSerif9pt7b);
  display.setCursor(x + iconOffset, y + lineHeight + iconSize - 5);
  display.print(String((int)currentWeather.humidity) + "%");
  
  // Vento con icona
  drawWindIcon(x, y + lineHeight * 2, iconSize);
  display.setCursor(x + iconOffset, y + lineHeight * 2 + iconSize - 5);
  display.print(String(currentWeather.wind_speed, 1) + " km/h");
}

// ============================================================================
// SCHERMATA ERRORE SD CARD - Versione Semplificata
// ============================================================================

void drawSDCardError() {
  // Bordo semplice
  display.drawRect(10, 10, display.width()-20, display.height()-20, GxEPD_BLACK);
  
  // Titolo in alto
  display.setFont(&FreeSansBold12pt7b);
  int16_t tbx, tby; uint16_t tbw, tbh;
  const char* title = "SD Card non trovata";
  display.getTextBounds(title, 0, 0, &tbx, &tby, &tbw, &tbh);
  display.setCursor((display.width() - tbw) / 2, 60);
  display.print(title);
  
  // Icona SD semplificata al centro
  int cardX = (display.width() - 100) / 2;
  int cardY = 100;
  
  // Corpo SD card (rettangolo)
  display.drawRect(cardX, cardY, 100, 120, GxEPD_BLACK);
  display.drawRect(cardX+1, cardY+1, 98, 118, GxEPD_BLACK);
  
  // Angolo tagliato (linea diagonale in alto a destra)
  display.drawLine(cardX + 80, cardY, cardX + 100, cardY + 20, GxEPD_BLACK);
  display.drawLine(cardX + 81, cardY, cardX + 100, cardY + 19, GxEPD_BLACK);
  
  // Contatti in alto (4 rettangoli)
  for (int i = 0; i < 4; i++) {
    display.fillRect(cardX + 15 + (i * 17), cardY + 25, 12, 15, GxEPD_BLACK);
  }
  
  // Testo "SD" sulla card
  display.setFont(&FreeSansBold12pt7b);
  display.getTextBounds("SD", 0, 0, &tbx, &tby, &tbw, &tbh);
  display.setCursor(cardX + 50 - tbw/2, cardY + 70);
  display.print("SD");
  
  // Grande X sulla card (linee dritte incrociate)
  display.drawLine(cardX + 20, cardY + 85, cardX + 80, cardY + 110, GxEPD_BLACK);
  display.drawLine(cardX + 21, cardY + 85, cardX + 81, cardY + 110, GxEPD_BLACK);
  display.drawLine(cardX + 20, cardY + 110, cardX + 80, cardY + 85, GxEPD_BLACK);
  display.drawLine(cardX + 21, cardY + 110, cardX + 81, cardY + 85, GxEPD_BLACK);
  
  // Messaggio sotto la card
  display.setFont(&FreeSerif12pt7b);
  const char* msg = "Inserisci la SD e riavvia";
  display.getTextBounds(msg, 0, 0, &tbx, &tby, &tbw, &tbh);
  display.setCursor((display.width() - tbw) / 2, cardY + 160);
  display.print(msg);
  
  // Faccina triste semplice (senza sqrt)
  int faceY = display.height() - 100;
  int faceX = display.width() / 2;
  
  // Occhi (cerchi pieni)
  display.fillCircle(faceX - 20, faceY, 4, GxEPD_BLACK);
  display.fillCircle(faceX + 20, faceY, 4, GxEPD_BLACK);
  
  // Bocca triste (arco semplice con linee)
  display.drawLine(faceX - 25, faceY + 20, faceX - 15, faceY + 25, GxEPD_BLACK);
  display.drawLine(faceX - 15, faceY + 25, faceX, faceY + 27, GxEPD_BLACK);
  display.drawLine(faceX, faceY + 27, faceX + 15, faceY + 25, GxEPD_BLACK);
  display.drawLine(faceX + 15, faceY + 25, faceX + 25, faceY + 20, GxEPD_BLACK);
  
  // Footer informativo
  display.setFont(&FreeSerif9pt7b);
  const char* footer = "Il dispositivo necessita della SD";
  display.getTextBounds(footer, 0, 0, &tbx, &tby, &tbw, &tbh);
  display.setCursor((display.width() - tbw) / 2, display.height() - 30);
  display.print(footer);
}

// ============================================================================
// LAYOUT ENHANCED - Design Migliorato
// ============================================================================

void drawEnhancedLayout() {
  // Cornice decorativa opzionale
  bool showFrame = true; // Imposta a false per disabilitare
  int margin = showFrame ? 12 : 5;
  
  if (showFrame) {
    drawDecorativeFrame(margin - 2);
  }
  
  // SEZIONE SUPERIORE: Città e Data/Ora
  display.setFont(&FreeSansBold12pt7b);
  display.setCursor(margin + 10, margin + 25);
  display.print(config.city);
  
  // Data e ora in alto a destra
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  
  char timeStr[6];
  sprintf(timeStr, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  
  int16_t tbx, tby; uint16_t tbw, tbh;
  display.setFont(&FreeSansBold12pt7b);
  display.getTextBounds(timeStr, 0, 0, &tbx, &tby, &tbw, &tbh);
  display.setCursor(display.width() - margin - tbw - 10, margin + 25);
  display.print(timeStr);
  
  // Separatore decorativo
  drawHorizontalDivider(margin + 10, margin + 35, display.width() - 2*margin - 20, true);
  
  // SEZIONE CENTRALE: Icona Meteo Grande + Dati con Icone
  int centerY = margin + 50;
  
  // Box per i dati meteo (sinistra)
  int dataBoxX = margin + 10;
  int dataBoxY = centerY;
  int dataBoxWidth = 150;
  int dataBoxHeight = 120;
  
  drawRoundedBox(dataBoxX, dataBoxY, dataBoxWidth, dataBoxHeight, 8, false);
  
  // Dati meteo con icone dentro il box
  drawWeatherDataWithIcons(dataBoxX + 15, dataBoxY + 20, 18);
  
  // Pressione in basso nel box
  display.setFont(&FreeSerif9pt7b);
  display.setCursor(dataBoxX + 15, dataBoxY + dataBoxHeight - 10);
  display.print(String((int)currentWeather.pressure) + " hPa");
  
  // Icona meteo grande (centro)
  int iconSize = 140;
  int iconX = dataBoxX + dataBoxWidth + 20;
  int iconY = centerY + (dataBoxHeight - iconSize) / 2;
  
  drawWeatherIcon(iconX, iconY, currentWeather.weather_id, isNightTime());
  
  // Calendario a destra
  int calX = display.width() - margin - 165;
  int calY = centerY;
  drawCalendar(calX, calY, 155, 160);
  
  // SEZIONE CITAZIONE
  int quoteY = centerY + dataBoxHeight + 15;
  
  // Separatore prima della citazione
  drawHorizontalDivider(margin + 10, quoteY, display.width() - 2*margin - 20, false);
  
  // Box per la citazione
  int quoteBoxX = margin + 10;
  int quoteBoxY = quoteY + 10;
  int quoteBoxWidth = display.width() - 2*margin - 20;
  int quoteBoxHeight = 85;
  
  drawRoundedBox(quoteBoxX, quoteBoxY, quoteBoxWidth, quoteBoxHeight, 6, false);
  
  // Citazione dentro il box
  drawQuote(quoteBoxX + 10, quoteBoxY + 20, quoteBoxWidth - 20);
  
  // FOOTER
  int footerY = display.height() - margin - 25;
  
  // Separatore footer
  drawHorizontalDivider(margin + 10, footerY - 5, display.width() - 2*margin - 20, false);
  
  // Ultimo aggiornamento (sinistra)
  drawLastUpdate(margin + 10, footerY + 15, currentWeather.last_update);
  
  // IP (centro)
  display.setFont(NULL);
  String ipString = WiFi.status() == WL_CONNECTED ? 
                    ("IP: " + WiFi.localIP().toString()) : "IP: N/A";
  display.getTextBounds(ipString.c_str(), 0, 0, &tbx, &tby, &tbw, &tbh);
  int ipX = (display.width() - tbw) / 2;
  display.setCursor(ipX, footerY + 15);
  display.print(ipString);
  
  // Batteria (destra)
  drawBattery(display.width() - margin - 70, footerY + 5, 80);
}

// [RIMOSSE] definizioni duplicate di drawBattery e drawProgress
