#ifndef QRCODE_HELPER_H
#define QRCODE_HELPER_H

#include <Arduino.h>
#include <GxEPD2_BW.h>

// Libreria QRCode leggera (includi qrcode.h nel progetto)
// https://github.com/ricmoo/QRCode
#include "qrcode.h"

class QRCodeHelper {
private:
    static const int MAX_QR_VERSION = 10;  // Versione QR (dimensione)
    
    // Buffer per QR code
    uint8_t qrcodeData[qrcode_getBufferSize(MAX_QR_VERSION)];
    
    // QR code generato
    QRCode qrcode;
    bool qrGenerated;
    
public:
    QRCodeHelper();
    
    // Genera QR code per setup WiFi
    bool generateWiFiQR(const char* ssid, const char* password, const char* security = "WPA");
    
    // Genera QR code per URL
    bool generateURLQR(const char* url);
    
    // Genera QR code per testo generico
    bool generateTextQR(const char* text);
    
    // Disegna QR code su display e-ink (template generico)
    template<typename T, uint16_t H>
    void drawQRCode(GxEPD2_BW<T, H>& display, int x, int y, int moduleSize = 3);
    
    // Ottieni dimensione QR code generato
    int getQRSize();
    
    // Helper per creare payload WiFi
    static String createWiFiPayload(const char* ssid, const char* password, const char* security = "WPA");
    
    // Helper per creare URL configurazione
    static String createConfigURL(IPAddress ip);
};

// Implementazione template drawQRCode (deve essere nel header)
template<typename T, uint16_t H>
void QRCodeHelper::drawQRCode(GxEPD2_BW<T, H>& display, int x, int y, int moduleSize) {
    if (!qrGenerated) {
        Serial.println("[QR] Errore: QR code non ancora generato, impossibile disegnare");
        return;
    }
    
    Serial.printf("[QR] Inizio disegno QR code: size=%d, x=%d, y=%d, moduleSize=%d\n", 
                  qrcode.size, x, y, moduleSize);
    
    // Test diagnostico: stampa primi 10 moduli
    Serial.print("[QR] Test primi 10 moduli: ");
    for (int i = 0; i < 10 && i < qrcode.size * qrcode.size; i++) {
        int testCol = i % qrcode.size;
        int testRow = i / qrcode.size;
        bool testVal = qrcode_getModule(&qrcode, testCol, testRow);
        Serial.print(testVal ? "█" : "·");
    }
    Serial.println();
    
    int blackModules = 0;
    int whiteModules = 0;
    
    // Disegna ogni modulo del QR code
    for (uint8_t row = 0; row < qrcode.size; row++) {
        for (uint8_t col = 0; col < qrcode.size; col++) {
            // Usa qrcode_getModule per leggere il modulo
            bool isBlack = qrcode_getModule(&qrcode, col, row);
            
            int modX = x + col * moduleSize;
            int modY = y + row * moduleSize;
            
            if (isBlack) {
                blackModules++;
                // Disegna modulo nero
                display.fillRect(modX, modY, moduleSize, moduleSize, GxEPD_BLACK);
            } else {
                whiteModules++;
                // Disegna modulo bianco esplicitamente per e-ink
                display.fillRect(modX, modY, moduleSize, moduleSize, GxEPD_WHITE);
            }
        }
    }
    
    Serial.printf("[QR] QR code completato: %d moduli neri, %d bianchi, totale=%d\n", 
                  blackModules, whiteModules, blackModules + whiteModules);
    Serial.printf("[QR] Dimensioni pixel: %dx%d\n", 
                  qrcode.size * moduleSize, qrcode.size * moduleSize);
}

// Funzione standalone per disegnare QR code direttamente (template generico)
template<typename T, uint16_t H>
void drawQuickQRCode(GxEPD2_BW<T, H>& display,
                     const char* text, int x, int y, int moduleSize = 3) {
    QRCodeHelper helper;
    if (helper.generateTextQR(text)) {
        helper.drawQRCode(display, x, y, moduleSize);
    }
}

#endif // QRCODE_HELPER_H
