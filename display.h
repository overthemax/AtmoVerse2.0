/*
 * AtmoVerse 2.0 - Funzioni Display E-Ink
 * 
 * Versione ottimizzata per la memoria
 */

#ifndef DISPLAY_H
#define DISPLAY_H

// Display implementation
#include <GxEPD2_BW.h>
#include <Fonts/FreeSans9pt7b.h>

#define EPD_CS    5
#define EPD_DC    22
#define EPD_RST   21
#define EPD_BUSY  4
#define EPD_SCK   18
#define EPD_MOSI  23

#define DISP_MARGIN 10
#define TEXT_LINE_HEIGHT 20

#define SMALL_FONT &FreeSans9pt7b

void updateEinkDisplay() {
  auto epd = GxEPD2_154_D67(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY);
  GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(epd);
  
  display.init(115200);
  display.setRotation(1);
  display.setTextColor(GxEPD_BLACK);
  display.setFont(SMALL_FONT);
  display.setFullWindow();
  display.firstPage();
  
  do {
    display.fillScreen(GxEPD_WHITE);
    int y = 30;
    
    display.setCursor(DISP_MARGIN, y);
    display.print(F("AtmoVerse 2.0"));
    
    y += 30;
    display.setCursor(DISP_MARGIN, y);
    display.print(F("Temp: "));
    display.print(currentWeather.temperature);
    display.print(F("C"));
    
    y += TEXT_LINE_HEIGHT;
    display.setCursor(DISP_MARGIN, y);
    display.print(F("Meteo: "));
    display.print(currentWeather.weatherCondition);
    
  } while (display.nextPage());
  
  display.powerOff();
}

#endif // DISPLAY_H
