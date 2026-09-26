#ifndef QRCODE_HELPER_H
#define QRCODE_HELPER_H

#include <Arduino.h>
#include <GxEPD2_BW.h>

// Codici QR generati con il componente "qrcode" di ESP-IDF (esp_qrcode),
// già incluso nel core ESP32: nessuna libreria esterna da installare.
class QRCodeHelper {
public:
  // Versione massima: 6 = 41x41 moduli, basta per SSID e password lunghi
  static const int MAX_VERSION = 6;
  static const int MAX_SIZE = MAX_VERSION * 4 + 17;

  // Codice per collegarsi a una rete WiFi (WIFI:T:WPA;S:...;P:...;;)
  bool generateWiFiQR(const char* ssid, const char* password, const char* security = "WPA");

  // Codice per un testo qualsiasi (es. un indirizzo http://...)
  bool generateTextQR(const char* text);

  // Lato del codice in moduli, 0 se non generato
  int getQRSize() const { return size; }

  // Disegna il codice con l'angolo in alto a sinistra in (x, y)
  template <typename T, uint16_t H>
  void drawQRCode(GxEPD2_BW<T, H>& display, int x, int y, int moduleSize = 3) const;

private:
  uint8_t size = 0;
  uint8_t modules[(MAX_SIZE * MAX_SIZE + 7) / 8] = {};

  bool module(int mx, int my) const {
    int i = my * size + mx;
    return modules[i / 8] & (1 << (i % 8));
  }
  friend void qrCopyModules(const uint8_t* qrcode);
};

template <typename T, uint16_t H>
void QRCodeHelper::drawQRCode(GxEPD2_BW<T, H>& display, int x, int y, int moduleSize) const {
  // Margine bianco di 2 moduli: i lettori QR ne hanno bisogno per riconoscerlo
  display.fillRect(x - 2 * moduleSize, y - 2 * moduleSize, (size + 4) * moduleSize, (size + 4) * moduleSize,
                   GxEPD_WHITE);
  for (int my = 0; my < size; my++) {
    for (int mx = 0; mx < size; mx++) {
      if (module(mx, my)) {
        display.fillRect(x + mx * moduleSize, y + my * moduleSize, moduleSize, moduleSize, GxEPD_BLACK);
      }
    }
  }
}

#endif // QRCODE_HELPER_H
