#ifndef WEBMINIMAL_H
#define WEBMINIMAL_H

#include <Arduino.h>
#include "WeatherUtils.h"

// Genera la pagina principale con stile minimalista
String generateMinimalMainPage();

// Genera una versione minimalista della pagina delle citazioni
String generateMinimalQuotesPage();

// Genera una pagina informativa con dettagli tecnici del sistema
String generateMinimalInfoPage();

// Genera la pagina di setup minimale (prototipo mancante)
String generateMinimalSetupPage(String ssid);

// Pagina di configurazione statica per modalità AP
// Questa pagina viene servita quando l'utente si connette in modalità Access Point
const char setupPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="it">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>AtmoVerse 2.0 - Configurazione</title>
  <link rel="stylesheet" href="/css/style.css">
</head>
<body>
  <div class="container">
    <h1>AtmoVerse 2.0</h1>
    <h2>Configurazione WiFi</h2>
    
    <form id="configForm">
      <div class="card">
        <h3>Rete WiFi</h3>
        <div class="form-group">
          <label for="ssid">Nome rete (SSID):</label>
          <div class="input-group">
            <input type="text" id="ssid" name="ssid" required>
            <button type="button" id="scanBtn" class="btn-small">Scan</button>
          </div>
        </div>
        <div class="form-group">
          <label for="password">Password:</label>
          <input type="password" id="password" name="password">
        </div>
      </div>

      <div class="card">
        <h3>Dati Meteo</h3>
        <div class="form-group">
          <label for="city">Città:</label>
          <input type="text" id="city" name="city" placeholder="Es: Roma,it" required>
        </div>
        <div class="form-group">
          <label for="api_key">OpenWeatherMap API Key:</label>
          <input type="text" id="api_key" name="api_key" placeholder="Inserisci la tua API key" required>
        </div>
      </div>

      <div class="btn-group">
        <button type="submit" class="btn primary">Salva configurazione</button>
      </div>
    </form>

    <div id="networksModal" class="modal">
      <div class="modal-content">
        <span class="close">&times;</span>
        <h3>Reti disponibili</h3>
        <ul id="networksList"></ul>
      </div>
    </div>

    <div id="statusMessage" class="hidden"></div>
  </div>

  <script src="/js/setup.js"></script>
</body>
</html>
)rawliteral";

// CSS minimale per la pagina di configurazione
const char basicCSS[] PROGMEM = R"rawliteral(
body {
  font-family: Arial, sans-serif;
  margin: 0;
  padding: 0;
  background-color: #f0f2f5;
  color: #333;
}

.container {
  max-width: 800px;
  margin: 0 auto;
  padding: 20px;
}

h1, h2, h3 {
  color: #2c3e50;
  text-align: center;
}

h1 {
  margin-bottom: 5px;
}

h2 {
  margin-top: 0;
  margin-bottom: 20px;
  font-size: 1.2em;
  color: #7f8c8d;
}

.card {
  background: white;
  border-radius: 5px;
  padding: 20px;
  margin-bottom: 20px;
  box-shadow: 0 2px 4px rgba(0,0,0,0.1);
}

.form-group {
  margin-bottom: 15px;
}

label {
  display: block;
  margin-bottom: 5px;
  font-weight: bold;
}

input[type="text"],
input[type="password"] {
  width: 100%;
  padding: 8px;
  border: 1px solid #ddd;
  border-radius: 4px;
  box-sizing: border-box;
}

.input-group {
  display: flex;
}

.input-group input {
  flex: 1;
  border-top-right-radius: 0;
  border-bottom-right-radius: 0;
}

.btn-small {
  border-top-left-radius: 0;
  border-bottom-left-radius: 0;
  background-color: #3498db;
  color: white;
  border: none;
  padding: 8px 10px;
  cursor: pointer;
}

.btn {
  padding: 10px 15px;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  font-size: 16px;
}

.primary {
  background-color: #2ecc71;
  color: white;
}

.btn-group {
  text-align: center;
}

#statusMessage {
  margin-top: 20px;
  padding: 10px;
  border-radius: 4px;
  text-align: center;
}

.success {
  background-color: #d4edda;
  color: #155724;
}

.error {
  background-color: #f8d7da;
  color: #721c24;
}

.hidden {
  display: none;
}

.modal {
  display: none;
  position: fixed;
  z-index: 1;
  left: 0;
  top: 0;
  width: 100%;
  height: 100%;
  overflow: auto;
  background-color: rgba(0,0,0,0.4);
}

.modal-content {
  background-color: #fefefe;
  margin: 15% auto;
  padding: 20px;
  border: 1px solid #888;
  width: 80%;
  max-width: 500px;
  border-radius: 5px;
}

.close {
  color: #aaa;
  float: right;
  font-size: 28px;
  font-weight: bold;
  cursor: pointer;
}

#networksList {
  list-style-type: none;
  padding: 0;
}

#networksList li {
  padding: 10px;
  border-bottom: 1px solid #eee;
  cursor: pointer;
}

#networksList li:hover {
  background-color: #f8f9fa;
}
)rawliteral";

// JavaScript per la pagina di configurazione
const char setupJS[] PROGMEM = R"rawliteral(
document.addEventListener('DOMContentLoaded', function() {
  const configForm = document.getElementById('configForm');
  const statusMessage = document.getElementById('statusMessage');
  const scanBtn = document.getElementById('scanBtn');
  const modal = document.getElementById('networksModal');
  const closeBtn = document.querySelector('.close');
  const networksList = document.getElementById('networksList');
  const ssidInput = document.getElementById('ssid');

  // Gestisci invio del form
  configForm.addEventListener('submit', function(e) {
    e.preventDefault();
    
    const formData = {
      ssid: document.getElementById('ssid').value,
      password: document.getElementById('password').value,
      city: document.getElementById('city').value,
      api_key: document.getElementById('api_key').value
    };
    
    statusMessage.textContent = 'Salvataggio configurazione...';
    statusMessage.className = '';
    statusMessage.classList.add('success');
    statusMessage.classList.remove('hidden');
    
    fetch('/api/config', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json'
      },
      body: JSON.stringify(formData)
    })
    .then(response => response.json())
    .then(data => {
      if (data.success) {
        statusMessage.textContent = 'Configurazione salvata! Il dispositivo si riavvierà a breve.';
        statusMessage.classList.add('success');
      } else {
        statusMessage.textContent = 'Errore: ' + data.message;
        statusMessage.classList.add('error');
      }
    })
    .catch(error => {
      statusMessage.textContent = 'Errore di connessione. Riprova.';
      statusMessage.classList.add('error');
      console.error('Errore:', error);
    });
  });

  // Gestisci scansione WiFi
  scanBtn.addEventListener('click', function() {
    networksList.innerHTML = '<li>Ricerca reti...</li>';
    modal.style.display = 'block';
    
    fetch('/api/wifi-scan')
      .then(response => response.json())
      .then(data => {
        networksList.innerHTML = '';
        if (data.networks && data.networks.length > 0) {
          data.networks.forEach(network => {
            const li = document.createElement('li');
            li.textContent = `${network.ssid} (${network.rssi} dBm)`;
            li.addEventListener('click', function() {
              ssidInput.value = network.ssid;
              modal.style.display = 'none';
            });
            networksList.appendChild(li);
          });
        } else {
          networksList.innerHTML = '<li>Nessuna rete trovata</li>';
        }
      })
      .catch(error => {
        networksList.innerHTML = '<li>Errore durante la scansione</li>';
        console.error('Errore:', error);
      });
  });

  // Gestisci chiusura modale
  closeBtn.addEventListener('click', function() {
    modal.style.display = 'none';
  });

  window.addEventListener('click', function(event) {
    if (event.target == modal) {
      modal.style.display = 'none';
    }
  });
});
)rawliteral";

#endif // WEBMINIMAL_H
