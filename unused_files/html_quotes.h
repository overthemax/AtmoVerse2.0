/*
 * AtmoVerse 2.0 - Pagina HTML Citazioni
 * 
 * Contiene la definizione della pagina delle citazioni personalizzate
 */

#ifndef HTML_QUOTES_H
#define HTML_QUOTES_H

// Pagina citazioni ottimizzata
const char QUOTES_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta charset="UTF-8">
  <title>AtmoVerse 2.0 - Custom Quotes</title>
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
    
    .card-subtitle {
      font-size: 1rem;
      color: #a99b85;
      margin-bottom: 25px;
      line-height: 1.5;
    }
    
    .weather-category {
      margin-bottom: 30px;
    }
    
    .category-title {
      display: flex;
      align-items: center;
      font-size: 1.1rem;
      font-weight: 500;
      margin-bottom: 15px;
      color: var(--accent-color);
    }
    
    .category-title .material-icons {
      margin-right: 10px;
      font-size: 1.2rem;
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
    
    textarea {
      width: 100%;
      padding: 12px 15px;
      font-size: 1rem;
      border-radius: 8px;
      border: 1px solid var(--border-color);
      background-color: #f9f9f9;
      box-sizing: border-box;
      color: var(--text-color);
      transition: var(--transition);
      height: 100px;
      resize: vertical;
      font-family: var(--font-family);
    }
    
    textarea:focus {
      outline: none;
      border-color: var(--accent-color);
      box-shadow: 0 0 0 2px rgba(142, 120, 96, 0.2);
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
      <div class="card-title">Custom Weather Quotes</div>
      <div class="card-subtitle">
        Personalize the quotes displayed for each weather condition. The quotes will randomly display based on the current weather.
      </div>
      
      <form action="/saveQuotes" method="post">
        <!-- Clear Sky Quotes -->
        <div class="weather-category">
          <div class="category-title">
            <span class="material-icons">wb_sunny</span> Clear Sky
          </div>
          
          <div class="form-group">
            <label for="clear1">Quote for Clear Sky:</label>
            <textarea id="clear1" name="clear1" placeholder="Enter a quote for clear sky...">%CLEAR_QUOTE_1%</textarea>
          </div>
        </div>
        
        <!-- Clouds Quotes -->
        <div class="weather-category">
          <div class="category-title">
            <span class="material-icons">cloud</span> Clouds
          </div>
          
          <div class="form-group">
            <label for="clouds1">Quote for Clouds:</label>
            <textarea id="clouds1" name="clouds1" placeholder="Enter a quote for cloudy weather...">%CLOUDS_QUOTE_1%</textarea>
          </div>
        </div>
        
        <!-- Rain Quotes -->
        <div class="weather-category">
          <div class="category-title">
            <span class="material-icons">water</span> Rain
          </div>
          
          <div class="form-group">
            <label for="rain1">Quote for Rain:</label>
            <textarea id="rain1" name="rain1" placeholder="Enter a quote for rainy weather...">%RAIN_QUOTE_1%</textarea>
          </div>
        </div>
        
        <!-- Snow Quotes -->
        <div class="weather-category">
          <div class="category-title">
            <span class="material-icons">ac_unit</span> Snow
          </div>
          
          <div class="form-group">
            <label for="snow1">Quote for Snow:</label>
            <textarea id="snow1" name="snow1" placeholder="Enter a quote for snowy weather...">%SNOW_QUOTE_1%</textarea>
          </div>
        </div>
        
        <!-- Thunderstorm Quotes -->
        <div class="weather-category">
          <div class="category-title">
            <span class="material-icons">flash_on</span> Thunderstorm
          </div>
          
          <div class="form-group">
            <label for="thunderstorm1">Quote for Thunderstorm:</label>
            <textarea id="thunderstorm1" name="thunderstorm1" placeholder="Enter a quote for thunderstorm...">%THUNDERSTORM_QUOTE_1%</textarea>
          </div>
        </div>
        
        <div class="btn-group">
          <a href="/" class="btn btn-secondary">Cancel</a>
          <button type="submit" class="btn btn-primary">Save Quotes</button>
        </div>
      </form>
    </div>
  </div>
</body>
</html>
)rawliteral";

#endif // HTML_QUOTES_H
