#include "BMPHelper.h"
#include "Hardware.h"
#include <SD.h>
#include <string.h>

bool BMPHelper::initialized = false;

bool BMPHelper::begin() {
  if (initialized) {
    return true;
  }
  
  // Verifica che la SD sia inizializzata
  if (!SD.begin(SD_CS, sdSPI, SD_SPI_FREQ)) {
    // Serial.println("[BMP] Errore: SD card non inizializzata");
    return false;
  }
  
  // Verifica che la cartella icons esista
  if (!SD.exists("/icons")) {
    // Serial.println("[BMP] ATTENZIONE: Cartella /icons non trovata sulla SD");
    // Serial.println("[BMP] Assicurarsi di aver copiato le icone BMP nella cartella /icons");
    return false;
  }
  
  // Serial.println("[BMP] Inizializzazione completata");
  initialized = true;
  return true;
}

const char* BMPHelper::getIconPath(WeatherIcon icon) {
  // Converte il percorso SVG in BMP (usa la cartella /icons/ e .bmp)
  switch(icon) {
    // GIORNO - Condizioni Base
    case ICON_DAY_SUNNY: return "/icons/wi-day-sunny.bmp";
    case ICON_DAY_CLOUDY: return "/icons/wi-day-cloudy.bmp";
    case ICON_DAY_FOG: return "/icons/wi-day-fog.bmp";
    case ICON_DAY_WINDY: return "/icons/wi-day-windy.bmp";
    
    // GIORNO - Pioggia
    case ICON_DAY_RAIN: return "/icons/wi-day-rain.bmp";
    case ICON_DAY_RAIN_WIND: return "/icons/wi-day-rain-wind.bmp";
    case ICON_DAY_SPRINKLE: return "/icons/wi-day-sprinkle.bmp";
    case ICON_DAY_SHOWERS: return "/icons/wi-day-showers.bmp";
    
    // GIORNO - Temporali
    case ICON_DAY_THUNDERSTORM: return "/icons/wi-day-thunderstorm.bmp";
    case ICON_DAY_STORM_SHOWERS: return "/icons/wi-day-storm-showers.bmp";
    case ICON_DAY_LIGHTNING: return "/icons/wi-day-lightning.bmp";
    
    // GIORNO - Neve
    case ICON_DAY_SNOW: return "/icons/wi-day-snow.bmp";
    case ICON_DAY_SNOW_WIND: return "/icons/wi-day-snow-wind.bmp";
    case ICON_DAY_SLEET: return "/icons/wi-day-sleet.bmp";
    case ICON_DAY_RAIN_MIX: return "/icons/wi-day-rain-mix.bmp";
    case ICON_DAY_HAIL: return "/icons/wi-day-hail.bmp";
    
    // NOTTE - Condizioni Base
    case ICON_NIGHT_CLEAR: return "/icons/wi-night-clear.bmp";
    case ICON_NIGHT_CLOUDY: return "/icons/wi-night-alt-cloudy.bmp";
    case ICON_NIGHT_FOG: return "/icons/wi-night-fog.bmp";
    
    // NOTTE - Pioggia
    case ICON_NIGHT_RAIN: return "/icons/wi-night-alt-rain.bmp";
    case ICON_NIGHT_RAIN_WIND: return "/icons/wi-night-alt-rain-wind.bmp";
    case ICON_NIGHT_SPRINKLE: return "/icons/wi-night-alt-sprinkle.bmp";
    case ICON_NIGHT_SHOWERS: return "/icons/wi-night-alt-showers.bmp";
    
    // NOTTE - Temporali
    case ICON_NIGHT_THUNDERSTORM: return "/icons/wi-night-alt-thunderstorm.bmp";
    case ICON_NIGHT_STORM_SHOWERS: return "/icons/wi-night-alt-storm-showers.bmp";
    
    // NOTTE - Neve
    case ICON_NIGHT_SNOW: return "/icons/wi-night-alt-snow.bmp";
    case ICON_NIGHT_SNOW_WIND: return "/icons/wi-night-alt-snow-wind.bmp";
    case ICON_NIGHT_SLEET: return "/icons/wi-night-alt-sleet.bmp";
    case ICON_NIGHT_RAIN_MIX: return "/icons/wi-night-alt-rain-mix.bmp";
    case ICON_NIGHT_HAIL: return "/icons/wi-night-alt-hail.bmp";
    
    // NEUTRO
    case ICON_CLOUDY: return "/icons/wi-cloudy.bmp";
    case ICON_RAIN: return "/icons/wi-rain.bmp";
    case ICON_SNOW: return "/icons/wi-snow.bmp";
    case ICON_THUNDERSTORM: return "/icons/wi-thunderstorm.bmp";
    case ICON_FOG: return "/icons/wi-fog.bmp";
    case ICON_WINDY: return "/icons/wi-windy.bmp";
    case ICON_TORNADO: return "/icons/wi-tornado.bmp";
    case ICON_HURRICANE: return "/icons/wi-hurricane.bmp";
    case ICON_HOT: return "/icons/wi-hot.bmp";
    case ICON_SNOWFLAKE_COLD: return "/icons/wi-snowflake-cold.bmp";
    
    // Default
    default: return "/icons/wi-day-sunny.bmp";
  }
}

bool BMPHelper::drawWeatherIcon(DisplayType& display, WeatherIcon icon, int x, int y, int size) {
  if (!initialized && !begin()) {
    return false;
  }
  
  const char* iconPath = getIconPath(icon);
  // Serial.print("[BMP] Caricamento icona: ");
  // Serial.println(iconPath);
  
  return drawBMP(display, iconPath, x, y, size, size);
}

bool BMPHelper::readBMPHeader(File& file, int& width, int& height, uint16_t& bitDepth) {
  // Leggi header BMP (54 bytes)
  uint8_t header[54];
  if (file.read(header, 54) != 54) {
    // Serial.println("[BMP] Errore lettura header");
    return false;
  }
  
  // Verifica firma BMP ("BM")
  if (header[0] != 'B' || header[1] != 'M') {
    // Serial.println("[BMP] File non è un BMP valido");
    return false;
  }
  
  // Estrai dimensioni (little-endian)
  width = *(int32_t*)&header[18];
  height = *(int32_t*)&header[22];
  bitDepth = *(uint16_t*)&header[28];
  
  // Serial.print("[BMP] Dimensioni: ");
  // Serial.print(width);
  // Serial.print("x");
  // Serial.print(height);
  // Serial.print(", bit depth: ");
  // Serial.println(bitDepth);
  
  // Supportiamo BMP 1-bit (monocromatico) e 8-bit (scala di grigi)
  if (bitDepth != 1 && bitDepth != 8) {
    // Serial.print("[BMP] Errore: supportati solo BMP 1-bit o 8-bit, trovato: ");
    // Serial.println(bitDepth);
    return false;
  }
  
  return true;
}

bool BMPHelper::drawMonochromeBMP(DisplayType& display, File& file, int x, int y, int width, int height, int maxWidth, int maxHeight, bool flipVertical) {
  // Rileggi dal file i campi principali dell'header necessari al rendering
  // bitDepth si trova a offset 28 (2 byte)
  file.seek(28);
  uint16_t bitDepth;
  file.read((uint8_t*)&bitDepth, 2);
  
  // Serial.print("[BMP] Formato: ");
  // Serial.print(bitDepth);
  // Serial.println("-bit");
  
  // Calcola scala se necessario (permette sia ingrandimento che riduzione)
  float scaleX = (float)maxWidth / width;
  float scaleY = (float)maxHeight / height;
  float scale = min(scaleX, scaleY);
  
  // Nota: Rimosso il clamp a scale >= 1.0 per permettere l'ingrandimento delle icone
  // Le icone possono ora essere scalate sia in alto che in basso
  
  int scaledWidth = (int)(width * scale);
  int scaledHeight = (int)(height * scale);
  
  // Serial.print("[BMP] Scala: ");
  // Serial.print(scale);
  // Serial.print(", dimensioni finali: ");
  // Serial.print(scaledWidth);
  // Serial.print("x");
  // Serial.println(scaledHeight);
  
  // Offset ai dati pixel: leggi il valore reale dal header BMP (offset 10, 4 byte)
  file.seek(10);
  uint32_t dataOffset = 54; // fallback
  file.read((uint8_t*)&dataOffset, 4);
  
  // Calcola rowSize basato sul bitDepth
  int rowSize;
  if (bitDepth == 1) {
    rowSize = ((width + 31) / 32) * 4; // 1-bit: allineato a 4 bytes
  } else { // 8-bit
    rowSize = ((width + 3) / 4) * 4; // 8-bit: width bytes allineati a 4
  }
  
  // Buffer per una riga
  uint8_t* rowBuffer = (uint8_t*)malloc(rowSize);
  if (!rowBuffer) {
    // Serial.println("[BMP] Errore allocazione memoria");
    return false;
  }
  
  // Posiziona al primo pixel (BMP è bottom-up)
  file.seek(dataOffset);
  
  // Disegna riga per riga
  for (int row = height - 1; row >= 0; row--) {
    // Leggi la riga
    file.read(rowBuffer, rowSize);
    
    // Disegna i pixel
    for (int col = 0; col < width; col++) {
      uint8_t grayValue = 0;
      
      if (bitDepth == 1) {
        // BMP 1-bit: estrai il bit
        int byteIndex = col / 8;
        int bitIndex = 7 - (col % 8);
        bool isBlack = (rowBuffer[byteIndex] & (1 << bitIndex)) == 0;
        grayValue = isBlack ? 0 : 255;
      } else {
        // BMP 8-bit: valore già in scala di grigi (0-255)
        grayValue = rowBuffer[col];
      }
      
      // Display e-ink 1-bit: converte grayscale in bianco/nero con threshold
      // BMP: 0 = nero, 255 = bianco
      // E-ink: GxEPD_BLACK = nero, GxEPD_WHITE = bianco
      // Threshold a 128 per decidere bianco o nero
      uint16_t einkColor = (grayValue < 128) ? GxEPD_BLACK : GxEPD_WHITE;
      
      // Calcola la riga di destinazione sul display.
      // Per default manteniamo l'orientamento originale; se flipVertical
      // è true, invertiamo verticalmente l'immagine.
      int destRow = flipVertical ? (height - 1 - row) : row;
      int px = x + (int)(col * scale);
      int py = y + (int)(destRow * scale);
      
      // Calcola la dimensione del pixel scalato (minimo 1 pixel)
      int pixelSize = max(1, (int)ceil(scale));
      
      if (pixelSize > 1) {
        // Disegna pixel ingrandito (per scale > 1.0)
        for (int dy = 0; dy < pixelSize; dy++) {
          for (int dx = 0; dx < pixelSize; dx++) {
            display.drawPixel(px + dx, py + dy, einkColor);
          }
        }
      } else {
        // Disegna pixel singolo (per scale <= 1.0)
        display.drawPixel(px, py, einkColor);
      }
    }
  }
  
  free(rowBuffer);
  return true;
}

bool BMPHelper::drawBMP(DisplayType& display, const char* filename, int x, int y, int maxWidth, int maxHeight) {
  // Verifica esistenza file
  if (!SD.exists(filename)) {
    // Serial.print("[BMP] File non trovato: ");
    // Serial.println(filename);
    return false;
  }
  
  // Apri file
  File bmpFile = SD.open(filename, FILE_READ);
  if (!bmpFile) {
    // Serial.print("[BMP] Impossibile aprire: ");
    // Serial.println(filename);
    return false;
  }
  
  // Leggi header
  int width, height;
  uint16_t bitDepth;
  
  if (!readBMPHeader(bmpFile, width, height, bitDepth)) {
    bmpFile.close();
    return false;
  }
  
  // Determina se applicare il flip verticale: solo per le icone nella cartella /icons/
  bool flipVertical = false;
  if (filename && strncmp(filename, "/icons/", 7) == 0) {
    flipVertical = true;
  }

  // Disegna BMP monocromatico
  bool success = drawMonochromeBMP(display, bmpFile, x, y, width, height, maxWidth, maxHeight, flipVertical);
  
  bmpFile.close();
  
  if (success) {
    // Serial.println("[BMP] ✓ Icona caricata con successo");
  } else {
    // Serial.println("[BMP] ✗ Errore nel disegno");
  }
  
  return success;
}
