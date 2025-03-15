#ifndef HTML_CONTENT_H
#define HTML_CONTENT_H

const char MAIN_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="it">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>AtmoVerse</title>
<link href="https://fonts.googleapis.com/icon?family=Material+Icons" rel="stylesheet">
<style>
*{margin:0;padding:0;box-sizing:border-box;font-family:Arial,sans-serif}
body{max-width:600px;margin:0 auto;padding:10px;background:#f5f5f5;color:#333}
.card{background:#fff;border-radius:5px;padding:15px;margin:10px 0;box-shadow:0 2px 5px rgba(0,0,0,.1)}
.weather-main{display:flex;align-items:center;margin-bottom:10px}
.weather-icon{font-size:48px;margin-right:15px;color:#1976d2}
.location{margin-bottom:15px}
.location h1{font-size:24px;margin-bottom:5px}
.weather-data{display:flex;flex-wrap:wrap;justify-content:space-between}
.weather-item{flex-basis:48%;margin-bottom:10px}
.label{font-size:12px;color:#666;display:block;margin-bottom:3px}
.value{font-size:16px;font-weight:bold}
.quote{font-style:italic;margin:15px 0;padding:10px;background:#f9f9f9;border-left:3px solid #1976d2}
.quote-category{font-size:12px;color:#666;margin-top:5px}
.system-info{font-size:12px;color:#888;margin-top:15px}
.system-info div{margin-bottom:3px}
.refresh-btn{background:#1976d2;color:#fff;border:none;padding:8px 15px;border-radius:3px;cursor:pointer;margin-top:10px}
@media (max-width:480px){.weather-item{flex-basis:100%}}
</style>
</head>
<body>
<div class="card location">
<h1>%CITY%, %COUNTRY%</h1>
</div>
<div class="card">
<div class="weather-main">
<i class="material-icons weather-icon">%WEATHER_ICON%</i>
<div>
<div class="value">%TEMPERATURE%&deg;C</div>
<div>%WEATHER_CONDITION%</div>
</div>
</div>
<div class="weather-data">
<div class="weather-item">
<span class="label">Umidità</span>
<span class="value">%HUMIDITY%%</span>
</div>
<div class="weather-item">
<span class="label">Vento</span>
<span class="value">%WIND_SPEED% km/h</span>
</div>
<div class="weather-item">
<span class="label">Pressione</span>
<span class="value">%PRESSURE% hPa</span>
</div>
</div>
</div>
<div class="card">
<div class="quote">
%QUOTE_CONTENT%
<div class="quote-category">Categoria: %QUOTE_CATEGORY%</div>
</div>
</div>
<div class="system-info">
<div>Stato WiFi: %WIFI_STATUS%</div>
<div>IP: %IP_ADDRESS%</div>
<div>Memoria libera: %FREE_HEAP% bytes</div>
<div>Ultimo aggiornamento: %LAST_UPDATE%</div>
</div>
<a href="/settings"><button class="refresh-btn">Impostazioni</button></a>
<a href="/quotes"><button class="refresh-btn">Citazioni</button></a>
</body>
</html>
)rawliteral";

const char SETTINGS_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="it">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Impostazioni</title>
<style>
*{margin:0;padding:0;box-sizing:border-box;font-family:Arial,sans-serif}
body{max-width:600px;margin:0 auto;padding:10px;background:#f5f5f5;color:#333}
.card{background:#fff;border-radius:5px;padding:15px;margin:10px 0;box-shadow:0 2px 5px rgba(0,0,0,.1)}
h1{font-size:24px;margin-bottom:15px}
.form-group{margin-bottom:15px}
label{display:block;margin-bottom:5px;font-weight:bold}
input[type="text"],input[type="password"]{width:100%;padding:8px;border:1px solid #ddd;border-radius:3px}
button{background:#1976d2;color:#fff;border:none;padding:8px 15px;border-radius:3px;cursor:pointer}
.checkbox-group{display:flex;align-items:center}
.checkbox-group input{margin-right:10px}
</style>
</head>
<body>
<div class="card">
<h1>Impostazioni WiFi e API</h1>
<form action="/save_settings" method="post">
<div class="form-group">
<label for="ssid">SSID WiFi</label>
<input type="text" id="ssid" name="ssid" value="%SSID%">
</div>
<div class="form-group">
<label for="password">Password WiFi</label>
<input type="password" id="password" name="password" value="%PASSWORD%">
</div>
<div class="form-group">
<label for="api_key">API key OpenWeatherMap</label>
<input type="text" id="api_key" name="api_key" value="%API_KEY%">
</div>
<div class="form-group">
<label for="city">Città</label>
<input type="text" id="city" name="city" value="%CITY%">
</div>
<div class="form-group">
<label for="country">Codice paese (2 lettere)</label>
<input type="text" id="country" name="country" value="%COUNTRY%">
</div>
<div class="form-group">
<label for="timezone">Timezone (es. Europe/Rome)</label>
<input type="text" id="timezone" name="timezone" value="%TIMEZONE%">
</div>
<div class="form-group checkbox-group">
<input type="checkbox" id="dst_enabled" name="dst_enabled" %DST_CHECKED%>
<label for="dst_enabled">Abilita ora legale</label>
</div>
<button type="submit">Salva</button>
</form>
</div>
<a href="/"><button>Torna alla home</button></a>
</body>
</html>
)rawliteral";

const char QUOTES_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="it">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Citazioni</title>
<style>
*{margin:0;padding:0;box-sizing:border-box;font-family:Arial,sans-serif}
body{max-width:600px;margin:0 auto;padding:10px;background:#f5f5f5;color:#333}
.card{background:#fff;border-radius:5px;padding:15px;margin:10px 0;box-shadow:0 2px 5px rgba(0,0,0,.1)}
h1{font-size:24px;margin-bottom:15px}
h2{font-size:18px;margin:15px 0 10px}
.form-group{margin-bottom:15px}
textarea{width:100%;height:80px;padding:8px;border:1px solid #ddd;border-radius:3px;resize:vertical}
button{background:#1976d2;color:#fff;border:none;padding:8px 15px;border-radius:3px;cursor:pointer}
</style>
</head>
<body>
<div class="card">
<h1>Configura Citazioni</h1>
<form action="/save_quotes" method="post">
<h2>Cielo Sereno</h2>
<div class="form-group">
<textarea name="clear_sky_1">%CLEAR_QUOTE_1%</textarea>
</div>
<h2>Nuvoloso</h2>
<div class="form-group">
<textarea name="clouds_1">%CLOUDS_QUOTE_1%</textarea>
</div>
<h2>Pioggia</h2>
<div class="form-group">
<textarea name="rain_1">%RAIN_QUOTE_1%</textarea>
</div>
<h2>Neve</h2>
<div class="form-group">
<textarea name="snow_1">%SNOW_QUOTE_1%</textarea>
</div>
<h2>Temporale</h2>
<div class="form-group">
<textarea name="thunderstorm_1">%THUNDERSTORM_QUOTE_1%</textarea>
</div>
<button type="submit">Salva</button>
</form>
</div>
<a href="/"><button>Torna alla home</button></a>
</body>
</html>
)rawliteral";

#endif
