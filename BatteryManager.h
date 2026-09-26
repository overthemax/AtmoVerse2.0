#ifndef BATTERY_MANAGER_H
#define BATTERY_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA219.h>

// Batteria misurata esclusivamente con l'INA219 (tensione e corrente).
// Senza INA219 la batteria risulta "non disponibile" e non viene mostrata.

// INA219 su I2C (bus Wire, pin liberi della LOLIN32)
#define INA219_I2C_ADDR 0x41  // Indirizzo con A0 saldato
#define INA219_SDA_PIN 32
#define INA219_SCL_PIN 33

// INA219 montato con VIN+/VIN- invertiti: VIN- è lato batteria.
// Conseguenze: la tensione della batteria è la sola tensione di bus (non
// bus + shunt) e la corrente letta è positiva in carica. Il codice la
// riporta alla convenzione "positiva = scarica" usata in tutto il modulo.
// Metti 0 se in futuro l'INA219 viene montato nel verso standard.
#define INA219_REVERSED 1

// Pacco batteria: 2 celle LiPo da 3500 mAh in parallelo.
// Resistenza interna stimata (ohm): ~0,1 ohm per cella, dimezzata dal
// parallelo, più collegamenti e protezione. Serve a stimare la tensione a
// vuoto, da cui dipende la percentuale, mentre scorre corrente.
#define BATTERY_INTERNAL_RESISTANCE 0.06f

// Capacità del pacco (mAh), usata per la stima dell'autonomia: 2 x 3500
#define BATTERY_CAPACITY_MAH 7000.0f

// Soglia di corrente (mA) oltre la quale la batteria è in carica/scarica
#define BATTERY_CURRENT_THRESHOLD_MA 20.0f

// Livelli di avviso (con isteresi, calcolati sulla tensione a riposo filtrata).
// Con la curva della batteria: 15% ~ 3,55 V, 5% ~ 3,44 V.
#define BATTERY_LOW_PERCENT            15  // Avviso nel piè di pagina
#define BATTERY_LOW_EXIT_PERCENT       20
#define BATTERY_CRITICAL_PERCENT        5  // Schermata "batteria scarica" e sonno profondo
#define BATTERY_CRITICAL_EXIT_PERCENT   8
#define BATTERY_MIN_FIRMWARE_UPDATE_PERCENT 25  // Sotto: niente installazione firmware (se non in carica)

enum BatteryLevel {
    BATTERY_LEVEL_OK = 0,
    BATTERY_LEVEL_LOW,
    BATTERY_LEVEL_CRITICAL
};

enum BatteryState {
    BATTERY_UNKNOWN = 0,
    BATTERY_CHARGING,
    BATTERY_DISCHARGING,
    BATTERY_FULL,
    BATTERY_LOW,
    BATTERY_CRITICAL
};

class BatteryManager {
private:
    Adafruit_INA219 ina219;
    bool ina219Available = false;

    float voltage = 0.0f;       // Tensione ai morsetti della batteria
    float restVoltage = 0.0f;   // Tensione a vuoto stimata e filtrata (da cui la percentuale)
    float current_mA = 0.0f;    // Positiva = scarica, negativa = carica, qualunque sia il montaggio
    float power_mW = 0.0f;
    float shuntVoltage_mV = 0.0f;
    int percentage = 0;
    BatteryState state = BATTERY_UNKNOWN;
    BatteryLevel level = BATTERY_LEVEL_OK;
    bool isCharging = false;

    unsigned long lastRead = 0;
    static const unsigned long READ_INTERVAL = 30000;  // Lettura ogni 30 secondi

    bool initINA219();
    void readINA219();
    void detectChargingState();
    void updateLevel();
    void measure();

public:
    // Cerca l'INA219 e fa subito una prima lettura
    void begin();

    // Nuova lettura (al massimo ogni 30 secondi)
    void update();

    float getVoltage() { return voltage; }
    int getPercentage() { return percentage; }
    BatteryState getState() { return state; }
    bool charging() { return isCharging; }
    bool isLow() { return state == BATTERY_LOW || state == BATTERY_CRITICAL; }
    bool isCritical() { return state == BATTERY_CRITICAL; }
    BatteryLevel getLevel() { return level; }  // Livello di avviso con isteresi

    float getCurrent() { return current_mA; }  // mA (positiva = scarica, negativa = carica)
    float getPower() { return power_mW; }      // mW
    bool hasINA219() { return ina219Available; }
    bool isAvailable() { return ina219Available; }

    // Percentuale dalla tensione a riposo (curva LiPo)
    int voltageToPercentage(float v);

    // Autonomia stimata in minuti (-1 se in carica o non stimabile)
    int getEstimatedTimeRemaining();

    String getStatusString();
};

extern BatteryManager battery;

#endif // BATTERY_MANAGER_H
