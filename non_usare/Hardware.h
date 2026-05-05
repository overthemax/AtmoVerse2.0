#ifndef HARDWARE_H
#define HARDWARE_H


// Definizione pin per display e-ink
#define EPD_BUSY    4    // GPIO04 - busy
#define EPD_RST     16   // GPIO16 - res (reset)
#define EPD_DC      17   // GPIO17 - d/c (data/command)
#define EPD_CS      5   // GPIO5 - cs (chip select)
#define EPD_SCK     18   // GPIO18 - sck (clock)
#define EPD_MOSI    23   // GPIO23 - sdi (data in/MOSI)

// Definizione pin per pulsanti
// Pin unico e coerente per il pulsante di reset, usato in tutto il progetto
#define RESET_BUTTON_PIN 35    // GPIO35 - Pulsante di reset (input only con pull-up esterno)

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
/**
 * @brief Inizializza l'hardware del sistema
 * @return true se l'inizializzazione è riuscita, false altrimenti
 */
bool initHardware();

// Include base libraries for e-paper display
#include <SPI.h>

// Include GxEPD2 base and display class
#include <GxEPD2_BW.h> // Classe display in bianco e nero

// La struttura moderna della libreria GxEPD2 include tutti i pannelli

// Define panel type explicitly - 5.83" display (648x480)
// GxEPD2_583 è stato rinominato in GxEPD2_583_T8 nella nuova libreria
typedef GxEPD2_583_T8 PanelType;

extern GxEPD2_BW<PanelType, 32> display; // Using 32px page height for paged drawing (ulteriormente ridotto per ottimizzare memoria)

// Lettura batteria (LOLIN32): pin ADC e fattore partitore
// Nota: molte LOLIN32 usano un partitore 100k/100k verso un pin ADC (tipicamente GPIO34)
// Se la tua revisione ha un rapporto diverso, aggiorna VBAT_DIVIDER di conseguenza
#ifndef VBAT_PIN
#define VBAT_PIN 34
#endif

#ifndef VBAT_DIVIDER
#define VBAT_DIVIDER 2.0f
#endif

#endif // HARDWARE_H
