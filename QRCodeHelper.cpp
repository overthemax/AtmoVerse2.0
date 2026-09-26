#include "QRCodeHelper.h"
#include <qrcode.h>  // Componente esp_qrcode del core ESP32

// esp_qrcode_generate() passa il risultato a una funzione senza contesto:
// l'helper di destinazione è indicato qui per la durata della chiamata
static QRCodeHelper* qrTarget = nullptr;

void qrCopyModules(const uint8_t* qrcode) {
  QRCodeHelper* t = qrTarget;
  if (!t) return;
  int n = esp_qrcode_get_size(qrcode);
  if (n <= 0 || n > QRCodeHelper::MAX_SIZE) return;
  t->size = n;
  memset(t->modules, 0, sizeof(t->modules));
  for (int y = 0; y < n; y++) {
    for (int x = 0; x < n; x++) {
      if (esp_qrcode_get_module(qrcode, x, y)) {
        int i = y * n + x;
        t->modules[i / 8] |= 1 << (i % 8);
      }
    }
  }
}

// Nel formato WIFI: i caratteri \ ; , : " vanno preceduti da una barra
static String escapeWiFiField(const char* s) {
  String out;
  for (; *s; s++) {
    if (strchr("\\;,:\"", *s)) out += '\\';
    out += *s;
  }
  return out;
}

bool QRCodeHelper::generateWiFiQR(const char* ssid, const char* password, const char* security) {
  String payload = String("WIFI:T:") + security + ";S:" + escapeWiFiField(ssid) +
                   ";P:" + escapeWiFiField(password) + ";;";
  return generateTextQR(payload.c_str());
}

bool QRCodeHelper::generateTextQR(const char* text) {
  esp_qrcode_config_t cfg = {};
  cfg.display_func = qrCopyModules;
  cfg.max_qrcode_version = MAX_VERSION;
  cfg.qrcode_ecc_level = ESP_QRCODE_ECC_MED;

  size = 0;
  qrTarget = this;
  esp_err_t err = esp_qrcode_generate(&cfg, text);
  qrTarget = nullptr;

  if (err != ESP_OK || size == 0) {
    Serial.printf("[QR] Generazione non riuscita (errore %d)\n", (int)err);
    size = 0;
    return false;
  }
  return true;
}
