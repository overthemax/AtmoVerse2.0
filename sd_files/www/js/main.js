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
        <div class="header-actions">
          <a href="/info" title="Informazioni">ℹ️</a>
          <a href="/settings.html" title="Impostazioni">⚙️</a>
          <a href="/quotes.html" title="Gestione Citazioni">📝</a>
        </div>
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
        <p>Ciao</p>
        <p class="quote-attribution">Citazione per: ${weatherCondition}</p>
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
  
  // Carica una citazione
  loadQuote(weatherCondition);
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
    .then(response => response.json())
    .then(data => {
      if (data.success) {
        messageDiv.innerHTML = `<p style="color:green;">✓ ${data.message}</p>`;
        // Aggiorna la pagina dopo 2 secondi
        setTimeout(() => location.reload(), 2000);
      } else {
        messageDiv.innerHTML = `<p style="color:red;">✗ ${data.message}</p><p>${data.error || ''}</p>`;
      }
    })
    .catch(error => {
      messageDiv.innerHTML = `<p style="color:red;">Errore nella richiesta: ${error.message}</p>`;
    });
}

// Carica una citazione relativa alle condizioni meteo
function loadQuote(condition) {
  // TODO: Implementare caricamento citazioni in base alle condizioni meteo
  const quote = document.getElementById('weather-quote');
  if (quote) {
    quote.textContent = "Una citazione ispirata dal tempo di oggi..."
  }
}
