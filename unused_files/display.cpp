/*
 * AtmoVerse 2.0 - Implementazione funzioni Display E-Ink
 * 
 * Versione ulteriormente ottimizzata per ridurre il consumo di memoria
 * Modifiche per risolvere problemi di memoria sull'ESP32 con GxEPD2
 */

#include "display.h"
#include "config.h"
#include "weather.h"
#include "quotes.h"

// Minimizzato per risparmiare spazio programma
#include <GxEPD2_BW.h>
#include <Fonts/FreeSans9pt7b.h>

// Collegamento pin e-ink display
#define EPD_CS    5
#define EPD_DC    22
#define EPD_RST   21
#define EPD_BUSY  4
#define EPD_SCK   18
#define EPD_MOSI  23

// Costanti per il layout
#define DISP_MARGIN 10
#define TEXT_LINE_HEIGHT 20

// Riferimento diretto per ottimizzare memoria
extern WeatherData currentWeather;

// Imposta font piccolo per risparmiare memoria
#define SMALL_FONT &FreeSans9pt7b

// Funzione per aggiornare il display con i dati meteo
// Ottimizzata per ridurre l'utilizzo di memoria
void updateEinkDisplay() {
    // Crea display solo quando necessario
    auto epd = GxEPD2_154_D67(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY);
    GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(epd);
    
    // Inizializza con parametri leggeri
    display.init(115200);
    
    // Usa disegno a pagine per ottimizzare memoria
    display.setRotation(1);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(SMALL_FONT);
    display.setFullWindow();
    display.firstPage();
    
    do {
        // Disegna solo informazioni essenziali
        display.fillScreen(GxEPD_WHITE);
        
        // Usa intero y come offset per ridurre variabili
        int y = 30;
        
        // Intestazione
        display.setCursor(DISP_MARGIN, y);
        display.print(F("AtmoVerse 2.0"));
        
        // Dati meteo principali
        y += 30;
        display.setCursor(DISP_MARGIN, y);
        display.print(F("Temp: "));
        display.print(currentWeather.temperature);
        display.print(F("C"));
        
        // Incrementa la posizione verticale
        y += TEXT_LINE_HEIGHT;
        display.setCursor(DISP_MARGIN, y);
        display.print(F("Meteo: "));
        display.print(currentWeather.weatherCondition);
        
    } while (display.nextPage());
    
    // Importante: spegni il display per risparmiare energia
    display.powerOff();
}
