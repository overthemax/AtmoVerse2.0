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

#include <SPI.h>
extern SPIClass sdSPI;

// Definizioni hardware generali
#define SERIAL_BAUD_RATE 115200

// Altri parametri hardware
#define WEB_SERVER_PORT 80

// Funzione per inizializzare l'hardware
void initHardware();

// Dichiarazione globale del display
#include <GxEPD2_BW.h>
extern GxEPD2_BW<GxEPD2_583_T8, 120> display; // Using 120px page height for paged drawing

#endif // HARDWARE_H
