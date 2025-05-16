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
#include "GxEPD.h"

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
    static bool drawWeatherIcon(GxEPD_Class& display, WeatherIcon icon, int x, int y, int size);
    
    /**
     * @brief Disegna una fase lunare
     * @param display Riferimento all'oggetto display
     * @param x Coordinata X di destinazione
     * @param y Coordinata Y di destinazione
     * @param size Dimensione dell'icona
     * @param phase Fase lunare (0-12, dove 0 è luna nuova e 6 è luna piena)
     * @return true se il disegno è riuscito, false altrimenti
     */
    static bool drawMoonPhase(GxEPD_Class& display, int x, int y, int size, int phase);
    
    /**
     * @brief Converte un ID meteo OpenWeatherMap nell'icona appropriata
     * @param weatherID ID meteo da OpenWeatherMap
     * @param isNight Se true, restituisce la versione notturna dell'icona
     * @return Il codice dell'icona corrispondente
     */
    static WeatherIcon getIconFromWeatherID(int weatherID, bool isNight);
    
  private:
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
    static bool loadSVG(GxEPD_Class& display, const char* filename, int x, int y, int width, int height);
    
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
    static bool extractAndDrawIcon(GxEPD_Class& display, const char* filename, WeatherIcon icon, int x, int y, int size);
    
    // Funzioni di supporto per il disegno
    static void drawCircle(GxEPD_Class& display, int cx, int cy, int r, bool fill);
    static void drawPath(GxEPD_Class& display, const String& path, int offsetX, int offsetY, float scale);
    static void drawEllipse(GxEPD_Class& display, int centerX, int centerY, int radiusX, int radiusY, bool fill);
    
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
    static const size_t MAX_PATH_LENGTH = 1024;  ///< Lunghezza massima di un percorso SVG
    
    // Timeout per le operazioni di file (in ms)
    static const uint32_t FILE_OPERATION_TIMEOUT = 5000;
};

#endif // SVGHELPER_H
