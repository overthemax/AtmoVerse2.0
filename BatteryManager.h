#ifndef BATTERY_MANAGER_H
#define BATTERY_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA219.h>

// Pin ADC per lettura tensione batteria (configurabile)
#define BATTERY_ADC_PIN 35  // GPIO35 per WEMOS Lolin32 Lite (ADC1_CH7)
#define VOLTAGE_DIVIDER_RATIO 2.0  // Se hai un voltage divider R1=R2

// INA219 I2C (pin liberi su Lolin32 Lite)
#define INA219_I2C_ADDR 0x41  // Indirizzo I2C con A0 saldato
#define INA219_SDA_PIN 32     // GPIO32 per Lolin32 Lite (libero)
#define INA219_SCL_PIN 33     // GPIO33 per Lolin32 Lite (libero)

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

// Tensioni batteria LiPo standard (3.7V nominale)
#define BATTERY_MAX_VOLTAGE 4.2   // 100% carica
#define BATTERY_MIN_VOLTAGE 3.0   // 0% (non scaricare sotto!)
#define BATTERY_NOMINAL_VOLTAGE 3.7

// Stati batteria
enum BatteryState {
    BATTERY_UNKNOWN = 0,
    BATTERY_CHARGING,
    BATTERY_DISCHARGING,
    BATTERY_FULL,
    BATTERY_LOW,
    BATTERY_CRITICAL
};

// Classe per gestione batteria
class BatteryManager {
private:
    // INA219
    Adafruit_INA219 ina219;
    bool ina219Available;
    float current_mA;      // Corrente in mA (positiva = scarica, negativa = carica), qualunque sia il montaggio
    float power_mW;        // Potenza in mW
    float shuntVoltage_mV; // Tensione shunt in mV
    
    // ADC fallback
    int adcPin;
    bool adcAvailable;
    float voltageRatio;
    
    // Dati correnti
    float voltage;          // Tensione ai morsetti della batteria
    float restVoltage;      // Tensione a vuoto stimata e filtrata (da cui la percentuale)
    int percentage;
    BatteryState state;
    bool isCharging;
    
    // Calibrazione
    float vref;  // Tensione riferimento ADC (tipicamente 1.1V o 3.3V)
    int adcMax;  // Risoluzione ADC (tipicamente 4095 per 12-bit)
    
    // Timing
    unsigned long lastRead;
    const unsigned long READ_INTERVAL = 30000; // Leggi ogni 30 secondi
    
    // Storia per calcolare trend
    float voltageHistory[10];
    int historyIndex;
    int historyCount;
    
    // Stuck detection
    float lastVoltage;
    int stuckCounter;
    
    // Metodi privati
    float readVoltageRaw();      // Lettura ADC fallback
    void updateHistory(float v);
    float getVoltageTrend();
    void detectChargingState();
    bool initINA219();            // Inizializza INA219
    void readINA219();            // Leggi dati da INA219
    
public:
    BatteryManager();
    
    // Inizializza (prova INA219, poi fallback ADC)
    void begin(int pin = BATTERY_ADC_PIN, float ratio = VOLTAGE_DIVIDER_RATIO);
    
    // Configura se ADC è disponibile
    void setADCAvailable(bool available);
    
    // Leggi stato batteria
    void update();
    
    // Getter
    float getVoltage() { return voltage; }
    int getPercentage() { return percentage; }
    BatteryState getState() { return state; }
    bool charging() { return isCharging; }
    bool isLow() { return state == BATTERY_LOW || state == BATTERY_CRITICAL; }
    bool isCritical() { return state == BATTERY_CRITICAL; }
    
    // Getter INA219 specifici
    float getCurrent() { return current_mA; }       // mA (positiva=scarica, negativa=carica)
    float getPower() { return power_mW; }           // mW
    bool hasINA219() { return ina219Available; }    // INA219 presente?
    bool isAvailable() { return ina219Available || adcAvailable; }  // Qualsiasi sensore disponibile?
    
    // Calcola percentuale da tensione
    int voltageToPercentage(float v);
    
    // Ottieni tempo rimanente stimato (minuti)
    int getEstimatedTimeRemaining();
    
    // Info batteria come stringa
    String getStatusString();
    
    // Calibrazione manuale
    void calibrate(float actualVoltage);
    
    // Test se ADC funziona
    bool testADC();
};

// Istanza globale
extern BatteryManager battery;

#endif // BATTERY_MANAGER_H
