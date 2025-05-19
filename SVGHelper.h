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
#include <GxEPD2_BW.h>
#include <GxEPD2_583_T8.h>



/**
 * @enum WeatherIcon
 * @brief Enumerazione delle icone meteo disponibili nel file SVG
 */
enum WeatherIcon {
  ICON_SOL_SEMICOPERTO = 0,     ///< Riga 1, Col 1: Sole parzialmente coperto
  ICON_SOLE = 1,                ///< Riga 1, Col 2: Sole pieno
  ICON_SOLE_NUVOLOSO = 2,       ///< Riga 1, Col 3: Sole con nuvole
  ICON_SOLE_TEMPESTA = 3,       ///< Riga 1, Col 4: Sole con tempesta
  
  ICON_NUVOLA_PIOGGIA1 = 4,    ///< Riga 2, Col 1: Nuvola con pioggia (gocce lunghe)
  ICON_NUVOLA_PIOGGIA2 = 5,    ///< Riga 2, Col 2: Nuvola con pioggia (gocce corte)
  ICON_NUVOLA_TEMPORALE1 = 6,  ///< Riga 2, Col 3: Nuvola con temporale (fulmini lunghi)
  ICON_NUVOLA_TEMPORALE2 = 7,  ///< Riga 2, Col 4: Nuvola con temporale (fulmini corti)
  
  ICON_NEVE_PIOGGIA1 = 8,      ///< Riga 3, Col 1: Neve e pioggia (gocce lunghe)
  ICON_NEVE_PIOGGIA2 = 9,      ///< Riga 3, Col 2: Neve e pioggia (gocce lunghe)
  ICON_NEVE1 = 10,             ///< Riga 3, Col 3: Neve (fiocchi lunghi)
  ICON_NEVE2 = 11,             ///< Riga 3, Col 4: Neve (fiocchi corti)
  
  ICON_COUNT = 12              ///< Numero totale di icone disponibili
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
    static const size_t MAX_PATH_LENGTH = 1024; ///< Lunghezza massima di un percorso SVG
    /**
     * @brief Inizializza il gestore SVG
     * @param csPin Pin del chip select della scheda SD (opzionale)
     * @return true se l'inizializzazione è riuscita, false altrimenti
     */
    static bool begin(int8_t csPin = SS);
    
    template<typename DisplayType>
    static bool drawWeatherIcon(DisplayType& display, WeatherIcon icon, int x, int y, int size);
    
    template<typename DisplayType>
    static bool loadSVG(DisplayType& display, const char* filename, int x, int y, int width, int height);
    
    template<typename DisplayType>
    static bool drawMoonPhase(DisplayType& display, int x, int y, int size, int phase);

    /**
     * @brief Converte un ID meteo OpenWeatherMap nell'icona appropriata
     * @param weatherID ID meteo da OpenWeatherMap
     * @param isNight Se true, restituisce la versione notturna dell'icona
     * @return Il codice dell'icona corrispondente
     */
    static WeatherIcon getIconFromWeatherID(int weatherID, bool isNight);
    
    // Funzione di supporto per estrarre attributi numerici da una stringa SVG
    static int extractAttribute(String& line, const char* attr, int defaultValue = 0);
    
  private:

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
    template<typename DisplayType>
    static bool extractAndDrawIcon(DisplayType& display, const char* filename, WeatherIcon icon, int x, int y, int size) {
        if (!initialized && !begin()) {
            Serial.println(F("[SVG] Errore: SVGHelper non inizializzato"));
            return false;
        }
        if (icon < 0 || icon >= ICON_COUNT) {
            Serial.print(F("[SVG] Errore: codice icona non valido: "));
            Serial.println(icon);
            return false;
        }
        if (size <= 0) {
            Serial.println(F("[SVG] Errore: dimensione non valida"));
            return false;
        }
        File svgFile = SD.open(filename);
        if (!svgFile) {
            Serial.print("Impossibile aprire il file: ");
            Serial.println(filename);
            return false;
        }
        const int bufferSize = 128;
        char buffer[bufferSize];
        String line = "";
        int viewBoxX = 0, viewBoxY = 0, viewBoxWidth = 0, viewBoxHeight = 0;
        int srcX = iconPositions[icon].x;
        int srcY = iconPositions[icon].y;
        int iconSize = 100;
        // Prima passata: trovare il viewBox
        // ... (puoi inserire qui il resto della logica della funzione dal .cpp, se necessario) ...
        svgFile.close();
        return true;
    }
    
    // Specializzazione esplicita per GxEPD2_583_T8
    static bool extractAndDrawIcon(GxEPD2_BW<GxEPD2_583_T8, GxEPD2_583_T8::HEIGHT>& display, const char* filename, WeatherIcon icon, int x, int y, int size);
    
    // Funzioni di supporto per il disegno
    template<typename DisplayType>
    static void drawCircle(DisplayType& display, int cx, int cy, int r, bool fill);
    
    // Specializzazioni esplicite per GxEPD2_583_T8
    static void drawCircle(GxEPD2_BW<GxEPD2_583_T8, GxEPD2_583_T8::HEIGHT>& display, int cx, int cy, int r, bool fill);
    
    template<typename DisplayType>
    static void drawPath(DisplayType& display, const String& path, int offsetX, int offsetY, float scale);
    
    // Specializzazioni esplicite per GxEPD2_583_T8
    static void drawPath(GxEPD2_BW<GxEPD2_583_T8, GxEPD2_583_T8::HEIGHT>& display, String path, int offsetX, int offsetY, float scale);
    
    template<typename DisplayType>
    static void drawEllipse(DisplayType& display, int centerX, int centerY, int radiusX, int radiusY, bool fill);
    
    // Specializzazioni esplicite per GxEPD2_583_T8
    static void drawEllipse(GxEPD2_BW<GxEPD2_583_T8, GxEPD2_583_T8::HEIGHT>& display, int centerX, int centerY, int radiusX, int radiusY, bool fill);
    
    /**
     * @brief Disegna una forma di luna crescente o calante
     * 
     * @param display Riferimento all'oggetto display
     * @param centerX Coordinata X del centro
     * @param centerY Coordinata Y del centro
     * @param radius Raggio della luna
     * @param isWaxing true per luna crescente, false per luna calante
     */
    template<typename DisplayType>
    static void drawMoonShape(DisplayType& display, int centerX, int centerY, int radius, bool isWaxing);
    
    // Specializzazione esplicita per GxEPD2_583_T8
    static void drawMoonShape(GxEPD2_BW<GxEPD2_583_T8, GxEPD2_583_T8::HEIGHT>& display, int centerX, int centerY, int radius, bool isWaxing);
    
    static bool initialized;  ///< Flag di inizializzazione
    
    /**
     * @brief Mappa delle posizioni delle icone nel file SVG
     * 
     * Questa matrice definisce le coordinate di ciascuna icona all'interno
     * del file SVG principale. Le coordinate sono espresse in pixel relativi
     * all'angolo in alto a sinistra dell'area di disegno.
     */
    static const IconCoords iconPositions[ICON_COUNT];
    
    // Costanti per il disegno delle fasi lunari
    static const uint8_t MOON_PHASES = 13;  ///< Numero di fasi lunari supportate (0-12)
    
    // Dimensione massima per i buffer di lavoro
    
    // Timeout per le operazioni di file (in ms)
    static const uint32_t FILE_OPERATION_TIMEOUT = 5000;
    
    // Funzioni di utilità per il parsing SVG
    static bool skipWhitespace(const char*& str);
    static bool parseNumber(const char*& str, float& value);
    static bool parseCommand(const char*& str, char& cmd, bool& relative);
    static bool parseCoord(const char*& str, float& x, float& y, bool relative, float lastX, float lastY);
};

#endif // SVGHELPER_H
