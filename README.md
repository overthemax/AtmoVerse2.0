# AtmoVerse 2.0

Sistema meteo avanzato basato su ESP32 con display e-ink, design minimalista e interfaccia web intuitiva. Progettato per massima leggibilità e semplicità d’uso.

![AtmoVerse 2.0](https://example.com/atmoverse_image.jpg)

## Caratteristiche Principali

- **Display e-Ink minimalista**: Visualizzazione chiara di dati meteo, calendario senza griglia, icone extra-large e layout ordinato.
- **Gestione avanzata icone meteo**: Icone SVG/PNG ottimizzate per e-ink, posizionamento personalizzato (nuvole, luna, ecc.), icone nere su sfondo bianco.
- **Citazioni dinamiche**: Box citazioni ingrandito, font serif, autore posizionato elegantemente, testo adattivo e cambio automatico in base a ora/meteo.
- **Gestione errori**: Mostra sempre l'ultimo dato valido in caso di problemi di rete, logging dettagliato.
- **Power Saving Mode**: Modalità risparmio energetico per aumentare la durata della batteria.
- **Configurazione WiFi/AP**: Se non configurato, crea una rete WiFi per la configurazione tramite web.
- **Interfaccia Web**: Configurazione WiFi, API key, località e gestione citazioni direttamente da browser.
- **SD Card**: Utilizzata per file di configurazione e citazioni, non per swap o cronologia meteo.

## Preferenze di Design

- Nessun bordo o griglia superflua
- Icone meteo extra-large senza cerchio nero
- Font serif per citazioni e autore (FreeSerif12pt7b)
- Posizionamento avanzato di luna, nuvole e autore
- Quote non troncate (max 35 caratteri/linea)
- Styling elegante e spaziatura ampia
- Nessun effetto speciale tra icone

## Architettura del Software

- **AtmoVerse_2.0.ino**: Loop principale e setup
- **WeatherIcons.h/cpp**: Gestione e disegno icone meteo
- **SVGHelper.h/cpp**: Parsing e rendering SVG
- **Config.h/cpp**: Gestione configurazione utente
- **Display.h/cpp**: Rendering grafico, calendario, citazioni
- **NetworkUtils.h/cpp**: Gestione WiFi, AP e web server
- **QuotesManager/QuotesUtils**: Gestione citazioni e file su SD

## Installazione e Configurazione

### Requisiti Hardware
- ESP32 (testato su ESP32-WROOM-32)
- Display e-ink (GxEPD2, GDEW0583T8 5.83" consigliato)
- Scheda SD (min 1GB, FAT32)
- Alimentazione 5V

### Collegamenti Pin (default)
- **Display e-ink**: BUSY GPIO04, RST GPIO16, DC GPIO17, CS GPIO05, SCK GPIO18, MOSI GPIO23
- **SD Card**: CS GPIO5, MOSI GPIO23, MISO GPIO19, SCK GPIO18

### Prima Configurazione
1. Inserisci la SD card formattata in FAT32.
2. Carica il firmware tramite Arduino IDE o PlatformIO.
3. All'avvio, se non configurato, il dispositivo crea una rete WiFi AP.
4. Collegati all'AP e accedi all’interfaccia web per configurare WiFi e API key.

## Utilizzo
- Dopo la configurazione, il dispositivo scarica i dati meteo e aggiorna il display ogni 30 minuti.
- In caso di errore di rete, viene mostrato l’ultimo dato valido.
- Le citazioni cambiano dinamicamente in base all’orario e alle condizioni meteo.
- Il calendario mostra solo il bordo e i giorni, senza griglie.

## Sviluppo e Contribuzione
- Codice documentato con Doxygen.
- Per contribuire: crea una branch, invia una pull request descrivendo la modifica.
- Segnala bug o suggerimenti tramite le Issues di GitHub.

## Licenza
Questo progetto è open source, rilasciato sotto licenza MIT.

## Autori e Credits
- Progetto originale: overthemax
- Contributi recenti: gestione icone, error handling, ottimizzazione memoria, UI minimalista

---

Per domande o supporto, apri una Issue su [GitHub](https://github.com/overthemax/AtmoVerse2.0).
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
