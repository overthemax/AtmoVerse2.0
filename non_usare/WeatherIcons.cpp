#include "WeatherIcons.h"
#include "DebugUtils.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <SD.h>
#include <SPI.h>
#include <ArduinoJson.h>
#include <GxEPD2_BW.h>
#include "Hardware.h" // Per PanelType e dichiarazione display (se necessario)
#include <PNGdec.h>

// Disabilita per default le istanziazioni e callback legate alle icone OWM per risparmiare flash
#ifndef ENABLE_OWM_ICONS
#define ENABLE_OWM_ICONS 0
#endif

// Inizializzazione dei membri statici
bool WeatherIcons::initialized = false;
static void* currentDisplay = nullptr;
static int drawX = 0, drawY = 0, drawSize = 0;
static PNG g_png; // Decoder PNG globale per uso nel callback

// Timeout predefinito per le operazioni di rete (in ms)
static const uint32_t DEFAULT_NETWORK_TIMEOUT = 10000;

// Dimensione chunk per il download streaming su SD (1KB)
static const size_t DOWNLOAD_CHUNK_SIZE = 1024;

// Template function for PNG draw callback (usa variabili globali)
template<typename DisplayType>
void pngDrawCallback(PNGDRAW *pDraw) {
  if (!pDraw || !currentDisplay) return;
  auto* display = static_cast<DisplayType*>(currentDisplay);
  
  // Buffer per i dati della riga corrente
  // Usa la larghezza effettiva del display 5.83" (600px)
  static const int DISPLAY_WIDTH = 600; // Dimensione effettiva del display GxEPD2_583_T8
  
#if defined(BOARD_HAS_PSRAM) && defined(CONFIG_ESP32_SPIRAM_SUPPORT)
  // Utilizzo PSRAM se disponibile
  static uint16_t* lineBuffer = nullptr;
  if (lineBuffer == nullptr) {
    lineBuffer = (uint16_t*)ps_malloc(DISPLAY_WIDTH * sizeof(uint16_t));
    if (lineBuffer == nullptr) {
      // Fallback a memoria heap normale se PSRAM non disponibile
      lineBuffer = (uint16_t*)malloc(DISPLAY_WIDTH * sizeof(uint16_t));
    }
  }
#else
  // Memoria heap normale
  static uint16_t lineBuffer[DISPLAY_WIDTH];
#endif
  
  // Assicurati di non superare la larghezza massima
  if (pDraw->iWidth > DISPLAY_WIDTH) {
    return; // Evita overflow del buffer
  }
  
  // Leggi la riga corrente in formato RGB565 tramite il decoder globale
  g_png.getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_BIG_ENDIAN, 0xffffffff);
  
  // Calcola la posizione Y nel display
  int yPos = drawY + pDraw->y;
  
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
    int xPos = drawX + x;
    
    // Disegna il pixel in base alla soglia
    display->drawPixel(xPos, yPos, (gray < 128) ? GxEPD_BLACK : GxEPD_WHITE);
  }
  
  // Non aggiorniamo più qui per evitare flickering
  // L'aggiornamento verrà fatto una sola volta dopo che tutta l'immagine è stata disegnata
}

// Dichiarazione esplicita del tipo di callback richiesto dalla libreria PNG
using PngDrawCallbackFn = int (*)(PNGDRAW*);

// Wrapper per convertire il nostro template void in int(*)(PNGDRAW*)
template<typename DisplayType>
inline int pngDrawCallbackWrapper(PNGDRAW* pDraw) {
  pngDrawCallback<DisplayType>(pDraw);
  return 1; // Ritorna sempre successo
}

#if ENABLE_OWM_ICONS
// Istanziazioni esplicite per i tipi di display utilizzati
// 1) Basate su PanelType (storiche)
typedef GxEPD2_BW<PanelType, GxEPD2_583_T8::HEIGHT> DisplayType;
template int pngDrawCallbackWrapper<DisplayType>(PNGDRAW*);
template void pngDrawCallback<DisplayType>(PNGDRAW*);

// 2) Specifiche per GxEPD2_583_T8 con paginazione 32 (usato in Display.cpp)
template int pngDrawCallbackWrapper<GxEPD2_BW<GxEPD2_583_T8, 32>>(PNGDRAW*);
template void pngDrawCallback<GxEPD2_BW<GxEPD2_583_T8, 32>>(PNGDRAW*);
#endif

bool WeatherIcons::begin() {
  if (initialized) {
    return true;
  }

  // Assicurati che la cartella icons esista nella cartella www
  if (!SD.exists("/www")) {
    if (!SD.mkdir("/www")) {
      DEBUG_PRINTLN(F("[ERROR] Impossibile creare la directory /www"));
      return false;
    }
  }

  if (!SD.exists("/www/icons")) {
    if (!SD.mkdir("/www/icons")) {
      DEBUG_PRINTLN(F("[ERROR] Impossibile creare la directory /www/icons"));
      return false;
    }
  }
  
  DEBUG_PRINTLN(F("[INFO] Inizializzazione WeatherIcons completata"));
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
    return false;
  }
  
  // Verifica che il codice dell'icona sia valido
  if (iconCode.length() == 0) {
    return false;
  }
  
  // Se l'icona esiste già, non è necessario scaricarla di nuovo
  if (iconExists(iconCode)) {
    DEBUG_PRINT(F("[INFO] Icona già presente: "));
    DEBUG_PRINT(iconCode);
    DEBUG_PRINTLN(F("\""));
    return true;
  }
  
  // Verifica che il WiFi sia connesso
  if (WiFi.status() != WL_CONNECTED) {
    DEBUG_PRINTLN(F("[ERROR] WiFi non connesso, impossibile scaricare l'icona"));
    return false;
  }
  
  HTTPClient http;
  String url = F("https://openweathermap.org/img/wn/");
  url += iconCode;
  url += F("@2x.png");
  
  DEBUG_PRINT(F("[INFO] Download icona: "));
  DEBUG_PRINTLN(url);
  
  // Configura il client HTTP
  http.setConnectTimeout(timeoutMs);
  http.setTimeout(timeoutMs);
  http.setReuse(false);
  
  if (!http.begin(url)) {
    DEBUG_PRINTLN(F("[ERROR] Impossibile avviare la connessione HTTP"));
    return false;
  }
  
  // Esegui la richiesta GET
  int httpCode = http.GET();
  
  if (httpCode != HTTP_CODE_OK) {
    DEBUG_PRINT(F("[ERROR] Errore HTTP nel download dell'icona: "));
    DEBUG_PRINT(httpCode);
    DEBUG_PRINT(F(" - "));
    DEBUG_PRINTLN(http.errorToString(httpCode).c_str());
    http.end();
    return false;
  }
  
  // Percorso destinazione su SD
  String iconPath = getIconPath(iconCode);
  String tmpPath = iconPath + ".tmp";

  // Apri file temporaneo su SD
  File out = SD.open(tmpPath, FILE_WRITE);
  if (!out) {
    DEBUG_PRINT(F("[ERROR] Impossibile creare file temporaneo: "));
    DEBUG_PRINTLN(tmpPath);
    http.end();
    return false;
  }

  // Stream dei dati direttamente su SD
  WiFiClient* stream = http.getStreamPtr();
  uint8_t chunk[DOWNLOAD_CHUNK_SIZE];
  uint32_t lastActivity = millis();
  int total = 0;
  int contentLength = http.getSize(); // potrebbe essere -1 se sconosciuto

  while (http.connected()) {
    size_t avail = stream->available();
    if (avail) {
      size_t toRead = avail > DOWNLOAD_CHUNK_SIZE ? DOWNLOAD_CHUNK_SIZE : avail;
      int c = stream->readBytes(chunk, toRead);
      if (c <= 0) break;
      out.write(chunk, c);
      total += c;
      lastActivity = millis();
    } else {
      // Timeout di inattività
      if (millis() - lastActivity > timeoutMs) {
        DEBUG_PRINTLN(F("[ERROR] Timeout download icona"));
        break;
      }
      delay(1);
    }
    // Se conosciamo la dimensione e l'abbiamo raggiunta, possiamo fermarci
    if (contentLength > 0 && total >= contentLength) break;
  }

  out.close();
  http.end();

  // Verifica esito
  if (total <= 0 || (contentLength > 0 && total != contentLength)) {
    SD.remove(tmpPath);
    DEBUG_PRINTLN(F("[ERROR] Download icona incompleto"));
    return false;
  }

  // Rinomina il file temporaneo a definitivo
  SD.remove(iconPath);
  if (!SD.rename(tmpPath, iconPath)) {
    DEBUG_PRINTLN(F("[ERROR] Rinominazione file icona fallita"));
    SD.remove(tmpPath);
    return false;
  }

  DEBUG_PRINT(F("[INFO] Icona salvata su SD: "));
  DEBUG_PRINT(iconPath);
  DEBUG_PRINT(F(" ("));
  DEBUG_PRINT(total);
  DEBUG_PRINTLN(F(" byte)"));
  return true;
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
    return false;
  }

  // Verifica che il codice dell'icona non sia vuoto
  if (iconCode.length() == 0) {
    return false;
  }

  // Prepara l'icona (scarica se necessario)
  if (!prepareIcon(iconCode)) {
    return false;
  }

  // Costruisci il percorso del file dell'icona
  String iconPath = getIconPath(iconCode);
  if (!SD.exists(iconPath)) {
    return false;
  }

  // Verifica le dimensioni dell'icona
  if (size <= 0) {
    return false;
  }

  // Nessun buffer globale richiesto: il decoder legge direttamente dal file su SD

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
  
  // Open PNG file using the proper API
  int16_t pngReturn = g_png.open(iconPath.c_str(), openPNG, closePNG, readPNG, seekPNG, pngDrawCallbackWrapper<DisplayType>);
  
  if (pngReturn != PNG_SUCCESS) {
    // Errore apertura PNG
    return false;
  }
  
  // Imposta i parametri globali usati dalla callback
  currentDisplay = static_cast<void*>(&display);
  drawX = x;
  drawY = y;

  // Set partial window for the icon area
  display.setPartialWindow(x, y, g_png.getWidth(), g_png.getHeight());
  
  // Decodifica e disegna l'immagine per ogni pagina
  display.firstPage();
  do {
    // Decodifica l'immagine PNG (la callback usa variabili globali)
    pngReturn = g_png.decode(nullptr, 0);
    if (pngReturn != PNG_SUCCESS) {
      // Se la decodifica fallisce, interrompiamo il ciclo
      DEBUG_PRINTF("[ERROR] Errore nella decodifica PNG: %d\n", pngReturn);
      break;
    }
  } while (display.nextPage());

  // Pulizia delle risorse
  g_png.close();
  currentDisplay = nullptr;
  
  // Verifica se la decodifica è andata a buon fine
  if (pngReturn != PNG_SUCCESS) {
    DEBUG_PRINTLN("[ERROR] Errore durante il rendering dell'immagine");
    return false;
  }
  
  return true;
}

  #if ENABLE_OWM_ICONS
  // Istanziazioni esplicite richieste dal linker
  template bool WeatherIcons::drawWeatherIcon<GxEPD2_BW<PanelType, 64>>(
    GxEPD2_BW<PanelType, 64>& display, 
    const String& iconCode, 
    int x, 
    int y, 
    int size
  );

  template bool WeatherIcons::drawWeatherIcon<GxEPD2_BW<GxEPD2_583_T8, 32>>(
    GxEPD2_BW<GxEPD2_583_T8, 32>& display,
    const String& iconCode,
    int x,
    int y,
    int size
  );
  #endif

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
