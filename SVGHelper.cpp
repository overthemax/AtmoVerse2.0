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

// Mappatura delle posizioni delle icone nel file SVG
// Coordinate relative all'angolo in alto a sinistra dell'area di disegno
const IconCoords SVGHelper::iconPositions[ICON_COUNT] = {
  {150, 100},  // ICON_SOL_SEMICOPERTO (Riga 1, Col 1)
  {350, 190},  // ICON_SOLE (Riga 1, Col 2)
  {550, 150},  // ICON_SOLE_NUVOLOSO (Riga 1, Col 3)
  {850, 150},  // ICON_SOLE_TEMPESTA (Riga 1, Col 4)
  
  {150, 350},  // ICON_NUVOLA_PIOGGIA1 (Riga 2, Col 1)
  {350, 350},  // ICON_NUVOLA_PIOGGIA2 (Riga 2, Col 2)
  {550, 350},  // ICON_NUVOLA_TEMPORALE1 (Riga 2, Col 3)
  {750, 350},  // ICON_NUVOLA_TEMPORALE2 (Riga 2, Col 4)
  
  {150, 600},  // ICON_NEVE_PIOGGIA1 (Riga 3, Col 1)
  {350, 600},  // ICON_NEVE_PIOGGIA2 (Riga 3, Col 2)
  {550, 600},  // ICON_NEVE1 (Riga 3, Col 3)
  {750, 600}   // ICON_NEVE2 (Riga 3, Col 4)
};

// Buffer per il parsing dei file SVG
static char pathBuffer[SVGHelper::MAX_PATH_LENGTH];

// Funzioni di utilità per il parsing SVG
static bool skipWhitespace(const char*& str);
static bool parseNumber(const char*& str, float& value);
static bool parseCommand(const char*& str, char& cmd, bool& relative);
static bool parseCoord(const char*& str, float& x, float& y, bool relative, float lastX, float lastY);

bool SVGHelper::begin(int8_t csPin) {
  if (initialized) {
    return true;
  }
  
  // Inizializza la scheda SD
  if (!SD.begin(csPin)) {
    Serial.println(F("[SVG] Errore nell'inizializzazione della scheda SD"));
    return false;
  }
  
  // Verifica la presenza della cartella delle icone
  if (!SD.exists("/www/icons")) {
    if (!SD.mkdir("/www")) {
      Serial.println(F("[SVG] Impossibile creare la directory /www"));
      return false;
    }
    if (!SD.mkdir("/www/icons")) {
      Serial.println(F("[SVG] Impossibile creare la directory /www/icons"));
      return false;
    }
  }
  
  Serial.println(F("[SVG] Inizializzazione completata con successo"));
  initialized = true;
  return true;
}

bool SVGHelper::drawWeatherIcon(GxEPD_Class& display, WeatherIcon icon, int x, int y, int size) {
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
  
  // Percorso del file SVG delle icone meteo
  const char* iconFile = "/www/icons/weather_icons.svg";
  
  // Verifica che il file esista
  if (!SD.exists(iconFile)) {
    Serial.print(F("[SVG] File non trovato: "));
    Serial.println(iconFile);
    return false;
  }
  
  // Estrai e disegna l'icona specifica
  return extractAndDrawIcon(display, iconFile, icon, x, y, size);
}

bool SVGHelper::drawMoonPhase(GxEPD_Class& display, int x, int y, int size, int phase) {
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

bool SVGHelper::loadSVG(GxEPD_Class& display, const char* filename, int x, int y, int width, int height) {
  if (!initialized && !begin()) {
    return false;
  }
  
  // Verifica se il file esiste
  if (!SD.exists(filename)) {
    Serial.print("File non trovato: ");
    Serial.println(filename);
    return false;
  }
  
  // Apri il file
  File svgFile = SD.open(filename);
  if (!svgFile) {
    Serial.print("Impossibile aprire il file: ");
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
            break;
          }
        }
      }
    }
  }
  
  // Se non è stato trovato un viewBox valido, usa valori predefiniti
  if (viewBoxWidth == 0 || viewBoxHeight == 0) {
    viewBoxWidth = 1000;
    viewBoxHeight = 1000;
    scaleX = (float)width / viewBoxWidth;
    scaleY = (float)height / viewBoxHeight;
  }
  
  // Riapri il file per la seconda passata
  svgFile.close();
  svgFile = SD.open(filename);
  
  // Seconda passata: analizzare e disegnare gli elementi
  while (svgFile.available()) {
    int bytesRead = svgFile.readBytesUntil('\n', buffer, bufferSize - 1);
    buffer[bytesRead] = '\0';
    line = String(buffer);
    
    // Gestione dei tag path (disegni complessi)
    if (line.indexOf("<path") != -1) {
      int dStart = line.indexOf("d=\"");
      if (dStart != -1) {
        dStart += 3; // Lunghezza di "d=\""
        int dEnd = line.indexOf('\"', dStart);
        if (dEnd != -1) {
          String pathData = line.substring(dStart, dEnd);
          drawPath(display, pathData, x, y, min(scaleX, scaleY));
        }
      }
    }
    
    // Gestione dei cerchi
    else if (line.indexOf("<circle") != -1) {
      int cx = extractAttribute(line, "cx", 0);
      int cy = extractAttribute(line, "cy", 0);
      int r = extractAttribute(line, "r", 0);
      bool fill = line.indexOf("fill=\"none\"") == -1;
      
      int scaledCX = x + cx * scaleX - viewBoxX * scaleX;
      int scaledCY = y + cy * scaleY - viewBoxY * scaleY;
      int scaledR = r * min(scaleX, scaleY);
      
      drawCircle(display, scaledCX, scaledCY, scaledR, fill);
    }
    
    // Gestione delle ellissi
    else if (line.indexOf("<ellipse") != -1) {
      int cx = extractAttribute(line, "cx", 0);
      int cy = extractAttribute(line, "cy", 0);
      int rx = extractAttribute(line, "rx", 0);
      int ry = extractAttribute(line, "ry", 0);
      bool fill = line.indexOf("fill=\"none\"") == -1;
      
      int scaledCX = x + cx * scaleX - viewBoxX * scaleX;
      int scaledCY = y + cy * scaleY - viewBoxY * scaleY;
      int scaledRX = rx * scaleX;
      int scaledRY = ry * scaleY;
      
      drawEllipse(display, scaledCX, scaledCY, scaledRX, scaledRY, fill);
    }
    
    // Gestione delle linee
    else if (line.indexOf("<line") != -1) {
      int x1 = extractAttribute(line, "x1", 0);
      int y1 = extractAttribute(line, "y1", 0);
      int x2 = extractAttribute(line, "x2", 0);
      int y2 = extractAttribute(line, "y2", 0);
      
      int scaledX1 = x + x1 * scaleX - viewBoxX * scaleX;
      int scaledY1 = y + y1 * scaleY - viewBoxY * scaleY;
      int scaledX2 = x + x2 * scaleX - viewBoxX * scaleX;
      int scaledY2 = y + y2 * scaleY - viewBoxY * scaleY;
      
      display.drawLine(scaledX1, scaledY1, scaledX2, scaledY2, GxEPD_BLACK);
    }
    
    // Gestione dei rettangoli
    else if (line.indexOf("<rect") != -1) {
      int rectX = extractAttribute(line, "x", 0);
      int rectY = extractAttribute(line, "y", 0);
      int rectWidth = extractAttribute(line, "width", 0);
      int rectHeight = extractAttribute(line, "height", 0);
      bool fill = line.indexOf("fill=\"none\"") == -1;
      
      int scaledX = x + rectX * scaleX - viewBoxX * scaleX;
      int scaledY = y + rectY * scaleY - viewBoxY * scaleY;
      int scaledWidth = rectWidth * scaleX;
      int scaledHeight = rectHeight * scaleY;
      
      if (fill) {
        display.fillRect(scaledX, scaledY, scaledWidth, scaledHeight, GxEPD_BLACK);
      } else {
        display.drawRect(scaledX, scaledY, scaledWidth, scaledHeight, GxEPD_BLACK);
      }
    }
  }
bool SVGHelper::extractAndDrawIcon(GxEPD_Class& display, const char* filename, WeatherIcon icon, int x, int y, int size) {
  // Verifica i parametri di input
  if (!initialized && !begin()) {
    Serial.println(F("[SVG] Errore: SVGHelper non inizializzato"));
    return false;
  }
  
  if (icon < 0 || icon >= ICON_COUNT) {
    Serial.print(F("[SVG] Errore: codice icona non valido: "));
    Serial.println(icon);
    return false;
  }
  
  if (size <= 0) {
    Serial.println(F("[SVG] Errore: dimensione non valida"));
  File svgFile = SD.open(filename);
  if (!svgFile) {
    Serial.print("Impossibile aprire il file: ");
    Serial.println(filename);
    return false;
  }
  
  const int bufferSize = 128;
  char buffer[bufferSize];
  String line = "";
  int viewBoxX = 0, viewBoxY = 0, viewBoxWidth = 0, viewBoxHeight = 0;
  
  // Coordinate dell'icona specifica
  int srcX = iconPositions[icon].x;
  int srcY = iconPositions[icon].y;
  int iconSize = 100; // Dimensione tipica di ogni icona nel file SVG
  
  // Prima passata: trovare il viewBox
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
            break;
          }
        }
      }
    }
  }
  
  // Se non è stato trovato un viewBox valido, usa valori predefiniti
  if (viewBoxWidth == 0 || viewBoxHeight == 0) {
    viewBoxWidth = 1200;
    viewBoxHeight = 900;
  }
  
  // Calcola il fattore di scala
  float scale = (float)size / iconSize;
  
  // Riapri il file
  svgFile.close();
  svgFile = SD.open(filename);
  
  bool inIconGroup = false;
  String currentPath = "";
  bool pathStarted = false;
  
  // Trasformazione per estrazione dell'icona specifica
  String targetTransform = String("translate(") + srcX + ", " + srcY + ")";
  
  // Cerca e disegna l'icona specifica
  while (svgFile.available()) {
    int bytesRead = svgFile.readBytesUntil('\n', buffer, bufferSize - 1);
    buffer[bytesRead] = '\0';
    line = String(buffer);
    
    // Trova il gruppo che corrisponde all'icona specifica
    if (line.indexOf("<g ") != -1 && line.indexOf(targetTransform) != -1) {
      inIconGroup = true;
      continue;
    }
    
    // Interrompi quando troviamo la fine del gruppo dell'icona
    if (inIconGroup && line.indexOf("</g>") != -1) {
      inIconGroup = false;
      break;
    }
    
    // Elabora gli elementi all'interno del gruppo dell'icona
    if (inIconGroup) {
      // Gestione dei tag path (disegni complessi)
      if (line.indexOf("<path") != -1) {
        int dStart = line.indexOf("d=\"");
        if (dStart != -1) {
          dStart += 3; // Lunghezza di "d=\""
          int dEnd = line.indexOf('\"', dStart);
          if (dEnd != -1) {
            String pathData = line.substring(dStart, dEnd);
            drawPath(display, pathData, x, y, scale);
          }
        }
      }
      
      // Gestione dei cerchi
      else if (line.indexOf("<circle") != -1) {
        int cx = extractAttribute(line, "cx", 0);
        int cy = extractAttribute(line, "cy", 0);
        int r = extractAttribute(line, "r", 0);
        bool fill = line.indexOf("fill=\"none\"") == -1;
        
        // Scala e posiziona l'elemento nell'area di destinazione
        int scaledCX = x + cx * scale;
        int scaledCY = y + cy * scale;
        int scaledR = r * scale;
        
        drawCircle(display, scaledCX, scaledCY, scaledR, fill);
      }
      
      // Gestione delle linee
      else if (line.indexOf("<line") != -1) {
        int x1 = extractAttribute(line, "x1", 0);
        int y1 = extractAttribute(line, "y1", 0);
        int x2 = extractAttribute(line, "x2", 0);
        int y2 = extractAttribute(line, "y2", 0);
        
        // Scala e posiziona l'elemento nell'area di destinazione
        int scaledX1 = x + x1 * scale;
        int scaledY1 = y + y1 * scale;
        int scaledX2 = x + x2 * scale;
        int scaledY2 = y + y2 * scale;
        
        display.drawLine(scaledX1, scaledY1, scaledX2, scaledY2, GxEPD_BLACK);
      }
      
      // Gestione dei rettangoli
      else if (line.indexOf("<rect") != -1) {
        int rectX = extractAttribute(line, "x", 0);
        int rectY = extractAttribute(line, "y", 0);
        int rectWidth = extractAttribute(line, "width", 0);
        int rectHeight = extractAttribute(line, "height", 0);
        bool fill = line.indexOf("fill=\"none\"") == -1;
        
        // Scala e posiziona l'elemento nell'area di destinazione
        int scaledX = x + rectX * scale;
        int scaledY = y + rectY * scale;
        int scaledWidth = rectWidth * scale;
        int scaledHeight = rectHeight * scale;
        
        if (fill) {
          display.fillRect(scaledX, scaledY, scaledWidth, scaledHeight, GxEPD_BLACK);
        } else {
          display.drawRect(scaledX, scaledY, scaledWidth, scaledHeight, GxEPD_BLACK);
        }
      }
    }
  }
  
  svgFile.close();
  
  // Se non siamo riusciti a trovare e disegnare l'icona, disegna un cerchio predefinito
  if (!inIconGroup) {
    drawCircle(display, x + size/2, y + size/2, size/2 - 2, false);
  }
  
  return true;
}

// Funzione di supporto per estrarre attributi numerici da una stringa SVG
int SVGHelper::extractAttribute(String& line, const char* attr, int defaultValue) {
  String attrStr = String(attr) + "=\"";
  int start = line.indexOf(attrStr);
  if (start < 0) return defaultValue;
  
  start += attrStr.length();
  int end = line.indexOf("\"", start);
  if (end < 0) return defaultValue;
  
  return line.substring(start, end).toInt();
}

// Disegna un'ellisse approssimata
void SVGHelper::drawEllipse(GxEPD_Class& display, int centerX, int centerY, int radiusX, int radiusY, bool fill) {
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
void SVGHelper::drawCircle(GxEPD_Class& display, int cx, int cy, int r, bool fill) {
  if (fill) {
    display.fillCircle(cx, cy, r, GxEPD_BLACK);
  } else {
    display.drawCircle(cx, cy, r, GxEPD_BLACK);
  }
}

// Funzione per disegnare un percorso SVG (path)
void SVGHelper::drawPath(GxEPD_Class& display, String path, int offsetX, int offsetY, float scale) {
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
void SVGHelper::drawMoonShape(GxEPD_Class& display, int centerX, int centerY, int radius, bool isWaxing) {
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
WeatherIcon SVGHelper::getIconFromWeatherID(int weatherID, bool isNight) {
  /*
   * Codici meteo principali OpenWeatherMap:
   * Gruppo 2xx: Temporale
   * Gruppo 3xx: Pioggerella
   * Gruppo 5xx: Pioggia
   * Gruppo 6xx: Neve
   * Gruppo 7xx: Atmosfera (nebbia, foschia)
   * Gruppo 800: Cielo sereno
   * Gruppo 80x: Nuvolosità
   */
  
  // Temporale
  if (weatherID >= 200 && weatherID < 300) {
    return ICON_NUVOLA_TEMPORALE1;
  }
  
  // Pioggia leggera/pioggerella
  else if ((weatherID >= 300 && weatherID < 400) || 
           (weatherID >= 500 && weatherID <= 504)) {
    return ICON_NUVOLA_PIOGGIA2; // Gocce corte
  }
  
  // Pioggia forte
  else if (weatherID >= 504 && weatherID < 600) {
    return ICON_NUVOLA_PIOGGIA1; // Gocce lunghe
  }
  
  // Neve
  else if (weatherID >= 600 && weatherID < 700) {
    return ICON_NEVE1;
  }
  
  // Atmosfera (nebbia, foschia)
  else if (weatherID >= 700 && weatherID < 800) {
    return ICON_SOL_SEMICOPERTO;
  }
  
  // Cielo sereno
  else if (weatherID == 800) {
    return isNight ? ICON_SOLE : ICON_SOLE; // Potremmo avere un'icona luna per la notte
  }
  
  // Nuvolosità
  else if (weatherID > 800 && weatherID < 900) {
    // Nuvolosità leggera/media
    if (weatherID <= 803) {
      return ICON_SOLE_NUVOLOSO;
    } 
    // Nuvolosità densa
    else {
      return ICON_SOL_SEMICOPERTO;
    }
  }
  
  // Valore di default
  return ICON_SOLE;
}

/**
 * @brief Estrae e disegna un'icona specifica da un file SVG
 * 
 * Questa funzione estrae un'icona specifica da un file SVG contenente più icone
 * e la disegna sul display nella posizione specificata con la dimensione desiderata.
 * 
 * @param display Riferimento all'oggetto display GxEPD
 * @param filename Percorso del file SVG contenente le icone
 * @param icon Indice dell'icona da estrarre
 * @param x Coordinata X di destinazione sul display
 * @param y Coordinata Y di destinazione sul display
 * @param size Dimensione desiderata dell'icona (larghezza e altezza)
 * @return true se l'icona è stata trovata e disegnata con successo, false altrimenti
 */
bool SVGHelper::extractAndDrawIcon(GxEPD_Class& display, const char* filename, WeatherIcon icon, int x, int y, int size) {
  // Verifica i parametri di input
  if (!initialized) {
    if (!begin()) {
      Serial.println(F("[SVG] Errore: inizializzazione fallita"));
      return false;
    }
  }

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

  if (!filename || strlen(filename) == 0) {
    Serial.println(F("[SVG] Errore: nome file non valido"));
    return false;
  }

  // Verifica che il file esista
  if (!SD.exists(filename)) {
    Serial.print(F("[SVG] File non trovato: "));
    Serial.print(filename);
    Serial.println('"');
    return false;
  }

  // Apri il file SVG
  File file = SD.open(filename, FILE_READ);
  if (!file) {
    Serial.print(F("[SVG] Impossibile aprire il file: "));
    Serial.println(filename);
    return false;
  }

  bool result = false;
  const size_t bufferSize = 256;
  char buffer[bufferSize];
  bool inSvg = false;
  bool inIcon = false;
  String currentId = "";
  
  // Crea un ID univoco per l'icona basato sulla sua posizione
  String targetId = "icon";
  targetId += icon;

  try {
    // Leggi il file riga per riga
    while (file.available()) {
      const int bytesRead = file.readBytesUntil('\n', buffer, bufferSize - 1);
      if (bytesRead <= 0) continue;
      
      buffer[bytesRead] = '\0';
      String line = String(buffer);
      
      // Cerca il tag di apertura SVG
      if (!inSvg) {
        if (line.indexOf("<svg") >= 0) {
          inSvg = true;
        }
        continue;
      }
      
      // Cerca il tag di chiusura SVG
      if (line.indexOf("</svg>") >= 0) {
        break;
      }
      
      // Cerca l'elemento con l'ID dell'icona
      const int idPos = line.indexOf("id=\"");
      if (idPos >= 0) {
        const int idStart = idPos + 4; // Lunghezza di "id=""
        const int idEnd = line.indexOf('"', idStart);
        if (idEnd > idStart) {
          currentId = line.substring(idStart, idEnd);
          inIcon = (currentId == targetId);
        }
      }
      
      // Se siamo all'interno dell'icona, disegna l'elemento
      if (inIcon) {
        // Sostituisci eventuali riferimenti a stili inline con colori espliciti
        line.replace("currentColor", "black");
        line.replace("#000", "black");
        line.replace("#000000", "black");
        
        // Disegna l'elemento in base al suo tipo
        if (line.indexOf("<path") >= 0) {
          // Estrai l'attributo d="..."
          const int dStart = line.indexOf("d=\"");
          if (dStart >= 0) {
            const int dEnd = line.indexOf('"', dStart + 3);
            if (dEnd > dStart) {
              String path = line.substring(dStart + 3, dEnd);
              const float scale = static_cast<float>(size) / 200.0f; // 200px è la dimensione predefinita
              drawPath(display, path, x, y, scale);
            }
          }
        }
        else if (line.indexOf("<circle") >= 0) {
          // Estrai gli attributi cx, cy, r, fill
          const int cx = extractAttribute(line, "cx", 0);
          const int cy = extractAttribute(line, "cy", 0);
          const int r = extractAttribute(line, "r", 0);
          const bool fill = (line.indexOf("fill=\"none\"") == -1);
          
          if (r > 0) {
            const float scale = static_cast<float>(size) / 200.0f;
            const int drawX = x + (cx - 100) * scale; // 100 è l'offset del centro
            const int drawY = y + (cy - 100) * scale;
            const int drawR = r * scale;
            
            drawCircle(display, drawX, drawY, drawR, fill);
          }
        }
        
        // Verifica se abbiamo raggiunto la fine dell'elemento
        if (line.indexOf("/>") >= 0 || line.indexOf("</") >= 0) {
          inIcon = false;
          result = true; // Abbiamo trovato e disegnato l'icona
          break;
        }
      }
    }
  } catch (...) {
    Serial.println(F("[SVG] Eccezione durante l'elaborazione del file SVG"));
    result = false;
  }
  
  // Chiudi il file
  file.close();
  
  if (!result) {
    Serial.print(F("[SVG] Impossibile trovare o disegnare l'icona: "));
    Serial.print(targetId);
    Serial.print(F(" in "));
    Serial.println(filename);
  }
  
  return result;
}
