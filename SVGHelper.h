/**
 * @file SVGHelper.h
 * @brief Gestione di file SVG per la visualizzazione di icone meteo e fasi lunari
 * 
 * Questa classe fornisce funzionalità per caricare e visualizzare icone meteo
 * e fasi lunari da file SVG su display e-ink.
 */

#ifndef SVGHELPER_H
#define SVGHELPER_H

#include <Arduino.h>
#include <SD.h>
#include "Hardware.h"

/**
 * @enum WeatherIcon
 * @brief Enumerazione delle icone meteo disponibili nel file SVG
 */
enum WeatherIcon {
  // GIORNO - Condizioni Base (0-9)
  ICON_DAY_SUNNY = 0,          ///< Sole pieno
  ICON_DAY_CLOUDY = 1,         ///< Parzialmente nuvoloso
  ICON_DAY_FOG = 2,            ///< Nebbia diurna
  ICON_DAY_WINDY = 3,          ///< Vento forte diurno
  
  // GIORNO - Pioggia (10-14)
  ICON_DAY_RAIN = 10,          ///< Pioggia diurna
  ICON_DAY_RAIN_WIND = 11,     ///< Pioggia e vento
  ICON_DAY_SPRINKLE = 12,      ///< Pioggerella diurna
  ICON_DAY_SHOWERS = 13,       ///< Acquazzoni diurni
  
  // GIORNO - Temporali (20-22)
  ICON_DAY_THUNDERSTORM = 20,  ///< Temporale diurno
  ICON_DAY_STORM_SHOWERS = 21, ///< Temporale intenso diurno
  ICON_DAY_LIGHTNING = 22,     ///< Fulmini diurni
  
  // GIORNO - Neve (30-34)
  ICON_DAY_SNOW = 30,          ///< Neve diurna
  ICON_DAY_SNOW_WIND = 31,     ///< Neve e vento
  ICON_DAY_SLEET = 32,         ///< Nevischio diurno
  ICON_DAY_RAIN_MIX = 33,      ///< Neve e pioggia mista
  ICON_DAY_HAIL = 34,          ///< Grandine diurna
  
  // NOTTE - Condizioni Base (40-43)
  ICON_NIGHT_CLEAR = 40,       ///< Notte serena
  ICON_NIGHT_CLOUDY = 41,      ///< Parzialmente nuvoloso notte
  ICON_NIGHT_FOG = 42,         ///< Nebbia notturna
  
  // NOTTE - Pioggia (50-53)
  ICON_NIGHT_RAIN = 50,        ///< Pioggia notturna
  ICON_NIGHT_RAIN_WIND = 51,   ///< Pioggia e vento notte
  ICON_NIGHT_SPRINKLE = 52,    ///< Pioggerella notturna
  ICON_NIGHT_SHOWERS = 53,     ///< Acquazzoni notturni
  
  // NOTTE - Temporali (60-61)
  ICON_NIGHT_THUNDERSTORM = 60, ///< Temporale notturno
  ICON_NIGHT_STORM_SHOWERS = 61, ///< Temporale intenso notte
  
  // NOTTE - Neve (70-74)
  ICON_NIGHT_SNOW = 70,        ///< Neve notturna
  ICON_NIGHT_SNOW_WIND = 71,   ///< Neve e vento notte
  ICON_NIGHT_SLEET = 72,       ///< Nevischio notturno
  ICON_NIGHT_RAIN_MIX = 73,    ///< Neve e pioggia mista notte
  ICON_NIGHT_HAIL = 74,        ///< Grandine notturna
  
  // NEUTRO - Non dipendente da giorno/notte (80-89)
  ICON_CLOUDY = 80,            ///< Nuvoloso
  ICON_RAIN = 81,              ///< Pioggia
  ICON_SNOW = 82,              ///< Neve
  ICON_THUNDERSTORM = 83,      ///< Temporale
  ICON_FOG = 84,               ///< Nebbia
  ICON_WINDY = 85,             ///< Ventoso
  ICON_TORNADO = 86,           ///< Tornado
  ICON_HURRICANE = 87,         ///< Uragano
  ICON_HOT = 88,               ///< Molto caldo
  ICON_SNOWFLAKE_COLD = 89,    ///< Molto freddo/neve intensa
  
  // GIORNO - Varianti Vento/Nubi (90-95)
  ICON_DAY_CLOUDY_WINDY = 90,  ///< Nuvoloso e ventoso diurno
  ICON_DAY_CLOUDY_GUSTS = 91,  ///< Nuvoloso con raffiche
  ICON_DAY_CLOUDY_HIGH = 92,   ///< Nubi alte diurne
  
  // NOTTE - Varianti Vento/Nubi (96-98)
  ICON_NIGHT_CLOUDY_WINDY = 96, ///< Nuvoloso e ventoso notturno
  ICON_NIGHT_CLOUDY_GUSTS = 97, ///< Nuvoloso con raffiche notte
  ICON_NIGHT_PARTLY_CLOUDY = 98, ///< Parzialmente nuvoloso notte
  
  // NEUTRO - Varianti Vento (99)
  ICON_CLOUDY_WINDY = 99,      ///< Nuvoloso e ventoso neutro
  
  // Condizioni Atmosferiche Specifiche (100-109)
  ICON_SMOKE = 100,            ///< Fumo
  ICON_HAZE = 101,             ///< Foschia/Caligine
  ICON_DUST = 102,             ///< Polvere
  ICON_SANDSTORM = 103,        ///< Tempesta di sabbia
  ICON_VOLCANO = 104,          ///< Cenere vulcanica
  
  // Precipitazioni Intense (110-115)
  ICON_DAY_SNOW_THUNDERSTORM = 110,  ///< Neve con temporale diurno
  ICON_NIGHT_SNOW_THUNDERSTORM = 111, ///< Neve con temporale notturno
  ICON_DAY_SLEET_STORM = 112,  ///< Nevischio con temporale diurno
  ICON_NIGHT_SLEET_STORM = 113, ///< Nevischio con temporale notturno
  ICON_HAIL = 114,             ///< Grandine neutra
  ICON_LIGHTNING = 115,        ///< Fulmini
  
  // Allerte e Pericoli (120-129)
  ICON_FLOOD = 120,            ///< Alluvione/Inondazione
  ICON_FIRE = 121,             ///< Incendio
  ICON_HURRICANE_WARNING = 122, ///< Allerta uragano
  ICON_GALE_WARNING = 123,     ///< Allerta tempesta
  ICON_EARTHQUAKE = 124,       ///< Terremoto
  ICON_METEOR = 125,           ///< Meteora
  
  ICON_COUNT = 130             ///< Numero totale di icone disponibili
};

/**
 * @struct IconCoords
 * @brief Coordinate relative di un'icona all'interno del file SVG
 */
struct IconCoords {
  int x;  ///< Coordinata X dell'angolo in alto a sinistra dell'icona
  int y;  ///< Coordinata Y dell'angolo in alto a sinistra dell'icona
};

/**
 * @class SVGHelper
 * @brief Classe per il caricamento e la visualizzazione di elementi SVG
 */
class SVGHelper {
  public:
    /**
     * @brief Inizializza il gestore SVG
     * @param csPin Pin del chip select della scheda SD (opzionale)
     * @return true se l'inizializzazione è riuscita, false altrimenti
     */
    static bool begin(int8_t csPin = SS);
    
    /**
     * @brief Disegna un'icona meteo specifica
     * @param display Riferimento all'oggetto display
     * @param icon Codice dell'icona da disegnare
     * @param x Coordinata X di destinazione
     * @param y Coordinata Y di destinazione
     * @param size Dimensione dell'icona
     * @return true se il disegno è riuscito, false altrimenti
     */
    static bool drawWeatherIcon(DisplayType& display, WeatherIcon icon, int x, int y, int size);
    
    /**
     * @brief Disegna una fase lunare
     * @param display Riferimento all'oggetto display
     * @param x Coordinata X di destinazione
     * @param y Coordinata Y di destinazione
     * @param size Dimensione dell'icona
     * @param phase Fase lunare (0-12, dove 0 è luna nuova e 6 è luna piena)
     * @return true se il disegno è riuscito, false altrimenti
     */
    static bool drawMoonPhase(DisplayType& display, int x, int y, int size, int phase);
    
    /**
     * @brief Ottiene l'icona appropriata per un dato codice meteo
     * @param weatherID Codice meteo da OpenWeatherMap
     * @param isNight Se true, restituisce la versione notturna dell'icona
     * @param windSpeed Velocità del vento in km/h (opzionale, default 0.0)
     * @return Il codice dell'icona corrispondente
     */
    static WeatherIcon getIconFromWeatherID(int weatherID, bool isNight, float windSpeed = 0.0f);
    
    /**
     * @brief Carica e disegna un file SVG completo
     * @param display Riferimento all'oggetto display
     * @param filename Percorso del file SVG
     * @param x Coordinata X di destinazione
     * @param y Coordinata Y di destinazione
     * @param width Larghezza di destinazione
     * @param height Altezza di destinazione
     * @return true se il caricamento è riuscito, false altrimenti
     */
    static bool loadSVG(DisplayType& display, const char* filename, int x, int y, int width, int height);
    
  private:
    
    /**
     * @brief Carica e disegna un file SVG con viewBox personalizzato
     * @param display Riferimento all'oggetto display
     * @param filename Percorso del file SVG
     * @param x Coordinata X di destinazione
     * @param y Coordinata Y di destinazione
     * @param width Larghezza di destinazione
     * @param height Altezza di destinazione
     * @param customViewBoxX X del viewBox personalizzato (-1 per usare quello del file)
     * @param customViewBoxY Y del viewBox personalizzato (-1 per usare quello del file)
     * @param customViewBoxWidth Larghezza del viewBox personalizzato (-1 per usare quello del file)
     * @param customViewBoxHeight Altezza del viewBox personalizzato (-1 per usare quello del file)
     * @return true se il caricamento è riuscito, false altrimenti
     */
    static bool loadSVGWithViewBox(DisplayType& display, const char* filename, int x, int y, int width, int height,
                                    int customViewBoxX, int customViewBoxY, int customViewBoxWidth, int customViewBoxHeight);
    
    /**
     * @brief Estrae e disegna un'icona da un file SVG completo
     * @param display Riferimento all'oggetto display
     * @param filename Percorso del file SVG
     * @param icon Codice dell'icona da estrarre
     * @param x Coordinata X di destinazione
     * @param y Coordinata Y di destinazione
     * @param size Dimensione dell'icona
     * @return true se l'estrazione è riuscita, false altrimenti
     */
    static bool extractAndDrawIcon(DisplayType& display, const char* filename, WeatherIcon icon, int x, int y, int size);
    
    // Funzioni di supporto per il disegno
    static void drawCircle(DisplayType& display, int cx, int cy, int r, bool fill);
    static void drawPath(DisplayType& display, const String& path, int offsetX, int offsetY, float scale);
    static void drawEllipse(DisplayType& display, int centerX, int centerY, int radiusX, int radiusY, bool fill);
    static void drawMoonShape(DisplayType& display, int centerX, int centerY, int radius, bool isWaxing);
    static int extractAttribute(const String& line, const char* attr, int defaultValue);
    
    static bool initialized;  ///< Flag di inizializzazione
    
    // Costanti per il disegno delle fasi lunari
    static const uint8_t MOON_PHASES = 13;  ///< Numero di fasi lunari supportate (0-12)
    
    // Dimensione massima per i buffer di lavoro
    static const size_t MAX_PATH_LENGTH = 1024;  ///< Lunghezza massima di un percorso SVG
    
    // Timeout per le operazioni di file (in ms)
    static const uint32_t FILE_OPERATION_TIMEOUT = 5000;
};

#endif // SVGHELPER_H
