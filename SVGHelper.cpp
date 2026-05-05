/**
 * @file SVGHelper.cpp
 * @brief Implementazione della gestione dei file SVG per la visualizzazione
 * 
 * Questo file contiene l'implementazione delle funzionalità per il caricamento
 * e la visualizzazione di elementi SVG su display e-ink.
 */

#include "SVGHelper.h"
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <Arduino.h>

// Inizializzazione dei membri statici
bool SVGHelper::initialized = false;

// NOTA: iconPositions non più necessario - ora usiamo file SVG individuali invece di una griglia

// Funzioni di utilità per il parsing SVG
static bool skipWhitespace(const char*& str);
static bool parseNumber(const char*& str, float& value);
static bool parseCommand(const char*& str, char& cmd, bool& relative);
static bool parseCoord(const char*& str, float& x, float& y, bool relative, float lastX, float lastY);

// Mappa icona → nome file SVG (Weather Icons by Erik Flowers)
static const char* getIconFilePath(WeatherIcon icon) {
  switch(icon) {
    // GIORNO - Condizioni Base (0-9)
    case ICON_DAY_SUNNY: return "/icons/wi-day-sunny.svg";
    case ICON_DAY_CLOUDY: return "/icons/wi-day-cloudy.svg";
    case ICON_DAY_FOG: return "/icons/wi-day-fog.svg";
    case ICON_DAY_WINDY: return "/icons/wi-day-windy.svg";
    
    // GIORNO - Pioggia (10-14)
    case ICON_DAY_RAIN: return "/icons/wi-day-rain.svg";
    case ICON_DAY_RAIN_WIND: return "/icons/wi-day-rain-wind.svg";
    case ICON_DAY_SPRINKLE: return "/icons/wi-day-sprinkle.svg";
    case ICON_DAY_SHOWERS: return "/icons/wi-day-showers.svg";
    
    // GIORNO - Temporali (20-22)
    case ICON_DAY_THUNDERSTORM: return "/icons/wi-day-thunderstorm.svg";
    case ICON_DAY_STORM_SHOWERS: return "/icons/wi-day-storm-showers.svg";
    case ICON_DAY_LIGHTNING: return "/icons/wi-day-lightning.svg";
    
    // GIORNO - Neve (30-34)
    case ICON_DAY_SNOW: return "/icons/wi-day-snow.svg";
    case ICON_DAY_SNOW_WIND: return "/icons/wi-day-snow-wind.svg";
    case ICON_DAY_SLEET: return "/icons/wi-day-sleet.svg";
    case ICON_DAY_RAIN_MIX: return "/icons/wi-day-rain-mix.svg";
    case ICON_DAY_HAIL: return "/icons/wi-day-hail.svg";
    
    // NOTTE - Condizioni Base (40-43)
    case ICON_NIGHT_CLEAR: return "/icons/wi-night-clear.svg";
    case ICON_NIGHT_CLOUDY: return "/icons/wi-night-alt-cloudy.svg";
    case ICON_NIGHT_FOG: return "/icons/wi-night-fog.svg";
    
    // NOTTE - Pioggia (50-53)
    case ICON_NIGHT_RAIN: return "/icons/wi-night-alt-rain.svg";
    case ICON_NIGHT_RAIN_WIND: return "/icons/wi-night-alt-rain-wind.svg";
    case ICON_NIGHT_SPRINKLE: return "/icons/wi-night-alt-sprinkle.svg";
    case ICON_NIGHT_SHOWERS: return "/icons/wi-night-alt-showers.svg";
    
    // NOTTE - Temporali (60-61)
    case ICON_NIGHT_THUNDERSTORM: return "/icons/wi-night-alt-thunderstorm.svg";
    case ICON_NIGHT_STORM_SHOWERS: return "/icons/wi-night-alt-storm-showers.svg";
    
    // NOTTE - Neve (70-74)
    case ICON_NIGHT_SNOW: return "/icons/wi-night-alt-snow.svg";
    case ICON_NIGHT_SNOW_WIND: return "/icons/wi-night-alt-snow-wind.svg";
    case ICON_NIGHT_SLEET: return "/icons/wi-night-alt-sleet.svg";
    case ICON_NIGHT_RAIN_MIX: return "/icons/wi-night-alt-rain-mix.svg";
    case ICON_NIGHT_HAIL: return "/icons/wi-night-alt-hail.svg";
    
    // NEUTRO - Non dipendente da giorno/notte (80-89)
    case ICON_CLOUDY: return "/icons/wi-cloudy.svg";
    case ICON_RAIN: return "/icons/wi-rain.svg";
    case ICON_SNOW: return "/icons/wi-snow.svg";
    case ICON_THUNDERSTORM: return "/icons/wi-thunderstorm.svg";
    case ICON_FOG: return "/icons/wi-fog.svg";
    case ICON_WINDY: return "/icons/wi-windy.svg";
    case ICON_TORNADO: return "/icons/wi-tornado.svg";
    case ICON_HURRICANE: return "/icons/wi-hurricane.svg";
    case ICON_HOT: return "/icons/wi-hot.svg";
    case ICON_SNOWFLAKE_COLD: return "/icons/wi-snowflake-cold.svg";
    
    // GIORNO - Varianti Vento/Nubi (90-95)
    case ICON_DAY_CLOUDY_WINDY: return "/icons/wi-day-cloudy-windy.svg";
    case ICON_DAY_CLOUDY_GUSTS: return "/icons/wi-day-cloudy-gusts.svg";
    case ICON_DAY_CLOUDY_HIGH: return "/icons/wi-day-cloudy-high.svg";
    
    // NOTTE - Varianti Vento/Nubi (96-98)
    case ICON_NIGHT_CLOUDY_WINDY: return "/icons/wi-night-alt-cloudy-windy.svg";
    case ICON_NIGHT_CLOUDY_GUSTS: return "/icons/wi-night-alt-cloudy-gusts.svg";
    case ICON_NIGHT_PARTLY_CLOUDY: return "/icons/wi-night-alt-partly-cloudy.svg";
    
    // NEUTRO - Varianti Vento (99)
    case ICON_CLOUDY_WINDY: return "/icons/wi-cloudy-windy.svg";
    
    // Condizioni Atmosferiche Specifiche (100-109)
    case ICON_SMOKE: return "/icons/wi-smoke.svg";
    case ICON_HAZE: return "/icons/wi-day-haze.svg";
    case ICON_DUST: return "/icons/wi-dust.svg";
    case ICON_SANDSTORM: return "/icons/wi-sandstorm.svg";
    case ICON_VOLCANO: return "/icons/wi-volcano.svg";
    
    // Precipitazioni Intense (110-115)
    case ICON_DAY_SNOW_THUNDERSTORM: return "/icons/wi-day-snow-thunderstorm.svg";
    case ICON_NIGHT_SNOW_THUNDERSTORM: return "/icons/wi-night-alt-snow-thunderstorm.svg";
    case ICON_DAY_SLEET_STORM: return "/icons/wi-day-sleet-storm.svg";
    case ICON_NIGHT_SLEET_STORM: return "/icons/wi-night-alt-sleet-storm.svg";
    case ICON_HAIL: return "/icons/wi-hail.svg";
    case ICON_LIGHTNING: return "/icons/wi-lightning.svg";
    
    // Allerte e Pericoli (120-129)
    case ICON_FLOOD: return "/icons/wi-flood.svg";
    case ICON_FIRE: return "/icons/wi-fire.svg";
    case ICON_HURRICANE_WARNING: return "/icons/wi-hurricane-warning.svg";
    case ICON_GALE_WARNING: return "/icons/wi-gale-warning.svg";
    case ICON_EARTHQUAKE: return "/icons/wi-earthquake.svg";
    case ICON_METEOR: return "/icons/wi-meteor.svg";
    
    default: return nullptr;
  }
}

bool SVGHelper::begin(int8_t csPin) {
  (void)csPin; // Il progetto utilizza un bus SD dedicato (sdSPI) con pin fissi
  if (initialized) {
    return true;
  }
  
  // Inizializza la scheda SD utilizzando la stessa configurazione condivisa
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, sdSPI, SD_SPI_FREQ)) {
    Serial.println(F("[SVG] Errore nell'inizializzazione della scheda SD"));
    return false;
  }
  
  // Verifica la presenza della cartella delle icone
  if (!SD.exists("/icons")) {
    if (!SD.mkdir("/icons")) {
      Serial.println(F("[SVG] Impossibile creare la directory /icons"));
      return false;
    }
  }
  
  Serial.println(F("[SVG] Inizializzazione completata con successo"));
  initialized = true;
  return true;
}

bool SVGHelper::drawWeatherIcon(DisplayType& display, WeatherIcon icon, int x, int y, int size) {
  // Verifica i parametri di input
  if (icon < 0 || icon >= ICON_COUNT) {
    Serial.print(F("[SVG] Errore: codice icona non valido: "));
    Serial.print(icon);
    Serial.println('"');
    return false;
  }
  
  if (size <= 0) {
    Serial.println(F("[SVG] Errore: dimensione non valida"));
    return false;
  }
  
  const char* iconFile = getIconFilePath(icon);
  
  // Verifica che l'icona sia supportata
  if (iconFile == nullptr) {
    Serial.print(F("[SVG] Icona non mappata: "));
    Serial.println(icon);
    return false;
  }
  
  // Verifica che il file esista
  if (!SD.exists(iconFile)) {
    Serial.print(F("[SVG] File non trovato: "));
    Serial.println(iconFile);
    
    // Prova senza slash iniziale
    String altPath = String(iconFile).substring(1);
    if (!SD.exists(altPath.c_str())) {
      Serial.print(F("[SVG] File non trovato: "));
      Serial.println(altPath);
      return false;
    }
    iconFile = altPath.c_str();
  }
  
  Serial.print(F("[SVG] Caricamento icona: "));
  Serial.println(iconFile);
  
  // Carica l'intero file SVG (è già una singola icona)
  return loadSVG(display, iconFile, x, y, size, size);
}

bool SVGHelper::drawMoonPhase(DisplayType& display, int x, int y, int size, int phase) {
  // Verifica i parametri di input
  if (size <= 0) {
    Serial.println(F("[SVG] Errore: dimensione non valida per la fase lunare"));
    return false;
  }
  
  // Normalizza la fase a un valore tra 0 e 12
  phase = phase % 13;
  if (phase < 0) phase += 13;
  
  // Calcola il raggio e il centro
  const int radius = size / 2;
  const int centerX = x + radius;
  const int centerY = y + radius;
  const int outlineWidth = 1;
  
  // Disegna il contorno esterno della luna
  display.drawCircle(centerX, centerY, radius - outlineWidth, GxEPD_BLACK);
  
  // Gestione delle diverse fasi lunari
  if (phase == 0 || phase == 12) {
    // 0: Luna nuova - solo contorno
    return true;
  } 
  else if (phase == 6) {
    // 6: Luna piena - cerchio pieno
    display.fillCircle(centerX, centerY, radius - outlineWidth - 1, GxEPD_BLACK);
  }
  else {
    // Fasi intermedie
    const bool isWaxing = (phase < 6); // Crescente (true) o calante (false)
    const int phaseRadius = radius - outlineWidth - 1;
    
    // Disegna la forma della luna in base alla fase
    display.fillCircle(centerX, centerY, phaseRadius, GxEPD_WHITE); // Sfondo bianco
    
    // Calcola l'offset per la fase corrente (da -radius a +radius)
    const int phaseOffset = (isWaxing ? -1 : 1) * 
                           (radius * 2 * phase / 13 - radius);
    
    // Disegna la parte scura della luna
    display.fillCircle(centerX + phaseOffset, centerY, phaseRadius, GxEPD_BLACK);
    
    // Ridisegna il bordo per pulire eventuali imperfezioni
    display.drawCircle(centerX, centerY, phaseRadius, GxEPD_BLACK);
  }
  
  return true;
}

bool SVGHelper::loadSVG(DisplayType& display, const char* filename, int x, int y, int width, int height) {
  // Chiama la versione con viewBox usando tutto il documento
  return loadSVGWithViewBox(display, filename, x, y, width, height, -1, -1, -1, -1);
}

// Nuova versione con supporto viewBox personalizzato
bool SVGHelper::loadSVGWithViewBox(DisplayType& display, const char* filename, int x, int y, int width, int height, 
                                    int customViewBoxX, int customViewBoxY, int customViewBoxWidth, int customViewBoxHeight) {
  if (!initialized && !begin()) {
    return false;
  }
  
  // Verifica se il file esiste
  if (!SD.exists(filename)) {
    Serial.print("[SVG] File non trovato: ");
    Serial.println(filename);
    return false;
  }
  
  // Apri il file
  File svgFile = SD.open(filename);
  if (!svgFile) {
    Serial.print("[SVG] Impossibile aprire il file: ");
    Serial.println(filename);
    return false;
  }
  
  const int bufferSize = 128;
  char buffer[bufferSize];
  String line = "";
  int viewBoxX = 0, viewBoxY = 0, viewBoxWidth = 0, viewBoxHeight = 0;
  bool inPath = false;
  String currentPath = "";
  
  // Calcolo del fattore di scala
  float scaleX = 1.0, scaleY = 1.0;
  
  // Prima passata: estrarre il viewBox
  while (svgFile.available()) {
    int bytesRead = svgFile.readBytesUntil('\n', buffer, bufferSize - 1);
    buffer[bytesRead] = '\0';
    line = String(buffer);
    
    // Cerca la definizione del viewBox
    if (line.indexOf("viewBox") != -1) {
      int start = line.indexOf("viewBox=\"");
      if (start != -1) {
        start += 9; // Lunghezza di "viewBox=\""
        int end = line.indexOf('\"', start);
        if (end != -1) {
          String viewBox = line.substring(start, end);
          
          // Formato viewBox: "x y width height"
          int space1 = viewBox.indexOf(' ');
          int space2 = viewBox.indexOf(' ', space1 + 1);
          int space3 = viewBox.indexOf(' ', space2 + 1);
          
          if (space1 != -1 && space2 != -1 && space3 != -1) {
            viewBoxX = viewBox.substring(0, space1).toInt();
            viewBoxY = viewBox.substring(space1 + 1, space2).toInt();
            viewBoxWidth = viewBox.substring(space2 + 1, space3).toInt();
            viewBoxHeight = viewBox.substring(space3 + 1).toInt();
            
            // Calcola il fattore di scala
            scaleX = (float)width / viewBoxWidth;
            scaleY = (float)height / viewBoxHeight;
            
            Serial.print("[SVG] ViewBox trovato: ");
            Serial.print(viewBoxX);
            Serial.print(" ");
            Serial.print(viewBoxY);
            Serial.print(" ");
            Serial.print(viewBoxWidth);
            Serial.print(" ");
            Serial.println(viewBoxHeight);
            Serial.print("[SVG] Scala: ");
            Serial.print(scaleX);
            Serial.print(" x ");
            Serial.println(scaleY);
            break;
          }
        }
      }
    }
  }
  
  // Se è stato specificato un viewBox personalizzato, usalo
  if (customViewBoxX >= 0 && customViewBoxY >= 0 && customViewBoxWidth > 0 && customViewBoxHeight > 0) {
    viewBoxX = customViewBoxX;
    viewBoxY = customViewBoxY;
    viewBoxWidth = customViewBoxWidth;
    viewBoxHeight = customViewBoxHeight;
    scaleX = (float)width / viewBoxWidth;
    scaleY = (float)height / viewBoxHeight;
  }
  // Se non è stato trovato un viewBox valido nel file, usa valori predefiniti
  else if (viewBoxWidth == 0 || viewBoxHeight == 0) {
    Serial.println("[SVG] ViewBox non trovato, uso valori predefiniti");
    viewBoxWidth = 1000;
    viewBoxHeight = 1000;
    scaleX = (float)width / viewBoxWidth;
    scaleY = (float)height / viewBoxHeight;
  }
  
  // Riapri il file per la seconda passata
  svgFile.close();
  svgFile = SD.open(filename);
  
  // Seconda passata: leggi l'intero file SVG (fino a 8KB, aumentato)
  String svgContent = "";
  const int MAX_SVG_SIZE = 8192; // Aumentato da 4KB a 8KB
  while (svgFile.available() && svgContent.length() < MAX_SVG_SIZE) {
    svgContent += (char)svgFile.read();
  }
  svgFile.close();
  
  Serial.print("[SVG] Contenuto SVG letto: ");
  Serial.print(svgContent.length());
  Serial.println(" bytes");
  
  int pathCount = 0;
  int circleCount = 0;
  const int MAX_PATHS = 50; // Limite massimo path per evitare freeze
  const int MAX_PATH_LENGTH = 3000; // Limite lunghezza singolo path
  
  // Cerca e disegna tutti i tag path nel contenuto
  int searchStart = 0;
  while (pathCount < MAX_PATHS) {
    int pathStart = svgContent.indexOf("<path", searchStart);
    if (pathStart == -1) break;
    
    int dStart = svgContent.indexOf("d=\"", pathStart);
    if (dStart == -1) {
      searchStart = pathStart + 5;
      continue;
    }
    
    dStart += 3; // Salta "d=\""
    int dEnd = svgContent.indexOf('\"', dStart);
    if (dEnd == -1) {
      searchStart = pathStart + 5;
      continue;
    }
    
    String pathData = svgContent.substring(dStart, dEnd);
    
    // Salta path troppo lunghi per evitare freeze
    if (pathData.length() > MAX_PATH_LENGTH) {
      Serial.print("[SVG] ⚠️  Path #");
      Serial.print(pathCount + 1);
      Serial.print(" troppo lungo (");
      Serial.print(pathData.length());
      Serial.println(" char), saltato");
      searchStart = dEnd + 1;
      continue;
    }
    
    Serial.print("[SVG] Disegno path #");
    Serial.print(++pathCount);
    Serial.print(" (lunghezza: ");
    Serial.print(pathData.length());
    Serial.println(")");
    drawPath(display, pathData, x, y, min(scaleX, scaleY));
    
    searchStart = dEnd + 1;
  }
  
  if (pathCount >= MAX_PATHS) {
    Serial.println("[SVG] ⚠️  Raggiunto limite massimo path, stop rendering");
  }
  
  Serial.print("[SVG] Trovati ");
  Serial.print(pathCount);
  Serial.println(" path");
  
  return true;
}

/* [RIMOSSO] duplicata implementazione precoce di extractAndDrawIcon */

// Funzione di supporto per estrarre attributi numerici da una stringa SVG
int SVGHelper::extractAttribute(const String& line, const char* attr, int defaultValue) {
  String attrStr = String(attr) + "=\"";
  int start = line.indexOf(attrStr);
  if (start < 0) return defaultValue;
  
  start += attrStr.length();
  int end = line.indexOf("\"", start);
  if (end < 0) return defaultValue;
  
  return line.substring(start, end).toInt();
}

// Disegna un'ellisse approssimata
void SVGHelper::drawEllipse(DisplayType& display, int centerX, int centerY, int radiusX, int radiusY, bool fill) {
  if (fill) {
    // Per riempire l'ellisse usiamo linee orizzontali
    for (int y = centerY - radiusY; y <= centerY + radiusY; y++) {
      int x_width = radiusX * sqrt(1.0 - pow((float)(y - centerY) / radiusY, 2));
      display.drawLine(centerX - x_width, y, centerX + x_width, y, GxEPD_BLACK);
    }
  } else {
    // Per il contorno disegniamo punti sulla circonferenza
    for (int i = 0; i < 360; i += 5) {
      float angle = i * PI / 180.0;
      int x = centerX + radiusX * cos(angle);
      int y = centerY + radiusY * sin(angle);
      display.drawPixel(x, y, GxEPD_BLACK);
    }
  }
}

// Funzione helper per disegnare cerchi
void SVGHelper::drawCircle(DisplayType& display, int cx, int cy, int r, bool fill) {
  if (fill) {
    display.fillCircle(cx, cy, r, GxEPD_BLACK);
  } else {
    display.drawCircle(cx, cy, r, GxEPD_BLACK);
  }
}

// Funzione per disegnare un percorso SVG (path)
void SVGHelper::drawPath(DisplayType& display, const String& path, int offsetX, int offsetY, float scale) {
  // Coordinate correnti per i comandi relativi
  float currentX = 0, currentY = 0;
  
  // Ultime coordinate del comando moveTo, necessarie per i comandi Z (chiusura percorso)
  float startX = 0, startY = 0;
  
  // Ultima coordinata di controllo per le curve bezier
  float lastControlX = 0, lastControlY = 0;
  
  // Indice corrente nella stringa del percorso
  int i = 0;
  int len = path.length();
  
  // Comando corrente (M, L, C, Q, Z, etc.)
  char command = ' ';
  
  // Funzione di supporto per saltare gli spazi bianchi
  auto skipWhitespace = [&]() {
    while (i < len && (path.charAt(i) == ' ' || path.charAt(i) == ',' || path.charAt(i) == '\t' || path.charAt(i) == '\n')) {
      i++;
    }
  };
  
  // Funzione per estrarre un numero decimale
  auto parseNumber = [&]() -> float {
    skipWhitespace();
    
    // Controlla se siamo alla fine della stringa
    if (i >= len) return 0;
    
    bool negative = false;
    if (path.charAt(i) == '-') {
      negative = true;
      i++;
    } else if (path.charAt(i) == '+') {
      i++;
    }
    
    float result = 0;
    // Parte intera
    while (i < len && isdigit(path.charAt(i))) {
      result = result * 10 + (path.charAt(i) - '0');
      i++;
    }
    
    // Parte decimale
    if (i < len && path.charAt(i) == '.') {
      i++;
      float fraction = 0.1;
      while (i < len && isdigit(path.charAt(i))) {
        result += (path.charAt(i) - '0') * fraction;
        fraction *= 0.1;
        i++;
      }
    }
    
    return negative ? -result : result;
  };
  
  while (i < len) {
    skipWhitespace();
    
    // Se siamo alla fine della stringa, esci
    if (i >= len) break;
    
    char c = path.charAt(i);
    
    // Se il carattere è una lettera, è un nuovo comando
    if (isAlpha(c)) {
      command = c;
      i++;
      skipWhitespace();
    }
    
    // Esecuzione del comando
    switch (command) {
      case 'M': // MoveTo assoluto
      case 'm': { // MoveTo relativo
        float x = parseNumber();
        float y = parseNumber();
        
        if (command == 'm') { // Relativo
          currentX += x;
          currentY += y;
        } else { // Assoluto
          currentX = x;
          currentY = y;
        }
        
        // Salva come punto iniziale (per la chiusura percorso)
        startX = currentX;
        startY = currentY;
        
        // Dopo il primo punto, MoveTo diventa LineTo (come da specifiche SVG)
        command = (command == 'M') ? 'L' : 'l';
        break;
      }
      
      case 'L': // LineTo assoluto
      case 'l': { // LineTo relativo
        float x = parseNumber();
        float y = parseNumber();
        
        float nextX, nextY;
        if (command == 'l') { // Relativo
          nextX = currentX + x;
          nextY = currentY + y;
        } else { // Assoluto
          nextX = x;
          nextY = y;
        }
        
        // Disegna la linea
        display.drawLine(offsetX + currentX * scale, offsetY + currentY * scale,
                        offsetX + nextX * scale, offsetY + nextY * scale,
                        GxEPD_BLACK);
        
        currentX = nextX;
        currentY = nextY;
        break;
      }
      
      case 'H': // Linea orizzontale assoluta
      case 'h': { // Linea orizzontale relativa
        float x = parseNumber();
        
        float nextX;
        if (command == 'h') { // Relativo
          nextX = currentX + x;
        } else { // Assoluto
          nextX = x;
        }
        
        // Disegna la linea orizzontale
        display.drawLine(offsetX + currentX * scale, offsetY + currentY * scale,
                        offsetX + nextX * scale, offsetY + currentY * scale,
                        GxEPD_BLACK);
        
        currentX = nextX;
        break;
      }
      
      case 'V': // Linea verticale assoluta
      case 'v': { // Linea verticale relativa
        float y = parseNumber();
        
        float nextY;
        if (command == 'v') { // Relativo
          nextY = currentY + y;
        } else { // Assoluto
          nextY = y;
        }
        
        // Disegna la linea verticale
        display.drawLine(offsetX + currentX * scale, offsetY + currentY * scale,
                        offsetX + currentX * scale, offsetY + nextY * scale,
                        GxEPD_BLACK);
        
        currentY = nextY;
        break;
      }
      
      case 'Z': // Chiudi percorso (sia 'Z' che 'z' sono uguali)
      case 'z': {
        // Disegna linea dal punto corrente al punto iniziale
        display.drawLine(offsetX + currentX * scale, offsetY + currentY * scale,
                        offsetX + startX * scale, offsetY + startY * scale,
                        GxEPD_BLACK);
        
        currentX = startX;
        currentY = startY;
        
        // Dopo una chiusura, dobbiamo avere un esplicito moveTo
        command = ' ';
        break;
      }
      
      // Casi base per le curve (implementazione semplificata)
      case 'C': // Curva Bezier cubica assoluta
      case 'c': // Curva Bezier cubica relativa
      case 'S': // Curva Bezier cubica di continuazione assoluta
      case 's': // Curva Bezier cubica di continuazione relativa
      case 'Q': // Curva Bezier quadratica assoluta
      case 'q': // Curva Bezier quadratica relativa
      case 'T': // Curva Bezier quadratica di continuazione assoluta
      case 't': {
        // Per le curve, una soluzione semplificata è convertirle in segmenti di linea
        // Le curve vengono approssimate con segmenti lineari per semplicità
        
        float x1, y1, x2, y2, x, y;
        
        // Per le curve cubiche (C, c, S, s)
        if (command == 'C' || command == 'c' || command == 'S' || command == 's') {
          // Per S/s, usiamo il riflesso dell'ultimo punto di controllo
          if (command == 'S' || command == 's') {
            x1 = 2 * currentX - lastControlX;
            y1 = 2 * currentY - lastControlY;
          } else {
            // Primo punto di controllo
            x1 = parseNumber();
            y1 = parseNumber();
            
            if (command == 'c') { // Relativo
              x1 += currentX;
              y1 += currentY;
            }
          }
          
          // Secondo punto di controllo
          x2 = parseNumber();
          y2 = parseNumber();
          
          if (command == 'c' || command == 's') { // Relativo
            x2 += currentX;
            y2 += currentY;
          }
          
          // Punto finale
          x = parseNumber();
          y = parseNumber();
          
          if (command == 'c' || command == 's') { // Relativo
            x += currentX;
            y += currentY;
          }
          
          // Salva l'ultimo punto di controllo per il comando S
          lastControlX = x2;
          lastControlY = y2;
          
          // Approssima la curva con segmenti (10 segmenti)
          float prevX = currentX;
          float prevY = currentY;
          
          for (int t = 1; t <= 10; t++) {
            float t_norm = t / 10.0;
            float t_inv = 1.0 - t_norm;
            
            // Calcola il punto sulla curva di Bezier cubica
            float pointX = t_inv*t_inv*t_inv*currentX + 3*t_inv*t_inv*t_norm*x1 + 3*t_inv*t_norm*t_norm*x2 + t_norm*t_norm*t_norm*x;
            float pointY = t_inv*t_inv*t_inv*currentY + 3*t_inv*t_inv*t_norm*y1 + 3*t_inv*t_norm*t_norm*y2 + t_norm*t_norm*t_norm*y;
            
            // Disegna un segmento di linea
            display.drawLine(offsetX + prevX * scale, offsetY + prevY * scale,
                          offsetX + pointX * scale, offsetY + pointY * scale,
                          GxEPD_BLACK);
            
            prevX = pointX;
            prevY = pointY;
          }
          
          currentX = x;
          currentY = y;
        }
        // Per le curve quadratiche (Q, q, T, t)
        else {
          // Per T/t, usiamo il riflesso dell'ultimo punto di controllo
          if (command == 'T' || command == 't') {
            x1 = 2 * currentX - lastControlX;
            y1 = 2 * currentY - lastControlY;
          } else {
            // Punto di controllo
            x1 = parseNumber();
            y1 = parseNumber();
            
            if (command == 'q') { // Relativo
              x1 += currentX;
              y1 += currentY;
            }
          }
          
          // Punto finale
          x = parseNumber();
          y = parseNumber();
          
          if (command == 'q' || command == 't') { // Relativo
            x += currentX;
            y += currentY;
          }
          
          // Salva l'ultimo punto di controllo per il comando T
          lastControlX = x1;
          lastControlY = y1;
          
          // Approssima la curva con segmenti (10 segmenti)
          float prevX = currentX;
          float prevY = currentY;
          
          for (int t = 1; t <= 10; t++) {
            float t_norm = t / 10.0;
            float t_inv = 1.0 - t_norm;
            
            // Calcola il punto sulla curva di Bezier quadratica
            float pointX = t_inv*t_inv*currentX + 2*t_inv*t_norm*x1 + t_norm*t_norm*x;
            float pointY = t_inv*t_inv*currentY + 2*t_inv*t_norm*y1 + t_norm*t_norm*y;
            
            // Disegna un segmento di linea
            display.drawLine(offsetX + prevX * scale, offsetY + prevY * scale,
                          offsetX + pointX * scale, offsetY + pointY * scale,
                          GxEPD_BLACK);
            
            prevX = pointX;
            prevY = pointY;
          }
          
          currentX = x;
          currentY = y;
        }
        
        break;
      }
      
      // Comando non riconosciuto o non implementato
      default:
        // Salta al prossimo comando
        while (i < len && !isAlpha(path.charAt(i))) {
          i++;
        }
        break;
    }
  }
}

// Disegna una forma di luna crescente o calante
void SVGHelper::drawMoonShape(DisplayType& display, int centerX, int centerY, int radius, bool isWaxing) {
  // Disegna il contorno della luna
  display.drawCircle(centerX, centerY, radius, GxEPD_BLACK);
  
  // Disegna la parte illuminata
  for (int y = centerY - radius + 1; y < centerY + radius; y++) {
    int width = sqrt(radius*radius - (y-centerY)*(y-centerY));
    if (isWaxing) {
      // Luna crescente (illuminata a destra)
      display.fillRect(centerX, y, width, 1, GxEPD_BLACK);
    } else {
      // Luna calante (illuminata a sinistra)
      display.fillRect(centerX - width, y, width, 1, GxEPD_BLACK);
    }
  }
}

// Converte un ID meteo OpenWeatherMap nell'icona appropriata
WeatherIcon SVGHelper::getIconFromWeatherID(int weatherID, bool isNight, float windSpeed) {
  /*
   * Codici meteo principali OpenWeatherMap:
   * Gruppo 2xx: Temporale (200-232)
   * Gruppo 3xx: Pioggerella (300-321)
   * Gruppo 5xx: Pioggia (500-531)
   * Gruppo 6xx: Neve (600-622)
   * Gruppo 7xx: Atmosfera (701-781: nebbia, foschia, polvere, sabbia, cenere, tornado)
   * Gruppo 800: Cielo sereno
   * Gruppo 80x: Nuvolosità (801-804)
   * Gruppo 90x: Eventi estremi (900-906: tornado, uragano, freddo/caldo estremo, vento forte)
   */
  
  // Temporale (200-232)
  if (weatherID >= 200 && weatherID < 300) {
    // Temporale con pioggia leggera/moderata (200-202, 230-232)
    if (weatherID <= 202 || weatherID >= 230) {
      return isNight ? ICON_NIGHT_THUNDERSTORM : ICON_DAY_THUNDERSTORM;
    }
    // Temporale intenso (210-221)
    else {
      return isNight ? ICON_NIGHT_STORM_SHOWERS : ICON_DAY_STORM_SHOWERS;
    }
  }
  
  // Pioggerella (300-321)
  else if (weatherID >= 300 && weatherID < 400) {
    return isNight ? ICON_NIGHT_SPRINKLE : ICON_DAY_SPRINKLE;
  }
  
  // Pioggia (500-531)
  else if (weatherID >= 500 && weatherID < 600) {
    // Pioggia leggera (500-501, 520)
    if (weatherID <= 501 || weatherID == 520) {
      return isNight ? ICON_NIGHT_SPRINKLE : ICON_DAY_SPRINKLE;
    }
    // Pioggia moderata/forte (502-504, 521-522, 531)
    else if (weatherID <= 504 || weatherID == 521 || weatherID == 522 || weatherID == 531) {
      return isNight ? ICON_NIGHT_RAIN : ICON_DAY_RAIN;
    }
    // Pioggia molto forte (511: freezing rain)
    else if (weatherID == 511) {
      return isNight ? ICON_NIGHT_SLEET : ICON_DAY_SLEET;
    }
    // Acquazzoni (520-531)
    else {
      return isNight ? ICON_NIGHT_SHOWERS : ICON_DAY_SHOWERS;
    }
  }
  
  // Neve (600-622)
  else if (weatherID >= 600 && weatherID < 700) {
    // Neve leggera (600)
    if (weatherID == 600) {
      return isNight ? ICON_NIGHT_SNOW : ICON_DAY_SNOW;
    }
    // Neve (601)
    else if (weatherID == 601) {
      return isNight ? ICON_NIGHT_SNOW : ICON_DAY_SNOW;
    }
    // Neve intensa (602)
    else if (weatherID == 602) {
      return ICON_SNOWFLAKE_COLD; // Icona neutra per neve molto intensa
    }
    // Sleet/Nevischio (611-615)
    else if (weatherID >= 611 && weatherID <= 615) {
      return isNight ? ICON_NIGHT_SLEET : ICON_DAY_SLEET;
    }
    // Sleet con temporale (616)
    else if (weatherID == 616) {
      return isNight ? ICON_NIGHT_SLEET_STORM : ICON_DAY_SLEET_STORM;
    }
    // Neve con temporale (620-621)
    else if (weatherID == 620 || weatherID == 621) {
      return isNight ? ICON_NIGHT_SNOW_THUNDERSTORM : ICON_DAY_SNOW_THUNDERSTORM;
    }
    // Neve intensa con temporale (622)
    else if (weatherID == 622) {
      return isNight ? ICON_NIGHT_SNOW_THUNDERSTORM : ICON_DAY_SNOW_THUNDERSTORM;
    }
    else {
      return isNight ? ICON_NIGHT_SNOW : ICON_DAY_SNOW;
    }
  }
  
  // Atmosfera (700-781)
  else if (weatherID >= 700 && weatherID < 800) {
    // Nebbia/Foschia (701, 741)
    if (weatherID == 701 || weatherID == 741) {
      return isNight ? ICON_NIGHT_FOG : ICON_DAY_FOG;
    }
    // Fumo (711)
    else if (weatherID == 711) {
      return ICON_SMOKE;
    }
    // Foschia/Caligine (721)
    else if (weatherID == 721) {
      return ICON_HAZE;
    }
    // Polvere (731, 761)
    else if (weatherID == 731 || weatherID == 761) {
      return ICON_DUST;
    }
    // Sabbia (751)
    else if (weatherID == 751) {
      return ICON_SANDSTORM;
    }
    // Cenere vulcanica (762)
    else if (weatherID == 762) {
      return ICON_VOLCANO;
    }
    // Tornado (781)
    else if (weatherID == 781) {
      return ICON_TORNADO;
    }
    // Altri fenomeni atmosferici
    else {
      return ICON_FOG; // Icona neutra nebbia
    }
  }
  
  // Cielo sereno (800)
  else if (weatherID == 800) {
    return isNight ? ICON_NIGHT_CLEAR : ICON_DAY_SUNNY;
  }
  
  // Nuvolosità (801-804)
  else if (weatherID > 800 && weatherID < 900) {
    // Controlla se c'è vento forte (> 30 km/h) per icone varianti
    bool strongWind = windSpeed > 30.0f;
    bool veryStrongWind = windSpeed > 50.0f;
    
    // Poche nuvole (801-802)
    if (weatherID <= 802) {
      if (veryStrongWind) {
        return isNight ? ICON_NIGHT_CLOUDY_GUSTS : ICON_DAY_CLOUDY_GUSTS;
      } else if (strongWind) {
        return isNight ? ICON_NIGHT_CLOUDY_WINDY : ICON_DAY_CLOUDY_WINDY;
      }
      return isNight ? ICON_NIGHT_CLOUDY : ICON_DAY_CLOUDY;
    }
    // Nubi sparse/coperto (803-804)
    else {
      if (strongWind) {
        return ICON_CLOUDY_WINDY;
      }
      return ICON_CLOUDY; // Icona neutra nuvoloso
    }
  }
  
  // Condizioni estreme (900-906) - Estensioni API OpenWeatherMap
  else if (weatherID >= 900 && weatherID < 910) {
    // Tornado (900)
    if (weatherID == 900) {
      return ICON_TORNADO;
    }
    // Tempesta tropicale (901)
    else if (weatherID == 901) {
      return ICON_HURRICANE;
    }
    // Uragano (902)
    else if (weatherID == 902) {
      return ICON_HURRICANE;
    }
    // Freddo estremo (903)
    else if (weatherID == 903) {
      return ICON_SNOWFLAKE_COLD;
    }
    // Caldo estremo (904)
    else if (weatherID == 904) {
      return ICON_HOT;
    }
    // Ventoso (905-906)
    else if (weatherID >= 905 && weatherID <= 906) {
      return ICON_WINDY;
    }
  }
  
  // Valore di default
  return isNight ? ICON_NIGHT_CLEAR : ICON_DAY_SUNNY;
}

/**
 * @brief Estrae e disegna un'icona specifica da un file SVG
 * 
 * DEPRECATO: Questa funzione non è più necessaria - usiamo file SVG individuali
 * Wrapper per compatibilità che chiama drawWeatherIcon
 * 
 * @param display Riferimento all'oggetto display
 * @param filename Percorso del file SVG (ignorato, usa getIconFilePath)
 * @param icon Indice dell'icona da estrarre
 * @param x Coordinata X di destinazione sul display
 * @param y Coordinata Y di destinazione sul display
 * @param size Dimensione desiderata dell'icona (larghezza e altezza)
 * @return true se l'icona è stata trovata e disegnata con successo, false altrimenti
 */
bool SVGHelper::extractAndDrawIcon(DisplayType& display, const char* filename, WeatherIcon icon, int x, int y, int size) {
  // Wrapper deprecato - chiama la nuova funzione
  (void)filename; // Ignora il filename, usa getIconFilePath invece
  return drawWeatherIcon(display, icon, x, y, size);
}
