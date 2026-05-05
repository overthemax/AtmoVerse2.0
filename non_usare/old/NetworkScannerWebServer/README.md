# WiFi Network Scanner

Questo è uno sketch minimale per ESP32 che fornisce:
- Un indirizzo IP fisso configurabile
- Una pagina web HTML responsive
- Funzionalità di scansione delle reti WiFi disponibili

## Configurazione

1. Aprire lo sketch `NetworkScannerWebServer.ino` in Arduino IDE
2. Installare le librerie necessarie:
   - WiFi (inclusa nel core ESP32)
   - WebServer (inclusa nel core ESP32)
3. Configurare le credenziali WiFi e l'indirizzo IP statico:
   ```cpp
   // WiFi credentials
   const char* ssid = "YourWiFiName";     // Sostituire con il nome della tua rete WiFi
   const char* password = "YourPassword";  // Sostituire con la password della tua rete WiFi

   // Fixed IP configuration
   IPAddress staticIP(192, 168, 1, 200);   // L'indirizzo IP fisso desiderato
   IPAddress gateway(192, 168, 1, 1);      // L'indirizzo IP del tuo router
   IPAddress subnet(255, 255, 255, 0);     // Subnet mask
   IPAddress dns(8, 8, 8, 8);              // DNS (Server DNS di Google)
   ```
4. Caricare lo sketch sulla scheda ESP32

## Utilizzo

1. Collegare la scheda ESP32 all'alimentazione
2. Aprire un browser web e navigare all'indirizzo IP configurato (es. `http://192.168.1.200`)
3. Nella pagina web, cliccare sul pulsante "Scan WiFi Networks" per avviare la scansione delle reti WiFi
4. Le reti trovate verranno visualizzate in una tabella con nome (SSID), potenza del segnale, canale e tipo di crittografia

## Note

Lo sketch utilizza la funzione `WiFi.scanNetworks()` dell'ESP32 per trovare tutte le reti WiFi disponibili nell'area. Per ogni rete trovata, vengono mostrate le seguenti informazioni:

- SSID (nome della rete)
- Potenza del segnale (in dBm e percentuale)
- Canale WiFi
- Tipo di crittografia (Open, WEP, WPA, WPA2, ecc.)

Per migliorare questo sketch, si potrebbero aggiungere:
- Salvare le configurazioni in EEPROM o SPIFFS
- Aggiungere autenticazione alla pagina web
- Aggiungere la possibilità di connettersi a una rete selezionata
- Aggiungere informazioni aggiuntive come MAC address (BSSID) dei router
