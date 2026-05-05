/**
 * QRCode - Libreria leggera per generazione QR Code
 * Basata su: https://github.com/ricmoo/QRCode
 * Licenza: MIT
 * Versione semplificata per ESP32
 */

#ifndef __QRCODE_H_
#define __QRCODE_H_

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C"{
#endif

// Error correction coding levels
typedef enum {
    ECC_LOW = 0,
    ECC_MEDIUM = 1,
    ECC_QUARTILE = 2,
    ECC_HIGH = 3
} qrcode_ecc;

// QRCode structure
typedef struct {
    uint8_t version;
    uint8_t size;
    uint8_t ecc;
    uint8_t mode;
    uint8_t mask;
    uint8_t *modules;
} QRCode;

// Get buffer size needed for a given version
#define qrcode_getBufferSize(version) (((version * 4 + 17) * (version * 4 + 17) + 7) / 8 + 1)

// Initialize QR Code with text
int8_t qrcode_initText(QRCode *qrcode, uint8_t *modules, uint8_t version, uint8_t ecc, const char *text);

// Initialize QR Code with bytes
int8_t qrcode_initBytes(QRCode *qrcode, uint8_t *modules, uint8_t version, uint8_t ecc, uint8_t *data, uint16_t length);

// Get module state at x, y
uint8_t qrcode_getModule(QRCode *qrcode, uint8_t x, uint8_t y);

#ifdef __cplusplus
}
#endif

#endif // __QRCODE_H_
