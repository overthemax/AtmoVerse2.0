#ifndef HARDWARE_H
#define HARDWARE_H

// E-ink display pins
#define EPD_BUSY    4    // GPIO04 - busy
#define EPD_RST     16   // GPIO16 - res (reset)
#define EPD_DC      17   // GPIO17 - d/c (data/command)
#define EPD_CS      5   // GPIO5 - cs (chip select)
#define EPD_SCK     18   // GPIO18 - sck (clock)
#define EPD_MOSI    23   // GPIO23 - sdi (data in/MOSI)

// SD card pins
#define SD_CS       14   // GPIO14 - cs (chip select SD)
#define SD_SCK      27   // GPIO27 - sck (clock SD)
#define SD_MOSI     26   // GPIO26 - mosi (data in SD)
#define SD_MISO     25   // GPIO25 - miso (data out SD)
// SD SPI clock: first attempt at 4 MHz (lower if needed)
#define SD_SPI_FREQ 4000000

#include <SPI.h>
extern SPIClass sdSPI;

// Definizioni hardware generali
#define SERIAL_BAUD_RATE 115200

// Altri parametri hardware
#define WEB_SERVER_PORT 80

// Hardware initialization
void initHardware();

// Safe, centralized SD card initialization
bool initSD();

// Why the board restarted last time (power on, crash, brownout...)
const char* resetReasonText();

// Global display object
// Black and white driver, as in the standard library
#include <GxEPD2_BW.h>
using DisplayType = GxEPD2_BW<GxEPD2_583_T8, GxEPD2_583_T8::HEIGHT>;
extern DisplayType display;

#endif // HARDWARE_H
