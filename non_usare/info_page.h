#ifndef INFO_PAGE_H
#define INFO_PAGE_H

// NOTA: Questo file è mantenuto solo per compatibilità.
// La versione completa è stata spostata sulla SD card.

// Questa funzione è stata sostituita da uno stub in WebMinimal.cpp
// e rimane qui solo come riferimento.
/*
String generateMinimalInfoPage() {
  // Intestazione HTML in PROGMEM
  static const char html_header[] PROGMEM = 
    "<!DOCTYPE html><html lang='it'>"
    "<head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
    "<title>Informazioni Sistema - AtmoVerse 2.0</title>"
    "<style>";

  // CSS minimale in PROGMEM
  static const char css_minimal[] PROGMEM = 
    "*{box-sizing:border-box;margin:0;padding:0;}"
    "body{font-family:'Helvetica Neue',Arial,sans-serif;background:#f7f0e3;color:#7a604a;line-height:1.5;}"
    ".container{max-width:800px;margin:0 auto;padding:15px;}"
    ".header{display:flex;justify-content:space-between;align-items:center;margin-bottom:20px;}"
    ".action-buttons{display:flex;gap:10px;}"
    ".info-card{background:#fff;border-radius:15px;padding:20px;box-shadow:0 2px 10px rgba(0,0,0,0.05);margin-bottom:20px;}"
    ".info-section{margin-bottom:20px;border-bottom:1px solid #f3e2c2;padding-bottom:15px;}"
    ".info-section:last-child{border-bottom:none;padding-bottom:0;margin-bottom:0;}"
    ".info-title{font-size:18px;font-weight:500;color:#7a604a;margin-bottom:15px;display:flex;align-items:center;}"
    ".info-title svg{width:20px;height:20px;margin-right:8px;fill:currentColor;}"
    ".info-item{display:flex;justify-content:space-between;margin-bottom:10px;}"
    ".info-label{font-weight:500;color:#8d6e46;}"
    ".info-value{color:#43281c;}"
    ".progress-bar{height:14px;background:#f3e2c2;border-radius:7px;margin-top:5px;overflow:hidden;}"
    ".progress-fill{height:100%;background:#e2b76a;border-radius:7px;}"
    ".btn{display:inline-flex;align-items:center;justify-content:center;width:44px;height:44px;border-radius:50%;background:#f3e2c2;color:#8d6e46;text-decoration:none;border:none;cursor:pointer;transition:all .2s;}"
    ".btn svg{width:24px;height:24px;fill:currentColor;}"
    ".btn:hover{background:#e2b76a;color:#fff;transform:translateY(-2px);box-shadow:0 2px 5px rgba(0,0,0,0.1);}";
  
  String html = FPSTR(html_header);
  html += FPSTR(css_minimal);
  html += F("</style>");
  html += F("</head>");
  html += F("<body>");
  html += F("<div class='container'>");
  
  // Header con titolo e bottone indietro
  html += F("<div class='header'>");
  html += F("<h1>Informazioni Sistema</h1>");
  html += F("<a href='/' class='btn' title='Indietro'>");
  // Icona indietro
  html += F("<svg viewBox='0 0 24 24'><path d='M20,11V13H8L13.5,18.5L12.08,19.92L4.16,12L12.08,4.08L13.5,5.5L8,11H20Z'/></svg>");
  html += F("</a>");
  html += F("</div>");
  
  // Card informazioni dispositivo
  html += F("<div class='info-card'>");
  
  // Info hardware
  html += F("<div class='info-section'>");
  html += F("<div class='info-title'>");
  html += FPSTR(SVG_SETTINGS_ICON);
  html += F("Informazioni Hardware</div>");
  
  html += F("<div class='info-item'><div class='info-label'>Dispositivo:</div><div class='info-value'>ESP32 AtmoVerse 2.0</div></div>");
  html += F("<div class='info-item'><div class='info-label'>CPU:</div><div class='info-value'>");
  html += String(ESP.getCpuFreqMHz());
  html += F(" MHz</div></div>");
  html += F("<div class='info-item'><div class='info-label'>Flash Size:</div><div class='info-value'>");
  html += String(ESP.getFlashChipSize() / 1024.0 / 1024.0, 2);
  html += F(" MB</div></div>");
  html += F("<div class='info-item'><div class='info-label'>SDK Version:</div><div class='info-value'>");
  html += ESP.getSdkVersion();
  html += F("</div></div>");
  html += F("</div>");
  
  // Info memoria
  html += F("<div class='info-section'>");
  html += F("<div class='info-title'>");
  html += FPSTR(SVG_REFRESH_ICON);
  html += F("Stato Memoria</div>");
  
  // Calcola l'utilizzo della memoria
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t totalHeap = ESP.getHeapSize();
  uint32_t usedHeap = totalHeap - freeHeap;
  int heapPercentage = (usedHeap * 100) / totalHeap;
  
  html += F("<div class='info-item'><div class='info-label'>Memoria Totale:</div><div class='info-value'>");
  html += String(totalHeap / 1024.0, 2);
  html += F(" KB</div></div>");
  
  html += F("<div class='info-item'><div class='info-label'>Memoria Utilizzata:</div><div class='info-value'>");
  html += String(usedHeap / 1024.0, 2);
  html += F(" KB (");
  html += String(heapPercentage);
  html += F("%)</div></div>");
  
  html += F("<div class='info-item'><div class='info-label'>Memoria Libera:</div><div class='info-value'>");
  html += String(freeHeap / 1024.0, 2);
  html += F(" KB</div></div>");
  
  // Barra di progresso per la memoria
  html += F("<div class='progress-bar'><div class='progress-fill' style='width:");
  html += String(heapPercentage);
  html += F("%'></div></div>");
  html += F("</div>");
  
  // Info SD Card
  html += F("<div class='info-section'>");
  html += F("<div class='info-title'>");
  html += FPSTR(SVG_LOCATION_ICON);
  html += F("SD Card</div>");
  
  // Non chiamiamo SD.begin() qui perché dovrebbe essere già inizializzata altrove nel codice
  // Verifichiamo se la scheda SD è disponibile controllando se cardSize restituisce un valore valido
  uint64_t cardSize = SD.cardSize();
  
  if (cardSize > 0) {
    // Conversione in MB
    cardSize = cardSize / (1024 * 1024);
    uint64_t usedSpace = 0; // Qui bisognerebbe usare una funzione per calcolare lo spazio usato
    uint64_t freeSpace = cardSize - usedSpace;
    int usedPercentage = (usedSpace * 100) / cardSize;
    
    html += F("<div class='info-item'><div class='info-label'>Capacità Totale:</div><div class='info-value'>");
    html += String((float)cardSize, 2);
    html += F(" MB</div></div>");
    
    html += F("<div class='info-item'><div class='info-label'>Spazio Libero:</div><div class='info-value'>");
    html += String((float)freeSpace, 2);
    html += F(" MB</div></div>");
    
    html += F("<div class='progress-bar'><div class='progress-fill' style='width:");
    html += String(usedPercentage);
    html += F("%'></div></div>");
  } else {
    html += F("<div class='info-item'><div class='info-value'>SD Card non rilevata o non montata.</div></div>");
  }
  html += F("</div>");
  
  // Info di rete
  html += F("<div class='info-section'>");
  html += F("<div class='info-title'>");
  html += FPSTR(SVG_UPDATE_ICON);
  html += F("Connessione</div>");
  
  if (WiFi.status() == WL_CONNECTED) {
    html += F("<div class='info-item'><div class='info-label'>Rete:</div><div class='info-value'>");
    html += WiFi.SSID();
    html += F("</div></div>");
    
    html += F("<div class='info-item'><div class='info-label'>Indirizzo IP:</div><div class='info-value'>");
    html += WiFi.localIP().toString();
    html += F("</div></div>");
    
    html += F("<div class='info-item'><div class='info-label'>Potenza Segnale:</div><div class='info-value'>");
    html += String(WiFi.RSSI());
    html += F(" dBm</div></div>");
  } else {
    html += F("<div class='info-item'><div class='info-value'>Non connesso a una rete WiFi.</div></div>");
  }
  html += F("</div>");
  
  // Info durata attività
  html += F("<div class='info-section'>");
  html += F("<div class='info-title'>");
  html += FPSTR(SVG_TIMEZONE_ICON);
  html += F("Tempo di Attività</div>");
  
  unsigned long uptime = millis() / 1000; // Converti in secondi
  unsigned long uptimeDays = uptime / 86400;
  unsigned long uptimeHours = (uptime % 86400) / 3600;
  unsigned long uptimeMinutes = (uptime % 3600) / 60;
  unsigned long uptimeSeconds = uptime % 60;
  
  html += F("<div class='info-item'><div class='info-label'>Uptime:</div><div class='info-value'>");
  if (uptimeDays > 0) {
    html += String(uptimeDays);
    html += F(" giorni, ");
  }
  html += String(uptimeHours);
  html += F(" ore, ");
  html += String(uptimeMinutes);
  html += F(" minuti, ");
  html += String(uptimeSeconds);
  html += F(" secondi</div></div>");
  html += F("</div>");
  
  html += F("</div>"); // Chiude info-card
  
  html += F("</div>"); // Chiude container
  html += F("</body></html>");
  
  return html;
}
*/

#endif // INFO_PAGE_H
