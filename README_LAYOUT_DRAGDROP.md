# AtmoVerse 2.0 - Sistema Layout Drag & Drop

## Panoramica

AtmoVerse 2.0 utilizza un sistema di layout completamente personalizzabile basato su **drag-and-drop** tramite file JSON. Non ci sono più temi predefiniti: tutto il display è configurabile posizionando elementi su una griglia.

## File di Configurazione

Il file `/layout.json` sulla SD card definisce l'intero layout del display.

### Struttura del File

```json
{
  "grid": {
    "cols": 24,           // Numero di colonne della griglia
    "row_height": 20,     // Altezza di ogni riga in pixel
    "margin": 4           // Margine tra le celle in pixel
  },
  "items": [
    // Array di elementi da visualizzare
  ]
}
```

## Griglia Display

Il display e-ink è **648x480 pixel**. La griglia divide lo spazio in:
- **24 colonne** (default)
- **Altezza variabile** per riga (20px default)
- **Margini** tra le celle (4px default)

### Calcolo Posizioni

Ogni elemento è posizionato con coordinate di griglia (x, y, w, h):
- **x**: colonna di partenza (0-23)
- **y**: riga di partenza (0-...)
- **w**: larghezza in colonne
- **h**: altezza in righe

**Esempio:** Un elemento a (x:8, y:1, w:8, h:7) occupa:
- Colonne dalla 8 alla 15 (8 colonne)
- Righe dalla 1 alla 7 (7 righe di altezza)

## Tipi di Elementi

### 1. `city` - Nome Città

Mostra il nome della città configurata.

```json
{
  "id": "city_label",
  "type": "city",
  "x": 0,
  "y": 0,
  "w": 8,
  "h": 2,
  "z": 1,
  "align": "left",      // "left", "center", "right"
  "font_size": 16,      // Dimensione font suggerita
  "visible": true
}
```

**Font automatico in base a font_size:**
- ≥20: FreeSerif12pt7b
- ≥12: FreeSansBold12pt7b  
- <12: FreeSerif9pt7b

### 2. `weather_icon` - Icona Meteo

Mostra l'icona meteo animata (140x140px fisso).

```json
{
  "id": "weather_icon_main",
  "type": "weather_icon",
  "x": 8,
  "y": 1,
  "w": 8,
  "h": 7,
  "z": 1,
  "visible": true
}
```

**Nota:** L'icona è sempre 140x140px e viene centrata nell'area assegnata.

### 3. `quote` - Citazione

Mostra una citazione motivazionale.

```json
{
  "id": "quote_box",
  "type": "quote",
  "x": 1,
  "y": 9,
  "w": 22,
  "h": 4,
  "z": 1,
  "visible": true
}
```

La citazione si adatta automaticamente allo spazio disponibile con word-wrap.

### 4. `footer_bar` - Barra Informazioni

Mostra ultimo aggiornamento, IP e batteria.

```json
{
  "id": "footer",
  "type": "footer_bar",
  "x": 0,
  "y": 13,
  "w": 24,
  "h": 2,
  "z": 1,
  "visible": true
}
```

**Include automaticamente:**
- Ultimo aggiornamento (sinistra)
- IP address (centro)
- Stato batteria (destra)

## Parametri Comuni

### Proprietà Obbligatorie

- **id**: Identificatore univoco dell'elemento
- **type**: Tipo di elemento (city, weather_icon, quote, footer_bar)
- **x, y**: Posizione nella griglia
- **w, h**: Dimensioni in celle di griglia
- **z**: Ordine di disegno (elementi con z più alto vengono disegnati sopra)
- **visible**: true/false per mostrare/nascondere

### Proprietà Opzionali

- **align**: Allineamento testo ("left", "center", "right") - solo per type:city
- **font_size**: Suggerimento dimensione font - solo per type:city

## Esempi di Layout

### Layout Minimale

Layout semplice con icona grande e citazione.

```json
{
  "grid": {"cols": 24, "row_height": 20, "margin": 4},
  "items": [
    {
      "id": "city",
      "type": "city",
      "x": 0, "y": 0, "w": 24, "h": 2,
      "z": 1, "align": "center", "font_size": 18,
      "visible": true
    },
    {
      "id": "icon",
      "type": "weather_icon",
      "x": 8, "y": 2, "w": 8, "h": 7,
      "z": 1, "visible": true
    },
    {
      "id": "quote",
      "type": "quote",
      "x": 2, "y": 10, "w": 20, "h": 3,
      "z": 1, "visible": true
    },
    {
      "id": "footer",
      "type": "footer_bar",
      "x": 0, "y": 13, "w": 24, "h": 2,
      "z": 1, "visible": true
    }
  ]
}
```

### Layout Complesso

Layout con multipli elementi sovrapposti.

```json
{
  "grid": {"cols": 24, "row_height": 20, "margin": 2},
  "items": [
    {
      "id": "city_top_left",
      "type": "city",
      "x": 0, "y": 0, "w": 10, "h": 2,
      "z": 1, "align": "left", "font_size": 14,
      "visible": true
    },
    {
      "id": "weather_left",
      "type": "weather_icon",
      "x": 2, "y": 3, "w": 8, "h": 7,
      "z": 2, "visible": true
    },
    {
      "id": "weather_right",
      "type": "weather_icon",
      "x": 14, "y": 3, "w": 8, "h": 7,
      "z": 2, "visible": true
    },
    {
      "id": "quote_center",
      "type": "quote",
      "x": 4, "y": 11, "w": 16, "h": 3,
      "z": 3, "visible": true
    },
    {
      "id": "footer_full",
      "type": "footer_bar",
      "x": 0, "y": 14, "w": 24, "h": 1,
      "z": 1, "visible": true
    }
  ]
}
```

## Web Interface Drag & Drop

L'interfaccia web permette di:
1. **Visualizzare** l'anteprima del layout corrente
2. **Trascinare** gli elementi per riposizionarli
3. **Ridimensionare** gli elementi usando i bordi
4. **Aggiungere** nuovi elementi dalla palette
5. **Eliminare** elementi non desiderati
6. **Salvare** il layout su `/layout.json`

### Workflow

1. Accedi all'interfaccia web su `http://IP_DEL_DISPOSITIVO/layout-editor.html`
2. Trascina gli elementi dalla palette sulla griglia
3. Ridimensiona e posiziona a piacimento
4. Clicca "Salva Layout" per scrivere su SD
5. Il display si aggiornerà automaticamente

## Layout di Default

Se `/layout.json` non esiste sulla SD, viene utilizzato un layout di default hardcoded che include:
- Città in alto a sinistra
- Ora sotto la città
- Temperatura e dati meteo
- Icona meteo grande centrata
- Calendario in alto a destra
- Citazione nella parte bassa
- Footer con ultimo aggiornamento, IP e batteria

## File sulla SD

### Struttura Consigliata

```
/
├── conf.json          # Configurazione WiFi e parametri
├── layout.json        # Layout display personalizzato
├── quotes.json        # Database citazioni
└── icons/             # Icone meteo BMP 1-bit
    ├── wi-cloudy.bmp
    ├── wi-day-sunny.bmp
    └── ...
```

## Tips & Tricks

### Sovrapposizione Elementi

Usa il parametro `z` per controllare l'ordine di disegno:
- `z: 1` - Elementi di sfondo
- `z: 2` - Elementi medi
- `z: 3` - Elementi in primo piano

### Spazi Vuoti

Lascia celle vuote per creare respiro visivo.

### Margini Personalizzati

Riduci `margin` nella griglia per layout più compatti:
```json
"grid": {"cols": 24, "row_height": 20, "margin": 2}
```

### Nascondere Elementi

Imposta `"visible": false` per nascondere temporaneamente un elemento senza cancellarlo.

### Debug Layout

Decommentare questa riga in Display.cpp per vedere i bordi delle celle:
```cpp
// display.drawRect(r.x, r.y, r.w, r.h, GxEPD_BLACK);
```

## Troubleshooting

**Il layout non viene caricato:**
- Verifica che `/layout.json` esista sulla SD
- Controlla la sintassi JSON (usa un validatore online)
- Controlla i log seriali per errori di parsing

**Gli elementi si sovrappongono in modo strano:**
- Verifica i valori di z-index
- Controlla che le coordinate non escano dalla griglia

**L'icona meteo non appare:**
- Assicurati che l'area assegnata sia almeno 7x7 righe
- Verifica che le icone BMP 1-bit siano in `/icons/`

**Il testo è tagliato:**
- Aumenta le dimensioni w e h dell'elemento
- Riduci font_size se troppo grande

## Limitazioni

- Massimo **30 elementi** per layout (limite memoria)
- Icona meteo sempre **140x140px** (non scalabile)
- Footer bar ha layout fisso interno
- Display e-ink: solo bianco e nero

## API REST per Layout

### GET /api/layout
Restituisce il layout corrente da `/layout.json`

### POST /api/layout
Salva un nuovo layout su `/layout.json`

**Body:**
```json
{
  "grid": {...},
  "items": [...]
}
```

## Prossimi Sviluppi

Funzionalità pianificate per versioni future:
- [ ] Più tipi di elementi (temperatura, umidità, vento separati)
- [ ] Supporto immagini custom
- [ ] Template predefiniti scaricabili
- [ ] Anteprima real-time nell'editor web
- [ ] Import/export layout tra dispositivi

---

## Risorse

- **File di esempio**: `sd_files/layout.json`
- **Documentazione API**: Vedi `README_API.md`
- **Community layouts**: GitHub repository

**Divertiti a creare il tuo layout personalizzato! 🎨**
