#ifndef WEBMINIMAL_H
#define WEBMINIMAL_H

#include <Arduino.h>

// Pagina di configurazione minima contenuta nel firmware (HTML, CSS e JS in un
// unico file, nessuna dipendenza dalla SD). Usata quando le pagine complete non
// sono sulla SD: SD nuova o vuota, prima installazione. Dopo la configurazione
// il dispositivo scarica le pagine complete da GitHub.
#include <WiFi.h>
void sendFallbackSetupPage(WiFiClient& client);

#endif // WEBMINIMAL_H
