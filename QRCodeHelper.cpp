#include "QRCodeHelper.h"

// NOTA: Questa implementazione richiede la libreria qrcode.h
// Installare con: arduino-cli lib install "QRCode"
// Oppure scaricare da: https://github.com/ricmoo/QRCode

QRCodeHelper::QRCodeHelper() {
    // Inizializza buffer
    memset(qrcodeData, 0, sizeof(qrcodeData));
    qrGenerated = false;
}

bool QRCodeHelper::generateWiFiQR(const char* ssid, const char* password, const char* security) {
    // Crea payload WiFi in formato standard
    String payload = createWiFiPayload(ssid, password, security);
    
    Serial.print("[QR] Generando QR WiFi per SSID: ");
    Serial.println(ssid);
    
    return generateTextQR(payload.c_str());
}

bool QRCodeHelper::generateURLQR(const char* url) {
    Serial.print("[QR] Generando QR URL: ");
    Serial.println(url);
    
    return generateTextQR(url);
}

bool QRCodeHelper::generateTextQR(const char* text) {
    // Determina versione QR necessaria in base alla lunghezza testo
    int textLen = strlen(text);
    Serial.printf("[QR] Lunghezza payload: %d caratteri\n", textLen);
    Serial.printf("[QR] Payload: %s\n", text);
    
    // Usa versione più alta per WiFi QR (almeno 4)
    uint8_t version = 4;  // Versione 4 per WiFi payload
    
    if (textLen > 100) version = 6;
    else if (textLen > 70) version = 5;
    else if (textLen > 50) version = 4;
    
    if (version > MAX_QR_VERSION) {
        Serial.println("[QR] Testo troppo lungo per QR code");
        qrGenerated = false;
        return false;
    }
    
    Serial.printf("[QR] Usando versione QR: %d\n", version);
    
    // Genera QR code e salva nel membro (prova con ECC_MEDIUM)
    Serial.println("[QR] Tentativo generazione con ECC_MEDIUM...");
    int8_t result = qrcode_initText(&qrcode, qrcodeData, version, ECC_MEDIUM, text);
    
    if (result != 0) {
        Serial.printf("[QR] ERRORE generazione QR: %d\n", result);
        Serial.println("[QR] Possibili cause: buffer troppo piccolo, versione insufficiente");
        qrGenerated = false;
        return false;
    }
    
    qrGenerated = true;
    Serial.printf("[QR] QR code generato con successo: %dx%d moduli\n", qrcode.size, qrcode.size);
    Serial.printf("[QR] Versione effettiva: %d, ECC: MEDIUM\n", qrcode.version);
    
    // Stampa QR code ASCII nel serial per debug
    Serial.println("[QR] ===== QR CODE ASCII =====");
    for (uint8_t y = 0; y < qrcode.size; y++) {
        Serial.print("[QR] ");
        for (uint8_t x = 0; x < qrcode.size; x++) {
            Serial.print(qrcode_getModule(&qrcode, x, y) ? "██" : "  ");
        }
        Serial.println();
    }
    Serial.println("[QR] ===========================");
    
    return true;
}

// drawQRCode è ora template implementato in QRCodeHelper.h

int QRCodeHelper::getQRSize() {
    if (!qrGenerated) {
        Serial.println("[QR] Errore: QR code non ancora generato");
        return 0;
    }
    return qrcode.size;
}

String QRCodeHelper::createWiFiPayload(const char* ssid, const char* password, const char* security) {
    // Formato: WIFI:T:WPA;S:mynetwork;P:mypassword;;
    String payload = "WIFI:";
    payload += "T:";
    payload += security;
    payload += ";S:";
    payload += ssid;
    payload += ";P:";
    payload += password;
    payload += ";;";
    
    return payload;
}

String QRCodeHelper::createConfigURL(IPAddress ip) {
    String url = "http://";
    url += ip.toString();
    url += "/";
    
    return url;
}

// drawQuickQRCode è ora template implementato in QRCodeHelper.h
