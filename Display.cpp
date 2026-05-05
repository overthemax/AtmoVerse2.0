#include "Display.h"
#include "Hardware.h"
#include <SD.h>
#include <ArduinoJson.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeSerif9pt7b.h>
#include <Fonts/FreeSerif12pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>    // Font sans-serif più moderno e leggibile
#include <Fonts/FreeSansBold18pt7b.h>    // Font sans-serif grande per citazioni
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
#include <string.h>

// Visualizza un semplice box di testo centrale con il messaggio passato
void showStatusOnDisplay(const char* msg) {
  DEBUG_TRACE("showStatusOnDisplay"); // added
  
  if (msg == nullptr) {
    // Serial.println("[DISPLAY] ERRORE: Tentativo di stampare messaggio NULL");
    msg = "Error: NULL message";
  }
  
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        // bordo
        display.drawRect(5, 5, display.width()-10, display.height()-10, GxEPD_BLACK);
        // testo centrato
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&FreeSerif12pt7b);
        int16_t tbx, tby; uint16_t tbw, tbh;
        display.getTextBounds(msg, 0, 0, &tbx, &tby, &tbw, &tbh);
        display.setCursor((display.width() - tbw) / 2, (display.height() + tbh) / 2);
        display.print(msg);
    } while (display.nextPage());
    // Serial.println(msg);
}

// Definizione pin per display e-ink già dichiarati nel file principale
// Usiamo solo la referenza all'oggetto display tramite extern

// Inizializzazione del display e-ink
void initDisplay() {
  DEBUG_TRACE("initDisplay"); // added
  // Serial.println("Inizializzazione display...");
  display.init(115200);
  display.setRotation(0);
  display.setTextColor(GxEPD_BLACK);
  display.setFullWindow();
  // Serial.println("Display inizializzato");
}

// Funzione per visualizzare la schermata di avvio
void displayStartupScreen() {
  DEBUG_TRACE("displayStartupScreen"); // added
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    
    // Bordo decorativo
    display.drawRect(5, 5, display.width()-10, display.height()-10, GxEPD_BLACK);
    
    // Logo AtmoVerse 2.0
    display.setFont(&FreeSerif12pt7b);
    int16_t tbx, tby; uint16_t tbw, tbh;
    display.getTextBounds("AtmoVerse 2.0", 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setCursor((display.width() - tbw) / 2, display.height() / 3);
    display.print("AtmoVerse 2.0");
    
    // Sottotitolo
    display.setFont(&FreeSerif9pt7b);
    display.getTextBounds("Sistema meteo con calendario", 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setCursor((display.width() - tbw) / 2, display.height() / 2);
    display.print("Sistema meteo con calendario");
    
    // Messaggio di caricamento invece della data
    display.getTextBounds("Caricamento...", 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setCursor((display.width() - tbw) / 2, display.height() * 2 / 3);
    display.print("Caricamento...");
    
    // Versione
    display.setFont(NULL);
    display.setCursor(display.width() / 2 - 30, display.height() - 20);
    display.print("v2.0 - 2025");
    
  } while (display.nextPage());
  
  // Serial.println("Schermata di avvio visualizzata");
}

// Disegna la temperatura usando cifre BMP in /fonts (0-9.bmp, degree.bmp)
// temp: temperatura in gradi Celsius
// glyphSize: dimensione massima del riquadro di ogni cifra
void drawTemperatureBMP(int x, int y, float temp, int glyphSize) {
  if (!initSD()) {
    return;
  }

  // Converte la temperatura in stringa formattata, es: "-5.3" -> "-5.3"
  // Limitiamo a una cifra decimale per leggibilità
  char buf[16];
  dtostrf(temp, 0, 1, buf);
  String t = String(buf);
  t.trim();

  // Rimuovi eventuali spazi iniziali generati da dtostrf
  while (t.startsWith(" ")) t.remove(0, 1);

  // Costruisci lista di simboli da disegnare: cifre, opzionale '-' e un solo '.'
  String symbols = "";
  bool decimalDrawn = false;
  for (size_t i = 0; i < t.length(); ++i) {
    char c = t[i];
    if (c == '-') {
      symbols += '-';
    } else if (c == '.' && !decimalDrawn) {
      symbols += '.';
      decimalDrawn = true;
    } else if (c >= '0' && c <= '9') {
      symbols += c;
    }
  }

  // Aggiungi simbolo dei gradi alla fine
  symbols += 'd'; // 'd' usato come placeholder per "degree"

  int cursorX = x;
  const int glyphSpacing = 0; // spazio extra minimo tra i glifi

  for (size_t i = 0; i < symbols.length(); ++i) {
    char c = symbols[i];
    const char* filename = nullptr;

    if (c >= '0' && c <= '9') {
      static char numPath[32];
      snprintf(numPath, sizeof(numPath), "/fonts/%c.bmp", c);
      filename = numPath;
    } else if (c == '.') {
      filename = "/fonts/dot.bmp";
    } else if (c == '-') {
      filename = "/fonts/colon.bmp"; // placeholder se non c'e' un BMP del meno
    } else if (c == 'd') {
      filename = "/fonts/degree.bmp";
    }

    if (filename) {
      BMPHelper::drawBMP(display, filename, cursorX, y, glyphSize, glyphSize);
      // Usa un advance piu' stretto del box per ridurre lo spazio percepito
      int advance;
      if (c >= '0' && c <= '9') {
        advance = (int)(glyphSize * 0.52f);
      } else if (c == 'd') {
        advance = (int)(glyphSize * 0.25f);
      } else {
        advance = (int)(glyphSize * 0.30f);
      }
      cursorX += advance + glyphSpacing;
    }
  }
}

// Funzione per visualizzare la schermata di configurazione
void displaySetupScreen(String apName, String ipAddress) {
  DEBUG_TRACE("displaySetupScreen"); // added
  // Serial.println("Visualizzazione schermata di configurazione...");
  
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    
    // Bordo esterno
    display.drawRect(5, 5, display.width()-10, display.height()-10, GxEPD_BLACK);
    
    // Indicatore batteria in basso a destra
    if (config.batteryMonitorEnabled) {
      int batteryPercentage = battery.getPercentage();
      
      // Disegna batteria nell'angolo in basso a destra
      int battX = display.width() - 80;
      int battY = display.height() - 60;
      drawBattery(battX, battY, batteryPercentage);
      
      // Testo percentuale sotto l'icona
    }
    
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
    display.setFont(&FreeSerif12pt7b);
    int16_t tbx, tby; uint16_t tbw, tbh;
    display.getTextBounds("AtmoVerse", 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setCursor((display.width() - tbw) / 2, 45);
    display.print("AtmoVerse");
    
    // Linea separatrice
    display.drawLine(20, 65, display.width() - 20, 65, GxEPD_BLACK);
    
    // Cambio da "Modalità Configurazione" a "Prima Configurazione"
    display.setFont(&FreeSerif12pt7b);
    const char* configTitle = "Prima Configurazione";
    display.getTextBounds(configTitle, 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setCursor((display.width() - tbw) / 2, 95);
    display.print(configTitle);
    
    // Istruzioni di connessione con font più grande
    // Aumento lo spazio tra le fasi
    int textY = 135;
    display.setFont(&FreeSerif9pt7b);
    
    // Passo 1 - Connessione alla rete AtmoVerse
    display.fillCircle(30, textY, 12, GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    display.setCursor(26, textY+4);
    display.print("1");
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(50, textY+4);
    // Uso font più grande per il titolo del passo
    display.setFont(&FreeSerif12pt7b);
    display.print("Rete: ");
    display.print(apName);
    
    // Spiegazione più concisa del passo 1
    display.setFont(&FreeSerif9pt7b);
    display.setCursor(50, textY+25);
    display.print("Cerca questa rete WiFi sul tuo dispositivo");
    
    // Passo 2 - Password per connettersi
    textY += 70; // Aumentata spaziatura tra le fasi
    display.fillCircle(30, textY, 12, GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    display.setCursor(26, textY+4);
    display.print("2");
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(50, textY+4);
    // Uso font più grande per il titolo del passo
    display.setFont(&FreeSerif12pt7b);
    display.print("Password: ");
    display.print(ATMOVERSE_AP_PASSWORD);
    
    // Spiegazione più concisa del passo 2
    display.setFont(&FreeSerif9pt7b);
    display.setCursor(50, textY+25);
    display.print("Inserisci questa password quando richiesto");
    
    // Passo 3 - Apertura browser
    textY += 70; // Aumentata spaziatura tra le fasi
    display.fillCircle(30, textY, 12, GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    display.setCursor(26, textY+4);
    display.print("3");
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(50, textY+4);
    // Uso font più grande per il titolo del passo
    display.setFont(&FreeSerif12pt7b);
    display.print("Apri nel browser:");
    
    // Indirizzo IP con font ancora più grande
    display.setFont(&FreeSerif12pt7b);
    display.setCursor(50, textY+35);
    display.print("http://192.168.4.1");
    
    // Spiegazione più concisa del passo 3
    display.setFont(&FreeSerif9pt7b);
    display.setCursor(50, textY+60);
    display.print("Configura il dispositivo e salva");
    
    
    // Versione in basso
    display.setFont(&FreeSerif9pt7b);
    display.setCursor(20, display.height() - 20);
    display.print("Calendario meteo integrato - AtmoVerse 2.0");
    
  } while (display.nextPage());
  
  // Serial.println("Schermata di configurazione visualizzata");
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

// Funzione che aggiorna solo l'ora (minimale)
void updateTimeOnly() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) return;
  
  char timeBuffer[10];
  strftime(timeBuffer, sizeof(timeBuffer), "%H:%M", &timeinfo);
  
  int16_t tbx, tby; uint16_t tbw, tbh;
  display.setFont(&FreeSerif12pt7b);
  display.getTextBounds(timeBuffer, 0, 0, &tbx, &tby, &tbw, &tbh);
  
  int x = 20 - 5;
  int y = 70 - tbh - 5;
  int w = tbw + 10;
  int h = tbh + 10;
  
  display.setPartialWindow(x, y, w, h);
  display.firstPage();
  do {
    display.fillRect(x, y, w, h, GxEPD_WHITE);
    display.setCursor(20, 70);
    display.print(timeBuffer);
  } while (display.nextPage());
}

// Funzione che aggiorna orario e citazioni
void updateTimeAndQuotes() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) return;
  
  char timeBuffer[10];
  strftime(timeBuffer, sizeof(timeBuffer), "%H:%M", &timeinfo);
  
  display.setFullWindow();
  display.firstPage();
  do {
    int timeX = 20;
    int timeY = 70;
    int16_t tbx, tby; uint16_t tbw, tbh;
    display.setFont(&FreeSerif12pt7b);
    display.getTextBounds(timeBuffer, 0, 0, &tbx, &tby, &tbw, &tbh);
    display.fillRect(timeX - 2, timeY - tbh - 2, tbw + 4, tbh + 4, GxEPD_WHITE);
    display.fillRect(10, display.height() - 25, 150, 20, GxEPD_WHITE);
    display.setCursor(timeX, timeY);
    display.print(timeBuffer);
    drawLastUpdate(10, display.height() - 30, currentWeather.last_update);
  } while (display.nextPage());
}

// Flag per indicare se è il primo avvio
static bool isFirstBoot = true;

// Variabile esterna per lo stato dell'ultimo aggiornamento
extern bool lastWeatherUpdateSuccess;

// Funzione esterna per verificare se siamo in modalità risparmio energetico
extern bool isPowerSavingMode();

// Implementazione completa della funzione di aggiornamento display
void updateDisplay() {
  // Ottieni l'ora corrente
  struct tm timeinfo;
  getLocalTime(&timeinfo);
  
  // Se non siamo connessi al WiFi, mostra la schermata AP
  if (WiFi.status() != WL_CONNECTED) {
    showAPModeInfo();
    return;
  }
  
  // Metodo semplificato per aggiornamento display
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    drawDisplayContent();
    // if (config.batteryMonitorEnabled) {
    //   Serial.printf("[BATTERY] Status: %s\n", battery.getStatusString().c_str());
    // }
    
    // Se l'ultimo aggiornamento meteo ha avuto problemi, mostra un indicatore
    if (!lastWeatherUpdateSuccess) {
      // Disegna un'icona di avvertimento in alto a destra
      int iconX = display.width() - 30;
      int iconY = 15;
      
      // Triangolo di avvertimento
      display.fillTriangle(
        iconX, iconY + 20,           // Base sinistra
        iconX + 20, iconY + 20,      // Base destra
        iconX + 10, iconY,           // Punta
        GxEPD_BLACK
      );
      
      // Punto esclamativo all'interno
      display.fillRect(iconX + 9, iconY + 5, 2, 10, GxEPD_WHITE);
      display.fillRect(iconX + 9, iconY + 16, 2, 2, GxEPD_WHITE);
    }
    
    // Se siamo in modalità risparmio energetico, mostra un'icona luna
    if (isPowerSavingMode()) {
      int iconX = display.width() - 30;
      int iconY = 40;
      
      // Luna per indicare risparmio energetico
      display.fillCircle(iconX + 10, iconY + 10, 10, GxEPD_BLACK);
      display.fillCircle(iconX + 15, iconY + 10, 9, GxEPD_WHITE);
    }
  } while (display.nextPage());
  
  if (isFirstBoot) isFirstBoot = false;
}

// Funzione che contiene tutto il codice per disegnare i contenuti
// Separata per evitare duplicazione di codice
void drawDisplayContent() {
  DEBUG_TRACE("drawDisplayContent"); // added
  // Nota: il controllo WiFi.status() è stato spostato in updateDisplay()
  // per evitare nesting di firstPage/nextPage loops
  
  // Layout fisso: per ora mettiamo in standby il layout personalizzato (/layout.json)
  // e il drag&drop. Usiamo sempre il layout di default disegnato da drawDefaultLayout().
  drawDefaultLayout();
  return;

  // --- CODICE LAYOUT PERSONALIZZATO IN STANDBY ---
  // Sistema unificato: usa SEMPRE il layout personalizzabile da /layout.json
  // Se il file non esiste, crea un layout di default
  
  // Verifica disponibilità SD
  if (!initSD()) {
    // SD non disponibile: mostra schermata errore SD (versione semplificata)
    // Serial.println("[DISPLAY] SD non disponibile - mostro schermata errore SD");
    drawSDCardError();
    return;
  }
  
  // Se non esiste layout.json, usa layout di default semplice
  if (!SD.exists("/layout.json")) {
    // Serial.println("[DISPLAY] /layout.json non trovato - uso layout default");
    drawDefaultLayout();
    return;
  }
  
  // Carica il file layout.json
  File f = SD.open("/layout.json", FILE_READ);
  if (!f) {
    // Serial.println("[DISPLAY] Errore apertura /layout.json - uso default");
    drawDefaultLayout();
    return;
  }
  
  String content;
  while (f.available()) {
    content += (char)f.read();
  }
  f.close();
  
  DynamicJsonDocument doc(4096);
  DeserializationError err = deserializeJson(doc, content);
  if (err) {
    // Serial.printf("[DISPLAY] Errore parsing JSON: %s - uso default\n", err.c_str());
    drawDefaultLayout();
    return;
  }
  
  // Serial.println("[DISPLAY] Layout personalizzato caricato da /layout.json");
  
  // Parametri griglia
  int cols = doc["grid"]["cols"] | 24;
  int rowH = doc["grid"]["row_height"] | 20;
  int m = doc["grid"]["margin"] | 4;
  if (cols <= 0) cols = 24;
  if (rowH <= 0) rowH = 20;
  if (m < 0) m = 0;
  int usableW = display.width() - (cols + 1) * m;
  if (usableW < cols) usableW = display.width();
  int cellW = usableW / cols;
  auto rectFor = [&](int gx, int gy, int gw, int gh) {
    int x = m + gx * (cellW + m);
    int y = m + gy * (rowH + m);
    int w = gw * cellW + (gw - 1) * m;
    int h = gh * rowH + (gh - 1) * m;
    // Clamp
    if (x < 0) x = 0; if (y < 0) y = 0;
    if (x + w > display.width()) w = display.width() - x;
    if (y + h > display.height()) h = display.height() - y;
    struct { int x; int y; int w; int h; } r { x, y, w, h };
    return r;
  };

  JsonArray items = doc["items"].as<JsonArray>();
  if (items.isNull()) items = doc.createNestedArray("items");
  // Calcola z max
  int maxZ = 0;
  for (JsonObject it : items) {
    int z = it["z"].as<int>(); if (z > maxZ) maxZ = z;
  }
  
  // Disegna per z crescente
  for (int z = 0; z <= maxZ; ++z) {
    for (JsonObject it : items) {
      if (it["visible"].is<bool>() && it["visible"] == false) continue;
      int iz = it["z"].as<int>(); if (iz != z) continue;
      const char* type = it["type"].as<const char*>();
      int gx = it["x"].as<int>();
      int gy = it["y"].as<int>();
      int gw = it["w"].as<int>(); if (gw <= 0) gw = 1;
      int gh = it["h"].as<int>(); if (gh <= 0) gh = 1;
      auto r = rectFor(gx, gy, gw, gh);

      // Bordi guida per debug (disattivati)
      // display.drawRect(r.x, r.y, r.w, r.h, GxEPD_BLACK);

      String align = it["align"].as<String>(); align.toLowerCase();
      int fontSize = it["font_size"].as<int>();
      if (strcmp(type, "city") == 0) {
        // Font mapping semplice
        if (fontSize >= 20) display.setFont(&FreeSerif12pt7b);
        else if (fontSize >= 12) display.setFont(&FreeSansBold12pt7b);
        else display.setFont(&FreeSerif9pt7b);
        int16_t tbx, tby; uint16_t tbw, tbh;
        display.getTextBounds(config.city, 0, 0, &tbx, &tby, &tbw, &tbh);
        int tx = r.x + 2;
        if (align == "center") tx = r.x + (r.w - tbw) / 2;
        else if (align == "right") tx = r.x + r.w - tbw - 2;
        int ty = r.y + tbh + 2;
        if (ty > r.y + r.h - 2) ty = r.y + r.h - 2;
        display.setCursor(tx, ty);
        display.setTextColor(GxEPD_BLACK);
        display.print(config.city);
      } else if (strcmp(type, "weather_icon") == 0) {
        // Dimensione icona: se definita nel layout usa icon_size, altrimenti adatta all'area
        int iconSize = it["icon_size"].as<int>();
        if (iconSize <= 0) {
          iconSize = min(r.w, r.h); // fallback: usa lato minore dell'area
        }
        int ix = r.x + (r.w - iconSize) / 2; if (ix < r.x) ix = r.x;
        int iy = r.y + (r.h - iconSize) / 2; if (iy < r.y) iy = r.y;
        drawWeatherIcon(ix, iy, currentWeather.weather_id, isNightTime(), iconSize);
      } else if (strcmp(type, "quote") == 0) {
        int marginX = 2;
        int maxW = r.w - 2 * marginX; if (maxW < 20) maxW = 20;
        int qx = r.x + marginX;
        int qy = r.y + 2;
        
        // Calcola fontSize in base all'altezza della box se non specificato
        int quoteFontSize = it["font_size"].as<int>();
        if (quoteFontSize <= 0) {
          // Calcola automaticamente: più alta è la box, più grande il font
          // Usa circa 1/6 dell'altezza come fontSize
          quoteFontSize = r.h / 6;
          if (quoteFontSize < 9) quoteFontSize = 9;
          if (quoteFontSize > 24) quoteFontSize = 24;
        }
        
        drawQuote(qx, qy, maxW, quoteFontSize);
      } else if (strcmp(type, "footer_bar") == 0) {
        // La footer bar viene disegnata separatamente fuori dal layout personalizzato
        continue;
      } else if (strcmp(type, "temperature") == 0) {
        // Temperatura - usa grafica vettoriale se fontSize > 40
        if (fontSize > 40) {
          // Numero grande in stile 7-segment
          int height = fontSize * 2;  // Converti pt in pixel (approssimato)
          if (height > r.h) height = r.h - 4;  // Limita all'altezza disponibile
          
          // Calcola posizione X in base all'allineamento
          int tx = r.x + 2;
          // Per centrare/allineare a destra, dovremmo calcolare la larghezza totale
          // Per semplicità ora partiamo da sinistra, poi aggiungiamo logica
          if (align == "center") {
            // Stima larghezza: circa height*0.6 per cifra + spazi
            int estimatedW = height * 2.5;  // Stima per "XX.X°"
            tx = r.x + (r.w - estimatedW) / 2;
            if (tx < r.x) tx = r.x;
          } else if (align == "right") {
            int estimatedW = height * 2.5;
            tx = r.x + r.w - estimatedW - 2;
            if (tx < r.x) tx = r.x;
          }
          
          int ty = r.y + (r.h - height) / 2;
          if (ty < r.y) ty = r.y;
          
          drawBigNumber(tx, ty, currentWeather.temp, height, true);
        } else {
          // Font normale per dimensioni piccole
          if (fontSize >= 16) display.setFont(&FreeSansBold12pt7b);
          else if (fontSize >= 12) display.setFont(&FreeSerif9pt7b);
          else display.setFont(NULL);
          
          String tempStr = String(currentWeather.temp, 1) + "\xB0" + "C";
          int16_t tbx, tby; uint16_t tbw, tbh;
          display.getTextBounds(tempStr.c_str(), 0, 0, &tbx, &tby, &tbw, &tbh);
          
          int tx = r.x + 2;
          if (align == "center") tx = r.x + (r.w - tbw) / 2;
          else if (align == "right") tx = r.x + r.w - tbw - 2;
          int ty = r.y + tbh + 2;
          
          display.setCursor(tx, ty);
          display.setTextColor(GxEPD_BLACK);
          display.print(tempStr);
        }
      } else if (strcmp(type, "humidity") == 0) {
        // Umidità - usa grafica vettoriale se fontSize > 40
        if (fontSize > 40) {
          int height = fontSize * 2;
          if (height > r.h) height = r.h - 4;
          
          int tx = r.x + 2;
          if (align == "center") {
            int estimatedW = height * 2.0;  // Stima per "XX%"
            tx = r.x + (r.w - estimatedW) / 2;
            if (tx < r.x) tx = r.x;
          } else if (align == "right") {
            int estimatedW = height * 2.0;
            tx = r.x + r.w - estimatedW - 2;
            if (tx < r.x) tx = r.x;
          }
          
          int ty = r.y + (r.h - height) / 2;
          if (ty < r.y) ty = r.y;
          
          // Disegna numero senza decimale
          drawBigNumber(tx, ty, currentWeather.humidity, height, false);
          
          // Aggiungi simbolo %
          int percentSize = height * 0.4;
          int percentX = tx + height * 1.5;  // Dopo il numero
          display.setFont(&FreeSansBold12pt7b);
          display.setCursor(percentX, ty + height - percentSize);
          display.print("%");
        } else {
          // Font normale
          if (fontSize >= 16) display.setFont(&FreeSansBold12pt7b);
          else if (fontSize >= 12) display.setFont(&FreeSerif9pt7b);
          else display.setFont(NULL);
          
          String humStr = String((int)currentWeather.humidity) + "%";
          int16_t tbx, tby; uint16_t tbw, tbh;
          display.getTextBounds(humStr.c_str(), 0, 0, &tbx, &tby, &tbw, &tbh);
          
          int tx = r.x + 2;
          if (align == "center") tx = r.x + (r.w - tbw) / 2;
          else if (align == "right") tx = r.x + r.w - tbw - 2;
          int ty = r.y + tbh + 2;
          
          display.setCursor(tx, ty);
          display.setTextColor(GxEPD_BLACK);
          display.print(humStr);
        }
      } else if (strcmp(type, "pressure") == 0) {
        // Pressione - usa grafica vettoriale se fontSize > 40
        if (fontSize > 40) {
          int height = fontSize * 2;
          if (height > r.h) height = r.h - 4;
          
          int tx = r.x + 2;
          if (align == "center") {
            int estimatedW = height * 3.5;  // Stima per "XXXX"
            tx = r.x + (r.w - estimatedW) / 2;
            if (tx < r.x) tx = r.x;
          } else if (align == "right") {
            int estimatedW = height * 3.5;
            tx = r.x + r.w - estimatedW - 2;
            if (tx < r.x) tx = r.x;
          }
          
          int ty = r.y + (r.h - height) / 2;
          if (ty < r.y) ty = r.y;
          
          drawBigNumber(tx, ty, currentWeather.pressure, height, false);
          
          // Aggiungi "hPa" piccolo
          int textSize = height * 0.3;
          int textX = tx + height * 3.0;
          display.setFont(&FreeSerif9pt7b);
          display.setCursor(textX, ty + height - textSize);
          display.print("hPa");
        } else {
          // Font normale
          if (fontSize >= 16) display.setFont(&FreeSansBold12pt7b);
          else if (fontSize >= 12) display.setFont(&FreeSerif9pt7b);
          else display.setFont(NULL);
          
          String presStr = String((int)currentWeather.pressure) + " hPa";
          int16_t tbx, tby; uint16_t tbw, tbh;
          display.getTextBounds(presStr.c_str(), 0, 0, &tbx, &tby, &tbw, &tbh);
          
          int tx = r.x + 2;
          if (align == "center") tx = r.x + (r.w - tbw) / 2;
          else if (align == "right") tx = r.x + r.w - tbw - 2;
          int ty = r.y + tbh + 2;
          
          display.setCursor(tx, ty);
          display.setTextColor(GxEPD_BLACK);
          display.print(presStr);
        }
      } else if (strcmp(type, "wind") == 0) {
        // Vento - usa grafica vettoriale se fontSize > 40
        if (fontSize > 40) {
          int height = fontSize * 2;
          if (height > r.h) height = r.h - 4;
          
          int tx = r.x + 2;
          if (align == "center") {
            int estimatedW = height * 3.0;  // Stima per "XX.X"
            tx = r.x + (r.w - estimatedW) / 2;
            if (tx < r.x) tx = r.x;
          } else if (align == "right") {
            int estimatedW = height * 3.0;
            tx = r.x + r.w - estimatedW - 2;
            if (tx < r.x) tx = r.x;
          }
          
          int ty = r.y + (r.h - height) / 2;
          if (ty < r.y) ty = r.y;
          
          drawBigNumber(tx, ty, currentWeather.wind_speed, height, true);
          
          // Aggiungi "km/h" piccolo
          int textSize = height * 0.25;
          int textX = tx + height * 2.8;
          display.setFont(&FreeSerif9pt7b);
          display.setCursor(textX, ty + height - textSize);
          display.print("km/h");
        } else {
          // Font normale
          if (fontSize >= 16) display.setFont(&FreeSansBold12pt7b);
          else if (fontSize >= 12) display.setFont(&FreeSerif9pt7b);
          else display.setFont(NULL);
          
          String windStr = String(currentWeather.wind_speed, 1) + " km/h";
          int16_t tbx, tby; uint16_t tbw, tbh;
          display.getTextBounds(windStr.c_str(), 0, 0, &tbx, &tby, &tbw, &tbh);
          
          int tx = r.x + 2;
          if (align == "center") tx = r.x + (r.w - tbw) / 2;
          else if (align == "right") tx = r.x + r.w - tbw - 2;
          int ty = r.y + tbh + 2;
          
          display.setCursor(tx, ty);
          display.setTextColor(GxEPD_BLACK);
          display.print(windStr);
        }
      }
    }
  }

  // Footer standard fuori dal layout personalizzato
  drawLastUpdate(10, display.height() - 30, currentWeather.last_update);
  display.setFont(NULL);
  String footerIpString = (WiFi.status() == WL_CONNECTED)
                          ? (String("IP: ") + WiFi.localIP().toString())
                          : String("IP: N/A");
  int16_t fbx, fby; uint16_t fbw, fbh;
  display.getTextBounds(footerIpString.c_str(), 0, 0, &fbx, &fby, &fbw, &fbh);
  int centerX = (display.width() - fbw) / 2;
  display.setCursor(centerX, display.height() - 10);
  display.setTextColor(GxEPD_BLACK);
  display.print(footerIpString);
  if (config.batteryMonitorEnabled && config.batteryShowOnDisplay) {
    int batteryPercentage = battery.getPercentage();
    drawBattery(display.width() - 60, display.height() - 55, batteryPercentage);
  }
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
  int topY  = 25;

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

  // Calcola una dimensione icona meteo condivisa, così possiamo allineare la temperatura alla stessa altezza
  int iconSize = min(W / 2, H / 2);
  if (iconSize < 80) iconSize = 80;

  // Temperatura grande sulla sinistra - usa cifre BMP da /fonts
  {
    int glyphSize = 80; // dimensione di riferimento per ogni cifra (leggermente piu' compatta)
    int tempX = leftX;

    // Allinea verticalmente il centro della temperatura al centro dell'icona meteo
    int iconYForAlign = topY + 10;
    int iconCenterY = iconYForAlign + iconSize / 2;
    int tempY = iconCenterY - glyphSize / 2;

    // Evita di salire troppo e sovrapporsi alle info in alto
    int minTempY = topY + 10;
    if (tempY < minTempY) {
      tempY = minTempY;
    }

    drawTemperatureBMP(tempX, tempY, currentWeather.temp, glyphSize);
  }

  // Icona meteo grande sulla destra
  int iconX = W - iconSize - 20;
  int iconY = topY + 10;
  drawWeatherIcon(iconX, iconY, currentWeather.weather_id, isNightTime(), iconSize);

  // Citazione nella parte centrale/bassa
  int quoteTop = H / 2 + 20;
  int margin = 10; // margine ridotto per sfruttare meglio la larghezza
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
  
  // Disegna sempre la batteria nel layout di default, indipendentemente dai flag di configurazione
  int batteryPercentage = battery.getPercentage();
  drawBattery(display.width() - 60, display.height() - 55, batteryPercentage);
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
      drawBattery(display.width() - 60, display.height() - 55, batteryPercentage);
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
    drawBattery(display.width() - 60, display.height() - 40, 80);
    
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
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  display.setFont(&FreeMonoBold9pt7b);
  display.setCursor(10, 30);
  display.print("ERRORE:");
  display.setCursor(10, 60);
  display.print(message);
  // Aggiornamento secondo API GxEPD2
  display.setFullWindow();
  display.firstPage();
  do {
    // Ridisegna i contenuti per ogni pagina
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(10, 30);
    display.print("ERRORE:");
    display.setCursor(10, 60);
    display.print(message);
  } while (display.nextPage());
}

// Funzione per mostrare schermata SD mancante
void showSDCardMissing() {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    
    // Disegna icona SD card (semplificata)
    int cardX = display.width() / 2 - 40;
    int cardY = 80;
    int cardW = 80;
    int cardH = 60;
    
    // Corpo SD card
    display.drawRect(cardX, cardY, cardW, cardH, GxEPD_BLACK);
    display.drawRect(cardX + 1, cardY + 1, cardW - 2, cardH - 2, GxEPD_BLACK);
    
    // "Taglio" angolare in alto a destra
    display.fillTriangle(
      cardX + cardW - 15, cardY,
      cardX + cardW, cardY,
      cardX + cardW, cardY + 15,
      GxEPD_WHITE
    );
    display.drawLine(cardX + cardW - 15, cardY, cardX + cardW, cardY + 15, GxEPD_BLACK);
    
    // Dettagli SD card (contatti)
    for (int i = 0; i < 5; i++) {
      display.fillRect(cardX + 15 + (i * 10), cardY + cardH - 15, 6, 10, GxEPD_BLACK);
    }
    
    // Logo SD stilizzato
    display.setFont(&FreeSansBold12pt7b);
    display.setCursor(cardX + 25, cardY + 35);
    display.print("SD");
    
    // Titolo
    display.setFont(&FreeSerif12pt7b);
    int16_t tbx, tby; uint16_t tbw, tbh;
    const char* title = "Ops! SD Card non trovata";
    display.getTextBounds(title, 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setCursor((display.width() - tbw) / 2, cardY + cardH + 50);
    display.print(title);
    
    // Messaggio
    display.setFont(&FreeSerif9pt7b);
    const char* msg1 = "Per favore inserisci una";
    display.getTextBounds(msg1, 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setCursor((display.width() - tbw) / 2, cardY + cardH + 80);
    display.print(msg1);
    
    const char* msg2 = "scheda SD per continuare";
    display.getTextBounds(msg2, 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setCursor((display.width() - tbw) / 2, cardY + cardH + 105);
    display.print(msg2);
    
    // Emoji/Icona simpatica
    display.setFont(&FreeSansBold12pt7b);
    display.setCursor(display.width() / 2 - 10, cardY + cardH + 140);
    display.print(":(");
    
    // Footer informativo
    display.setFont(NULL);
    const char* footer = "Il sistema si riavviera' automaticamente quando la SD sara' disponibile";
    display.getTextBounds(footer, 0, 0, &tbx, &tby, &tbw, &tbh);
    int footerX = (display.width() - tbw) / 2;
    if (footerX < 5) footerX = 5;
    display.setCursor(footerX, display.height() - 20);
    display.print(footer);
    
  } while (display.nextPage());
  
  // Serial.println("[DISPLAY] Mostrata schermata SD mancante");
}

// Mostra conferma visiva di salvataggio configurazione
void showConfigSaved() {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    
    // Disegna un grande checkmark (✓)
    int checkX = display.width() / 2;
    int checkY = 100;
    int checkSize = 80;
    
    // Cerchio attorno al checkmark
    display.drawCircle(checkX, checkY, checkSize / 2, GxEPD_BLACK);
    display.drawCircle(checkX, checkY, checkSize / 2 - 1, GxEPD_BLACK);
    display.drawCircle(checkX, checkY, checkSize / 2 - 2, GxEPD_BLACK);
    
    // Disegna il checkmark
    // Braccio corto (verso sinistra-basso)
    for (int i = 0; i < 6; i++) {
      display.drawLine(
        checkX - 20, checkY + 5 + i,
        checkX - 5, checkY + 20 + i,
        GxEPD_BLACK
      );
    }
    
    // Braccio lungo (verso destra-alto)
    for (int i = 0; i < 6; i++) {
      display.drawLine(
        checkX - 5, checkY + 20 + i,
        checkX + 25, checkY - 15 + i,
        GxEPD_BLACK
      );
    }
    
    // Titolo principale
    display.setFont(&FreeSansBold12pt7b);
    int16_t tbx, tby; uint16_t tbw, tbh;
    const char* title = "CONFIGURAZIONE SALVATA!";
    display.getTextBounds(title, 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setCursor((display.width() - tbw) / 2, checkY + checkSize / 2 + 60);
    display.print(title);
    
    // Messaggio di riavvio
    display.setFont(&FreeSerif9pt7b);
    const char* msg1 = "Il dispositivo si riavviera'";
    display.getTextBounds(msg1, 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setCursor((display.width() - tbw) / 2, checkY + checkSize / 2 + 95);
    display.print(msg1);
    
    const char* msg2 = "tra pochi istanti...";
    display.getTextBounds(msg2, 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setCursor((display.width() - tbw) / 2, checkY + checkSize / 2 + 120);
    display.print(msg2);
    
    // Emoji felice
    display.setFont(&FreeSansBold12pt7b);
    display.setCursor(display.width() / 2 - 15, checkY + checkSize / 2 + 160);
    display.print(":)");
    
    // Footer
    display.setFont(NULL);
    const char* footer = "AtmoVerse 2.0";
    display.getTextBounds(footer, 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setCursor((display.width() - tbw) / 2, display.height() - 20);
    display.print(footer);
    
  } while (display.nextPage());
  
  // Serial.println("[DISPLAY] Mostrata conferma salvataggio configurazione");
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
    display.setFont(&FreeSerif12pt7b);
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
  
  // Seleziona il font dell'autore in base al fontSize della citazione (leggermente più piccolo)
  if (fontSize >= 16) {
    display.setFont(&FreeMonoBoldOblique9pt7b);
  } else if (fontSize >= 12) {
    display.setFont(&FreeMonoBoldOblique9pt7b);
  } else {
    display.setFont(NULL);  // Font di default piccolo
  }
  
  // Costruisci la stringa dell'autore completa (senza troncare)
  String authorLine = String("— ") + (q.author.length() ? q.author : "Anonimo");
  
  // Calcola la larghezza del testo dell'autore
  int16_t abx, aby; uint16_t abw, abh;
  display.getTextBounds(authorLine.c_str(), 0, 0, &abx, &aby, &abw, &abh);

  // Se troppo lunga per lo schermo, tronca con "..." per evitare overflow
  int maxAuthorWidth = display.width() - 12; // margine complessivo leggermente ridotto
  if ((int)abw > maxAuthorWidth) {
    const String ellipsis = "...";
    String base = authorLine;
    while (base.length() > 0) {
      String candidate = base + ellipsis;
      int16_t cbx, cby; uint16_t cbw, cbh;
      display.getTextBounds(candidate.c_str(), 0, 0, &cbx, &cby, &cbw, &cbh);
      if ((int)cbw <= maxAuthorWidth) {
        authorLine = candidate;
        abw = cbw;
        break;
      }
      base.remove(base.length() - 1);
    }
  }
  
  // Allinea a DESTRA: calcola la X in modo che il testo finisca a 3px dal bordo destro
  int authorX = display.width() - (int)abw - 3;
  
  // Assicura margine minimo da sinistra (non far partire troppo a sinistra)
  if (authorX < 6) authorX = 6;
  
  display.setCursor(authorX, cursorY);
  display.print(authorLine);
}

// Funzione per disegnare lo stato della batteria
void drawBattery(int x, int y, int percentage) {
  // Dichiarazione esterna dell'istanza battery
  extern BatteryManager battery;
  
  // Dimensioni icona batteria
  int width = 25;
  int height = 12;
  
  // Assicura margini di sicurezza (min 10px da destra, max 30px dal fondo)
  if (x > display.width() - width - 10) x = display.width() - width - 10;
  if (y > display.height() - height - 30) y = display.height() - height - 30;
  if (x < 0) x = 0;
  if (y < 0) y = 0;

  int safePercentage = percentage;
  // Controlla se ADC disponibile
  if (battery.getVoltage() == 0.0) {
    // ADC non disponibile - mostra alert
    safePercentage = 0;
  }
  
  // Disegniamo il contorno della batteria
  display.drawRect(x, y, width, height, GxEPD_BLACK);
  display.drawRect(x + width, y + 3, 2, height - 6, GxEPD_BLACK);
  
  // Disegniamo il livello della batteria
  int fillWidth = map(safePercentage, 0, 100, 0, width - 4);
  if (fillWidth > 0) {
    display.fillRect(x + 2, y + 2, fillWidth, height - 4, GxEPD_BLACK);
  }
  
  // Se in carica, disegna fulmine sulla batteria
  if (battery.charging()) {
    // Disegna fulmine stilizzato al centro della batteria
    int centerX = x + width / 2;
    int centerY = y + height / 2;
    
    // Fulmine: ⚡ (come forma vettoriale semplice)
    // Linea superiore: da centro-sinistra a centro
    display.drawLine(centerX - 3, centerY - 3, centerX, centerY, GxEPD_WHITE);
    // Linea centrale: da centro a destra
    display.drawLine(centerX, centerY, centerX + 3, centerY, GxEPD_WHITE);
    // Linea inferiore: da centro a basso-sinistra
    display.drawLine(centerX, centerY, centerX - 2, centerY + 3, GxEPD_WHITE);
    
    // Rinforza il fulmine
    display.drawLine(centerX - 2, centerY - 3, centerX + 1, centerY, GxEPD_WHITE);
    display.drawLine(centerX + 1, centerY, centerX + 2, centerY + 3, GxEPD_WHITE);
  }
  
  // Disegniamo la percentuale
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
