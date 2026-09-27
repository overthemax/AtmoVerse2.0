#ifndef QRCODE_HELPER_H
#define QRCODE_HELPER_H

#include <Arduino.h>
#include <GxEPD2_BW.h>

// QR codes generated with the ESP-IDF "qrcode" component (esp_qrcode),
// already part of the ESP32 core: no external library to install.
class QRCodeHelper {
public:
  // Maximum version: 6 = 41x41 modules, enough for long SSIDs and passwords
  static const int MAX_VERSION = 6;
  static const int MAX_SIZE = MAX_VERSION * 4 + 17;

  // Code to join a WiFi network (WIFI:T:WPA;S:...;P:...;;)
  bool generateWiFiQR(const char* ssid, const char* password, const char* security = "WPA");

  // Code for any text (e.g. an http://... address)
  bool generateTextQR(const char* text);

  // Side of the code in modules, 0 if not generated
  int getQRSize() const { return size; }

  // Draws the code with its top-left corner at (x, y)
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
  // White margin of 2 modules: QR readers need it to recognize the code
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
