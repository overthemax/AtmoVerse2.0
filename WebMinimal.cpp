#include "WebMinimal.h"
#include "Config.h"

// NOTA: Funzioni minimali per risparmiare spazio nel firmware.
// I contenuti web sono stati spostati sulla SD card nella cartella /www/

// Pagina principale (stub minimo)
String generateMinimalMainPage() {
  return "<!DOCTYPE html><html><body><h1>AtmoVerse</h1><p>Verifica SD card</p></body></html>";
}

// Pagina citazioni (stub minimo)
String generateMinimalQuotesPage() {
  return "<!DOCTYPE html><html><body><h1>Citazioni</h1><p>Verifica SD card</p></body></html>";
}

// Pagina info (stub minimo)
String generateMinimalInfoPage() {
  return "<!DOCTYPE html><html><body><h1>Info</h1></body></html>";
}

// Pagina setup WiFi (stub minimo)
String generateMinimalSetupPage(String ssid) {
  String html = "<!DOCTYPE html><html><body><h1>Setup WiFi</h1>";
  html += "<form action='/connect' method='post'>";
  html += "<input name='ssid' value='" + ssid + "'>";
  html += "<input name='password' type='password'>";
  html += "<button>Connetti</button></form></body></html>";
  return html;
}