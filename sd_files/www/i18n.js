/*
 * AtmoVerse - lingua delle pagine web.
 * Le pagine sono scritte in italiano. Se nelle impostazioni la lingua non è
 * "it", questo script le traduce in inglese: testi, segnaposto, titoli e
 * anche i contenuti aggiunti dopo (elenchi, messaggi, finestre di conferma).
 */
(function () {
  const EN = {
    // Navigazione e pagina principale
    'Home': 'Home', 'Impostazioni': 'Settings', 'Citazioni': 'Quotes', 'Meteo': 'Weather',
    'Citazione sul display': 'Quote on the display', 'Dispositivo': 'Device',
    'Umidità': 'Humidity', 'Vento': 'Wind', 'Pressione': 'Pressure',
    'Dati meteo non ancora disponibili.': 'Weather data not available yet.',
    'Impossibile leggere i dati meteo.': 'Cannot read the weather data.',
    'Impossibile leggere lo stato del dispositivo.': 'Cannot read the device status.',
    'Rete WiFi': 'WiFi network', 'Modalità configurazione': 'Setup mode', 'Non configurata': 'Not configured',
    'Città': 'City', 'Aggiornamento meteo': 'Weather update', 'Aggiornamenti': 'Updates',
    'Batteria': 'Battery', 'Stato': 'Status',
    // Impostazioni
    'Impostazioni - AtmoVerse 2.0': 'Settings - AtmoVerse 2.0',
    'Configurazione WiFi': 'WiFi setup', 'SSID WiFi *': 'WiFi SSID *', 'Password WiFi *': 'WiFi password *',
    'Nome della rete WiFi': 'WiFi network name', 'Scansiona Reti': 'Scan networks', 'Reti Disponibili:': 'Available networks:',
    '⚠️ Lascia vuoto per mantenere la password attuale': '⚠️ Leave empty to keep the current password',
    'Lascia vuoto per non modificare': 'Leave empty to keep it unchanged',
    'Configurazione Meteo': 'Weather setup', 'Città *': 'City *', 'Es: Rome, Milan, Naples': 'E.g. Rome, London, Paris',
    'API Key OpenWeatherMap *': 'OpenWeatherMap API key *', 'Inserisci la tua API key': 'Enter your API key',
    'Ottieni una chiave gratuita su': 'Get a free key at', 'Unità di Misura': 'Units',
    'Metrico (°C, km/h)': 'Metric (°C, km/h)', 'Imperiale (°F, mph)': 'Imperial (°F, mph)',
    'Lingua': 'Language', 'Configurazione Ora': 'Time setup', 'Fuso Orario (GMT offset)': 'Time zone (GMT offset)',
    'GMT+1 (Italia)': 'GMT+1 (Italy)', 'Formato Orario': 'Time format', '24 ore (14:30)': '24 hours (14:30)',
    '12 ore (2:30 PM)': '12 hours (2:30 PM)', 'Risparmio Energetico': 'Power saving',
    'Abilita Risparmio Energetico': 'Enable power saving', 'Ora Inizio (0-23)': 'Start hour (0-23)',
    'Ora Fine (0-23)': 'End hour (0-23)', 'Intervalli Aggiornamento': 'Update intervals',
    'Intervallo Normale (minuti)': 'Normal interval (minutes)', 'Intervallo Risparmio (minuti)': 'Power saving interval (minutes)',
    'Refresh Display (secondi)': 'Display refresh (seconds)', 'Tentativi Massimi Rete': 'Max network retries',
    'Mostra la batteria sul display': 'Show the battery on the display',
    'Salva Configurazione': 'Save settings', 'Riavvia': 'Restart', 'lettura in corso...': 'reading...',
    'Impostazioni caricate': 'Settings loaded',
    '✅ Configurazione salvata! Il dispositivo si riavvierà...': '✅ Settings saved! The device will restart...',
    '✅ Dispositivo in riavvio...': '✅ Device restarting...', '🔄 Riavvio in corso...': '🔄 Restarting...',
    '⚠️ Riavvio iniziato (connessione persa)': '⚠️ Restart started (connection lost)',
    '🔍 Scansione reti WiFi in corso...': '🔍 Scanning WiFi networks...',
    'Riavviare il dispositivo?': 'Restart the device?',
    'Controlla aggiornamenti': 'Check for updates',
    "🔍 Controllo aggiornamenti avviato: il display mostra l'avanzamento se c'è una nuova versione":
      '🔍 Update check started: the display shows the progress if there is a new version',
    // Editor delle citazioni
    'Editor Citazioni': 'Quote editor', 'Editor Citazioni - AtmoVerse 2.0': 'Quote editor - AtmoVerse 2.0',
    'Salva': 'Save', 'Nuova Citazione': 'New quote', 'Meteo e programmate': 'Weather and scheduled',
    'Orologio letterario': 'Literary clock', 'Statistiche': 'Statistics', 'Totale citazioni:': 'Total quotes:',
    'Categorie:': 'Categories:', 'Filtra Citazioni': 'Filter quotes', 'Categoria': 'Category',
    'Tutte le categorie': 'All categories', 'Cerca nel testo': 'Search text', 'Cerca...': 'Search...',
    'Elimina Tutte': 'Delete all', 'Modifica': 'Edit', 'Elimina': 'Delete', 'Nessuna citazione': 'No quotes',
    'Inizia aggiungendo la tua prima citazione!': 'Start by adding your first quote!',
    'Aggiungi Citazione': 'Add quote', 'Modifica Citazione': 'Edit quote', 'Testo Citazione *': 'Quote text *',
    'Grassetto': 'Bold', 'Corsivo': 'Italic', 'A capo': 'New line',
    'Usa **grassetto**, *corsivo*, \\n per andare a capo': 'Use **bold**, *italic*, \\n for a new line',
    'Anteprima Live': 'Live preview', 'La tua citazione apparirà qui...': 'Your quote will appear here...',
    'Autore *': 'Author *', 'Autore': 'Author', 'Note Autore': 'Author notes', 'es: filosofo, scrittore': 'e.g. philosopher, writer',
    'es: motivazione, pioggia, mattina': 'e.g. motivazione, pioggia, mattina',
    'Categorie suggerite: motivazione, pioggia, pioggia_leggera, cielo_sereno, poche_nuvole, nuvole_sparse, nuvole_abbondanti, temporale, neve, nebbia, vento, mattina, pomeriggio, sera':
      'Suggested categories (internal names): motivazione, pioggia, pioggia_leggera, cielo_sereno, poche_nuvole, nuvole_sparse, nuvole_abbondanti, temporale, neve, nebbia, vento, mattina, pomeriggio, sera',
    'Programmazione (facoltativa)': 'Schedule (optional)',
    'Compila ora e/o data per mostrare la citazione in un momento preciso: ha la precedenza su quelle del meteo. Con la sola data vale tutto il giorno.':
      'Fill in time and/or date to show the quote at a precise moment: it takes priority over weather quotes. With only a date it lasts all day.',
    'Ora di inizio': 'Start time', 'Durata (minuti)': 'Duration (minutes)', 'Data': 'Date',
    '12-25 (ogni anno) oppure 2026-12-25 (una volta)': '12-25 (every year) or 2026-12-25 (once)',
    'Giorni (nessuno = tutti)': 'Days (none = all)',
    'Dom': 'Sun', 'Lun': 'Mon', 'Mar': 'Tue', 'Mer': 'Wed', 'Gio': 'Thu', 'Ven': 'Fri', 'Sab': 'Sat',
    'Dimensione Font': 'Font size', 'Piccolo': 'Small', 'Medio': 'Medium', 'Grande': 'Large', 'Normale': 'Normal',
    'Allineamento': 'Alignment', 'Sinistra': 'Left', 'Centro': 'Center', 'Destra': 'Right',
    'Stile Autore': 'Author style', 'Autore sulla stessa riga': 'Author on the same line', 'Annulla': 'Cancel',
    'Applica': 'Apply',
    'Citazioni che citano l\'orario: hanno la precedenza su quelle del meteo e compaiono nel minuto indicato. Si modifica un\'ora alla volta; le citazioni restano solo sulla SD.':
      'Quotes that mention the time: they take priority over weather quotes and appear at their minute. You edit one hour at a time; the quotes stay on the SD card only.',
    'Citazioni:': 'Quotes:', 'Minuti coperti:': 'Minutes covered:', 'su 1440': 'of 1440', 'Ore': 'Hours',
    'Modifiche non salvate': 'Unsaved changes', 'Cerca nel testo o nell\'autore': 'Search text or author',
    'Mostra anche i minuti senza citazioni': 'Also show minutes without quotes',
    'Solo quelle troppo lunghe per il display': 'Only those too long for the display',
    'Minuto': 'Minute', 'Testo': 'Text', 'Opera': 'Work', 'Citazione delle': 'Quote for',
    '"Applica" aggiorna l\'elenco; per scrivere sulla SD premi poi "Salva".': '"Apply" updates the list; press "Save" to write to the SD card.',
    '+ Aggiungi': '+ Add', 'Nessuna citazione: si usa quella del meteo': 'No quote: the weather quote is used',
    'Troppo lunga: non viene mostrata': 'Too long: not shown', 'Da salvare': 'Not saved yet',
    'Nessuna citazione corrisponde ai filtri.': 'No quote matches the filters.',
    'Modifica citazione': 'Edit quote', 'Servono almeno testo e autore.': 'Text and author are required.',
    'Eliminare questa citazione?': 'Delete this quote?',
    '⚠️ ATTENZIONE: Eliminare TUTTE le citazioni? Questa azione non può essere annullata!': '⚠️ WARNING: delete ALL quotes? This cannot be undone!',
    '✅ Citazione aggiunta': '✅ Quote added', '✅ Citazione eliminata': '✅ Quote deleted', '✅ Citazione modificata': '✅ Quote updated',
    '✅ Tutte le citazioni sono state eliminate': '✅ All quotes deleted',
    '⚠️ Nessuna citazione da salvare': '⚠️ No quotes to save',
    '⚠️ Nessun file quotes.json trovato. Inizia creando nuove citazioni.': '⚠️ No quotes.json found. Start by creating new quotes.',
    '⚠️ Impossibile caricare le citazioni. File non presente sulla SD.': '⚠️ Cannot load the quotes. File not on the SD card.',
    '❌ Errore durante il salvataggio': '❌ Error while saving', '❌ Testo e autore sono obbligatori': '❌ Text and author are required',
    '❌ Data non valida: usa 12-25 oppure 2026-12-25': '❌ Invalid date: use 12-25 or 2026-12-25',
  };

  // Messaggi composti (con numeri o dettagli dell'errore)
  const RULES = [
    [/^✅ Citazioni caricate: (\d+)$/, '✅ Quotes loaded: $1'],
    [/^✅ Salvate (\d+) citazioni su \/quotes\.json$/, '✅ Saved $1 quotes to /quotes.json'],
    [/^✅ Trovate (\d+) reti$/, '✅ Found $1 networks'],
    [/^✅ Ore (\d+) salvate: (\d+) citazioni(, (\d+) troppo lunghe per il display)?$/,
      (m, h, n, x, l) => `✅ Hour ${h} saved: ${n} quotes` + (l ? `, ${l} too long for the display` : '')],
    [/^Nuova citazione delle (\d+)$/, 'New quote for $1:00'],
    [/^Sei assolutamente sicuro\? Verranno eliminate (\d+) citazioni!$/, 'Are you absolutely sure? $1 quotes will be deleted!'],
    [/^Le modifiche alle ore (\d+) non sono salvate\. Continuare e perderle\?$/, 'Changes to hour $1 are not saved. Continue and lose them?'],
    [/^ogni (\d+) min$/, 'every $1 min'],
    // Stato degli aggiornamenti (dal firmware)
    [/^Nessun controllo eseguito$/, 'No check yet'],
    [/^Aggiornato \(versione (.*)\)$/, 'Up to date (version $1)'],
    [/^Server degli aggiornamenti non raggiungibile$/, 'Update server unreachable'],
    [/^Manifest non valido$/, 'Invalid manifest'],
    [/^Download non riuscito: (.*)$/, 'Download failed: $1'],
    [/^Installata la versione (.*), riavvio$/, 'Version $1 installed, restarting'],
    [/^File della SD aggiornati$/, 'SD card files updated'],
    [/^Firmware (.*) disponibile: si installa con la batteria sopra il (\d+)% o in carica$/,
      'Firmware $1 available: it installs with the battery above $2% or while charging'],
    [/^La versione (.*) non si è avviata: ripristinata la (.*)$/, 'Version $1 did not start: restored $2'],
    [/^Aggiornato (.*)$/, 'Updated $1'],
    [/^(\d+) caratteri( — troppo lunga, il display non la mostrerà| — lunga: verrà rimpicciolita)?$/,
      (m, n, x) => `${n} characters` + (x ? (x.includes('troppo') ? ' — too long, the display will not show it' : ' — long: it will be shrunk') : '')],
    [/^(\d+) citazioni, (\d+) minuti su 60$/, '$1 quotes, $2 minutes of 60'],
    [/^Errore caricamento impostazioni: (.*)$/, 'Error loading settings: $1'],
    [/^❌ Errore salvataggio: (.*)$/, '❌ Save error: $1'],
    [/^❌ Errore scansione: (.*)$/, '❌ Scan error: $1'],
    [/^❌ Salvataggio non riuscito: (.*)$/, '❌ Save failed: $1'],
    [/^❌ Impossibile leggere le citazioni delle (\d+): (.*)$/, '❌ Cannot read the quotes for $1:00: $2'],
    [/^❌ Impossibile leggere l'orologio letterario: (.*)$/, '❌ Cannot read the literary clock: $1'],
    [/^❌ Errore: (.*)$/, '❌ Error: $1'],
  ];

  function tr(s) {
    if (typeof s !== 'string') return s;
    const key = s.trim();
    if (!key) return s;
    let out = null;
    if (Object.prototype.hasOwnProperty.call(EN, key)) out = EN[key];
    else {
      for (const [re, rep] of RULES) {
        if (re.test(key)) { out = key.replace(re, rep); break; }
      }
    }
    if (out === null) return s;
    // Mantiene gli spazi attorno al testo originale
    return s.slice(0, s.indexOf(key)) + out + s.slice(s.indexOf(key) + key.length);
  }

  const ATTRS = ['placeholder', 'title', 'aria-label'];
  function translate(root) {
    if (!root) return;
    if (root.nodeType === Node.TEXT_NODE) {
      const t = tr(root.nodeValue);
      if (t !== root.nodeValue) root.nodeValue = t;
      return;
    }
    if (root.nodeType !== Node.ELEMENT_NODE || root.tagName === 'SCRIPT' || root.tagName === 'STYLE') return;
    for (const a of ATTRS) {
      const v = root.getAttribute(a);
      if (v) { const t = tr(v); if (t !== v) root.setAttribute(a, t); }
    }
    for (const c of Array.from(root.childNodes)) translate(c);
  }

  window.t = (s) => (window.AV_LANG_EN ? tr(s) : s);

  async function init() {
    try {
      const r = await fetch('/api/settings', { cache: 'no-store' });
      const d = await r.json();
      window.AV_LANG_EN = !!d.language && d.language !== 'it';
    } catch (e) {
      return;
    }
    if (!window.AV_LANG_EN) return;
    document.documentElement.lang = 'en';
    document.title = tr(document.title);
    translate(document.body);
    new MutationObserver((list) => {
      for (const m of list) {
        if (m.type === 'characterData') translate(m.target);
        else m.addedNodes.forEach(translate);
      }
    }).observe(document.body, { childList: true, subtree: true, characterData: true });
    const origConfirm = window.confirm.bind(window);
    const origAlert = window.alert.bind(window);
    window.confirm = (msg) => origConfirm(tr(String(msg)));
    window.alert = (msg) => origAlert(tr(String(msg)));
  }

  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', init);
  else init();
})();
