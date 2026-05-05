#include "WeatherIcons.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <SD.h>
#include <SPI.h>
#include <ArduinoJson.h>
#include "Hardware.h"
#include "BMPHelper.h" // Inclusione necessaria per gestire i BMP

// Inizializzazione dei membri statici
bool WeatherIcons::initialized = false;

// Timeout predefinito per le operazioni di rete (in ms)
static const uint32_t DEFAULT_NETWORK_TIMEOUT = 10000;

// Dimensione massima per il buffer di download (100KB)
static const size_t DOWNLOAD_BUFFER_SIZE = 102400;

bool WeatherIcons::begin() {
  if (initialized) {
    return true;
  }

  // Verifica se la SD è accessibile (usando initSD centralizzato)
  if (!initSD()) {
    // Serial.println(F("[ERROR] SD Card non inizializzata in WeatherIcons"));
    return false;
  }

  // Verifica che la cartella icons esista
  if (!SD.exists("/icons")) {
     // Serial.println(F("[WARN] Cartella /icons non trovata sulla SD. Le icone non verranno caricate."));
     // Non ritorniamo false qui, permettiamo il fallback geometrico
  }
  
  // Serial.println(F("[INFO] Inizializzazione WeatherIcons completata"));
  initialized = true;
  return true;
}

String WeatherIcons::getIconNameFromCode(const String& iconCode, int weatherId) {
  // Determina se è notte o giorno dal codice icona
  bool isNight = iconCode.indexOf("n") >= 0;
  
  // Se abbiamo l'ID meteo preciso, usiamolo per scegliere l'icona specifica
  if (weatherId != -1) {
    // --- GRUPPO 2xx: Temporale (Thunderstorm) ---
    if (weatherId >= 200 && weatherId < 300) {
      if (weatherId >= 210 && weatherId <= 221) return isNight ? "wi-night-alt-thunderstorm" : "wi-day-thunderstorm"; // Temporale forte
      if (weatherId <= 202 || weatherId >= 230) return isNight ? "wi-night-alt-storm-showers" : "wi-day-storm-showers"; // Temporale con pioggia
      return "wi-thunderstorm"; // Generico
    }
    
    // --- GRUPPO 3xx: Pioggerella (Drizzle) ---
    if (weatherId >= 300 && weatherId < 400) {
      if (weatherId == 300 || weatherId == 301) return isNight ? "wi-night-alt-sprinkle" : "wi-day-sprinkle"; // Pioggerella leggera
      return "wi-sprinkle"; // Pioggerella generica
    }
    
    // --- GRUPPO 5xx: Pioggia (Rain) ---
    if (weatherId >= 500 && weatherId < 600) {
      if (weatherId == 500) return isNight ? "wi-night-alt-rain" : "wi-day-rain"; // Pioggia leggera
      if (weatherId == 501) return isNight ? "wi-night-alt-rain" : "wi-day-rain"; // Pioggia moderata
      if (weatherId == 502 || weatherId == 503 || weatherId == 504) return "wi-rain"; // Pioggia forte (neutra per impatto) o wi-rain-wind
      if (weatherId == 511) return isNight ? "wi-night-alt-sleet" : "wi-day-sleet"; // Freezing rain
      if (weatherId >= 520 && weatherId <= 531) return isNight ? "wi-night-alt-showers" : "wi-day-showers"; // Acquazzoni
      return "wi-rain";
    }
    
    // --- GRUPPO 6xx: Neve (Snow) ---
    if (weatherId >= 600 && weatherId < 700) {
      if (weatherId == 600 || weatherId == 601) return isNight ? "wi-night-alt-snow" : "wi-day-snow"; // Neve leggera
      if (weatherId == 602) return "wi-snowflake-cold"; // Neve forte
      if (weatherId >= 611 && weatherId <= 613) return isNight ? "wi-night-alt-sleet" : "wi-day-sleet"; // Nevischio
      if (weatherId == 615 || weatherId == 616) return isNight ? "wi-night-alt-rain-mix" : "wi-day-rain-mix"; // Pioggia e neve
      if (weatherId == 620 || weatherId == 621 || weatherId == 622) return isNight ? "wi-night-alt-snow-wind" : "wi-day-snow-wind"; // Neve e vento
      return "wi-snow";
    }
    
    // --- GRUPPO 7xx: Atmosfera (Atmosphere) ---
    if (weatherId >= 700 && weatherId < 800) {
      if (weatherId == 701 || weatherId == 741) return isNight ? "wi-night-fog" : "wi-day-fog"; // Nebbia
      if (weatherId == 711) return "wi-smoke"; // Fumo
      if (weatherId == 721) return isNight ? "wi-night-fog" : "wi-day-haze"; // Foschia (Haze di giorno)
      if (weatherId == 731 || weatherId == 751 || weatherId == 761) return "wi-dust"; // Polvere/Sabbia
      if (weatherId == 771) return "wi-strong-wind"; // Squalls
      if (weatherId == 781) return "wi-tornado"; // Tornado
      return "wi-fog";
    }
    
    // --- GRUPPO 800: Sereno (Clear) ---
    if (weatherId == 800) {
      return isNight ? "wi-night-clear" : "wi-day-sunny";
    }
    
    // --- GRUPPO 80x: Nuvole (Clouds) ---
    if (weatherId > 800) {
      if (weatherId == 801) return isNight ? "wi-night-alt-partly-cloudy" : "wi-day-sunny-overcast"; // Poche nuvole (11-25%)
      if (weatherId == 802) return isNight ? "wi-night-alt-cloudy" : "wi-day-cloudy"; // Nubi sparse (25-50%)
      if (weatherId == 803) return isNight ? "wi-night-alt-cloudy-high" : "wi-day-cloudy-high"; // Nuvoloso (51-84%)
      if (weatherId == 804) return "wi-cloudy"; // Coperto (85-100%)
    }
  }
  
  // --- FALLBACK: Usa solo il codice icona stringa (meno preciso) ---
  
  if (iconCode == "01d") return "wi-day-sunny";
  if (iconCode == "01n") return "wi-night-clear";
  
  if (iconCode == "02d") return "wi-day-cloudy";
  if (iconCode == "02n") return "wi-night-alt-cloudy";
  
  if (iconCode == "03d" || iconCode == "03n") return "wi-cloud";
  if (iconCode == "04d" || iconCode == "04n") return "wi-cloudy";
  
  if (iconCode == "09d" || iconCode == "09n") return "wi-showers";
  
  if (iconCode == "10d") return "wi-day-rain";
  if (iconCode == "10n") return "wi-night-alt-rain";
  
  if (iconCode == "11d" || iconCode == "11n") return "wi-thunderstorm";
  
  if (iconCode == "13d" || iconCode == "13n") return "wi-snow";
  
  if (iconCode == "50d" || iconCode == "50n") return "wi-fog";
  
  // Fallback generici finali
  if (iconCode.endsWith("d")) return "wi-day-sunny";
  return "wi-night-clear";
}

String WeatherIcons::getIconPath(const String& iconCode, int weatherId) {
  // Ottieni il nome del file mappato usando anche l'ID meteo se disponibile
  String filename = getIconNameFromCode(iconCode, weatherId);
  // Percorso: /icons/wi-day-sunny.bmp
  return "/icons/" + filename + ".bmp";
}

bool WeatherIcons::iconExists(const String& iconCode) {
  if (!initSD()) return false;
  return SD.exists(getIconPath(iconCode)); // Nota: verifica solo il path di default senza ID
}

// Funzione deprecata ma mantenuta per compatibilità interfaccia
bool WeatherIcons::downloadIcon(const String& iconCode, uint32_t timeoutMs) {
    Serial.println(F("[WARN] Download icone disabilitato. Usare icone BMP su SD in /icons/"));
    return false;
}

bool WeatherIcons::prepareIcon(const String& iconCode) {
  // Verifica solo esistenza
  return iconExists(iconCode);
}

bool WeatherIcons::drawWeatherIcon(DisplayType& display, const String& iconCode, int x, int y, int size, int weatherId) {
  if (!initialized) begin();
  
  // Verifica i parametri di input
  if (size <= 0) {
    Serial.println(F("[ERROR] Dimensione icona non valida"));
    return false;
  }

  // 1. Tenta di caricare l'icona BMP dalla SD (usando weatherId per precisione)
  String path = getIconPath(iconCode, weatherId);
  
  // Se il file specifico non esiste, prova il fallback generico (solo codice)
  if (!SD.exists(path)) {
      // Serial.print(F("[WARN] Icona specifica non trovata: "));
      // Serial.print(path);
      // Serial.println(F(", provo fallback generico"));
      path = getIconPath(iconCode, -1); // Fallback senza ID
  }

  if (SD.exists(path)) {
      // Usa BMPHelper per disegnare
      if (BMPHelper::drawBMP(display, path.c_str(), x, y, size, size)) {
          // Successo!
          return true;
      } else {
          // Serial.print(F("[ERROR] Fallito disegno BMP: "));
          // Serial.println(path);
      }
  } else {
      // Serial.print(F("[WARN] Icona non trovata: "));
      // Serial.println(path);
  }

  // 2. Fallback: Disegno geometrico (Nuvola/Sole)
  // Nuvola: ellisse + rettangolo arrotondato
  int w = size;
  int h = size * 3 / 5;
  int cx = x + w / 2;
  int cy = y + h / 2;

  // Corpo nuvola (approssimazione con cerchi)
  display.fillCircle(cx - w/4, cy, h/3, GxEPD_BLACK);
  display.fillCircle(cx,        cy - h/6, h/2, GxEPD_BLACK);
  display.fillCircle(cx + w/4,  cy, h/3, GxEPD_BLACK);
  display.fillRect(x + w/6, cy, 2*w/3, h/3, GxEPD_BLACK);

  // Se il codice icona contiene 'd' (giorno), disegna un sole in alto a sinistra
  if (iconCode.indexOf('d') >= 0) {
    int rs = max(4, size / 8);
    int sx = x + rs + 2;
    int sy = y + rs + 2;
    // Sole cavo per non coprire la nuvola completamente
    display.drawCircle(sx, sy, rs, GxEPD_BLACK);
    // Raggi semplici
    display.drawLine(sx - rs - 3, sy, sx - rs + 2, sy, GxEPD_BLACK);
    display.drawLine(sx + rs - 2, sy, sx + rs + 3, sy, GxEPD_BLACK);
    display.drawLine(sx, sy - rs - 3, sx, sy - rs + 2, GxEPD_BLACK);
    display.drawLine(sx, sy + rs - 2, sx, sy + rs + 3, GxEPD_BLACK);
  }

  // Serial.print("[INFO] Disegnata icona FALLBACK per codice: ");
  // Serial.println(iconCode);
  return true;
}

// Funzioni helper non più usate ma mantenute per compilazione
void WeatherIcons::convertToBlackAndWhite(uint8_t* buffer, int width, int height) {
  // No-op
}
