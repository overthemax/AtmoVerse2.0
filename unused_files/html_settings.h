/*
 * AtmoVerse 2.0 - Pagina HTML Impostazioni
 * 
 * Contiene la definizione della pagina delle impostazioni
 */

#ifndef HTML_SETTINGS_H
#define HTML_SETTINGS_H

// Pagina impostazioni ottimizzata
const char SETTINGS_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta charset="UTF-8">
  <title>AtmoVerse 2.0 - Settings</title>
  <link href="https://fonts.googleapis.com/css2?family=Roboto:wght@300;400;500&display=swap" rel="stylesheet">
  <link href="https://fonts.googleapis.com/icon?family=Material+Icons" rel="stylesheet">
  <style>
    :root {
      --bg-color: #f8f1e0;
      --card-bg: #ffffff;
      --text-color: #907b61;
      --logo-color: #907b61;
      --accent-color: #8e7860;
      --border-color: #e5dfd5;
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
    
    .back-btn {
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
      text-decoration: none;
    }
    
    .back-btn:hover {
      background: rgba(142, 120, 96, 0.1);
    }
    
    .back-btn .material-icons {
      margin-right: 5px;
      font-size: 1.2rem;
    }
    
    .card {
      background: var(--card-bg);
      border-radius: var(--border-radius);
      box-shadow: var(--card-shadow);
      padding: 20px;
      margin-bottom: 20px;
    }
    
    .card-title {
      font-size: 1.3rem;
      font-weight: 500;
      margin-bottom: 20px;
    }
    
    .section-title {
      font-size: 1.1rem;
      font-weight: 500;
      margin: 25px 0 15px;
      color: var(--accent-color);
    }
    
    .form-group {
      margin-bottom: 15px;
    }
    
    label {
      display: block;
      margin-bottom: 8px;
      font-weight: 500;
      font-size: 0.95rem;
    }
    
    input[type="text"],
    input[type="password"] {
      width: 100%;
      padding: 10px 15px;
      font-size: 1rem;
      border-radius: 8px;
      border: 1px solid var(--border-color);
      background-color: #f9f9f9;
      box-sizing: border-box;
      color: var(--text-color);
      transition: var(--transition);
    }
    
    input[type="text"]:focus,
    input[type="password"]:focus {
      outline: none;
      border-color: var(--accent-color);
      box-shadow: 0 0 0 2px rgba(142, 120, 96, 0.2);
    }
    
    .checkbox-group {
      display: flex;
      align-items: center;
    }
    
    input[type="checkbox"] {
      margin-right: 10px;
      accent-color: var(--accent-color);
    }
    
    .btn {
      display: inline-block;
      padding: 12px 25px;
      font-size: 1rem;
      font-weight: 500;
      text-align: center;
      border: none;
      border-radius: 50px;
      cursor: pointer;
      transition: var(--transition);
      text-decoration: none;
      margin-top: 10px;
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
    
    .btn-group {
      display: flex;
      justify-content: space-between;
      margin-top: 30px;
    }
    
    @media (max-width: 768px) {
      .btn-group {
        flex-direction: column;
      }
      
      .btn {
        width: 100%;
        margin-bottom: 10px;
      }
      
      .header {
        flex-direction: column;
        gap: 10px;
        align-items: flex-start;
      }
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <div class="logo">AtmoVerse 2.0</div>
      <a href="/" class="back-btn">
        <span class="material-icons">arrow_back</span> Back to Main
      </a>
    </div>
    
    <div class="card">
      <div class="card-title">System Settings</div>
      <form action="/saveSettings" method="post">
        <div class="section-title">WiFi Configuration</div>
        
        <div class="form-group">
          <label for="ssid">WiFi SSID:</label>
          <input type="text" id="ssid" name="ssid" value="%SSID%" required>
        </div>
        
        <div class="form-group">
          <label for="password">WiFi Password:</label>
          <input type="password" id="password" name="password" value="%PASSWORD%">
        </div>
        
        <div class="section-title">Weather API</div>
        
        <div class="form-group">
          <label for="api_key">OpenWeatherMap API Key:</label>
          <input type="text" id="api_key" name="api_key" value="%API_KEY%" required>
        </div>
        
        <div class="form-group">
          <label for="city">City:</label>
          <input type="text" id="city" name="city" value="%CITY%" required>
        </div>
        
        <div class="form-group">
          <label for="country">Country Code (ISO 3166 - 2 letters):</label>
          <input type="text" id="country" name="country" value="%COUNTRY%" maxlength="2">
        </div>
        
        <div class="section-title">Time Settings</div>
        
        <div class="form-group">
          <label for="timezone">Timezone String:</label>
          <input type="text" id="timezone" name="timezone" value="%TIMEZONE%">
        </div>
        
        <div class="form-group checkbox-group">
          <input type="checkbox" id="dst_enabled" name="dst_enabled" %DST_CHECKED%>
          <label for="dst_enabled">Enable Daylight Saving Time adjustments</label>
        </div>
        
        <div class="btn-group">
          <a href="/" class="btn btn-secondary">Cancel</a>
          <button type="submit" class="btn btn-primary">Save Settings</button>
        </div>
      </form>
    </div>
  </div>
</body>
</html>
)rawliteral";

#endif // HTML_SETTINGS_H
