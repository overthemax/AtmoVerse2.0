// Script principale per la pagina AtmoVerse
// Stili CSS inline per i pulsanti di navigazione
const styles = `
.nav-buttons {
  display: flex;
  justify-content: flex-end;
  padding: 10px;
  background-color: rgba(255, 255, 255, 0.7);
  border-bottom: 1px solid #ddd;
  margin-bottom: 10px;
}

.nav-button {
  display: inline-block;
  font-size: 24px;
  margin-left: 10px;
  padding: 5px 10px;
  text-decoration: none;
  border-radius: 5px;
  transition: background-color 0.3s;
}

.nav-button:hover {
  background-color: rgba(0, 0, 0, 0.1);
}
`;

document.addEventListener('DOMContentLoaded', function() {
  // Aggiungi gli stili CSS alla pagina
  const styleElement = document.createElement('style');
  styleElement.textContent = styles;
  document.head.appendChild(styleElement);
  
  const mainContent = document.getElementById('main-content');
  
  // Carica i dati meteo attuali
  fetch('/api/weather')
    .then(response => response.json())
    .then(data => {
      // Costruisci il contenuto principale
      mainContent.innerHTML = buildWeatherContent(data);
      // Dopo che il DOM è stato popolato, carica la citazione corrente
      const condition = (data && data.weather && data.weather.condition) ? data.weather.condition : '';
      loadQuote(condition);
      // Avvia sincronizzazione periodica con la citazione mostrata sul display
      startQuoteSync();
    })
    .catch(error => {
      mainContent.innerHTML = `
        <div class="error-container">
          <h2>Errore nel caricamento dei dati</h2>
          <p>${error.message}</p>
          <button onclick="location.reload()">Riprova</button>
        </div>
      `;
    });
});

// Costruisce il contenuto HTML per i dati meteo
function buildWeatherContent(data) {
  const weatherCondition = data.weather.condition || 'Nuvole Sparse';
  const weatherIcon = getWeatherEmoji(weatherCondition);
  
  return `
    <div class="weather-container">
      <!-- Header con titolo e pulsanti azione -->
      <div class="header">
        <h1>AtmoVerse</h1>
      </div>
      
      <!-- Icona meteo centrale -->
      <div class="weather-icon">
        ${weatherIcon}
      </div>
      
      <!-- Condizione meteo -->
      <div class="weather-condition">
        ${weatherCondition}
      </div>
      
      <!-- Dati meteo in righe -->
      <div class="weather-data">
        <!-- Temperatura -->
        <div class="data-row">
          <div class="data-label">
            <span class="data-icon">🌡</span> Temperatura
          </div>
          <div class="data-value">
            ${data.weather.temperature || '13.0'}°C
          </div>
        </div>
        
        <!-- Umidità -->
        <div class="data-row">
          <div class="data-label">
            <span class="data-icon">💧</span> Umidità
          </div>
          <div class="data-value">
            ${data.weather.humidity || '59.0'} %
          </div>
        </div>
        
        <!-- Pressione -->
        <div class="data-row">
          <div class="data-label">
            <span class="data-icon">🔄</span> Pressione
          </div>
          <div class="data-value">
            ${data.weather.pressure || '1025'} hPa
          </div>
        </div>
        
        <!-- Località -->
        <div class="data-row">
          <div class="data-label">
            <span class="data-icon">🏙️</span> Località
          </div>
          <div class="data-value">
            ${data.location?.city || 'Naso'}, ${data.location?.country || 'IT'}
          </div>
        </div>
        
        <!-- Fuso Orario -->
        <div class="data-row">
          <div class="data-label">
            <span class="data-icon">🕒</span> Fuso Orario
          </div>
          <div class="data-value">
            UTC+1 (Roma, Parigi)
          </div>
        </div>
        
        <!-- Ultimo Aggiornamento -->
        <div class="data-row">
          <div class="data-label">
            <span class="data-icon">🔁</span> Ultimo Aggiornamento
          </div>
          <div class="data-value">
            ${data.weather.last_update || '0 secondi fa'}
          </div>
        </div>
      </div>
      
      <!-- Citazione -->
      <div class="quote-container">
        <p id="quote-text" class="quote-text">Caricamento citazione…</p>
        <p id="quote-attribution" class="quote-attribution">Citazione per: ${weatherCondition}</p>
      </div>
      
      <!-- Bottoni aggiornamento -->
      <div class="update-info">
        <div class="button-group">
          <button onclick="location.reload()" class="refresh-button">
            🔄 Aggiorna pagina
          </button>
          <button onclick="forceWeatherUpdate()" class="refresh-button force-update">
            ☁️ Forza aggiornamento meteo
          </button>
        </div>
        <div id="update-message"></div>
      </div>
    </div>
  `;
}

// Definizione di fallback per compatibilità (previene errori)
function determineWeatherIcon(condition) {
  // Questa funzione è stata sostituita da getWeatherEmoji
  // ma la definiamo comunque per evitare errori JavaScript
  return 'default';
}

// Ottiene l'emoji in base alle condizioni meteo
function getWeatherEmoji(condition) {
  condition = (condition || '').toLowerCase();
  
  if (condition.includes('pioggia') || condition.includes('pioviggine')) {
    return '🌧️';
  } else if (condition.includes('neve') || condition.includes('nevica')) {
    return '❄️';
  } else if (condition.includes('temporale') || condition.includes('fulmini')) {
    return '⛈️';
  } else if (condition.includes('nebbia') || condition.includes('foschia')) {
    return '🌫️';
  } else if (condition.includes('nuvoloso') || condition.includes('nuvole') || condition.includes('coperto')) {
    return '☁️';
  } else if (condition.includes('sereno') || condition.includes('soleggiato')) {
    return '☀️';
  } else {
    // Fallback se nessuna condizione corrisponde, usare cielo nuvoloso come default invece del punto interrogativo
    console.log('Condizione meteo non riconosciuta:', condition);
    return '☁️';
  }
}

// Forza l'aggiornamento dei dati meteo attraverso una chiamata API dedicata
function forceWeatherUpdate() {
  const messageDiv = document.getElementById('update-message');
  messageDiv.innerHTML = '<p style="color:#666;">Aggiornamento in corso...</p>';
  
  fetch('/api/force-weather-update')
    .then(async response => {
      // Prova a leggere JSON; se fallisce, usa testo grezzo
      let rawText = '';
      try {
        const text = await response.text();
        rawText = text;
        try {
          return JSON.parse(text);
        } catch {
          return {
            success: response.ok,
            message: text && text.trim() ? text.trim() : (response.ok ? 'Aggiornamento richiesto' : 'Errore sconosciuto'),
            status: response.status,
            contentType: response.headers.get('content-type') || ''
          };
        }
      } catch (e) {
        return { success: response.ok, message: 'Impossibile leggere la risposta', status: response.status };
      }
    })
    .then(data => {
      if (data.success) {
        messageDiv.innerHTML = `<p style="color:green;">✓ ${data.message}</p>`;
        // Aggiorna la pagina dopo 2 secondi
        setTimeout(() => location.reload(), 2000);
      } else {
        const details = data.error || (data.status ? `HTTP ${data.status}` : '');
        messageDiv.innerHTML = `<p style="color:red;">✗ ${data.message || 'Errore'}</p>${details ? `<p>${details}</p>` : ''}`;
      }
    })
    .catch(error => {
      messageDiv.innerHTML = `<p style="color:red;">Errore nella richiesta: ${error.message}</p>`;
    });
}

// Carica la citazione corrente dal dispositivo, con fallback alle categorie
async function loadQuote(condition) {
  const quoteTextEl = document.getElementById('quote-text');
  const quoteAttrEl = document.getElementById('quote-attribution');

  // Aggiorna attribution in base alla condizione, se disponibile
  if (quoteAttrEl && condition) {
    quoteAttrEl.textContent = `Citazione per: ${condition}`;
  }

  // 1) Prova endpoint dedicato (se presente in firmware): /api/quote/current
  try {
    const resp = await fetch('/api/quote/current');
    if (resp.ok) {
      // Prova JSON, fallback a testo semplice
      const raw = await resp.text();
      let data;
      try { data = JSON.parse(raw); } catch { data = { text: raw } }
      const text = (data && data.text) ? String(data.text).trim() : '';
      const author = (data && data.author) ? String(data.author).trim() : '';
      if (text) {
        if (quoteTextEl) quoteTextEl.textContent = text;
        if (quoteAttrEl) quoteAttrEl.textContent = author ? `— ${author}` : (quoteAttrEl.textContent || '');
        return;
      }
    }
  } catch (_) {
    // Ignora, useremo il fallback
  }

  // 2) Fallback: usa /api/quotes/all e mappa la condizione meteo in categoria firmware
  try {
    const resp = await fetch('/api/quotes/all');
    const data = await resp.json();
    const quotesMap = (data && data.quotes) ? data.quotes : {};
    const category = mapUiCategoryToFirmware(condition || 'motivazione');
    const arr = quotesMap[category] || quotesMap['motivazione'] || [];
    // Scegli in modo deterministico la prima disponibile
    const combined = arr.length > 0 ? arr[0] : '';
    if (combined) {
      // combined è del tipo "testo — autore" oppure solo "testo"
      const parts = String(combined).split(' — ');
      const text = parts[0] ? parts[0].trim() : '';
      const author = parts[1] ? parts[1].trim() : '';
      if (quoteTextEl) quoteTextEl.textContent = text || '—';
      if (quoteAttrEl) quoteAttrEl.textContent = author ? `— ${author}` : (quoteAttrEl.textContent || '');
      return;
    }
  } catch (e) {
    // ignore
  }

  // 3) Fallback finale: messaggio di default
  if (quoteTextEl) quoteTextEl.textContent = 'Ogni giorno è una nuova opportunità.';
  if (quoteAttrEl) quoteAttrEl.textContent = '— Anonimo';
}

// Mappa le categorie lato UI a quelle usate dal firmware (coerente con WebServer.cpp)
function mapUiCategoryToFirmware(uiCat) {
  const lc = String(uiCat || '').toLowerCase();
  if (lc.includes('nuvoloso') || lc.includes('nuvole') || lc.includes('cloud')) return 'nuvole';
  if (lc.includes('sereno') || lc.includes('sole') || lc.includes('clear')) return 'sole';
  if (lc.includes('pioggia') || lc.includes('pioviggine') || lc.includes('temporale') || lc.includes('thunder')) return 'pioggia';
  if (lc.includes('neve') || lc.includes('snow')) return 'neve';
  if (lc.includes('nebbia') || lc.includes('foschia') || lc.includes('mist') || lc.includes('fog')) return 'nuvole';
  if (lc.includes('vento') || lc.includes('wind')) return 'vento';
  if (lc.includes('mattina') || lc.includes('morning')) return 'mattina';
  if (lc.includes('pomeriggio') || lc.includes('afternoon')) return 'pomeriggio';
  if (lc.includes('sera') || lc.includes('evening')) return 'sera';
  return 'motivazione';
}

// =========================
// Sincronizzazione citazioni
// =========================

// Memorizza l'ultima citazione mostrata per evitare aggiornamenti inutili
let __lastQuote = { text: '', author: '' };

function startQuoteSync() {
  // Aggiorna subito
  refreshQuoteFromDevice();
  // Aggiorna ogni 30 secondi
  setInterval(refreshQuoteFromDevice, 30000);
  // Aggiorna quando la tab torna visibile
  document.addEventListener('visibilitychange', () => {
    if (!document.hidden) refreshQuoteFromDevice();
  });
}

async function refreshQuoteFromDevice() {
  const quoteTextEl = document.getElementById('quote-text');
  const quoteAttrEl = document.getElementById('quote-attribution');
  if (!quoteTextEl || !quoteAttrEl) return;

  try {
    const resp = await fetch('/api/quote/current', { cache: 'no-store' });
    if (!resp.ok) return;
    const data = await resp.json();
    const text = (data && data.text) ? String(data.text).trim() : '';
    const author = (data && data.author) ? String(data.author).trim() : '';
    if (!text) return;
    if (text !== __lastQuote.text || author !== __lastQuote.author) {
      quoteTextEl.textContent = text;
      quoteAttrEl.textContent = author ? `— ${author}` : '';
      __lastQuote = { text, author };
    }
  } catch (e) {
    // silenzioso
  }
}
