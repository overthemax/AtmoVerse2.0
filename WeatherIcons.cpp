#include "WeatherIcons.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <SD.h>
#include <SPI.h>
#include <ArduinoJson.h>
#include "GxEPD.h"

// Inizializzazione dei membri statici
bool WeatherIcons::initialized = false;

// Buffer per i dati delle icone decodificate
static uint8_t* pngBuffer = nullptr;
static GxEPD_Class* currentDisplay = nullptr;
static int drawX = 0, drawY = 0, drawSize = 0;

// Timeout predefinito per le operazioni di rete (in ms)
static const uint32_t DEFAULT_NETWORK_TIMEOUT = 10000;

// Dimensione massima per il buffer di download (100KB)
static const size_t DOWNLOAD_BUFFER_SIZE = 102400;

// Struttura per passare i dati durante la decodifica PNG
struct PNGDrawData {
  GxEPD_Class* display;
  int x;
  int y;
  int size;
};

// Callback per la decodifica PNG
void pngDraw(PNGDRAW *pDraw) {
  if (!currentDisplay || !pngBuffer) return;
  
  uint16_t lineBuffer[pDraw->iWidth];
  
  // Ottieni i dati della linea corrente
  PNG.getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_BIG_ENDIAN, 0xffffffff);
  
  // Scala e disegna ogni pixel sull'e-ink display
  float scaleX = (float)drawSize / pDraw->iWidth;
  int yPos = drawY + (int)(pDraw->y * scaleX);
  
  // Convert RGB565 to 1-bit black and white using dithering
  for (int x = 0; x < pDraw->iWidth; x++) {
    uint16_t rgb = lineBuffer[x];
    
    // Estrai i valori RGB
    uint8_t r = (rgb >> 11) & 0x1F;
    uint8_t g = (rgb >> 5) & 0x3F;
    uint8_t b = rgb & 0x1F;
    
    // Calcola la luminosità (semplificata)
    uint8_t luminance = (r * 77 + g * 151 + b * 28) / 64; // Pesatura standard per la luminosità
    
    // Applica la soglia per decidere bianco o nero (128 è a metà tra 0 e 255)
    bool isBlack = (luminance < 128);
    
    // Calcola la posizione scalata
    int xPos = drawX + (int)(x * scaleX);
    
    // Disegna il pixel
    if (isBlack) {
      currentDisplay->drawPixel(xPos, yPos, GxEPD_BLACK);
    }
  }
}

bool WeatherIcons::begin() {
  if (initialized) {
    return true;
  }

  // Assicurati che la cartella icons esista nella cartella www
  if (!SD.exists("/www")) {
    if (!SD.mkdir("/www")) {
      Serial.println(F("[ERROR] Impossibile creare la directory /www"));
      return false;
    }
  }

  if (!SD.exists("/www/icons")) {
    if (!SD.mkdir("/www/icons")) {
      Serial.println(F("[ERROR] Impossibile creare la directory /www/icons"));
      return false;
    }
  }
  
  Serial.println(F("[INFO] Inizializzazione WeatherIcons completata"));
  initialized = true;
  return true;
}

String WeatherIcons::getIconPath(const String& iconCode) {
  return "/www/icons/" + iconCode + ".png";
}

bool WeatherIcons::iconExists(const String& iconCode) {
  return SD.exists(getIconPath(iconCode));
}

bool WeatherIcons::downloadIcon(const String& iconCode, uint32_t timeoutMs) {
  if (!initialized && !begin()) {
    Serial.println(F("[ERROR] WeatherIcons non inizializzato"));
    return false;
  }
  
  // Verifica che il codice dell'icona sia valido
  if (iconCode.length() == 0) {
    Serial.println(F("[ERROR] Codice icona non valido"));
    return false;
  }
  
  // Se l'icona esiste già, non è necessario scaricarla di nuovo
  if (iconExists(iconCode)) {
    Serial.print(F("[INFO] Icona già presente: "));
    Serial.print(iconCode);
    Serial.println(F("\""));
    return true;
  }
  
  // Verifica che il WiFi sia connesso
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("[ERROR] WiFi non connesso, impossibile scaricare l'icona"));
    return false;
  }
  
  HTTPClient http;
  String url = F("https://openweathermap.org/img/wn/");
  url += iconCode;
  url += F("@2x.png");
  
  Serial.print(F("[INFO] Download icona: "));
  Serial.println(url);
  
  // Configura il client HTTP
  http.setConnectTimeout(timeoutMs);
  http.setTimeout(timeoutMs);
  http.setReuse(false);
  
  if (!http.begin(url)) {
    Serial.println(F("[ERROR] Impossibile avviare la connessione HTTP"));
    return false;
  }
  
  // Esegui la richiesta GET
  int httpCode = http.GET();
  
  if (httpCode != HTTP_CODE_OK) {
    Serial.print(F("[ERROR] Errore HTTP nel download dell'icona: "));
    Serial.print(httpCode);
    Serial.print(F(" - "));
    Serial.println(http.errorToString(httpCode).c_str());
    http.end();
    return false;
  }
  
  // Ottieni la dimensione del file
  int contentLength = http.getSize();
  if (contentLength <= 0 || contentLength > MAX_ICON_SIZE) {
    Serial.print(F("[ERROR] Dimensione file non valida: "));
    Serial.println(contentLength);
    http.end();
    return false;
  }
  
  // Alloca il buffer per i dati
  uint8_t* buffer = (uint8_t*)malloc(contentLength);
  if (!buffer) {
    Serial.println(F("[ERROR] Memoria insufficiente per il buffer di download"));
    http.end();
    return false;
  }
  
  // Scarica i dati nel buffer
  WiFiClient* stream = http.getStreamPtr();
  int bytesRead = 0;
  uint32_t startTime = millis();
  
  while (http.connected() && (bytesRead < contentLength)) {
    // Verifica il timeout
    if (millis() - startTime > timeoutMs) {
      Serial.println(F("[ERROR] Timeout durante il download dell'icona"));
      free(buffer);
      http.end();
      return false;
    }
    
    // Leggi i dati disponibili
    size_t size = stream->available();
    if (size > 0) {
      // Leggi al massimo la quantità di dati mancanti
      size_t toRead = min((size_t)(contentLength - bytesRead), size);
      size_t c = stream->readBytes(buffer + bytesRead, toRead);
      
      if (c <= 0) {
        // Errore di lettura
        break;
      }
      
      bytesRead += c;
    }
    delay(1); // Piccola pausa per evitare WDT reset
  }
  
  // Verifica se il download è stato completato correttamente
  bool success = (bytesRead == contentLength);
  
  if (success) {
    // Salva l'icona sulla SD card
    String iconPath = getIconPath(iconCode);
    File iconFile = SD.open(iconPath, FILE_WRITE);
    
    if (!iconFile) {
      Serial.print(F("[ERROR] Impossibile aprire il file per la scrittura: "));
      Serial.println(iconPath);
      success = false;
    } else {
      // Scrivi i dati sul file
      size_t bytesWritten = iconFile.write(buffer, bytesRead);
      iconFile.close();
      
      if (bytesWritten != (size_t)bytesRead) {
        Serial.print(F("[ERROR] Errore durante la scrittura del file: "));
        Serial.println(iconPath);
        success = false;
        
        // Elimina il file parzialmente scritto
        SD.remove(iconPath);
      } else {
        Serial.print(F("[INFO] Icona scaricata e salvata: "));
        Serial.print(iconCode);
        Serial.print(F(" ("));
        Serial.print(bytesRead);
        Serial.println(F(" byte)"));
      }
    }
  } else {
    Serial.print(F("[ERROR] Download incompleto: "));
    Serial.print(bytesRead);
    Serial.print(F("/"));
    Serial.print(contentLength);
    Serial.println(F(" byte ricevuti"));
  }
  
  // Libera la memoria e chiudi la connessione
  free(buffer);
  http.end();
  
  return success;
}

bool WeatherIcons::prepareIcon(const String& iconCode) {
  // Se l'icona non esiste, provare a scaricarla
  if (!iconExists(iconCode)) {
    return downloadIcon(iconCode);
  }
  return true;
}

bool WeatherIcons::drawWeatherIcon(GxEPD_Class& display, const String& iconCode, int x, int y, int size) {
  if (!initialized && !begin()) {
    Serial.println(F("[ERROR] WeatherIcons non inizializzato"));
    return false;
  }
  
  // Verifica i parametri di input
  if (size <= 0) {
    Serial.println(F("[ERROR] Dimensione icona non valida"));
    return false;
  }
  
  // Assicurati che l'icona esista
  if (!prepareIcon(iconCode)) {
    Serial.print(F("[ERROR] Icona non disponibile: \""));
    Serial.print(iconCode);
    Serial.println(F("\""));
    return false;
  }
  
  // Ottieni il percorso completo del file
  String iconPath = getIconPath(iconCode);
  
  // Apri il file dell'icona
  File iconFile = SD.open(iconPath, FILE_READ);
  if (!iconFile) {
    Serial.print(F("[ERROR] Impossibile aprire il file: \""));
    Serial.print(iconPath);
    Serial.println(F("\""));
    return false;
  }
  
  // Inizializza il decoder PNG
  PNG png;
  
  // Configura le funzioni di callback per il file system
  int16_t rc = png.openFILE(
    iconPath.c_str(),
    [](const char* filename) -> void* { return (void*)SD.open(filename, FILE_READ).filePtr(); },
    [](void* handle) { if (handle) ((File*)handle)->close(); },
    [](void* handle, uint8_t* buffer, int32_t length) { return ((File*)handle)->read(buffer, length); },
    [](void* handle, int32_t position) { return ((File*)handle)->seek(position); },
    [](PNGDRAW* pDraw) {
      if (!currentDisplay) return;
      
      uint16_t lineBuffer[pDraw->iWidth];
      
      // Ottieni i dati della linea corrente
      PNG.getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_BIG_ENDIAN, 0xffffffff);
      
      // Scala e disegna ogni pixel sull'e-ink display
      float scaleX = (float)drawSize / pDraw->iWidth;
      int yPos = drawY + (int)(pDraw->y * scaleX);
      
      // Converti RGB565 in bianco e nero usando il dithering
      for (int x = 0; x < pDraw->iWidth; x++) {
        uint16_t rgb = lineBuffer[x];
        
        // Estrai i valori RGB
        uint8_t r = (rgb >> 11) & 0x1F;
        uint8_t g = (rgb >> 5) & 0x3F;
        uint8_t b = rgb & 0x1F;
        
        // Calcola la luminosità (semplificata)
        uint8_t luminance = (r * 77 + g * 151 + b * 28) / 64; // Pesatura standard per la luminosità
        
        // Applica la soglia per decidere bianco o nero (128 è a metà tra 0 e 255)
        bool isBlack = (luminance < 128);
        
        // Calcola la posizione scalata
        int xPos = drawX + (int)(x * scaleX);
        
        // Disegna il pixel
        if (isBlack) {
          currentDisplay->drawPixel(xPos, yPos, GxEPD_BLACK);
        }
      }
    }
  );
  
  if (rc != PNG_SUCCESS) {
    Serial.print(F("[ERROR] Errore nell'apertura del file PNG: "));
    Serial.println(rc);
    iconFile.close();
    return false;
  }
  
  // Ottieni le informazioni sull'immagine PNG
  rc = png.getInfo();
  
  Serial.printf("Dimensioni PNG: %d x %d, %d bpp\n", png.getWidth(), png.getHeight(), png.getBpp());
  
  // Configura i parametri di disegno
  currentDisplay = &display;
  drawX = x;
  drawY = y;
  drawSize = size;
  
  // Alloca il buffer per i dati dell'immagine
  pngBuffer = (uint8_t*)malloc(png.getWidth() * 2); // 2 byte per pixel RGB565
  
  if (!pngBuffer) {
    Serial.println("Memoria insufficiente per decodificare l'icona");
    png.close();
    iconFile.close();
    return false;
  }
  
  // Decodifica l'immagine PNG
  rc = png.decode(pngBuffer, 0);
  
  if (rc != PNG_SUCCESS) {
    Serial.printf("Errore nella decodifica del PNG: %d\n", rc);
    free(pngBuffer);
    pngBuffer = nullptr;
    currentDisplay = nullptr;
    png.close();
    iconFile.close();
    return false;
  }
  
  // Pulizia
  free(pngBuffer);
  pngBuffer = nullptr;
  currentDisplay = nullptr;
  png.close();
  iconFile.close();
  
  Serial.print("Disegnata icona: ");
  Serial.println(iconCode);
  
  return true;
}



// Implementazione della funzione di conversione in bianco e nero con dithering Floyd-Steinberg
void WeatherIcons::convertToBlackAndWhite(uint8_t* buffer, int width, int height) {
  if (!buffer || width <= 0 || height <= 0) return;
  
  // Crea un buffer temporaneo per l'immagine in scala di grigi
  uint8_t* grayBuffer = (uint8_t*)malloc(width * height);
  if (!grayBuffer) return;
  
  // Converti in scala di grigi
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      int idx = (y * width + x) * 3; // Buffer originale in formato RGB
      uint8_t r = buffer[idx];
      uint8_t g = buffer[idx + 1];
      uint8_t b = buffer[idx + 2];
      
      // Formula standard per la luminosità
      grayBuffer[y * width + x] = (r * 77 + g * 151 + b * 28) >> 8;
    }
  }
  
  // Applica dithering Floyd-Steinberg
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      int idx = y * width + x;
      uint8_t oldPixel = grayBuffer[idx];
      uint8_t newPixel = (oldPixel < 128) ? 0 : 255;
      
      // Assegna il nuovo valore al buffer originale (solo R, G e B tutti uguali per B/N)
      int origIdx = idx * 3;
      buffer[origIdx] = buffer[origIdx + 1] = buffer[origIdx + 2] = newPixel;
      
      // Calcola l'errore
      int error = oldPixel - newPixel;
      
      // Distribuisci l'errore ai pixel vicini
      if (x < width - 1)
        grayBuffer[idx + 1] += error * 7 / 16;
      
      if (y < height - 1) {
        if (x > 0)
          grayBuffer[idx + width - 1] += error * 3 / 16;
          
        grayBuffer[idx + width] += error * 5 / 16;
        
        if (x < width - 1)
          grayBuffer[idx + width + 1] += error * 1 / 16;
      }
    }
  }
  
  free(grayBuffer);
}
