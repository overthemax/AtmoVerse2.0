# File da Copiare sulla SD Card

## Struttura Completa

Questa cartella contiene tutti i file da copiare sulla SD card dell'AtmoVerse 2.0.

```
SD CARD (F:/)
├── conf.json              # Configurazione WiFi e parametri (creato automaticamente)
├── layout.json            # Layout display personalizzato
├── quotes.json            # Database citazioni (opzionale)
├── icons/                 # Icone meteo BMP 1-bit
│   ├── wi-cloudy.bmp
│   ├── wi-day-sunny.bmp
│   └── ... (219 icone)
└── www/                   # Interfaccia web
    ├── index.html         # Dashboard principale
    ├── settings.html      # Configurazione completa
    ├── layout-editor.html # Editor layout drag-and-drop
    ├── quotes-editor.html # Editor citazioni
    └── style.css          # Stili CSS
```

## File Presenti

### ✅ layout.json
Layout di esempio per il display con griglia 24x20.
Elementi: città, icona meteo, citazione, footer bar.

### ✅ quotes.json
Database di 20 citazioni motivazionali categorizzate.
Include citazioni per diverse condizioni meteo e momenti della giornata.

### ✅ www/index.html
Dashboard principale con:
- Visualizzazione meteo in tempo reale
- Stato sistema (WiFi, città, risparmio energetico)
- Link rapidi a impostazioni e editor layout

### ✅ www/settings.html
Interfaccia completa per configurare:
- 📡 WiFi (SSID, Password, scansione reti)
- 🌤️ Città e API key OpenWeatherMap
- 🕐 Fuso orario e formato orario
- 🔄 Intervalli di aggiornamento
- 🔋 Risparmio energetico (orari personalizzabili)
- 🔌 Battery monitor (pin ADC, voltage divider)
- 💬 Posizione citazione (coordinate pixel)

### ✅ www/layout-editor.html
Editor drag-and-drop completo per personalizzare il display:
- 🎨 Canvas visuale 648x480px con griglia
- 📦 Palette elementi trascinabili (città, icona meteo, citazione, footer)
- 🔧 Pannello proprietà per ogni elemento
- 📐 Controlli griglia (colonne, altezza riga, margini)
- 🖱️ Drag & drop + ridimensionamento live
- ⌨️ Keyboard shortcuts (frecce, Delete, Ctrl+S)
- 💾 Salvataggio diretto su /layout.json

### ✅ www/quotes-editor.html
Editor completo per gestire le citazioni:
- 📊 Statistiche (totale citazioni, categorie)
- 🔍 Filtri per categoria e ricerca testuale
- ➕ Aggiungi nuove citazioni
- ✏️ Modifica citazioni esistenti
- 🗑️ Elimina citazioni singole o tutte
- 📝 Modal di editing intuitivo
- 💾 Salvataggio diretto su /quotes.json
- 🏷️ Gestione categorie (meteo, momenti giornata, motivazione)

### ✅ www/style.css
Stili CSS completi per tutte le pagine:
- Design moderno e responsivo
- Supporto editor drag-and-drop
- Tema consistente con variabili CSS
- Animazioni e transizioni fluide

## File Non Inclusi

### Icons BMP 1-bit
Le icone meteo devono essere copiate dalla cartella `icons_bmp_1bit/` del progetto:

```bash
Sorgente: C:\Users\Marco\OneDrive\Desktop\AtmoVerse_2.0\icons_bmp_1bit\
Destinazione: F:\icons\
```

**IMPORTANTE:** Usare SOLO le icone da `icons_bmp_1bit/` (1-bit, ~1.6KB)
NON usare quelle da `icons_bmp/` (24-bit, ~30KB) - non funzionano!

## Come Copiare i File

### Windows

1. Inserisci la SD card nel lettore
2. Verifica la lettera del drive (es. F:)
3. Copia tutti i file:
   ```powershell
   # Crea le cartelle
   New-Item -ItemType Directory -Path "F:\www" -Force
   New-Item -ItemType Directory -Path "F:\icons" -Force
   
   # Copia i file
   Copy-Item "sd_files\layout.json" "F:\layout.json"
   Copy-Item "sd_files\www\*" "F:\www\" -Recurse
   
   # Copia le icone
   Copy-Item "icons_bmp_1bit\*" "F:\icons\" -Recurse
   ```

### Manuale

1. Apri Esplora File
2. Naviga in `sd_files/`
3. Copia `layout.json` nella root della SD
4. Copia la cartella `www/` nella root della SD
5. Copia tutte le icone da `icons_bmp_1bit/` a `icons/` sulla SD

## Verifica

Dopo aver copiato i file, la SD dovrebbe contenere:

```
F:\
├── conf.json            ⚠️  (creato automaticamente dall'ESP32)
├── layout.json          ✅ (820 bytes)
├── quotes.json          ✅ (1.2 KB, 20 citazioni)
├── icons\               ✅ (219 file, ~350 KB totali)
│   ├── wi-cloudy.bmp
│   ├── wi-day-sunny.bmp
│   └── ... (altre 217 icone)
└── www\                 ✅ COMPLETO
    ├── index.html         ✅ Dashboard (2.8 KB)
    ├── settings.html      ✅ Configurazione (14.5 KB)
    ├── layout-editor.html ✅ Editor layout (14.2 KB)
    ├── quotes-editor.html ✅ Editor citazioni (12.8 KB)
    └── style.css          ✅ Stili (7.1 KB)
```

## File che Verranno Creati Automaticamente

### conf.json
Verrà creato automaticamente dall'ESP32 al primo avvio se non presente.
Contiene la configurazione WiFi e tutti i parametri di sistema.

## Note

- **Formato SD:** FAT32 (obbligatorio)
- **Capacità SD:** 1-32 GB consigliata
- **Spazio richiesto:** ~2 MB (con icone)
- **File conf.json:** NON sovrascrivere se già presente (contiene la tua configurazione)

## Troubleshooting

**SD non riconosciuta:**
- Verifica che sia formattata in FAT32
- Prova a riformattare con [SD Card Formatter](https://www.sdcard.org/downloads/formatter/)

**Interfaccia web non si carica:**
- Verifica che i file siano in `/www/` (non `/SD/www/`)
- Controlla che i file HTML abbiano l'estensione corretta
- Verifica i log seriali per errori

**Layout non applicato:**
- Controlla che `/layout.json` sia valido (usa un validatore JSON online)
- Verifica i log seriali per errori di parsing

**Icone meteo non visualizzate:**
- DEVI usare le icone da `icons_bmp_1bit/` (1-bit)
- NON usare quelle da `icons_bmp/` (24-bit non supportate)
- Verifica che siano in `/icons/` sulla SD

## Prossimi Passi

1. ✅ Tutti i file sono pronti in `sd_files/`
2. 📋 Copia i file sulla SD:
   ```powershell
   # Esempio Windows PowerShell
   Copy-Item "sd_files\layout.json" "F:\"
   Copy-Item "sd_files\quotes.json" "F:\"
   Copy-Item "sd_files\www" "F:\" -Recurse
   Copy-Item "icons_bmp_1bit\*" "F:\icons\" -Recurse
   ```
3. 🔌 Inserisci la SD nell'ESP32
4. 🚀 Compila e carica il firmware aggiornato
5. 🌐 Accedi all'interfaccia web:
   - Dashboard: `http://IP_DEL_DISPOSITIVO/`
   - Impostazioni: `http://IP_DEL_DISPOSITIVO/settings.html`
   - Layout Editor: `http://IP_DEL_DISPOSITIVO/layout-editor.html`
   - Editor Citazioni: `http://IP_DEL_DISPOSITIVO/quotes-editor.html`

---

## ✨ Caratteristiche Interfacce

### 🏠 Dashboard (index.html)
- Visualizzazione meteo real-time
- Stato sistema (WiFi, città, power saving)
- Auto-refresh ogni 60 secondi

### ⚙️ Impostazioni (settings.html)
- Configurazione WiFi con scansione reti
- Setup OpenWeatherMap API
- Gestione risparmio energetico
- Battery monitor avanzato
- Salvataggio con riavvio automatico

### 🎨 Layout Editor (layout-editor.html)
- Canvas 648x480px drag-and-drop
- 4 tipi di elementi (città, icona, citazione, footer)
- Ridimensionamento live
- Proprietà personalizzabili per elemento
- Griglia configurabile
- Keyboard shortcuts (frecce, Delete, Ctrl+S)
- Salvataggio diretto su SD

### 💬 Editor Citazioni (quotes-editor.html)
- Gestione completa database citazioni
- Statistiche in tempo reale
- Filtri per categoria e ricerca testuale
- Aggiungi/Modifica/Elimina citazioni
- 14 categorie predefinite (meteo, momenti, motivazione)
- Modal di editing intuitivo
- Salvataggio su /quotes.json

**Tutto pronto per essere copiato sulla SD! 🚀**
