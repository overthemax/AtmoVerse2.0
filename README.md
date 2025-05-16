# AtmoVerse 2.0

Sistema meteo avanzato basato su ESP32 con display e-ink e interfaccia web. Utilizza un innovativo sistema di swap su SD card per estendere la memoria disponibile e memorizzare fino a 24 ore di dati meteo storici.

![AtmoVerse 2.0](https://example.com/atmoverse_image.jpg)

## Caratteristiche Principali

- **Display e-Ink**: Visualizza dati meteo correnti e previsioni con un'interfaccia grafica ottimizzata
- **Sistema di Swap su SD**: Estende la memoria dell'ESP32 con 1MB di swap su SD card
- **Cronologia Meteo 24h**: Memorizza e visualizza fino a 24 ore di dati meteo (144 campioni, uno ogni 10 minuti)
- **Configurazione Web**: Interfaccia web per configurare WiFi, API key, località e altre impostazioni
- **Modalità AP Automatica**: Se non configurato, il dispositivo crea una rete WiFi per la configurazione
- **Aggiornamenti Automatici**: Aggiorna i dati meteo ogni 30 minuti via API OpenWeather

## Architettura del Software

Il progetto è organizzato in moduli per facilitare lo sviluppo e la manutenzione:

- **AtmoVerse_2.0.ino**: File principale con le funzioni `setup()` e `loop()`
- **Hardware.h**: Definizioni hardware (pin, parametri fisici)
- **Config.h/cpp**: Gestione della configurazione del dispositivo
- **WeatherData.h/cpp**: Strutture dati e funzioni per i dati meteo
- **Display.h/cpp**: Gestione del display e-ink e dell'interfaccia grafica
- **Network.h/cpp**: Funzioni di rete, server web e gestione WiFi
- **SwapManager.h/cpp**: Sistema di swap su SD per estendere la memoria

## Sistema di Swap su SD

AtmoVerse 2.0 implementa un innovativo sistema di swap file su SD card che estende la memoria disponibile dell'ESP32. Questo permette di memorizzare una cronologia meteo completa di 24 ore.

### Caratteristiche tecniche:
- File di swap da 1MB sulla SD card
- Sistema di blocchi da 4KB per accesso efficiente
- Gestione FIFO (First In, First Out) per i dati storici
- Ottimizzazione delle strutture dati per minimizzare l'uso della memoria

## Installazione e Configurazione

### Requisiti Hardware
- ESP32 (testato su ESP32-WROOM-32)
- Display e-ink (compatibile GxEPD2, testato su GDEW0583T8 5.83")
- Scheda SD (minimo 1GB)
- Alimentazione 5V stabile

### Collegamenti Pin
- **Display e-ink**:
  - BUSY: GPIO04
  - RST: GPIO16
  - DC: GPIO17
  - CS: GPIO05
  - SCK: GPIO18
  - MOSI: GPIO23

- **SD Card**:
  - CS: GPIO5
  - MOSI: GPIO23
  - MISO: GPIO19
  - SCK: GPIO18

### Prima Configurazione
1. Caricare il firmware sull'ESP32
2. Il dispositivo entrerà in modalità AP (Access Point) 
3. Collegati alla rete WiFi generata ("AtmoVerse-XXXX")
4. Apri il browser e vai all'indirizzo 192.168.4.1
5. Configura la tua rete WiFi, API key OpenWeather e località
6. Il dispositivo si riavvierà e si connetterà alla tua rete WiFi

## API Web Disponibili

Il server web integrato fornisce le seguenti API:
- `/` - Pagina di configurazione principale
- `/config` - Visualizzazione/modifica configurazione (GET/POST)
- `/wifi/scan` - Scansione reti WiFi disponibili
- `/weather` - Dati meteo correnti (JSON)
- `/weather/update` - Forza aggiornamento dati meteo
- `/weather/history` - Cronologia dati meteo (JSON)
- `/restart` - Riavvia il dispositivo

## Sviluppo Futuro

Funzionalità pianificate per le future versioni:
- Grafici storici dei parametri meteo
- Previsioni meteo a 5 giorni
- Supporto per sensori meteo locali
- Modalità a basso consumo per alimentazione a batteria
- Integrazione con piattaforme domotiche

## Licenza

Questo progetto è distribuito con licenza [MIT](LICENSE).

## Credits

Sviluppato originariamente come parte del progetto AtmoVerse.
