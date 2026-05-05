#include "SDFileManager.h"
#include "Hardware.h"

// Inizializza la struttura dei file necessari sulla SD card
bool initSDFileStructure() {
  if (!SD.begin(SD_CS, sdSPI, SD_SPI_FREQ)) {
    Serial.println("Errore: Inizializzazione SD fallita");
    return false;
  }
  
  // Crea le cartelle principali se non esistono
  createDirectoryIfNotExists("/www");
  createDirectoryIfNotExists("/www/css");
  createDirectoryIfNotExists("/www/js");
  createDirectoryIfNotExists("/www/icons");
  
  // Crea file HTML principali se non esistono
  if (!fileExists("/www/index.html")) {
    writeFileToSD("/www/index.html", "<!DOCTYPE html><html lang='it'><head><meta charset='UTF-8'><title>AtmoVerse</title><link rel='stylesheet' href='css/style.css'></head><body><div id='main-content'></div><script src='js/main.js'></script></body></html>");
  }
  
  if (!fileExists("/www/quotes.html")) {
    writeFileToSD("/www/quotes.html", "<!DOCTYPE html><html lang='it'><head><meta charset='UTF-8'><title>Citazioni - AtmoVerse</title><link rel='stylesheet' href='css/style.css'></head><body><div id='quotes-content'></div><script src='js/quotes.js'></script></body></html>");
  }
  
  // Crea file CSS se non esiste
  if (!fileExists("/www/css/style.css")) {
    writeFileToSD("/www/css/style.css", "body{font-family:sans-serif;margin:0;padding:20px;background:#f5f5f5;color:#333}.container{max-width:800px;margin:0 auto}h1,h2{color:#7a604a}");
  }
  
  // Salva tutte le icone SVG
  saveIconsToSD();
  
  return true;
}

// Controlla se un file esiste sulla SD
bool fileExists(const char* path) {
  return SD.exists(path);
}

// Legge un file dalla SD e restituisce il contenuto come stringa
String readFileFromSD(const char* path) {
  File file = SD.open(path, FILE_READ);
  if (!file) {
    Serial.print("Errore apertura file: ");
    Serial.println(path);
    return "";
  }
  
  String content = "";
  while (file.available()) {
    content += (char)file.read();
  }
  
  file.close();
  return content;
}

// Scrive contenuto in un file sulla SD
bool writeFileToSD(const char* path, const char* content) {
  File file = SD.open(path, FILE_WRITE);
  if (!file) {
    Serial.print("Errore creazione file: ");
    Serial.println(path);
    return false;
  }
  
  file.print(content);
  file.close();
  return true;
}

// Crea una directory se non esiste
void createDirectoryIfNotExists(const char* path) {
  if (!SD.exists(path)) {
    if (SD.mkdir(path)) {
      Serial.print("Cartella creata: ");
      Serial.println(path);
    } else {
      Serial.print("Errore creazione cartella: ");
      Serial.println(path);
    }
  }
}

// Salva tutte le icone SVG sulla SD
void saveIconsToSD() {
  // Importa le definizioni delle icone
  extern const char SVG_CLOUD_ICON[] PROGMEM;
  extern const char SVG_THUNDER_ICON[] PROGMEM;
  extern const char SVG_RAIN_ICON[] PROGMEM;
  extern const char SVG_SNOW_ICON[] PROGMEM;
  extern const char SVG_MIST_ICON[] PROGMEM;
  extern const char SVG_SUN_ICON[] PROGMEM;
  extern const char SVG_UNKNOWN_ICON[] PROGMEM;
  extern const char SVG_QUOTE_ICON[] PROGMEM;
  extern const char SVG_TEMP_ICON[] PROGMEM;
  extern const char SVG_HUMIDITY_ICON[] PROGMEM;
  extern const char SVG_PRESSURE_ICON[] PROGMEM;
  extern const char SVG_LOCATION_ICON[] PROGMEM;
  extern const char SVG_TIMEZONE_ICON[] PROGMEM;
  extern const char SVG_UPDATE_ICON[] PROGMEM;
  extern const char SVG_REFRESH_ICON[] PROGMEM;
  
  // Array di coppie {nome, puntatore}
  struct {
    const char* filename;
    const char* icon;
  } icons[] = {
    {"cloud.svg", SVG_CLOUD_ICON},
    {"thunder.svg", SVG_THUNDER_ICON},
    {"rain.svg", SVG_RAIN_ICON},
    {"snow.svg", SVG_SNOW_ICON},
    {"mist.svg", SVG_MIST_ICON},
    {"sun.svg", SVG_SUN_ICON},
    {"unknown.svg", SVG_UNKNOWN_ICON},
    {"quote.svg", SVG_QUOTE_ICON},
    {"temp.svg", SVG_TEMP_ICON},
    {"humidity.svg", SVG_HUMIDITY_ICON},
    {"pressure.svg", SVG_PRESSURE_ICON},
    {"location.svg", SVG_LOCATION_ICON},
    {"timezone.svg", SVG_TIMEZONE_ICON},
    {"update.svg", SVG_UPDATE_ICON},
    {"refresh.svg", SVG_REFRESH_ICON},
  };
  
  char filepath[40];
  for (size_t i = 0; i < sizeof(icons) / sizeof(icons[0]); i++) {
    sprintf(filepath, "/www/icons/%s", icons[i].filename);
    if (!fileExists(filepath)) {
      // Converti da PROGMEM a stringa
      String iconStr = "";
      const char* progmemStr = icons[i].icon;
      for (int j = 0; j < strlen_P(progmemStr); j++) {
        iconStr += (char)pgm_read_byte(progmemStr + j);
      }
      
      writeFileToSD(filepath, iconStr.c_str());
      Serial.print("Salvata icona: ");
      Serial.println(filepath);
    }
  }
}
