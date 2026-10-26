#ifndef BATTERY_MANAGER_H
#define BATTERY_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA219.h>

// Battery measured only with the INA219 (voltage and current).
// Without the INA219 the battery is "not available" and is not shown.

// INA219 on I2C (Wire bus, free pins of the LOLIN32)
#define INA219_I2C_ADDR 0x41  // Address with A0 soldered
#define INA219_SDA_PIN 32
#define INA219_SCL_PIN 33

// INA219 mounted with VIN+/VIN- swapped: VIN- is on the battery side.
// As a result the battery voltage is the bus voltage alone (not
// bus + shunt) and the current reads positive while charging. The code
// converts it to the "positive = discharging" convention used in this module.
// Set to 0 if the INA219 is ever mounted the standard way.
#define INA219_REVERSED 1

// Battery pack: 2 LiPo cells of 3500 mAh in parallel.
// Estimated internal resistance (ohm): ~0.1 ohm per cell, halved by the
// parallel connection, plus wiring and protection. Used to estimate the
// open-circuit voltage, which gives the percentage, while current flows.
#define BATTERY_INTERNAL_RESISTANCE 0.06f

// Pack capacity (mAh), used for the runtime estimate: 2 x 3500
#define BATTERY_CAPACITY_MAH 7000.0f

// Current (mA) above which the battery is charging/discharging
#define BATTERY_CURRENT_THRESHOLD_MA 20.0f

// Below this discharge current (mA) the board runs from the charger
// even if the battery, being full, draws no more current
#define BATTERY_EXTERNAL_POWER_MAX_MA 8.0f

// Warning levels (with hysteresis, on the filtered resting voltage).
// With the battery curve: 15% ~ 3.55 V, 5% ~ 3.44 V.
#define BATTERY_LOW_PERCENT            15  // Warning in the footer
#define BATTERY_LOW_EXIT_PERCENT       20
#define BATTERY_CRITICAL_PERCENT        5  // "Battery empty" screen and deep sleep
#define BATTERY_CRITICAL_EXIT_PERCENT   8
#define BATTERY_MIN_FIRMWARE_UPDATE_PERCENT 25  // Below this: no firmware installation (unless charging)

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

    float voltage = 0.0f;       // Voltage at the battery terminals
    float restVoltage = 0.0f;   // Estimated and filtered open-circuit voltage (gives the percentage)
    float current_mA = 0.0f;    // Positive = discharging, negative = charging, whatever the mounting
    float power_mW = 0.0f;
    float shuntVoltage_mV = 0.0f;
    int percentage = 0;
    // What the voltage curve gives for this pack when it is full (a full
    // LiPo rests at ~4.14 V, not at the curve's 4.20 V): learned at the end
    // of every charge and kept in NVS. Percentages are scaled so that this
    // value reads 100%, otherwise unplugging the charger jumped to ~96%.
    float fullPercent = 100.0f;
    BatteryState state = BATTERY_UNKNOWN;
    BatteryLevel level = BATTERY_LEVEL_OK;
    bool isCharging = false;

    unsigned long lastRead = 0;
    static const unsigned long READ_INTERVAL = 30000;  // One reading every 30 seconds

    bool initINA219();
    void readINA219();
    void detectChargingState();
    void updateLevel();
    void measure();
    void learnFullPercent(float raw);

public:
    // Looks for the INA219 and takes a first reading at once
    void begin();

    // New reading (at most every 30 seconds)
    void update();

    // Current only, to notice at once that the charger was plugged in/out.
    // Returns true if the "charging" state changed.
    bool pollCharging();

    float getVoltage() { return voltage; }
    int getPercentage() { return percentage; }
    BatteryState getState() { return state; }
    bool charging() { return isCharging; }
    // Charger plugged in and charge finished (battery at 100%)
    bool chargeComplete() { return isCharging && state == BATTERY_FULL && percentage >= 100; }
    bool isLow() { return state == BATTERY_LOW || state == BATTERY_CRITICAL; }
    bool isCritical() { return state == BATTERY_CRITICAL; }
    BatteryLevel getLevel() { return level; }  // Warning level with hysteresis

    float getCurrent() { return current_mA; }  // mA (positive = discharging, negative = charging)
    float getPower() { return power_mW; }      // mW
    bool hasINA219() { return ina219Available; }
    bool isAvailable() { return ina219Available; }

    // Percentage from the resting voltage (LiPo curve)
    int voltageToPercentage(float v);

    // Estimated runtime in minutes (-1 if charging or unknown)
    int getEstimatedTimeRemaining();

};

extern BatteryManager battery;

#endif // BATTERY_MANAGER_H
