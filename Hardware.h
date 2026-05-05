#ifndef HARDWARE_H
#define HARDWARE_H

// Definizione pin per display e-ink
#define EPD_BUSY    4    // GPIO04 - busy
#define EPD_RST     16   // GPIO16 - res (reset)
#define EPD_DC      17   // GPIO17 - d/c (data/command)
#define EPD_CS      5   // GPIO5 - cs (chip select)
#define EPD_SCK     18   // GPIO18 - sck (clock)
#define EPD_MOSI    23   // GPIO23 - sdi (data in/MOSI)

// Definizione pin per SD card
#define SD_CS       14   // GPIO14 - cs (chip select SD)
#define SD_SCK      27   // GPIO27 - sck (clock SD)
#define SD_MOSI     26   // GPIO26 - mosi (data in SD)
#define SD_MISO     25   // GPIO25 - miso (data out SD)
// Frequenza SPI SD: tentativo iniziale a 4 MHz (poi scalerà se necessario)
#define SD_SPI_FREQ 4000000

#include <SPI.h>
extern SPIClass sdSPI;

// Definizioni hardware generali
#define SERIAL_BAUD_RATE 115200

// Altri parametri hardware
#define WEB_SERVER_PORT 80

// Funzione per inizializzare l'hardware
void initHardware();

// Funzione per inizializzare la SD card in modo sicuro e centralizzato
bool initSD();

// Dichiarazione globale del display
// Ripristino a BW per compatibilità libreria standard
#include <GxEPD2_BW.h>
using DisplayType = GxEPD2_BW<GxEPD2_583_T8, GxEPD2_583_T8::HEIGHT>;
extern DisplayType display;

// Definizioni colori per 4-Gray (se non già definiti dalla libreria)
#ifndef GxEPD_BLACK
#define GxEPD_BLACK     0x00
#endif
#ifndef GxEPD_DARKGREY
#define GxEPD_DARKGREY  0x01 // O 0x55 a seconda della mappatura
#endif
#ifndef GxEPD_LIGHTGREY
#define GxEPD_LIGHTGREY 0x02 // O 0xAA
#endif
#ifndef GxEPD_WHITE
#define GxEPD_WHITE     0x03 // O 0xFF
#endif

// Nota: GxEPD2_4G usa solitamente:
// 0x0 = Nero
// 0x1 = Grigio Scuro
// 0x2 = Grigio Chiaro
// 0x3 = Bianco
// Assicuriamoci di usare questi valori se la libreria li aspetta così.

#endif // HARDWARE_H
