#pragma once

// Include this file in WebServer.cpp to enable optimized functions
// that reduce sketch size when OPTIMIZE_SIZE is defined

#ifdef OPTIMIZE_SIZE

// Versioni ottimizzate di alcune funzioni chiave per risparmiare spazio
// Necessari per i tipi usati (WiFiClient, millis, FPSTR)
#include <Arduino.h>
#include <WiFi.h>
#include "WebMinimal.h"

// Dichiarazioni esterne necessarie
extern WiFiServer server;

// Gestione cliente ulteriormente ottimizzata
inline void handleClientRequestsMinimal() {
  WiFiClient client = server.accept();
  
  if (client) {
    // Semplice timeout
    unsigned long timeout = millis() + 2000; // Ridotto il timeout
#if defined(MINIMIZE_BUFFERS)
    // Usa char array invece di String per ridurre l'uso della DRAM
    char currentLine[48] = ""; // Buffer più piccolo per la linea corrente
    char requestPath[32] = ""; // Buffer più piccolo per il percorso
    uint8_t currentLinePos = 0;
#else
    String currentLine = "";
    String requestPath = "";
#endif
    bool requestLineComplete = false;
    
    while (client.connected() && millis() < timeout) {
      if (client.available()) {
        char c = client.read();
        
        // Raccogliamo la riga della richiesta
        if (!requestLineComplete) {
          if (c == '\n') {
#if defined(MINIMIZE_BUFFERS)
            // Versione ottimizzata per char array
            char* start = strchr(currentLine, ' ');
            if (start) {
              start++;
              char* end = strchr(start, ' ');
              if (end) {
                int pathLen = end - start;
                if (pathLen < 31) { // Previeni overflow
                  memcpy(requestPath, start, pathLen);
                  requestPath[pathLen] = '\0';
                }
              }
            }
#else
            // Estrai il percorso
            int startPos = currentLine.indexOf(' ') + 1;
            int endPos = currentLine.indexOf(' ', startPos);
            requestPath = currentLine.substring(startPos, endPos);
#endif
            requestLineComplete = true;
          } else if (c != '\r') {
#if defined(MINIMIZE_BUFFERS)
            // Aggiungi al buffer statico con controllo overflow
            if (currentLinePos < sizeof(currentLine) - 1) {
              currentLine[currentLinePos++] = c;
              currentLine[currentLinePos] = '\0';
            }
#else
            currentLine += c;
#endif
          }
        }
        
        // Fine intestazioni
        if (c == '\n'
#if defined(MINIMIZE_BUFFERS)
            && strlen(currentLine) == 0
#else
            && currentLine.length() == 0
#endif
        ) {
          // Rispondiamo alla richiesta (con flag per evitare else-if tra rami di preprocessore)
          bool handled = false;
#if defined(MINIMIZE_BUFFERS)
          // Verifica se il percorso inizia con "/api/"
          if (strncmp(requestPath, "/api/", 5) == 0) {
            handled = true;
            // API minime ulteriormente ottimizzate
            if (strcmp(requestPath, "/api/weather") == 0) {
              static const char JSON_RESPONSE[] PROGMEM = "{\"t\":20.5,\"w\":\"c\"}"; // Nomi campo accorciati
              client.println(F("HTTP/1.1 200 OK"));
              client.println(F("Content-Type: application/json"));
              client.println(F("Connection: close"));
              client.println();
              client.print(FPSTR(JSON_RESPONSE));
            } else {
              client.println(F("HTTP/1.1 404 Not Found"));
              client.println(F("Connection: close"));
              client.println();
            }
          }
#else
          if (requestPath.startsWith("/api/")) {
            handled = true;
            // API minime
            if (requestPath == "/api/weather") {
              static const char JSON_RESPONSE[] PROGMEM = "{\"temp\":20.5,\"weather\":\"clear\"}";
              client.println(F("HTTP/1.1 200 OK"));
              client.println(F("Content-Type: application/json"));
              client.println(F("Connection: close"));
              client.println();
              client.print(FPSTR(JSON_RESPONSE));
            } else {
              client.println(F("HTTP/1.1 404 Not Found"));
              client.println(F("Connection: close"));
              client.println();
            }
          }
#endif

          if (!handled) {
#if defined(MINIMIZE_BUFFERS)
            if (strcmp(requestPath, "/") == 0 || strcmp(requestPath, "/index.html") == 0) {
#else
            if (requestPath == "/" || requestPath == "/index.html") {
#endif
              // Pagina principale minima
              client.println(F("HTTP/1.1 200 OK"));
              client.println(F("Content-Type: text/html"));
              client.println(F("Connection: close"));
              client.println();
              client.print(generateMinimalMainPage());
#if defined(MINIMIZE_BUFFERS)
            } else if (strcmp(requestPath, "/quotes") == 0) {
#else
            } else if (requestPath == "/quotes") {
#endif
              // Pagina citazioni minima
              client.println(F("HTTP/1.1 200 OK"));
              client.println(F("Content-Type: text/html"));
              client.println(F("Connection: close"));
              client.println();
              client.print(generateMinimalQuotesPage());
#if defined(MINIMIZE_BUFFERS)
            } else if (strcmp(requestPath, "/setup") == 0) {
#else
            } else if (requestPath == "/setup") {
#endif
              // Pagina setup minima
              client.println(F("HTTP/1.1 200 OK"));
              client.println(F("Content-Type: text/html"));
              client.println(F("Connection: close"));
              client.println();
              client.print(generateMinimalSetupPage("AtmoVerse"));
            } else {
              // Prova a servire dalla SD se esiste, altrimenti 404
              if (!serveFileFromSD(client,
#if defined(MINIMIZE_BUFFERS)
                                   requestPath
#else
                                   requestPath.c_str()
#endif
                                   )) {
                client.println(F("HTTP/1.1 404 Not Found"));
                client.println(F("Connection: close"));
                client.println();
                client.print(F("<html><body><h1>404 Not Found</h1></body></html>"));
              }
            }
          }
          break;
        }
        
        // Reset linea corrente dopo ogni newline
        if (c == '\n') {
#if defined(MINIMIZE_BUFFERS)
          currentLine[0] = '\0';
          currentLinePos = 0;
#else
          currentLine = "";
#endif
        }
      }
    }
    
    // Chiudi la connessione
    client.stop();
  }
}

// Versione ottimizzata della funzione per processare richieste web
inline void processWebRequestsMinimal() {
  handleClientRequestsMinimal();
}

// Nota: le macro di sostituzione sono state rimosse per evitare conflitti
// con le definizioni in WebServer.cpp. Se vuoi forzare la sostituzione,
// definisci WEBSERVEROPT_REPLACE prima di includere questo header.
#ifdef WEBSERVEROPT_REPLACE
#define handleClientRequests handleClientRequestsMinimal
#define processWebRequests processWebRequestsMinimal
#endif

#endif // OPTIMIZE_SIZE
