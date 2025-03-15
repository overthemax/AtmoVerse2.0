/*
 * AtmoVerse 2.0 - Pagina HTML Principale
 * 
 * Contiene la definizione della pagina principale
 */

#ifndef HTML_MAIN_H
#define HTML_MAIN_H

// Pagina principale ottimizzata
const char MAIN_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta charset="UTF-8">
  <title>AtmoVerse 2.0</title>
  <link href="https://fonts.googleapis.com/css2?family=Roboto:wght@300;400;500&display=swap" rel="stylesheet">
  <link href="https://fonts.googleapis.com/icon?family=Material+Icons" rel="stylesheet">
  <style>
    :root {
      --bg-color: #f8f1e0;
      --card-bg: #ffffff;
      --text-color: #907b61;
      --logo-color: #907b61;
      --accent-color: #8e7860;
      --icon-color: #8e7860;
      --border-radius: 16px;
      --card-shadow: 0 2px 8px rgba(0,0,0,0.05);
      --font-family: 'Roboto', sans-serif;
      --transition: all 0.3s ease;
    }
    
    body { 
      font-family: var(--font-family); 
      margin: 0; 
      padding: 0;
      background: var(--bg-color); 
      color: var(--text-color);
      line-height: 1.5;
    }
    
    .container {
      width: 90%;
      max-width: 800px;
      margin: 0 auto;
      padding: 20px 0;
    }
    
    .header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 20px;
    }
    
    .logo {
      font-size: 1.8rem;
      font-weight: 500;
      color: var(--logo-color);
    }
    
    .card {
      background: var(--card-bg);
      border-radius: var(--border-radius);
      box-shadow: var(--card-shadow);
      padding: 20px;
      margin-bottom: 20px;
    }
    
    .settings-btn {
      display: flex;
      align-items: center;
      background: none;
      border: none;
      color: var(--accent-color);
      cursor: pointer;
      font-size: 0.9rem;
      padding: 8px 12px;
      border-radius: 50px;
      transition: var(--transition);
    }
    
    .settings-btn:hover {
      background: rgba(142, 120, 96, 0.1);
    }
    
    .settings-btn .material-icons {
      margin-right: 5px;
      font-size: 1.2rem;
    }
    
    .btn-group {
      display: flex;
      gap: 10px;
    }
    
    .weather-card {
      display: flex;
      flex-direction: column;
    }
    
    .weather-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 15px;
    }
    
    .location {
      font-size: 1.3rem;
      font-weight: 500;
    }
    
    .datetime {
      font-size: 0.9rem;
      color: #a99b85;
    }
    
    .weather-details {
      display: flex;
      flex-wrap: wrap;
      margin-bottom: 20px;
    }
    
    .weather-temp {
      display: flex;
      align-items: center;
      flex: 1;
      min-width: 150px;
    }
    
    .temp-icon {
      font-size: 3rem;
      color: var(--icon-color);
      margin-right: 15px;
    }
    
    .temp-value {
      font-size: 2.5rem;
      font-weight: 300;
    }
    
    .temp-unit {
      font-size: 1rem;
      align-self: flex-start;
      margin-top: 8px;
      margin-left: 2px;
    }
    
    .weather-metrics {
      flex: 1;
      min-width: 150px;
      display: grid;
      grid-template-columns: repeat(2, 1fr);
      grid-gap: 15px;
    }
    
    .metric {
      display: flex;
      align-items: center;
    }
    
    .metric-icon {
      color: var(--icon-color);
      margin-right: 8px;
      font-size: 1.2rem;
    }
    
    .metric-value {
      font-size: 1rem;
    }
    
    .weather-condition {
      text-align: center;
      margin-top: 10px;
      font-size: 1.1rem;
      font-weight: 500;
      text-transform: capitalize;
    }
    
    .quote-card {
      position: relative;
      padding: 25px;
    }
    
    .quote-content {
      font-size: 1.1rem;
      font-style: italic;
      line-height: 1.6;
      margin-bottom: 10px;
      quotes: """ """ "'" "'";
    }
    
    .quote-content:before {
      content: open-quote;
      font-size: 1.5em;
      line-height: 0.1em;
      margin-right: 0.1em;
      vertical-align: -0.2em;
      color: var(--accent-color);
    }
    
    .quote-content:after {
      content: close-quote;
      font-size: 1.5em;
      line-height: 0.1em;
      margin-left: 0.1em;
      vertical-align: -0.2em;
      color: var(--accent-color);
    }
    
    .quote-category {
      text-align: right;
      font-size: 0.9rem;
      color: #a99b85;
    }
    
    .modal {
      display: none;
      position: fixed;
      z-index: 1000;
      top: 0;
      left: 0;
      width: 100%;
      height: 100%;
      background-color: rgba(0,0,0,0.4);
      overflow: auto;
    }
    
    .modal-content {
      position: relative;
      background-color: var(--card-bg);
      margin: 10% auto;
      width: 90%;
      max-width: 500px;
      border-radius: var(--border-radius);
      box-shadow: 0 4px 20px rgba(0,0,0,0.1);
      animation: modalopen 0.3s;
    }
    
    @keyframes modalopen {
      from {opacity: 0; transform: translateY(-20px);}
      to {opacity: 1; transform: translateY(0);}
    }
    
    .modal-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 15px 20px;
      border-bottom: 1px solid #f5f5f5;
    }
    
    .modal-title {
      font-size: 1.2rem;
      font-weight: 500;
    }
    
    .close {
      font-size: 1.5rem;
      font-weight: bold;
      cursor: pointer;
    }
    
    .modal-body {
      padding: 20px;
    }
    
    .system-info {
      display: grid;
      grid-template-columns: auto 1fr;
      grid-gap: 10px 15px;
    }
    
    .info-label {
      font-weight: 500;
    }
    
    .info-value {
      text-align: right;
    }
    
    .btn {
      display: inline-block;
      padding: 10px 20px;
      font-size: 1rem;
      font-weight: 500;
      text-align: center;
      border: none;
      border-radius: 50px;
      cursor: pointer;
      transition: var(--transition);
      text-decoration: none;
    }
    
    .btn-primary {
      background-color: var(--accent-color);
      color: white;
    }
    
    .btn-secondary {
      background-color: #f0e9df;
      color: var(--accent-color);
    }
    
    .btn-primary:hover {
      background-color: #7c6a53;
    }
    
    .btn-secondary:hover {
      background-color: #e5dfd5;
    }
    
    @media (max-width: 768px) {
      .header {
        flex-direction: column;
        gap: 10px;
        align-items: flex-start;
      }
      
      .weather-details {
        flex-direction: column;
      }
      
      .weather-metrics {
        margin-top: 20px;
      }
      
      .btn-group {
        flex-direction: column;
      }
      
      .btn {
        width: 100%;
      }
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <div class="logo">AtmoVerse 2.0</div>
      <div class="btn-group">
        <button class="settings-btn" onclick="openSystemInfo()">
          <span class="material-icons">info</span> Info
        </button>
        <a href="/settings" class="settings-btn">
          <span class="material-icons">settings</span> Settings
        </a>
        <a href="/quotes" class="settings-btn">
          <span class="material-icons">format_quote</span> Quotes
        </a>
      </div>
    </div>
    
    <div class="card weather-card">
      <div class="weather-header">
        <div class="location">%CITY%, %COUNTRY%</div>
        <div class="datetime">%DATETIME%</div>
      </div>
      
      <div class="weather-details">
        <div class="weather-temp">
          <span class="material-icons temp-icon">%WEATHER_ICON%</span>
          <div class="temp-value">%TEMPERATURE%</div>
          <div class="temp-unit">°C</div>
        </div>
        
        <div class="weather-metrics">
          <div class="metric">
            <span class="material-icons metric-icon">water_drop</span>
            <div class="metric-value">%HUMIDITY%<span>%</span></div>
          </div>
          <div class="metric">
            <span class="material-icons metric-icon">air</span>
            <div class="metric-value">%WIND_SPEED%<span>m/s</span></div>
          </div>
          <div class="metric">
            <span class="material-icons metric-icon">compress</span>
            <div class="metric-value">%PRESSURE%<span>hPa</span></div>
          </div>
        </div>
      </div>
      
      <div class="weather-condition">%WEATHER_CONDITION%</div>
    </div>
    
    <div class="card quote-card">
      <div class="quote-content">%QUOTE_CONTENT%</div>
      <div class="quote-category">Weather: %QUOTE_CATEGORY%</div>
    </div>
  </div>
  
  <!-- System Info Modal -->
  <div id="systemInfoModal" class="modal">
    <div class="modal-content">
      <div class="modal-header">
        <h2 class="modal-title">System Information</h2>
        <span class="close" onclick="closeModal()">&times;</span>
      </div>
      <div class="modal-body">
        <div class="system-info">
          <div class="info-label">WiFi Status:</div>
          <div class="info-value">%WIFI_STATUS%</div>
          
          <div class="info-label">IP Address:</div>
          <div class="info-value">%IP_ADDRESS%</div>
          
          <div class="info-label">Free Memory:</div>
          <div class="info-value">%FREE_HEAP% bytes</div>
          
          <div class="info-label">Last Update:</div>
          <div class="info-value">%LAST_UPDATE%</div>
        </div>
      </div>
    </div>
  </div>
  
  <script>
    // Modal functionality
    function openSystemInfo() {
      document.getElementById('systemInfoModal').style.display = 'block';
    }
    
    function closeModal() {
      document.getElementById('systemInfoModal').style.display = 'none';
    }
    
    // Close modal if clicked outside
    window.onclick = function(event) {
      var modal = document.getElementById('systemInfoModal');
      if (event.target == modal) {
        modal.style.display = 'none';
      }
    }
  </script>
</body>
</html>
)rawliteral";

#endif // HTML_MAIN_H
