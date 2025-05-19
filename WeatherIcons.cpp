#include "WeatherIcons.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <SD.h>
#include <SPI.h>
#include <ArduinoJson.h>
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <GxEPD2_7C.h>
#include <GxEPD2_750_T7.h>
#include <PNGdec.h>

// Inizializzazione dei membri statici
bool WeatherIcons::initialized = false;

// Buffer per i dati delle icone decodificate
static uint8_t* pngBuffer = nullptr;
static void* currentDisplay = nullptr;
static int drawX = 0, drawY = 0, drawSize = 0;

// Timeout predefinito per le operazioni di rete (in ms)
static const uint32_t DEFAULT_NETWORK_TIMEOUT = 10000;

// Dimensione massima per il buffer di download (100KB)
static const size_t DOWNLOAD_BUFFER_SIZE = 102400;

// Struttura per passare i dati durante la decodifica PNG
template<typename DisplayType>
struct PNGDrawData {
  DisplayType* display;
  int x;
  int y;
  int size;
  PNG* pngPtr;
  int height;
};

// Template function for PNG draw callback
template<typename DisplayType>
void pngDrawCallback(PNGDRAW *pDraw) {
  if (!pDraw || !pDraw->pUser) return;
  
  auto* data = static_cast<PNGDrawData<DisplayType>*>(pDraw->pUser);
  if (!data || !data->display) return;
  
  // Buffer per i dati della riga corrente
  // Usa la larghezza massima supportata dal display per evitare overflow
  static const int MAX_DISPLAY_WIDTH = 800; // Larghezza massima supportata
  static uint16_t lineBuffer[MAX_DISPLAY_WIDTH];
  
  // Assicurati di non superare la larghezza massima
  if (pDraw->iWidth > MAX_DISPLAY_WIDTH) {
    return; // Evita overflow del buffer
  }
  
  // Leggi la riga corrente in formato RGB565
  PNG* png = static_cast<PNG*>(data->pngPtr);
  if (!png) return;
  
  png->getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_BIG_ENDIAN, 0xffffffff);
  
  // Calcola la posizione Y nel display
  int yPos = data->y + pDraw->y;
  
  // Disegna ogni pixel della riga
  for (int x = 0; x < pDraw->iWidth; x++) {
    // Estrai i componenti di colore
    uint16_t rgb = lineBuffer[x];
    uint8_t r = (rgb >> 11) & 0x1F;
    uint8_t g = (rgb >> 5) & 0x3F;
    uint8_t b = rgb & 0x1F;
    
    // Calcola la luminosità (formula standard per la conversione RGB in scala di grigi)
    uint8_t gray = (r * 77 + g * 151 + b * 28) >> 8;
    
    // Calcola la posizione X nel display
    int xPos = data->x + x;
    
    // Disegna il pixel in base alla soglia
    data->display->drawPixel(xPos, yPos, (gray < 128) ? GxEPD_BLACK : GxEPD_WHITE);
  }
  
  // Non aggiorniamo più qui per evitare flickering
  // L'aggiornamento verrà fatto una sola volta dopo che tutta l'immagine è stata disegnata
}

// Explicit instantiation for common display types
template void pngDrawCallback<GxEPD2_BW<GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT>>(PNGDRAW *pDraw);
template void pngDrawCallback<GxEPD2_BW<GxEPD2_583_T8, 120>>(PNGDRAW *pDraw);

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

template<typename DisplayType>
bool WeatherIcons::drawWeatherIcon(DisplayType& display, const String& iconCode, int x, int y, int size) {
  if (!initialized && !begin()) {
    Serial.println(F("[ERROR] WeatherIcons non inizializzato"));
    return false;
  }

  // Verifica che il codice dell'icona non sia vuoto
  if (iconCode.length() == 0) {
    Serial.println(F("[ERROR] Codice icona non valido"));
    return false;
  }

  // Prepara l'icona (scarica se necessario)
  if (!prepareIcon(iconCode)) {
    Serial.print(F("[ERROR] Impossibile preparare l'icona: "));
    Serial.println(iconCode);
    return false;
  }

  // Apri il file dell'icona
  String iconPath = getIconPath(iconCode);
  File iconFile = SD.open(iconPath.c_str(), FILE_READ);
  if (!iconFile) {
    Serial.print(F("[ERROR] Impossibile aprire il file dell'icona: "));
    Serial.println(iconPath);
    return false;
  }

  // Alloca il buffer per i dati PNG
  if (pngBuffer) {
    free(pngBuffer);
    pngBuffer = nullptr;
  }
  
  // Verifica le dimensioni dell'icona
  if (size <= 0) {
    Serial.println(F("[ERROR] Dimensione non valida"));
    return false;
  }

  // Usa una dimensione fissa per il buffer PNG
  const size_t PNG_BUFFER_SIZE = 1024;
  pngBuffer = (uint8_t*)malloc(PNG_BUFFER_SIZE);
  if (!pngBuffer) {
    Serial.println(F("[ERROR] Memoria insufficiente per il buffer PNG"));
    iconFile.close();
    return false;
  }

  // Inizializza il decoder PNG
  PNG png; // Made non-static
  
  // Prepare file reading functions for PNG decoder
  // These are the standard functions needed by the PNGdec library
  static auto openPNG = [](const char* filename, int32_t* size) -> void* {
    File* f = new File(SD.open(filename, FILE_READ));
    if (*f) {
      *size = f->size();
      return f;
    }
    delete f;
    return nullptr;
  };
  
  static auto closePNG = [](void* handle) {
    File* f = static_cast<File*>(handle);
    if (f) {
      f->close();
      delete f;
    }
  };
  
  static auto readPNG = [](PNGFILE* file, uint8_t* buffer, int32_t length) -> int32_t {
    File* f = static_cast<File*>(file->fHandle);
    return f->read(buffer, length);
  };
  
  static auto seekPNG = [](PNGFILE* file, int32_t position) -> int32_t {
    File* f = static_cast<File*>(file->fHandle);
    return f->seek(position) ? 0 : -1;
  };
  
  // Prepara la struttura dati per il callback
  PNGDrawData<DisplayType> drawData; // Made non-static
  drawData.display = &display;
  drawData.x = x;
  drawData.y = y;
  drawData.size = size;
  drawData.pngPtr = &png;
  drawData.height = png.getHeight(); // Imposta l'altezza corretta per il callback
  
  // Open PNG file using the proper API
  int16_t pngReturn = png.open(iconPath.c_str(), openPNG, closePNG, readPNG, seekPNG, pngDrawCallback<DisplayType>);
  
  if (pngReturn != PNG_SUCCESS) {
    Serial.print(F("[ERROR] Errore nell'apertura del PNG: "));
    Serial.println(pngReturn);
    free(pngBuffer);
    pngBuffer = nullptr;
    iconFile.close();
    return false;
  }
  
  // In PNGdec library, the user data is passed via the callback function
  // when registering it with the PNG object in the open call
  
  // Create a global static pointer that can be accessed by the callback
  static void* g_pUserData = nullptr;
  g_pUserData = &drawData;

  // Set partial window for the icon area
  display.setPartialWindow(x, y, png.getWidth(), png.getHeight());
  
  // Decodifica e disegna l'immagine per ogni pagina
  display.firstPage();
  do {
    // Nota: La callback pngDrawCallback disegnerà solo sulla pagina corrente.
    // Il decoder PNGDEc potrebbe dover essere resettato o riaperto per ogni pagina
    // se non gestisce nativamente il rendering a pezzi. Testare attentamente.
    // Per ora, assumiamo che chiamare decode() ripetutamente funzioni correttamente
    // per il disegno a pagine, dato che la callback disegna solo pixel visibili sulla pagina.
    pngReturn = png.decode(nullptr, 0);
    if (pngReturn != PNG_SUCCESS) { // PNG_FEW_PIXELS might be ok if area is small
      // If decode fails mid-way through pages, it's problematic.
      // Consider how to handle. For now, log and break.
      Serial.print(F("[ERROR] Errore nella decodifica del PNG per una pagina: "));
      Serial.println(pngReturn);
      // No need to free buffers here, will be done after loop
      png.close();
      iconFile.close();
      if (pngBuffer) { free(pngBuffer); pngBuffer = nullptr; }
      return false; // Abort paged drawing if a page fails
    }
  } while (display.nextPage());

  // Check final status after loop if needed, though errors are caught inside
  if (pngReturn != PNG_SUCCESS) {
    Serial.print(F("[ERROR] Errore nella decodifica del PNG: "));
    Serial.println(pngReturn);
    free(pngBuffer);
    pngBuffer = nullptr;
    png.close();
    iconFile.close();
    return false;
  }
  
  // Pulizia
  free(pngBuffer);
  pngBuffer = nullptr;
  png.close();
  iconFile.close();
  
  Serial.println(F("[SUCCESS] Icona disegnata con successo"));
  return true;
}

// Explicit template instantiation
template bool WeatherIcons::drawWeatherIcon<GxEPD2_BW<GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT>>(
  GxEPD2_BW<GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT>& display, 
  const String& iconCode, 
  int x, 
  int y, 
  int size
);

// Explicit template instantiation for the GxEPD2_583_T8 display
template bool WeatherIcons::drawWeatherIcon<GxEPD2_BW<GxEPD2_583_T8, 120>>(
  GxEPD2_BW<GxEPD2_583_T8, 120>& display, 
  const String& iconCode, 
  int x, 
  int y, 
  int size
);

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
