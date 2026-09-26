<div align="center">

# AtmoVerse

**Stazione meteo e-ink per la casa: ora, meteo e una citazione al giorno, su carta elettronica.**

ESP32 · display e-ink 5,83″ · aggiornamenti automatici da GitHub

<img src="docs/images/display-principale.png" width="648" alt="Schermata principale di AtmoVerse">

</div>

---

## Cosa fa

- **Meteo sempre visibile.** Temperatura, percepita, umidità, vento e pressione da OpenWeatherMap, con icone disegnate per l'e-ink.
- **Una citazione che cambia con la giornata.** Scelta in base al meteo e all'orario, oppure programmata per un giorno e un'ora precisi. Il carattere si adatta alla lunghezza, così il testo non viene mai tagliato.
- **Configurazione dal telefono.** Al primo avvio il display mostra un codice QR: lo inquadri, il telefono si collega e si apre da sola la pagina di configurazione.
- **Si aggiorna da solo.** Firmware, pagine web e icone arrivano dalle release di GitHub, verificati e con ritorno automatico alla versione precedente se qualcosa va storto.
- **Continua a funzionare offline.** L'orologio RTC mantiene l'ora anche senza internet; se la rete di casa cade, AtmoVerse riprova da solo a ricollegarsi.
- **Batteria sotto controllo.** Percentuale, carica e autonomia misurate con un sensore INA219.

<table>
  <tr>
    <td align="center" width="50%">
      <img src="docs/images/display-configurazione.png" alt="Schermata di configurazione con codice QR"><br>
      <sub><b>Prima configurazione</b> · inquadra il QR e segui i tre passi</sub>
    </td>
    <td align="center" width="50%">
      <img src="docs/images/display-citazione-lunga.png" alt="Citazione lunga con carattere adattivo"><br>
      <sub><b>Carattere adattivo</b> · le citazioni lunghe restano intere</sub>
    </td>
  </tr>
</table>

<sub>Le immagini sono generate dal codice con i font reali del display (<code>tools/anteprima_display.py</code>); dati di esempio.</sub>

---

## Hardware

| Componente | Modello | Collegamento |
|---|---|---|
| Scheda | WEMOS LOLIN32 (ESP32, 4 MB flash) | — |
| Display | e-ink 5,83″ 648×480, GDEW0583T8 | SPI (VSPI): BUSY 4 · RST 16 · DC 17 · CS 5 · SCK 18 · MOSI 23 |
| Scheda SD | microSD FAT32 | SPI (HSPI): CS 14 · SCK 27 · MOSI 26 · MISO 25 |
| Orologio | DS3231 | I²C (Wire1): SDA 13 · SCL 15 |
| Batteria | 2 celle LiPo 3500 mAh in parallelo | misurate da INA219 su I²C (Wire): SDA 32 · SCL 33, indirizzo 0x41 |

---

## Installazione

### 1. Primo caricamento del firmware (una sola volta)

Serve un solo caricamento via USB: da lì in poi AtmoVerse si aggiorna da solo.

Requisiti: [Arduino CLI](https://arduino.github.io/arduino-cli/) o Arduino IDE con core **esp32 3.3.8** e le librerie

| Libreria | Versione |
|---|---|
| GxEPD2 | 1.6.9 |
| Adafruit GFX Library · Adafruit BusIO | 1.12.6 · 1.17.4 |
| U8g2_for_Adafruit_GFX | 1.8.0 |
| ArduinoJson | 7.4.3 |
| RTClib | 2.1.4 |
| Adafruit INA219 | 1.2.3 |

Collega la scheda e lancia, da PowerShell nella cartella del progetto:

```powershell
.\upload_lolin32_bigapp.ps1 -Port COM4
```

Lo script usa `partitions.csv`, che divide la memoria in due aree da 1,9 MB: servono agli aggiornamenti automatici.

### 2. Scheda SD

Basta una microSD formattata FAT32, anche vuota: pagine web, icone e font vengono scaricati da GitHub al primo collegamento.

### 3. Prima configurazione

1. Accendi AtmoVerse: senza una rete configurata mostra la schermata con il codice QR.
2. Inquadra il QR con la fotocamera del telefono. Il telefono si collega alla rete `AtmoVerse_AP_xxxx` e si apre la pagina di configurazione (altrimenti apri `http://192.168.4.1`).
3. Inserisci rete WiFi di casa, città e [API key di OpenWeatherMap](https://openweathermap.org/api) (gratuita) e tocca **Salva**.

AtmoVerse si riavvia, si collega, scarica ciò che manca e mostra il meteo.

---

## Citazioni

Le citazioni sono in `quotes.json` sulla SD e si modificano dall'editor web (`/quotes-editor.html`). Per ogni aggiornamento del display la citazione viene scelta così:

1. **Programmata**, se ce n'è una attiva: con ora, durata, giorni della settimana o data (ogni anno o una volta sola).
2. **A orario**: una frase che cita l'ora attuale, dai file `/orari/00.txt … 23.txt` (facoltativi).
3. **Meteo**: una citazione adatta al tempo e al momento della giornata.

Esempio di citazione programmata in `quotes.json`:

```json
"programmate": [
  { "text": "Buongiorno! Ogni mattina è una pagina bianca.", "author": "Anonimo",
    "ora": "07:00", "durata": 90, "giorni": "lun,mar,mer,gio,ven" }
]
```

I file `/orari` si generano da un CSV `HH:MM|frase|testo|opera|autore` e restano solo sulla SD, fuori dal repository:

```bash
python tools/prepara_citazioni_orarie.py citazioni.csv E:/
```

---

## Aggiornamenti automatici

All'avvio e ogni 6 ore AtmoVerse legge `manifest.json` dall'ultima release e scarica solo ciò che è cambiato.

- **File della SD** (`sd_files/`): verificati con SHA-256, preparati in `/upd` e applicati tutti insieme, anche dopo un'interruzione.
- **Firmware**: scritto nella seconda area di memoria. Se la nuova versione non supera i primi 60 secondi, il bootloader torna a quella precedente e la versione difettosa non viene più scaricata.
- **File personali** (configurazione, `quotes.json`, `layout.json`, `/orari`): mai sovrascritti.

Tutti i download avvengono in HTTPS con verifica del certificato del server.

### Pubblicare una nuova versione

```bash
git tag v2.1.1
git push origin v2.1.1
```

La GitHub Action [`release.yml`](.github/workflows/release.yml) compila il firmware, genera il manifest con [`tools/make_manifest.py`](tools/make_manifest.py) e pubblica la release. I dispositivi la installano al controllo successivo, oppure subito con **Controlla ora** nella pagina Info.

---

## Interfaccia web

Raggiungibile all'indirizzo IP mostrato in basso sul display (in configurazione: `http://192.168.4.1`).

| API | Descrizione |
|---|---|
| `GET /api/weather` | Dati meteo correnti |
| `GET/POST /api/settings` | Legge o salva la configurazione. L'API key non viene mai restituita; password e API key lasciate vuote restano invariate |
| `GET /api/wifi-scan` | Reti WiFi visibili |
| `GET/POST /api/quotes` | Legge o salva le citazioni |
| `POST /api/update/check` | Controlla subito gli aggiornamenti |

> L'interfaccia web non ha una password: chiunque sia collegato alla rete di casa può modificare le impostazioni. Usa AtmoVerse solo su reti di cui ti fidi.

---

## Architettura

L'ESP32 ha due core, usati così:

| Core | Cosa esegue |
|---|---|
| **Core 1** · loop principale | Web server e captive portal, meteo, aggiornamenti, citazioni, batteria, RTC, accesso alla SD |
| **Core 0** · task del display | Disegno e refresh del pannello e-ink (circa 4 s), a bassa priorità accanto allo stack WiFi |

Il loop prepara una "fotografia" della schermata (dati, citazione, icona già letta dalla SD) e la consegna al task del display, poi torna subito a servire la rete: durante un refresh l'interfaccia web continua a rispondere. Il task del display non accede mai alla SD né alla rete; l'unico dato condiviso è la schermata in attesa, protetta da un mutex.

## Struttura del progetto

| File | Contenuto |
|---|---|
| `AtmoVerse_2.0.ino` | Avvio, loop principale, rete e aggiornamenti periodici |
| `DisplayTask.cpp` | Task del display sul core 0 e consegna delle schermate |
| `Screens.cpp` | Schermate del display: layout, tipografia, citazione adattiva |
| `Display.cpp` | Preparazione delle schermate nel loop (dati, citazione, icona) |
| `Updater.cpp` | Aggiornamento automatico da GitHub con rollback |
| `WeatherUtils.cpp` | Meteo da OpenWeatherMap (HTTPS) |
| `QuotesManager.cpp` | Scelta delle citazioni |
| `WebServer.cpp` · `WebMinimal.cpp` | Interfaccia web, API e pagina di configurazione minima nel firmware |
| `NetworkUtils.cpp` | WiFi, modalità AP, fuso orario |
| `BatteryManager.cpp` · `RTCManager.cpp` | Batteria (INA219) e orologio (DS3231) |
| `sd_files/` | Contenuto della SD distribuito con gli aggiornamenti |
| `tools/` | Anteprima del display, manifest, citazioni a orario |

---

## Crediti

- Icone meteo: [Weather Icons](https://erikflowers.github.io/weather-icons/) di Erik Flowers (SIL OFL 1.1), convertite in bitmap per l'e-ink.
- Caratteri: FreeUniversal e Lucida Sans tramite [U8g2](https://github.com/olikraus/u8g2).
- Dati meteo: [OpenWeatherMap](https://openweathermap.org).

## Licenza

[MIT](LICENSE) © overthemax
