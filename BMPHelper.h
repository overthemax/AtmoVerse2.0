#ifndef BMP_HELPER_H
#define BMP_HELPER_H

#include <Arduino.h>
#include "Hardware.h"
#include "SVGHelper.h"

class BMPHelper {
public:
    // Inizializza il supporto BMP (verifica SD card)
    static bool begin();
    
    // Disegna un'icona meteo BMP dal file
    // Restituisce true se il caricamento ha successo
    static bool drawWeatherIcon(DisplayType& display, WeatherIcon icon, int x, int y, int size);
    
    // Disegna un file BMP generico dalla SD
    static bool drawBMP(DisplayType& display, const char* filename, int x, int y, int maxWidth, int maxHeight);
    
    // Converte WeatherIcon enum in percorso file BMP
    static const char* getIconPath(WeatherIcon icon);
    
private:
    static bool initialized;
    
    // Legge l'header del file BMP
    static bool readBMPHeader(File& file, int& width, int& height, uint16_t& bitDepth);
    
    // Disegna BMP monocromatico sul display
    static bool drawMonochromeBMP(DisplayType& display, File& file, int x, int y, int width, int height, int maxWidth, int maxHeight, bool flipVertical);
};

#endif // BMP_HELPER_H
