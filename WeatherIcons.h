/**
 * @file WeatherIcons.h
 * @brief Gestione delle icone meteo per il display e-ink
 * 
 * Questa classe fornisce funzionalità per scaricare, memorizzare e visualizzare
 * icone meteo da OpenWeatherMap su un display e-ink.
 */

#ifndef WEATHERICONS_H
#define WEATHERICONS_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <SD.h>
#include "Hardware.h"

/**
 * @brief Classe per la gestione delle icone meteo
 */
class WeatherIcons {
  public:
    /**
     * @brief Inizializza la gestione delle icone
     * @return true se l'inizializzazione è riuscita, false altrimenti
     */
    static bool begin();
    
    /**
     * @brief Scarica un'icona da OpenWeatherMap
     * @param iconCode Codice dell'icona da scaricare
     * @param timeoutMs Timeout per la richiesta HTTP in millisecondi (default: 10000)
     * @return true se il download è riuscito, false altrimenti
     */
    static bool downloadIcon(const String& iconCode, uint32_t timeoutMs = 10000);
    
    /**
     * @brief Verifica se un'icona è già presente nella memoria
     * @param iconCode Codice dell'icona da verificare
     * @return true se l'icona esiste, false altrimenti
     */
    static bool iconExists(const String& iconCode);
    
    /**
     * @brief Disegna un'icona meteo sul display
     * @param display Riferimento all'oggetto display
     * @param iconCode Codice dell'icona da disegnare (es. "01d")
     * @param x Coordinata X di partenza
     * @param y Coordinata Y di partenza
     * @param size Dimensione dell'icona
     * @param weatherId ID meteo numerico opzionale (es. 800) per maggiore precisione
     * @return true se il disegno è riuscito, false altrimenti
     */
    static bool drawWeatherIcon(DisplayType& display, const String& iconCode, int x, int y, int size, int weatherId = -1);
    
    /**
     * @brief Ottiene il percorso completo del file dell'icona
     * @param iconCode Codice dell'icona
     * @param weatherId ID meteo numerico opzionale
     * @return Stringa contenente il percorso completo del file
     */
    static String getIconPath(const String& iconCode, int weatherId = -1);
    
    /**
     * @brief Mappa il codice OpenWeatherMap al nome file Weather Icons
     * @param iconCode Codice dell'icona
     * @param weatherId ID meteo numerico opzionale
     * @return Nome del file dell'icona senza estensione
     */
    static String getIconNameFromCode(const String& iconCode, int weatherId = -1);

    /**
     * @brief Prepara un'icona per l'uso
     * @param iconCode Codice dell'icona da preparare
     * @return true se l'icona è pronta, false in caso di errore
     */
    static bool prepareIcon(const String& iconCode);
    
  private:
    /**
     * @brief Converte un'immagine in scala di grigi in bianco e nero
     * @param buffer Buffer contenente i dati dell'immagine
     * @param width Larghezza dell'immagine
     * @param height Altezza dell'immagine
     */
    static void convertToBlackAndWhite(uint8_t* buffer, int width, int height);
    
    static const int DEFAULT_ICON_SIZE = 100;  ///< Dimensione predefinita per le icone
    static bool initialized;  ///< Flag di inizializzazione
    
    // Dimensione massima del buffer per il download delle icone (100KB)
    static const size_t MAX_ICON_SIZE = 102400;
};

#endif // WEATHERICONS_H
