#include "WebUIPages.h"
#include "NetworkUtils.h"
#include "Config.h"
#include "WeatherUtils.h"
#include <time.h>
#include <SD.h>

// Riferimenti esterni necessari
extern const int SD_CS;
extern const char* AP_SSID;

// Formatta una data/ora in modo leggibile
String formatDateTime(unsigned long timestamp) {
  time_t rawtime = timestamp;
  struct tm timeinfo;
  localtime_r(&rawtime, &timeinfo);
  char buffer[80];
  strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", &timeinfo);
  return String(buffer);
}

// Genera il CSS comune - versione migliorata
String generateStyleCSS() {
  return 
    "* { box-sizing: border-box; margin: 0; padding: 0; }"
    "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #f0f2f5; color: #333; line-height: 1.6; }"
    "h1, h2, h3 { color: #555; margin-bottom: 20px; }"
    "h1 { font-size: 32px; font-weight: 300; }"
    "h2 { font-size: 24px; font-weight: 400; }"
    "p { margin-bottom: 15px; }"
    ".container { max-width: 800px; margin: 0 auto; padding: 20px; }"
    ".card { background: #fff; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.08); padding: 25px; margin-bottom: 25px; }"
    ".section { margin-bottom: 30px; }"
    ".section-title { font-size: 18px; font-weight: 500; color: #444; margin-bottom: 15px; display: flex; align-items: center; }"
    ".section-icon { margin-right: 10px; font-size: 22px; }"
    ".form-group { margin-bottom: 15px; }"
    "label { display: block; margin-bottom: 8px; color: #555; font-weight: 500; }"
    "input[type=text], input[type=password], input[type=number], select { width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 5px; font-size: 16px; }"
    "input:focus { outline: none; border-color: #4a90e2; }"
    ".button-group { margin-top: 20px; display: flex; justify-content: flex-end; }"
    ".button { display: inline-block; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; text-decoration: none; text-align: center; }"
    ".primary { background: #4a90e2; color: white; }"
    ".secondary { background: #f0f0f0; color: #444; }"
    ".danger { background: #e25c5c; color: white; }"
    ".button:hover { opacity: 0.9; }"
    ".nav-buttons { display: flex; justify-content: flex-end; margin-bottom: 20px; }"
    ".nav-button { font-size: 24px; margin-left: 15px; text-decoration: none; }";
}

// Genera la pagina delle impostazioni - versione migliorata
String generateSettingsPage(String ssid) {
  String html = "<!DOCTYPE html>\n<html lang=\"it\">\n<head>\n";
  html += "<meta charset=\"UTF-8\">\n";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
  html += "<title>Impostazioni AtmoVerse</title>\n";
  html += "<style>\n";
  html += generateStyleCSS();
  html += "</style>\n";
  html += "</head>\n<body>\n";
  html += "<div class=\"container\">\n";
  html += "<h1>Impostazioni AtmoVerse</h1>\n";
  
  // Form principale
  html += "<form action=\"/api/settings\" method=\"post\">\n";
  
  // Card principale con tutte le impostazioni
  html += "<div class=\"card\">\n";
  
  // WiFi
  html += "<div class=\"section\">\n";
  html += "<div class=\"section-title\"><div class=\"section-icon\">&#128246;</div>Configurazione WiFi</div>\n";
  html += "<div class=\"form-group\">\n";
  html += "<label for=\"ssid\">Nome rete WiFi</label>\n";
  html += "<div style=\"display:flex;gap:10px;\">\n";
  html += "<input type=\"text\" name=\"ssid\" id=\"ssid\" value=\"";
  html += ssid.isEmpty() ? String(config.ssid) : ssid;
  html += "\" placeholder=\"Nome rete WiFi\">\n";
  html += "<a href=\"/wifi-scan\" class=\"button secondary\" style=\"width:auto;white-space:nowrap;\">Scan</a>\n";
  html += "</div>\n";
  html += "</div>\n";
  html += "<div class=\"form-group\">\n";
  html += "<label for=\"password\">Password WiFi</label>\n";
  html += "<input type=\"password\" name=\"password\" id=\"password\" placeholder=\"Password rete WiFi\">\n";
  html += "</div>\n";
  html += "</div>\n"; // Fine sezione WiFi
  
  // OpenWeatherMap
  html += "<div class=\"section\">\n";
  html += "<div class=\"section-title\"><div class=\"section-icon\">&#127780;</div>OpenWeatherMap</div>\n";
  html += "<div class=\"form-group\">\n";
  html += "<label for=\"city\">Citt&agrave;</label>\n";
  html += "<input type=\"text\" name=\"city\" id=\"city\" value=\"";
  html += String(config.city);
  html += "\" placeholder=\"Nome citt&agrave; (es. Roma)\">\n";
  html += "</div>\n";
  html += "<div class=\"form-group\">\n";
  html += "<label for=\"api_key\">API Key</label>\n";
  html += "<input type=\"text\" name=\"api_key\" id=\"api_key\" value=\"";
  html += String(config.api_key);
  html += "\" placeholder=\"API key OpenWeatherMap\">\n";
  html += "</div>\n";
  html += "</div>\n"; // Fine sezione OpenWeatherMap
  
  // NTP
  html += "<div class=\"section\">\n";
  html += "<div class=\"section-title\"><div class=\"section-icon\">&#128338;</div>Impostazioni Orario</div>\n";
  html += "<div class=\"form-group\">\n";
  html += "<label for=\"ntp_server\">Server NTP</label>\n";
  html += "<input type=\"text\" name=\"ntp_server\" id=\"ntp_server\" value=\"";
  html += String(config.ntpServer);
  html += "\" placeholder=\"Server NTP (es. pool.ntp.org)\">\n";
  html += "</div>\n";
  html += "<div class=\"form-group\">\n";
  html += "<label for=\"timezone\">Fuso orario (in ore)</label>\n";
  html += "<input type=\"number\" name=\"timezone\" id=\"timezone\" value=\"";
  html += String(config.gmtOffset_sec / 3600);
  html += "\" min=\"-12\" max=\"14\" step=\"1\">\n";
  html += "</div>\n";
  html += "</div>\n"; // Fine sezione NTP
  
  html += "<div class=\"button-group\">\n";
  html += "<input type=\"submit\" class=\"button primary\" value=\"Salva impostazioni\">\n";
  html += "</div>\n";
  html += "</div>\n"; // Chiude card
  html += "</form>\n";
  
  // Reset
  html += "<div class=\"card\">\n";
  html += "<div class=\"section-title\"><div class=\"section-icon\">&#9888;</div>Reset di fabbrica</div>\n";
  html += "<p>Questa operazione canceller&agrave; tutte le impostazioni e riporter&agrave; il dispositivo alle condizioni iniziali.</p>\n";
  html += "<div class=\"button-group\">\n";
  html += "<a href=\"/reset\" class=\"button danger\" onclick=\"return confirm('Sei sicuro di voler resettare tutte le impostazioni? Questa operazione non pu&ograve; essere annullata.')\">Reset configurazione</a>\n";
  html += "</div>\n";
  html += "</div>\n"; // Chiude card
  
  html += "</div>\n"; // Chiude container
  html += "</body>\n</html>";
  
  return html;
}

// Genera la pagina di scansione WiFi - versione completa
String generateWiFiScanPage() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<style>";
  html += generateStyleCSS();
  html += ".container{max-width:800px;margin:40px auto;padding:0 15px;}";
  html += ".card{background:#fff;border-radius:16px;padding:28px;margin-bottom:25px;box-shadow:0 4px 16px rgba(141,110,70,0.08);border:1.5px solid #f3e2c2;}";
  html += ".back-button{display:flex;align-items:center;justify-content:center;width:42px;height:42px;border-radius:50%;background:#f3e2c2;color:#a97b3a;text-decoration:none;font-size:18px;}";
  html += ".back-button:hover{background:#e2b76a;color:#fff;}";
  html += ".wifi-list{margin-top:20px;}";
  html += ".wifi-item{padding:15px;border:1px solid #f3e2c2;border-radius:8px;margin-bottom:10px;display:flex;align-items:center;justify-content:space-between;background:#fffdf7;cursor:pointer;transition:all 0.2s ease;}";
  html += ".wifi-item:hover{background:#f7f0e3;transform:translateY(-1px)}";
  html += ".wifi-info{flex:1;}";
  html += ".wifi-name{font-weight:600;color:#7a604a;}";
  html += ".wifi-signal{font-size:0.85rem;color:#a97b3a;}";
  html += ".loader{border:5px solid #f3f3f3;border-top:5px solid #a97b3a;border-radius:50%;width:50px;height:50px;animation:spin 1s linear infinite;margin:50px auto;}";
  html += "@keyframes spin{0%{transform:rotate(0deg);}100%{transform:rotate(360deg);}}";
  html += ".loader-text{text-align:center;margin-top:15px;color:#a97b3a;font-style:italic;}";
  html += ".signal-bars{display:inline-block;width:20px;}";
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<div class='container'>";
  html += "<header><h1>Reti WiFi disponibili</h1><a href='/settings' class='back-button'><span class='icon-round'>◀</span></a></header>";
  
  html += "<div class='card'>";
  html += "<div id='wifi-scanning'>";
  html += "<div class='loader'></div>";
  html += "<div class='loader-text'>Ricerca reti WiFi in corso...</div>";
  html += "</div>";
  html += "<div id='wifi-list' class='wifi-list' style='display:none;'></div>";
  html += "</div>"; // Chiude card
  
  html += "<script>";
  html += "document.addEventListener('DOMContentLoaded', function() {";
  html += "  setTimeout(scanWifi, 500);";
  html += "});";
  
  html += "function scanWifi() {";
  html += "  fetch('/api/wifi-scan')";
  html += "  .then(response => response.json())";
  html += "  .then(data => {";
  html += "    displayNetworks(data.networks);";
  html += "  })";
  html += "  .catch(error => {";
  html += "    console.error('Errore durante la scansione:', error);";
  html += "    document.getElementById('wifi-scanning').innerHTML = '<p>Errore durante la scansione delle reti WiFi. <a href=\"#\" onclick=\"scanWifi(); return false;\">Riprova</a>.</p>';";
  html += "  });";
  html += "}";
  
  html += "function getSignalStrength(rssi) {";
  html += "  if (rssi >= -50) return '●●●●';";
  html += "  if (rssi >= -65) return '●●●○';";
  html += "  if (rssi >= -75) return '●●○○';";
  html += "  return '●○○○';";
  html += "}";
  
  html += "function displayNetworks(networks) {";
  html += "  const container = document.getElementById('wifi-list');";
  html += "  container.innerHTML = '';";
  html += "  document.getElementById('wifi-scanning').style.display = 'none';";
  html += "  container.style.display = 'block';";
  
  html += "  if (networks.length === 0) {";
  html += "    container.innerHTML = '<p>Nessuna rete WiFi trovata. <a href=\"#\" onclick=\"scanWifi(); return false;\">Riprova</a>.</p>';";
  html += "    return;";
  html += "  }";
  
  html += "  // Ordina le reti per potenza del segnale";
  html += "  networks.sort((a, b) => b.rssi - a.rssi);";
  
  html += "  networks.forEach(network => {";
  html += "    const div = document.createElement('div');";
  html += "    div.className = 'wifi-item';";
  html += "    div.onclick = function() { selectNetwork(network.ssid); };";
  html += "    div.innerHTML = `";
  html += "      <div class='wifi-info'>";
  html += "        <div class='wifi-name'>${network.ssid}</div>";
  html += "        <div class='wifi-signal'>${network.encryption ? 'Protetta' : 'Aperta'}</div>";
  html += "      </div>";
  html += "      <div class='wifi-signal'><span class='signal-bars'>${getSignalStrength(network.rssi)}</span> ${network.rssi} dBm</div>";
  html += "    `;";
  html += "    container.appendChild(div);";
  html += "  });";
  html += "}";
  
  html += "function selectNetwork(ssid) {";
  html += "  window.location.href = '/select-network?ssid=' + encodeURIComponent(ssid);";
  html += "}";
  
  html += "</script>";
  html += "</body></html>";
  
  return html;
}

// Ottieni la stringa IP locale
String getLocalIPString() {
  return WiFi.localIP().toString();
}

// La funzione generateInfoPage() è stata rimossa.
// Ora viene utilizzato un file HTML statico (info.html) sulla SD card.

// Visualizza le informazioni dell'Access Point sul display
void displayAPInfo(const char* ssid, const char* password) {
  Serial.println(F("AP Mode Info:"));
  Serial.print(F("SSID: "));
  Serial.println(ssid);
  Serial.print(F("Password: "));
  Serial.println(password);
  Serial.print(F("IP: "));
  Serial.println(WiFi.softAPIP());
}
